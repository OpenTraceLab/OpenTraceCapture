/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2020 Andreas Sandberg <andreas@sandberg.pp.se>
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

#ifndef LIBSIGROK_HARDWARE_RDTECH_TC_PROTOCOL_H
#define LIBSIGROK_HARDWARE_RDTECH_TC_PROTOCOL_H

#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "rdtech-tc"

/*
 * Keep request and response buffers of sufficient size. The maximum
 * request text currently involved is "bgetva\r\n" which translates
 * to 9 bytes. The poll response (a measurement, the largest amount
 * of data that is currently received) is 192 bytes in length. Add
 * some slack for alignment, and for in-flight messages or adjacent
 * data during synchronization to the data stream.
 */
#define RDTECH_TC_MAXREQLEN 12
#define RDTECH_TC_RSPBUFSIZE 256

struct rdtech_dev_info {
	char *model_name;
	char *fw_ver;
	uint32_t serial_num;
};

struct rdtech_tc_channel_desc {
	const char *name;
	struct binary_value_spec spec;
	struct otc_rational scale;
	int digits;
	enum otc_mq mq;
	enum otc_unit unit;
};

struct dev_context {
	gboolean is_bluetooth;
	char req_text[RDTECH_TC_MAXREQLEN];
	struct rdtech_dev_info dev_info;
	const struct rdtech_tc_channel_desc *channels;
	size_t channel_count;
	struct feed_queue_analog **feeds;
	struct otc_sw_limits limits;
	uint8_t buf[RDTECH_TC_RSPBUFSIZE];
	size_t rdlen;
	int64_t cmd_sent_at;
	size_t rx_after_tx;
};

OTC_PRIV int rdtech_tc_probe(struct otc_serial_dev_inst *serial, struct dev_context *devc);
OTC_PRIV int rdtech_tc_receive_data(int fd, int revents, void *cb_data);
OTC_PRIV int rdtech_tc_poll(const struct otc_dev_inst *sdi, gboolean force);

#endif
