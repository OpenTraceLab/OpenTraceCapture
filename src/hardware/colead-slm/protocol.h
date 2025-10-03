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

#ifndef LIBSIGROK_HARDWARE_COLEAD_SLM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_COLEAD_SLM_PROTOCOL_H

#include <stdint.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "colead-slm"

enum {
	IDLE,
	COMMAND_SENT,
};

struct dev_context {
	struct otc_sw_limits limits;

	int state;
	char buf[10];
	int buflen;
};

OTC_PRIV int colead_slm_receive_data(int fd, int revents, void *cb_data);

#endif
