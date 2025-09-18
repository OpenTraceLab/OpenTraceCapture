/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <string.h>
#include "protocol.h"

OTC_PRIV int testo_set_serial_params(struct otc_usb_dev_inst *usb)
{
	int ret;

	if ((ret = libusb_control_transfer(usb->devhdl, 0x40, FTDI_SET_BAUDRATE,
			FTDI_BAUDRATE_115200, FTDI_INDEX, NULL, 0, 10)) < 0) {
		otc_err("Failed to set baudrate: %s", libusb_error_name(ret));
		return OTC_ERR;
	}

	if ((ret = libusb_control_transfer(usb->devhdl, 0x40, FTDI_SET_PARAMS,
			FTDI_PARAMS_8N1, FTDI_INDEX, NULL, 0, 10)) < 0) {
		otc_err("Failed to set comm parameters: %s", libusb_error_name(ret));
		return OTC_ERR;
	}

	if ((ret = libusb_control_transfer(usb->devhdl, 0x40, FTDI_SET_FLOWCTRL,
			FTDI_FLOW_NONE, FTDI_INDEX, NULL, 0, 10)) < 0) {
		otc_err("Failed to set flow control: %s", libusb_error_name(ret));
		return OTC_ERR;
	}

	if ((ret = libusb_control_transfer(usb->devhdl, 0x40, FTDI_SET_MODEMCTRL,
			FTDI_MODEM_ALLHIGH, FTDI_INDEX, NULL, 0, 10)) < 0) {
		otc_err("Failed to set modem control: %s", libusb_error_name(ret));
		return OTC_ERR;
	}

	return OTC_OK;
}

/* Due to the modular nature of the Testo hardware, you can't assume
 * which measurements the device will supply. Fetch a single result
 * set synchronously to see which measurements it has. */
OTC_PRIV int testo_probe_channels(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_usb_dev_inst *usb;
	int unit, packet_len, len, i;
	unsigned char packet[MAX_REPLY_SIZE], buf[MAX_REPLY_SIZE];
	const char *probe_name;

	devc = sdi->priv;
	usb = sdi->conn;

	otc_dbg("Probing for channels.");
	if (otc_dev_open(sdi) != OTC_OK)
		return OTC_ERR;
	if (testo_set_serial_params(usb) != OTC_OK)
		return OTC_ERR;

	/* Flush anything buffered from a previous run. */
	do {
		libusb_bulk_transfer(usb->devhdl, EP_IN, buf, MAX_REPLY_SIZE, &len, 10);
	} while (len > 2);

	if (libusb_bulk_transfer(usb->devhdl, EP_OUT, (unsigned char *)devc->model->request,
			devc->model->request_size, &devc->reply_size, 10) < 0)
		return OTC_ERR;

	packet_len = 0;
	while (TRUE) {
		if (libusb_bulk_transfer(usb->devhdl, EP_IN, buf, MAX_REPLY_SIZE,
				&len, 250) < 0)
			return OTC_ERR;
		if (len == 2)
			/* FTDI cruft */
			continue;
		if (packet_len + len - 2 > MAX_REPLY_SIZE)
			return OTC_ERR;

		memcpy(packet + packet_len, buf + 2, len - 2);
		packet_len += len - 2;
		if (packet_len < 5)
			/* Not even enough to check prefix yet. */
			continue;

		if (!testo_check_packet_prefix(packet, packet_len)) {
			/* Tail end of some previous data, drop it. */
			packet_len = 0;
			continue;
		}

		if (packet_len >= 7 + packet[6] * 7 + 2)
			/* Got a complete packet. */
			break;
	}
	otc_dev_close(sdi);

	if (packet[6] > MAX_CHANNELS) {
		otc_err("Device says it has %d channels!", packet[6]);
		return OTC_ERR;
	}

	for (i = 0; i < packet[6]; i++) {
		unit = packet[7 + i * 7 + 4];
		devc->channel_units[i] = unit;
		switch (unit) {
		case 1:
			probe_name = "Temperature";
			break;
		case 3:
			probe_name = "Humidity";
			break;
		case 5:
			probe_name = "Windspeed";
			break;
		case 24:
			probe_name = "Pressure";
			break;
		default:
			otc_dbg("Unsupported measurement unit %d", unit);
			return OTC_ERR;
		}
		otc_channel_new(sdi, i, OTC_CHANNEL_ANALOG, TRUE, probe_name);
	}
	devc->num_channels = packet[6];
	otc_dbg("Found %d channel%s.", devc->num_channels,
			devc->num_channels > 1 ? "s" : "");

	return OTC_OK;
}

OTC_PRIV int testo_request_packet(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_usb_dev_inst *usb;
	int ret;

	devc = sdi->priv;
	usb = sdi->conn;

	libusb_fill_bulk_transfer(devc->out_transfer, usb->devhdl, EP_OUT,
			(unsigned char *)devc->model->request, devc->model->request_size,
			receive_transfer, (void *)sdi, 100);
	if ((ret = libusb_submit_transfer(devc->out_transfer) != 0)) {
		otc_err("Failed to request packet: %s.", libusb_error_name(ret));
		otc_dev_acquisition_stop((struct otc_dev_inst *)sdi);
		return OTC_ERR;
	}
	otc_dbg("Requested new packet.");

	return OTC_OK;
}

/* Check if the packet is well-formed. This matches packets for the
 * Testo 175/177/400/650/950/435/635/735/445/645/945/946/545. */
OTC_PRIV gboolean testo_check_packet_prefix(unsigned char *buf, int len)
{
	static const unsigned char check[] = { 0x21, 0, 0, 0, 1 };
	int i;

	if (len < 5)
		return FALSE;

	for (i = 0; i < 5; i++) {
		if (buf[i] != check[i]) {
			otc_dbg("Packet has invalid prefix.");
			return FALSE;
		}
	}

	return TRUE;
}

OTC_PRIV uint16_t crc16_mcrf4xx(uint16_t crc, uint8_t *data, size_t len)
{
	int i;

	if (!data || !len)
		return crc;

	while (len--) {
		crc ^= *data++;
		for (i = 0; i < 8; i++) {
			if (crc & 1)
				crc = (crc >> 1) ^ 0x8408;
			else
				crc = (crc >> 1);
		}
	}

	return crc;
}

static float binary32_le_to_float(unsigned char *buf)
{
	GFloatIEEE754 f;

	f.v_float = 0;
	f.mpn.sign = (buf[3] & 0x80) ? 1 : 0;
	f.mpn.biased_exponent = (buf[3] << 1) | (buf[2] >> 7);
	f.mpn.mantissa = buf[0] | (buf[1] << 8) | ((buf[2] & 0x7f) << 16);

	return f.v_float;
}

OTC_PRIV void testo_receive_packet(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	struct otc_channel *ch;
	GString *dbg;
	float value;
	int i;
	unsigned char *buf;

	devc = sdi->priv;
	otc_dbg("Got %d-byte packet.", devc->reply_size);

	if (otc_log_loglevel_get() >= OTC_LOG_SPEW) {
		dbg = g_string_sized_new(128);
		g_string_printf(dbg, "Packet:");
		for (i = 0; i < devc->reply_size; i++)
			g_string_append_printf(dbg, " %.2x", devc->reply[i]);
		otc_spew("%s", dbg->str);
		g_string_free(dbg, TRUE);
	}

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	/* TODO: Use proper 'digits' value for this device (and its modes). */
	otc_analog_init(&analog, &encoding, &meaning, &spec, 2);
	analog.num_samples = 1;
	analog.meaning->mqflags = 0;
	analog.data = &value;
	/* Decode 7-byte values */
	for (i = 0; i < devc->reply[6]; i++) {
		buf = devc->reply + 7 + i * 7;
		value = binary32_le_to_float(buf);
		switch (buf[4]) {
		case 1:
			analog.meaning->mq = OTC_MQ_TEMPERATURE;
			analog.meaning->unit = OTC_UNIT_CELSIUS;
			break;
		case 3:
			analog.meaning->mq = OTC_MQ_RELATIVE_HUMIDITY;
			analog.meaning->unit = OTC_UNIT_HUMIDITY_293K;
			break;
		case 5:
			analog.meaning->mq = OTC_MQ_WIND_SPEED;
			analog.meaning->unit = OTC_UNIT_METER_SECOND;
			break;
		case 24:
			analog.meaning->mq = OTC_MQ_PRESSURE;
			analog.meaning->unit = OTC_UNIT_HECTOPASCAL;
			break;
		default:
			otc_dbg("Unsupported measurement unit %d.", buf[4]);
			return;
		}

		/* Match this measurement with its channel. */
		for (i = 0; i < devc->num_channels; i++) {
			if (devc->channel_units[i] == buf[4])
				break;
		}
		if (i == devc->num_channels) {
			/* Shouldn't happen. */
			otc_err("Some channel hotswapped in!");
			return;
		}
		ch = g_slist_nth_data(sdi->channels, i);
		analog.meaning->channels = g_slist_append(NULL, ch);
		otc_session_send(sdi, &packet);
		g_slist_free(analog.meaning->channels);
	}
}
