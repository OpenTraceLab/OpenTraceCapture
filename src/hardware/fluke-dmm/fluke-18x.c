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
#include <math.h>
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "protocol.h"

OTC_PRIV void fluke_handle_qm_18x(const struct otc_dev_inst *sdi, char **tokens)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	float fvalue;
	char *e, *u;
	gboolean is_oor;
	int digits;
	int exponent;
	enum otc_mq mq;
	enum otc_unit unit;
	enum otc_mqflag mqflags;

	devc = sdi->priv;

	if (strcmp(tokens[0], "QM") || !tokens[1])
		return;

	if ((e = strstr(tokens[1], "Out of range"))) {
		is_oor = TRUE;
		fvalue = -1;
		digits = 0;
		while (*e && *e != '.')
			e++;
	} else {
		is_oor = FALSE;
		/* Delimit the float, since otc_atof_ascii() wants only
		 * a valid float here. */
		e = tokens[1];
		while (*e && *e != ' ')
			e++;
		*e++ = '\0';
		if (otc_atof_ascii_digits(tokens[1], &fvalue, &digits) != OTC_OK) {
			/* Happens all the time, when switching modes. */
			otc_dbg("Invalid float: '%s'", tokens[1]);
			return;
		}
	}
	while (*e && *e == ' ')
		e++;

	if (is_oor)
		fvalue = NAN;

	mq = 0;
	unit = 0;
	exponent = 0;
	mqflags = 0;
	if ((u = strstr(e, "V DC")) || (u = strstr(e, "V AC"))) {
		mq = OTC_MQ_VOLTAGE;
		unit = OTC_UNIT_VOLT;
		if (!is_oor && e[0] == 'm')
			exponent = -3;
		/* This catches "V AC", "V DC" and "V AC+DC". */
		if (strstr(u, "AC"))
			mqflags |= OTC_MQFLAG_AC | OTC_MQFLAG_RMS;
		if (strstr(u, "DC"))
			mqflags |= OTC_MQFLAG_DC;
	} else if ((u = strstr(e, "dBV")) || (u = strstr(e, "dBm"))) {
		mq = OTC_MQ_VOLTAGE;
		if (u[2] == 'm')
			unit = OTC_UNIT_DECIBEL_MW;
		else
			unit = OTC_UNIT_DECIBEL_VOLT;
		mqflags |= OTC_MQFLAG_AC | OTC_MQFLAG_RMS;
	} else if ((u = strstr(e, "Ohms"))) {
		mq = OTC_MQ_RESISTANCE;
		unit = OTC_UNIT_OHM;
		if (is_oor)
			fvalue = INFINITY;
		else if (e[0] == 'k')
			exponent = 3;
		else if (e[0] == 'M')
			exponent = 6;
	} else if (!strcmp(e, "nS")) {
		mq = OTC_MQ_CONDUCTANCE;
		unit = OTC_UNIT_SIEMENS;
		exponent = -9;
	} else if ((u = strstr(e, "Farads"))) {
		mq = OTC_MQ_CAPACITANCE;
		unit = OTC_UNIT_FARAD;
		if (!is_oor) {
			if (e[0] == 'm')
				exponent = -3;
			else if (e[0] == 'u')
				exponent = -6;
			else if (e[0] == 'n')
				exponent = -9;
		}
	} else if ((u = strstr(e, "Deg C")) || (u = strstr(e, "Deg F"))) {
		mq = OTC_MQ_TEMPERATURE;
		if (u[4] == 'C')
			unit = OTC_UNIT_CELSIUS;
		else
			unit = OTC_UNIT_FAHRENHEIT;
	} else if ((u = strstr(e, "A AC")) || (u = strstr(e, "A DC"))) {
		mq = OTC_MQ_CURRENT;
		unit = OTC_UNIT_AMPERE;
		/* This catches "A AC", "A DC" and "A AC+DC". */
		if (strstr(u, "AC"))
			mqflags |= OTC_MQFLAG_AC | OTC_MQFLAG_RMS;
		if (strstr(u, "DC"))
			mqflags |= OTC_MQFLAG_DC;
		if (!is_oor) {
			if (e[0] == 'm')
				exponent = -3;
			else if (e[0] == 'u')
				exponent = -6;
		}
	} else if ((u = strstr(e, "Hz"))) {
		mq = OTC_MQ_FREQUENCY;
		unit = OTC_UNIT_HERTZ;
		if (e[0] == 'k')
			exponent = 3;
	} else if (!strcmp(e, "%")) {
		mq = OTC_MQ_DUTY_CYCLE;
		unit = OTC_UNIT_PERCENTAGE;
	} else if ((u = strstr(e, "ms"))) {
		mq = OTC_MQ_PULSE_WIDTH;
		unit = OTC_UNIT_SECOND;
		exponent = -3;
	}

	if (mq != 0) {
		/* Got a measurement. */
		digits -= exponent;
		fvalue *= pow(10.0f, exponent);

		otc_analog_init(&analog, &encoding, &meaning, &spec, digits);
		analog.data = &fvalue;
		analog.num_samples = 1;
		analog.meaning->unit = unit;
		analog.meaning->mq = mq;
		analog.meaning->mqflags = mqflags;
		analog.meaning->channels = sdi->channels;

		packet.type = OTC_DF_ANALOG;
		packet.payload = &analog;
		otc_session_send(sdi, &packet);
		otc_sw_limits_update_samples_read(&devc->limits, 1);
	}
}
