/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-opentracelab@heidbrink.biz>
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

#ifndef LIBSIGROK_HARDWARE_CONRAD_DIGI_35_CPU_PROTOCOL_H
#define LIBSIGROK_HARDWARE_CONRAD_DIGI_35_CPU_PROTOCOL_H

#include <stdint.h>
#include <string.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "conrad-digi-35-cpu"

struct dev_context {
	struct otc_sw_limits limits;
};

OTC_PRIV int send_msg1(const struct otc_dev_inst *sdi, char cmd, int param);

#endif
