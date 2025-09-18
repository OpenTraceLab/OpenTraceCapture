/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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
#include <stdlib.h>
#include <glib.h>
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "protocol.h"

static void process_packet(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	GString *dbg;
	float fvalue;
	int checksum, mode, i;

	devc = sdi->priv;
	if (otc_log_loglevel_get() >= OTC_LOG_SPEW) {
		dbg = g_string_sized_new(128);
		g_string_printf(dbg, "received packet:");
		for (i = 0; i < 10; i++)
			g_string_append_printf(dbg, " %.2x", (devc->buf)[i]);
		otc_spew("%s", dbg->str);
		g_string_free(dbg, TRUE);
	}

	if (devc->buf[0] != 0x08 || devc->buf[1] != 0x04) {
		otc_dbg("invalid packet header.");
		return;
	}

	if (devc->buf[8] != 0x01) {
		otc_dbg("invalid measurement.");
		return;
	}

	checksum = 0;
	for (i = 0; i < 9; i++)
		checksum += devc->buf[i];
	if ((checksum & 0xff) != devc->buf[9]) {
		otc_dbg("invalid packet checksum.");
		return;
	}

	fvalue = 0.0;
	for (i = 3; i < 8; i++) {
		if (devc->buf[i] > 0x09)
			continue;
		fvalue *= 10;
		fvalue += devc->buf[i];
	}
	fvalue /= 10;

	otc_analog_init(&analog, &encoding, &meaning, &spec, 1);
	analog.meaning->mq = OTC_MQ_SOUND_PRESSURE_LEVEL;
	analog.meaning->unit = OTC_UNIT_DECIBEL_SPL;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	analog.data = &fvalue;

	/* High nibble should only have 0x01 or 0x02. */
	mode = (devc->buf[2] >> 4) & 0x0f;
	if (mode == 0x02)
		analog.meaning->mqflags |= OTC_MQFLAG_HOLD;
	else if (mode != 0x01) {
		otc_dbg("unknown measurement mode 0x%.2x", mode);
		return;
	}

	/* Low nibble has 14 combinations of direct/long-term average,
	 * time scale of that average, frequency weighting, and time
	 * weighting. */
	mode = devc->buf[2] & 0x0f;
	switch (mode) {
	case 0x0:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_F;
		break;
	case 0x1:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_S;
		break;
	case 0x2:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_C \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_F;
		break;
	case 0x3:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_C \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_S;
		break;
	case 0x4:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_FLAT \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_F;
		break;
	case 0x5:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_FLAT \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_S;
		break;
	case 0x6:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_PCT_OVER_ALARM \
				| OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_F;
		break;
	case 0x7:
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_PCT_OVER_ALARM \
				| OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_S;
		break;
	case 0x8:
		/* 10-second mean, but we don't have MQ flags to express it. */
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_LAT \
				| OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_F;
		break;
	case 0x9:
		/* Mean over a time period between 11 seconds and 24 hours.
		 * Which is so silly that there's no point in expressing
		 * either this or the previous case. */
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_LAT \
				| OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_F;
		break;
	case 0xa:
		/* 10-second mean. */
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_LAT \
				| OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_S;
		break;
	case 0xb:
		/* Mean over a time period between 11 seconds and 24 hours. */
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_LAT \
				| OTC_MQFLAG_SPL_FREQ_WEIGHT_A \
				| OTC_MQFLAG_SPL_TIME_WEIGHT_S;
		break;
	case 0xc:
		/* Internal calibration on 1kHz sine at 94dB, not useful
		 * to anything but the device. */
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_FLAT;
		break;
	case 0xd:
		/* Internal calibration on 1kHz sine at 94dB, not useful
		 * to anything but the device. */
		analog.meaning->mqflags |= OTC_MQFLAG_SPL_FREQ_WEIGHT_FLAT;
		break;
	default:
		otc_dbg("unknown configuration 0x%.2x", mode);
		return;
	}

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	otc_session_send(sdi, &packet);

	otc_sw_limits_update_samples_read(&devc->limits, 1);

	if (otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop((struct otc_dev_inst *)sdi);
}

OTC_PRIV int colead_slm_receive_data(int fd, int revents, void *cb_data)
{
	const struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	int delay_ms, len;
	char buf[128];

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (revents != G_IO_IN)
		/* Timeout event. */
		return TRUE;

	serial = sdi->conn;
	if (devc->state == IDLE) {
		if (serial_read_nonblocking(serial, buf, sizeof(buf)) != 1 || buf[0] != 0x10)
			/* Nothing there, or caught the tail end of a previous packet,
			 * or some garbage. Unless it's a single "data ready" byte,
			 * we don't want it. */
			return TRUE;
		/* Got 0x10, "measurement ready". */
		delay_ms = serial_timeout(serial, 1);
		if (serial_write_blocking(serial, "\x20", 1, delay_ms) < 1)
			otc_err("unable to send command");
		else {
			devc->state = COMMAND_SENT;
			devc->buflen = 0;
		}
	} else {
		len = serial_read_nonblocking(serial, devc->buf + devc->buflen,
				10 - devc->buflen);
		if (len < 1)
			return TRUE;
		devc->buflen += len;
		if (devc->buflen > 10) {
			otc_dbg("buffer overrun");
			devc->state = IDLE;
			return TRUE;
		}
		if (devc->buflen == 10) {
			/* If we got here, we're sure the device sent a "data ready"
			 * notification, we asked for data, and got it. */
			process_packet(sdi);
			devc->state = IDLE;
		}
	}

	return TRUE;
}
