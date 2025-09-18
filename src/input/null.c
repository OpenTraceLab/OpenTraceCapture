/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2018 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

#define LOG_PREFIX "input/null"

static int init(struct otc_input *in, GHashTable *options)
{
	(void)in;
	(void)options;

	return OTC_OK;
}

static int receive(struct otc_input *in, GString *buf)
{
	(void)in;
	(void)buf;

	return OTC_OK;
}

static int end(struct otc_input *in)
{
	(void)in;

	return OTC_OK;
}

OTC_PRIV struct otc_input_module input_null = {
	.id = "null",
	.name = "Null",
	.desc = "Null input (discards all input)",
	.exts = NULL,
	.options = NULL,
	.init = init,
	.receive = receive,
	.end = end,
	.reset = NULL,
};
