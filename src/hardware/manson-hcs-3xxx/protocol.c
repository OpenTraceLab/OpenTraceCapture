/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2014 Matthias Heidbrink <m-opentracelab@heidbrink.biz>
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

#define REQ_TIMEOUT_MS 500

OTC_PRIV int hcs_send_cmd(struct otc_serial_dev_inst *serial, const char *cmd, ...)
{
	int ret;
	char cmdbuf[50];
	char *cmd_esc;
	va_list args;

	va_start(args, cmd);
	vsnprintf(cmdbuf, sizeof(cmdbuf), cmd, args);
	va_end(args);

	cmd_esc = g_strescape(cmdbuf, NULL);
	otc_dbg("Sending '%s'.", cmd_esc);
	g_free(cmd_esc);

	if ((ret = serial_write_blocking(serial, cmdbuf, strlen(cmdbuf),
			serial_timeout(serial, strlen(cmdbuf)))) < 0) {
		otc_err("Error sending command: %d.", ret);
		return ret;
	}

	return ret;
}

/**
 * Read data from interface into buffer blocking until @a lines number of \\r chars
 * received.
 *
 * @param serial Previously initialized serial port structure.
 * @param[in] lines Number of \\r-terminated lines to read (1-n).
 * @param buf Buffer for result. Contents is NUL-terminated on success.
 * @param[in] buflen Buffer length (>0).
 *
 * @retval OTC_OK Lines received and ending with "OK\r" (success).
 * @retval OTC_ERR Error.
 * @retval OTC_ERR_ARG Invalid argument.
 */
OTC_PRIV int hcs_read_reply(struct otc_serial_dev_inst *serial, int lines, char *buf, int buflen)
{
	int l_recv = 0;
	int bufpos = 0;
	int retc;

	if (!serial || (lines <= 0) || !buf || (buflen <= 0))
		return OTC_ERR_ARG;

	while ((l_recv < lines) && (bufpos < (buflen + 1))) {
		retc = serial_read_blocking(serial, &buf[bufpos], 1, 0);
		if (retc != 1)
			return OTC_ERR;
		if (buf[bufpos] == '\r')
			l_recv++;
		bufpos++;
	}
	buf[bufpos] = '\0';

	if ((l_recv == lines) && (g_str_has_suffix(buf, "OK\r")))
		return OTC_OK;
	else
		return OTC_ERR;
}

/** Interpret result of GETD command. */
OTC_PRIV int hcs_parse_volt_curr_mode(struct otc_dev_inst *sdi, char **tokens)
{
	char *str;
	double val;
	struct dev_context *devc;

	devc = sdi->priv;

	/* Bytes 0-3: Voltage. */
	str = g_strndup(tokens[0], 4);
	val = g_ascii_strtod(str, NULL) / 100;
	devc->voltage = val;
	g_free(str);

	/* Bytes 4-7: Current. */
	str = g_strndup((tokens[0] + 4), 4);
	val = g_ascii_strtod(str, NULL) / 100;
	devc->current = val;
	g_free(str);

	/* Byte 8: Mode ('0' means CV, '1' means CC). */
	devc->cc_mode = (tokens[0][8] == '1');

	/* Output enabled? Works because voltage cannot be set to 0.0 directly. */
	devc->output_enabled = devc->voltage != 0.0;

	return OTC_OK;
}

static void send_sample(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;

	devc = sdi->priv;

	otc_analog_init(&analog, &encoding, &meaning, &spec, 2);

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;

	analog.meaning->mq = OTC_MQ_VOLTAGE;
	analog.meaning->unit = OTC_UNIT_VOLT;
	analog.meaning->mqflags = OTC_MQFLAG_DC;
	analog.data = &devc->voltage;
	otc_session_send(sdi, &packet);

	analog.meaning->mq = OTC_MQ_CURRENT;
	analog.meaning->unit = OTC_UNIT_AMPERE;
	analog.meaning->mqflags = 0;
	analog.data = &devc->current;
	otc_session_send(sdi, &packet);


	otc_sw_limits_update_samples_read(&devc->limits, 1);
}

static int parse_reply(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	char *reply_esc, **tokens;
	int retc;

	devc = sdi->priv;

	reply_esc = g_strescape(devc->buf, NULL);
	otc_dbg("Received '%s'.", reply_esc);
	g_free(reply_esc);

	tokens = g_strsplit(devc->buf, "\r", 0);
	retc = hcs_parse_volt_curr_mode(sdi, tokens);
	g_strfreev(tokens);
	if (retc < 0)
		return OTC_ERR;

	send_sample(sdi);

	return OTC_OK;
}

static int handle_new_data(struct otc_dev_inst *sdi)
{
	int len;
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;

	devc = sdi->priv;
	serial = sdi->conn;

	len = serial_read_blocking(serial, devc->buf + devc->buflen, 1, 0);
	if (len < 1)
		return OTC_ERR;

	devc->buflen += len;
	devc->buf[devc->buflen] = '\0';

	/* Wait until we received an "OK\r" (among other bytes). */
	if (!g_str_has_suffix(devc->buf, "OK\r"))
		return OTC_OK;

	parse_reply(sdi);

	devc->buf[0] = '\0';
	devc->buflen = 0;

	devc->reply_pending = FALSE;

	return OTC_OK;
}

/** Driver/serial data reception function. */
OTC_PRIV int hcs_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	uint64_t elapsed_us;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;

	if (revents == G_IO_IN) {
		/* New data arrived. */
		handle_new_data(sdi);
	} else {
		/* Timeout. */
	}

	if (otc_sw_limits_check(&devc->limits)) {
		otc_dev_acquisition_stop(sdi);
		return TRUE;
	}

	/* Request next packet, if required. */
	if (sdi->status == OTC_ST_ACTIVE) {
		if (devc->reply_pending) {
			elapsed_us = g_get_monotonic_time() - devc->req_sent_at;
			if (elapsed_us > (REQ_TIMEOUT_MS * 1000))
				devc->reply_pending = FALSE;
			return TRUE;
		}

		/* Send command to get voltage, current, and mode (CC or CV). */
		if (hcs_send_cmd(serial, "GETD\r") < 0)
			return TRUE;

		devc->req_sent_at = g_get_monotonic_time();
		devc->reply_pending = TRUE;
	}

	return TRUE;
}
