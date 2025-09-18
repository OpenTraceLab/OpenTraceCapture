/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2021 LUMERIIX
 * Copyright (C) 2024 Daniel Anselmi <danselmi@gmx.ch>
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

#ifndef LIBSIGROK_HARDWARE_BKPRECISION_1856D_PROTOCOL_H
#define LIBSIGROK_HARDWARE_BKPRECISION_1856D_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "bkprecision-1856d"
#define BKPRECISION1856D_MSG_SIZE 15
#define BKPRECISION1856D_MSG_NUMBER_SIZE 10


enum {
	InputA = 0,
	InputC = 1,
};

struct dev_context {
	struct otc_sw_limits sw_limits;
	unsigned int sel_input;
	unsigned int curr_sel_input;
	unsigned int gate_time;

	char buffer[BKPRECISION1856D_MSG_SIZE];
	unsigned int buffer_level;
};

OTC_PRIV int bkprecision_1856d_receive_data(int fd, int revents, void *cb_data);
OTC_PRIV void bkprecision_1856d_init(const struct otc_dev_inst *sdi);
OTC_PRIV void bkprecision_1856d_set_gate_time(struct dev_context *devc,
											 int time);
OTC_PRIV void bkprecision_1856d_select_input(struct dev_context *devc,
											int intput);

#endif
