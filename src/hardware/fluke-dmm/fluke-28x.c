/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2019-2020, 2024 Andreas Sandberg <andreas@sandberg.uk>
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

enum measurement_state {
	MEAS_S_INVALID = 0,
	MEAS_S_NORMAL,
	MEAS_S_BLANK,
	MEAS_S_DISCHARGE,
	MEAS_S_OL,
	MEAS_S_OL_MINUS,
	MEAS_S_OPEN_TC,
};

struct state_mapping {
	const char *name;
	enum measurement_state state;
};

static struct state_mapping state_map[] = {
	{ "INVALID", MEAS_S_INVALID },
	{ "NORMAL", MEAS_S_NORMAL },
	{ "BLANK", MEAS_S_BLANK },
	{ "DISCHARGE", MEAS_S_DISCHARGE },
	{ "OL", MEAS_S_OL },
	{ "OL_MINUS", MEAS_S_OL_MINUS },
	{ "OPEN_TC", MEAS_S_OPEN_TC },
};

enum measurement_attribute {
	MEAS_A_INVALID = 0,
	MEAS_A_NONE,
	MEAS_A_OPEN_CIRCUIT,
	MEAS_A_SHORT_CIRCUIT,
	MEAS_A_GLITCH_CIRCUIT,
	MEAS_A_GOOD_DIODE,
	MEAS_A_LO_OHMS,
	MEAS_A_NEGATIVE_EDGE,
	MEAS_A_POSITIVE_EDGE,
	MEAS_A_HIGH_CURRENT,
};

struct attribute_mapping {
	const char *name;
	enum measurement_attribute attribute;
};

static struct attribute_mapping attribute_map[] = {
	{ "NONE", MEAS_A_NONE },
	{ "OPEN_CIRCUIT", MEAS_A_OPEN_CIRCUIT },
	{ "SHORT_CIRCUIT", MEAS_A_SHORT_CIRCUIT },
	{ "GLITCH_CIRCUIT", MEAS_A_GLITCH_CIRCUIT },
	{ "GOOD_DIODE", MEAS_A_GOOD_DIODE },
	{ "LO_OHMS", MEAS_A_LO_OHMS },
	{ "NEGATIVE_EDGE", MEAS_A_NEGATIVE_EDGE },
	{ "POSITIVE_EDGE", MEAS_A_POSITIVE_EDGE },
	{ "HIGH_CURRENT", MEAS_A_HIGH_CURRENT },
};

struct unit_mapping {
	const char *name;
	enum otc_mq mq;
	enum otc_unit unit;
	enum otc_mqflag mqflags;
};

static struct unit_mapping unit_map[] = {
	{ "VDC", OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, OTC_MQFLAG_DC },
	{ "VAC", OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, OTC_MQFLAG_AC | OTC_MQFLAG_RMS },
	{ "ADC", OTC_MQ_CURRENT, OTC_UNIT_AMPERE, OTC_MQFLAG_DC },
	{ "AAC", OTC_MQ_CURRENT, OTC_UNIT_AMPERE, OTC_MQFLAG_AC | OTC_MQFLAG_RMS },
	{ "VAC_PLUS_DC", OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, 0 },
	{ "AAC_PLUS_DC", OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, 0 },
	/* Used in peak */
	{ "V", OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, 0 },
	/* Used in peak */
	{ "A", OTC_MQ_VOLTAGE, OTC_UNIT_AMPERE, 0 },
	{ "OHM", OTC_MQ_RESISTANCE, OTC_UNIT_OHM, 0 },
	{ "SIE", OTC_MQ_CONDUCTANCE, OTC_UNIT_SIEMENS, 0 },
	{ "Hz", OTC_MQ_FREQUENCY, OTC_UNIT_HERTZ, 0 },
	{ "S", OTC_MQ_PULSE_WIDTH, OTC_UNIT_SECOND, 0 },
	{ "F", OTC_MQ_CAPACITANCE, OTC_UNIT_FARAD, 0 },
	{ "CEL", OTC_MQ_TEMPERATURE, OTC_UNIT_CELSIUS, 0 },
	{ "FAR", OTC_MQ_TEMPERATURE, OTC_UNIT_FAHRENHEIT, 0 },
	{ "PCT", OTC_MQ_DUTY_CYCLE, OTC_UNIT_PERCENTAGE, 0 },
	{ "dBm", OTC_MQ_VOLTAGE, OTC_UNIT_DECIBEL_MW, OTC_MQFLAG_AC | OTC_MQFLAG_RMS },
	{ "dBV", OTC_MQ_VOLTAGE, OTC_UNIT_DECIBEL_VOLT, OTC_MQFLAG_AC | OTC_MQFLAG_RMS },
};

static const struct unit_mapping *parse_unit(const char *name)
{
	unsigned int i;

	if (!name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(unit_map); i++) {
		if (!strcmp(unit_map[i].name, name))
			return &unit_map[i];
	}

	otc_warn("Unknown unit '%s'", name);
	return NULL;
}

static enum measurement_state parse_measurement_state(const char *name)
{
	unsigned int i;

	if (!name)
		return MEAS_S_INVALID;

	for (i = 0; i < ARRAY_SIZE(state_map); i++) {
		if (!strcmp(state_map[i].name, name))
			return state_map[i].state;
	}

	otc_warn("Unknown measurement state '%s'", name);
	return MEAS_S_INVALID;
}

static enum measurement_attribute parse_attribute(const char *name)
{
	unsigned int i;

	if (!name)
		return MEAS_A_INVALID;

	for (i = 0; i < ARRAY_SIZE(attribute_map); i++) {
		if (!strcmp(attribute_map[i].name, name))
			return attribute_map[i].attribute;
	}

	otc_warn("Unknown measurement attribute '%s'", name);
	return MEAS_A_INVALID;
}

OTC_PRIV void fluke_handle_qm_28x(const struct otc_dev_inst *sdi, char **tokens)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;

	float fvalue;
	int digits;
	const struct unit_mapping *unit;
	enum measurement_state state;
	enum measurement_attribute attr;

	devc = sdi->priv;

	/* We should have received four values:
	 * value, unit, state, attribute
	 */
	if (otc_atof_ascii_digits(tokens[0], &fvalue, &digits) != OTC_OK) {
		otc_err("Invalid float '%s'.", tokens[0]);
		return;
	}

	unit = parse_unit(tokens[1]);
	if (!unit) {
		otc_err("Invalid unit '%s'.", tokens[1]);
		return;
	}

	state = parse_measurement_state(tokens[2]);
	attr = parse_attribute(tokens[3]);

	otc_analog_init(&analog, &encoding, &meaning, &spec, digits);
	analog.data = &fvalue;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	analog.meaning->mq = unit->mq;
	analog.meaning->mqflags = unit->mqflags;
	analog.meaning->unit = unit->unit;

	if (unit->mq == OTC_MQ_RESISTANCE) {
		switch (attr) {
		case MEAS_A_NONE:
			/* Normal reading */
			break;
		case MEAS_A_OPEN_CIRCUIT:
		case MEAS_A_SHORT_CIRCUIT:
			/* Continuity measurement */
			analog.meaning->mq = OTC_MQ_CONTINUITY;
			analog.meaning->unit = OTC_UNIT_BOOLEAN;
			fvalue = attr == MEAS_A_OPEN_CIRCUIT ? 0.0 : 1.0;
			break;
		default:
			analog.meaning->mq = 0;
			break;
		};
	}

	switch (state) {
	case MEAS_S_NORMAL:
		break;

	case MEAS_S_OL:
		fvalue = INFINITY;
		break;

	case MEAS_S_OL_MINUS:
		fvalue = -INFINITY;
		break;

	case MEAS_S_OPEN_TC:
		fvalue = NAN;
		break;

	default:
		analog.meaning->mq = 0;
		break;
	}

	if (analog.meaning->mq) {
		/* Got a measurement. */
		packet.type = OTC_DF_ANALOG;
		packet.payload = &analog;
		otc_session_send(sdi, &packet);
		otc_sw_limits_update_samples_read(&devc->limits, 1);
	}
}
