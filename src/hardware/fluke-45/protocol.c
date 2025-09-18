/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2017 John Chajecki <subs@qcontinuum.plus.com>
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

#include <stdio.h>
#include <config.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <glib.h>
#include <errno.h>
#include "../../scpi.h"
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "protocol.h"

/* Get the current state of the meter and sets analog object parameters. */
OTC_PRIV int fl45_get_status(const struct otc_dev_inst *sdi,
		struct otc_datafeed_analog *analog, int idx)
{
	struct dev_context *devc;
	char *cmd, *func;
	int res;

	res = 0;

	/* Command string to read current function. */
	cmd = g_strdup_printf("FUNC%d?", idx + 1);
	otc_dbg("Sent command: %s.", cmd);

	if (!(devc = sdi->priv))
		return TRUE;

	/* Default settings. */
	analog[idx].meaning->mq = 0;
	analog[idx].meaning->unit = 0;
	analog[idx].meaning->mqflags = 0;

	/* Get a response to the FUNC? command. */
	res = fl45_scpi_get_response(sdi, cmd);
	if (res == OTC_ERR)
		return res;
	otc_dbg("Response to FUNC: %s.", devc->response);

	/* Set up analog mq, unit and flags. */
	if (res == OTC_OK && devc->response != NULL) {
		func = devc->response;
		if (strcmp(func, "AAC") == 0) {
			analog[idx].meaning->mq = OTC_MQ_CURRENT;
			analog[idx].meaning->unit = OTC_UNIT_AMPERE;
			analog[idx].meaning->mqflags = OTC_MQFLAG_AC;
		} else if (strcmp(func, "AACDC") == 0) {
			analog[idx].meaning->mq = OTC_MQ_CURRENT;
			analog[idx].meaning->unit = OTC_UNIT_AMPERE;
			analog[idx].meaning->mqflags = OTC_MQFLAG_AC;
		} else if (strcmp(func, "ADC") == 0) {
			analog[idx].meaning->mq = OTC_MQ_CURRENT;
			analog[idx].meaning->unit = OTC_UNIT_AMPERE;
			analog[idx].meaning->mqflags = OTC_MQFLAG_DC;
		} else if (strcmp(func, "CONT") == 0) {
			analog[idx].meaning->mq = OTC_MQ_CONTINUITY;
			analog->meaning->unit = OTC_UNIT_BOOLEAN;
		} else if (strcmp(func, "DIODE") == 0) {
			analog[idx].meaning->mq = OTC_MQ_VOLTAGE;
			analog[idx].meaning->unit = OTC_UNIT_VOLT;
			analog[idx].meaning->mqflags = OTC_MQFLAG_DIODE;
		} else if (strcmp(func, "FREQ") == 0) {
			analog[idx].meaning->mq = OTC_MQ_FREQUENCY;
			analog[idx].meaning->unit = OTC_UNIT_HERTZ;
		} else if (strcmp(func, "OHMS") == 0) {
			analog[idx].meaning->mq = OTC_MQ_RESISTANCE;
			analog[idx].meaning->unit = OTC_UNIT_OHM;
		} else if (strcmp(func, "VAC") == 0) {
			analog[idx].meaning->mq = OTC_MQ_VOLTAGE;
			analog[idx].meaning->unit = OTC_UNIT_VOLT;
			analog[idx].meaning->mqflags = OTC_MQFLAG_AC;
		} else if (strcmp(func, "VACDC") == 0) {
			analog[idx].meaning->mq = OTC_MQ_VOLTAGE;
			analog[idx].meaning->unit = OTC_UNIT_VOLT;
			analog[idx].meaning->mqflags |= OTC_MQFLAG_AC;
			analog[idx].meaning->mqflags |= OTC_MQFLAG_DC;
		} else if (strcmp(func, "VDC") == 0) {
			analog[idx].meaning->mq = OTC_MQ_VOLTAGE;
			analog[idx].meaning->unit = OTC_UNIT_VOLT;
			analog[idx].meaning->mqflags = OTC_MQFLAG_DC;
		}
	}

	/* Is the meter in autorange mode? */
	res = fl45_scpi_get_response(sdi, "AUTO?");
	if (res == OTC_ERR)
		return res;
	otc_dbg("Response to AUTO: %s.", devc->response);
	if (res == OTC_OK && devc->response != NULL) {
		if (strcmp(devc->response, "1") == 0)
			analog[idx].meaning->mqflags |= OTC_MQFLAG_AUTORANGE;
	}

	return OTC_OK;
}

OTC_PRIV int fl45_get_modifiers(const struct otc_dev_inst *sdi,
		struct otc_datafeed_analog *analog, int idx)
{
	struct dev_context *devc;
	int res, mod;

	if (!(devc = sdi->priv))
		return TRUE;

	/* Get modifier value. */
	res = fl45_scpi_get_response(sdi, "MOD?");
	if (res == OTC_ERR)
		return res;
	otc_dbg("Response to MOD: %s.", devc->response);
	if (res == OTC_OK && devc->response != NULL) {
		mod = atoi(devc->response);
		if (mod & 0x01) {
			analog[idx].meaning->mqflags |= OTC_MQFLAG_MIN;
			otc_dbg("MIN bit set: %s.", "1");
		}
		if (mod & 0x02) {
			analog[idx].meaning->mqflags |= OTC_MQFLAG_MAX;
			otc_dbg("MAX bit set: %s.", "2");
		}
		if (mod & 0x04) {
			analog[idx].meaning->mqflags |= OTC_MQFLAG_HOLD;
			otc_dbg("HOLD bit set: %s.", "4");
		}
		if (mod & 0x08) {
			otc_dbg("dB bit set: %s.", "8");
			analog[idx].meaning->mq = OTC_MQ_POWER_FACTOR;
			analog[idx].meaning->unit = OTC_UNIT_DECIBEL_MW;
			analog[idx].meaning->mqflags = 0;
			analog[idx].encoding->digits = 2;
			analog[idx].spec->spec_digits = 2;
		}
		if (mod & 0x10) {
			otc_dbg("dB Power mod bit set: %s.", "16");
			analog[idx].meaning->mq = OTC_MQ_POWER;
			analog[idx].meaning->unit = OTC_UNIT_DECIBEL_SPL;
			analog[idx].meaning->mqflags = 0;
			analog[idx].encoding->digits = 2;
			analog[idx].spec->spec_digits = 2;
		}
		if (mod & 0x20) {
			otc_dbg("REL bit set: %s.", "32");
			analog[idx].meaning->mqflags |= OTC_MQFLAG_HOLD;
		}
	}

	return OTC_OK;
}

int get_reading_dd(char *reading, size_t size)
{
	int pe, pd, digits;
	unsigned int i;
	char expstr[3];
	char *eptr;
	long exp;

	/* Calculate required precision. */

	pe = pd = digits = 0;

	/* Get positions for '.' end 'E'. */
	for (i = 0; i < size; i++) {
		if (reading[i] == '.')
			pd = i;
		if (reading[i] == 'E') {
			pe = i;
			break;
		}
	}

	digits = (pe - pd) - 1;

	/* Get exponent element. */
	expstr[0] = reading[pe + 1];
	expstr[1] = reading[pe + 2];
	expstr[2] = '\0';
	errno = 0;
	exp = strtol(expstr, &eptr, 10);
	if (errno != 0)
		return 2;
	/* A negative exponent increses digits, a positive one reduces. */
	exp = exp * (-1);

	/* Adjust digits taking into account exponent. */
	digits = digits + exp;

	return digits;
}

OTC_PRIV int fl45_scpi_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog[2];
	struct otc_analog_encoding encoding[2];
	struct otc_analog_meaning meaning[2];
	struct otc_analog_spec spec[2];
	struct otc_channel *channel;
	char *reading;
	float fv;
	int res, digits;
	unsigned int i;
	int sent_ch[2];

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	res = 0;
	sent_ch[0] = sent_ch[1] = 0;

	/* Process the list of channels. */
	for (i = 0; i < devc->num_channels; i++) {
		/* Note: digits/spec_digits will be overridden later. */
		otc_analog_init(&analog[i], &encoding[i], &meaning[i], &spec[i], 0);

		/* Detect current meter function. */
		res = fl45_get_status(sdi, analog, i);

		/* Get channel data. */
		if (i == 0)
			channel = sdi->channels->data;
		else
			channel = sdi->channels->next->data;

		/* Is channel enabled? */
		if (analog[i].meaning->mq != 0 && channel->enabled) {
			/* Then get a reading from it. */
			if (i == 0)
				res = fl45_scpi_get_response(sdi, "VAL1?");
			if (i == 1)
				res = fl45_scpi_get_response(sdi, "VAL2?");
			/* Note: Fluke 45 sends all data in text strings. */
			reading = devc->response;

			/* Deal with OL reading. */
			if (strcmp(reading, "+1E+9") == 0) {
				fv = INFINITY;
				otc_dbg("Reading OL (infinity): %s.",
					devc->response);
			} else if (res == OTC_OK && reading != NULL) {
				/* Convert reading to float. */
				otc_dbg("Meter reading string: %s.", reading);
				res = otc_atof_ascii(reading, &fv);
				digits = get_reading_dd(reading, strlen(reading));
				analog[i].encoding->digits = digits;
				analog[i].spec->spec_digits = digits;

			} else {
				otc_dbg("Invalid float string: '%s'.", reading);
				return OTC_ERR;
			}

			/* Are we on a little or big endian system? */
#ifdef WORDS_BIGENDIAN
			analog[i].encoding->is_bigendian = TRUE;
#else
			analog[i].encoding->is_bigendian = FALSE;
#endif

			/* Apply any modifiers. */
			res = fl45_get_modifiers(sdi, analog, i);

			/* Channal flag. */
			sent_ch[i] = 1;

			/* Set up analog object. */
			analog[i].num_samples = 1;
			analog[i].data = &fv;
			analog[i].meaning->channels = g_slist_append(NULL, channel);

			packet.type = OTC_DF_ANALOG;
			packet.payload = &analog[i];

			otc_session_send(sdi, &packet);

			g_slist_free(analog[i].meaning->channels);
		}
	}

	/* Update appropriate channel limits. */
	if (sent_ch[0] || sent_ch[1])
		otc_sw_limits_update_samples_read(&devc->limits, 1);

	/* Are we done collecting samples? */
	if (otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop(sdi);

	return TRUE;
}

OTC_PRIV int fl45_scpi_get_response(const struct otc_dev_inst *sdi, char *cmd)
{
	struct dev_context *devc;
	devc = sdi->priv;

	/* Attempt to get a SCPI reponse. */
	if (otc_scpi_get_string(sdi->conn, cmd, &devc->response) != OTC_OK)
		return OTC_ERR;

	/* Deal with RS232 '=>' prompt. */
	if (strcmp(devc->response, "=>") == 0) {
		/*
		 * If the response is a prompt then ignore and read the next
		 * response in the buffer.
		 */
		g_free(devc->response);
		devc->response = NULL;
		/* Now attempt to read again. */
		if (otc_scpi_get_string(sdi->conn, NULL, &devc->response) != OTC_OK)
			return OTC_ERR;
	}

	/* NULL RS232 error prompts. */
	if (strcmp(devc->response, "!>") == 0 ||
	    (strcmp(devc->response, "?>") == 0)) {
		/* Unable to execute CMD. */
		g_free(devc->response);
		devc->response = NULL;
	}

	return OTC_OK;
}
