/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2023 Mathieu Pilato <pilato.mathieu@free.fr>
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

#ifndef LIBSIGROK_HARDWARE_ATORCH_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ATORCH_PROTOCOL_H

#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include <stdint.h>

#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "atorch"

#define ATORCH_BUFSIZE	128

struct atorch_device_profile {
	uint8_t device_type;
	const char *device_name;
	const struct atorch_channel_desc *channels;
	size_t channel_count;
};

struct atorch_channel_desc {
	const char *name;
	struct binary_value_spec spec;
	struct otc_rational scale;
	int digits;
	enum otc_mq mq;
	enum otc_unit unit;
	enum otc_mqflag flags;
};

enum atorch_msg_type {
	MSG_REPORT = 0x01,
	MSG_REPLY = 0x02,
	MSG_COMMAND = 0x11,
};

struct dev_context {
	const struct atorch_device_profile *profile;
	struct otc_sw_limits limits;
	struct feed_queue_analog **feeds;
	uint8_t buf[ATORCH_BUFSIZE];
	size_t wr_idx;
	size_t rd_idx;
};

OTC_PRIV int atorch_probe(struct otc_serial_dev_inst *serial, struct dev_context *devc);
OTC_PRIV int atorch_receive_data_callback(int fd, int revents, void *cb_data);

#endif
