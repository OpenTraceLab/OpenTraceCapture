/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

#define LOG_PREFIX "transform/invert"

static int receive(const struct otc_transform *t,
		struct otc_datafeed_packet *packet_in,
		struct otc_datafeed_packet **packet_out)
{
	const struct otc_datafeed_logic *logic;
	const struct otc_datafeed_analog *analog;
	uint8_t *b;
	int64_t p;
	uint64_t i, j, q;

	if (!t || !t->sdi || !packet_in || !packet_out)
		return OTC_ERR_ARG;

	switch (packet_in->type) {
	case OTC_DF_LOGIC:
		logic = packet_in->payload;
		for (i = 0; i <= logic->length - logic->unitsize; i += logic->unitsize) {
			for (j = 0; j < logic->unitsize; j++) {
				/* For now invert every bit in every byte. */
				b = (uint8_t *)logic->data + i + logic->unitsize - 1 - j;
				*b = ~(*b);
			}
		}
		break;
	case OTC_DF_ANALOG:
		analog = packet_in->payload;
		p = analog->encoding->scale.p;
		q = analog->encoding->scale.q;
		if (q > INT64_MAX)
			return OTC_ERR;
		analog->encoding->scale.p = (p < 0) ? -q : q;
		analog->encoding->scale.q = (p < 0) ? -p : p;
		break;
	default:
		otc_spew("Unsupported packet type %d, ignoring.", packet_in->type);
		break;
	}

	/* Return the in-place-modified packet. */
	*packet_out = packet_in;

	return OTC_OK;
}

OTC_PRIV struct otc_transform_module transform_invert = {
	.id = "invert",
	.name = "Invert",
	.desc = "Invert values",
	.options = NULL,
	.init = NULL,
	.receive = receive,
	.cleanup = NULL,
};
