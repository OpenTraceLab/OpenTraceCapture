/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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

#ifndef LIBSIGROK_HARDWARE_TONDAJ_SL_814_PROTOCOL_H
#define LIBSIGROK_HARDWARE_TONDAJ_SL_814_PROTOCOL_H

#include <stdint.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "tondaj-sl-814"

struct dev_context {
	struct otc_sw_limits limits;
	int state;

	uint8_t buf[4];
	uint8_t buflen;
};

OTC_PRIV int tondaj_sl_814_receive_data(int fd, int revents, void *cb_data);

#endif
