/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2018 Bastian Schmitz <bastian.schmitz@udo.edu>
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

#ifndef LIBSIGROK_HARDWARE_GWINSTEK_GPD_PROTOCOL_H
#define LIBSIGROK_HARDWARE_GWINSTEK_GPD_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "gwinstek-gpd"

enum {
	GPD_2303S,
	GPD_3303S,
};

/* Maximum number of output channels handled by this driver. */
#define MAX_CHANNELS 2

#define CHANMODE_INDEPENDENT (1 << 0)
#define CHANMODE_SERIES      (1 << 1)
#define CHANMODE_PARALLEL    (1 << 2)

struct channel_spec {
	/* Min, max, step. */
	gdouble voltage[3];
	gdouble current[3];
};

struct gpd_model {
	int modelid;
	const char *name;
	int channel_modes;
	unsigned int num_channels;
	struct channel_spec channels[MAX_CHANNELS];
};

struct per_channel_config {
	/* Received from device. */
	float output_voltage_last;
	float output_current_last;
	/* Set by frontend. */
	float output_voltage_max;
	float output_current_max;
};

struct dev_context {
	/* Received from device. */
	gboolean output_enabled;
	int64_t req_sent_at;
	gboolean reply_pending;

	struct otc_sw_limits limits;
	int channel_mode;
	struct per_channel_config *config;
	const struct gpd_model *model;
};

OTC_PRIV int gpd_send_cmd(struct otc_serial_dev_inst *serial, const char *cmd, ...);
OTC_PRIV int gpd_receive_data(int fd, int revents, void *cb_data);
OTC_PRIV int gpd_receive_reply(struct otc_serial_dev_inst *serial, char *buf, int buflen);

#endif
