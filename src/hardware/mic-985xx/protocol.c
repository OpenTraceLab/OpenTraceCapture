/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"

static int mic_send(struct otc_serial_dev_inst *serial, const char *cmd)
{
	int ret;

	if ((ret = serial_write_blocking(serial, cmd, strlen(cmd),
			serial_timeout(serial, strlen(cmd)))) < 0) {
		otc_err("Error sending '%s' command: %d.", cmd, ret);
		return OTC_ERR;
	}

	return OTC_OK;
}

OTC_PRIV int mic_cmd_get_device_info(struct otc_serial_dev_inst *serial)
{
	return mic_send(serial, "I\r");
}

static int mic_cmd_set_realtime_mode(struct otc_serial_dev_inst *serial)
{
	return mic_send(serial, "S 1 M 2 32 3\r");
}

OTC_PRIV gboolean packet_valid_temp(const uint8_t *buf)
{
	if (buf[0] != 'v' || buf[1] != ' ' || buf[5] != '\r')
		return FALSE;

	if (!isdigit(buf[2]) || !isdigit(buf[3]) || !isdigit(buf[4]))
		return FALSE;

	return TRUE;
}

OTC_PRIV gboolean packet_valid_temp_hum(const uint8_t *buf)
{
	if (buf[0] != 'v' || buf[1] != ' ' || buf[5] != ' ' || buf[9] != '\r')
		return FALSE;

	if (!isdigit(buf[2]) || !isdigit(buf[3]) || !isdigit(buf[4]))
		return FALSE;

	if (!isdigit(buf[6]) || !isdigit(buf[7]) || !isdigit(buf[8]))
		return FALSE;

	return TRUE;
}

static int packet_parse(const char *buf, int idx, float *temp, float *humidity)
{
	char tmp[4];

	/* Packet format MIC98581: "v ttt\r". */
	/* Packet format MIC98583: "v ttt hhh\r". */

	/* TODO: Sanity check on buf. For now we assume well-formed ASCII. */

	tmp[3] = '\0';

	strncpy((char *)&tmp, &buf[2], 3);
	*temp = g_ascii_strtoull((const char *)&tmp, NULL, 10) / 10;

	if (mic_devs[idx].has_humidity) {
		strncpy((char *)&tmp, &buf[6], 3);
		*humidity = g_ascii_strtoull((const char *)&tmp, NULL, 10) / 10;
	}

	return OTC_OK;
}

static int handle_packet(const uint8_t *buf, struct otc_dev_inst *sdi, int idx)
{
	float temperature, humidity;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	struct dev_context *devc;
	GSList *l;
	int ret;

	(void)idx;

	devc = sdi->priv;

	ret = packet_parse((const char *)buf, idx, &temperature, &humidity);
	if (ret < 0) {
		otc_err("Failed to parse packet.");
		return OTC_ERR;
	}

	otc_analog_init(&analog, &encoding, &meaning, &spec, 1);

	/* Common values for both channels. */
	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	analog.num_samples = 1;

	/* Temperature. */
	l = g_slist_copy(sdi->channels);
	l = g_slist_remove_link(l, g_slist_nth(l, 1));
	meaning.channels = l;
	meaning.mq = OTC_MQ_TEMPERATURE;
	meaning.unit = OTC_UNIT_CELSIUS; /* TODO: Use C/F correctly. */
	analog.data = &temperature;
	otc_session_send(sdi, &packet);
	g_slist_free(l);

	/* Humidity. */
	if (mic_devs[idx].has_humidity) {
		l = g_slist_copy(sdi->channels);
		l = g_slist_remove_link(l, g_slist_nth(l, 0));
		meaning.channels = l;
		meaning.mq = OTC_MQ_RELATIVE_HUMIDITY;
		meaning.unit = OTC_UNIT_PERCENTAGE;
		analog.data = &humidity;
		otc_session_send(sdi, &packet);
		g_slist_free(l);
	}

	otc_sw_limits_update_samples_read(&devc->limits, 1);

	return OTC_OK;
}

static void handle_new_data(struct otc_dev_inst *sdi, int idx)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	int len, offset;

	devc = sdi->priv;
	serial = sdi->conn;

	/* Try to get as much data as the buffer can hold. */
	len = SERIAL_BUFSIZE - devc->buflen;
	len = serial_read_nonblocking(serial, devc->buf + devc->buflen, len);
	if (len < 1) {
		otc_err("Serial port read error: %d.", len);
		return;
	}

	devc->buflen += len;

	/* Now look for packets in that data. */
	offset = 0;
	while ((devc->buflen - offset) >= mic_devs[idx].packet_size) {
		if (mic_devs[idx].packet_valid(devc->buf + offset)) {
			handle_packet(devc->buf + offset, sdi, idx);
			offset += mic_devs[idx].packet_size;
		} else {
			offset++;
		}
	}

	/* If we have any data left, move it to the beginning of our buffer. */
	if (offset < devc->buflen)
		memmove(devc->buf, devc->buf + offset, devc->buflen - offset);
	devc->buflen -= offset;
}

static int receive_data(int fd, int revents, int idx, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	static gboolean first_time = TRUE;
	struct otc_serial_dev_inst *serial;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;

	if (revents == G_IO_IN) {
		/* New data arrived. */
		handle_new_data(sdi, idx);
	} else {
		/* Timeout. */
		if (first_time) {
			mic_cmd_set_realtime_mode(serial);
			first_time = FALSE;
		}
	}

	if (otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop(sdi);

	return TRUE;
}

#define RECEIVE_DATA(ID_UPPER) \
OTC_PRIV int receive_data_##ID_UPPER(int fd, int revents, void *cb_data) { \
	return receive_data(fd, revents, ID_UPPER, cb_data); }

/* Driver-specific receive_data() wrappers */
RECEIVE_DATA(MIC_98581)
RECEIVE_DATA(MIC_98583)
