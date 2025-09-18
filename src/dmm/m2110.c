/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2013 Matthias Heidbrink <m-opentracelab@heidbrink.biz>
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

/**
 * @file
 *
 * BBC Goerz Metrawatt M2110 ASCII protocol parser.
 *
 * Most probably the simplest multimeter protocol ever ;-) .
 */

#include <config.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

#define LOG_PREFIX "m2110"

OTC_PRIV gboolean otc_m2110_packet_valid(const uint8_t *buf)
{
	float val;

	if ((buf[7] != '\r') || (buf[8] != '\n'))
		return FALSE;

	if (!strncmp((const char *)buf, "OVERRNG", 7))
		return TRUE;

	if (otc_atof_ascii((const char *)buf, &val) == OTC_OK)
		return TRUE;
	else
		return FALSE;
}

OTC_PRIV int otc_m2110_parse(const uint8_t *buf, float *floatval,
				struct otc_datafeed_analog *analog, void *info)
{
	int dot_pos, digits = 0;
	float val;

	(void)info;

	/* We don't know the unit, so that's the best we can do. */
	analog->meaning->mq = OTC_MQ_GAIN;
	analog->meaning->unit = OTC_UNIT_UNITLESS;
	analog->meaning->mqflags = 0;

	if (!strncmp((const char *)buf, "OVERRNG", 7))
		*floatval = INFINITY;
	else if (otc_atof_ascii((const char *)buf, &val) == OTC_OK) {
		*floatval = val;
		dot_pos = strcspn((const char *)buf, ".");
		if (dot_pos < 7)
			digits = 6 - dot_pos;
	}

	analog->encoding->digits  = digits;
	analog->spec->spec_digits = digits;

	return OTC_OK;
}
