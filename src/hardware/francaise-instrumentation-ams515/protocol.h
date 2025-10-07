/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2022 François Revol <revol@free.fr>
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

#ifndef LIBSIGROK_HARDWARE_FRANCAISE_INSTRUMENTATION_AMS515_PROTOCOL_H
#define LIBSIGROK_HARDWARE_FRANCAISE_INSTRUMENTATION_AMS515_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "francaise-instrumentation-ams515"
#define SERIAL_WRITE_TIMEOUT_MS 1
#define SERIAL_READ_TIMEOUT_MS 100
#define ANSWER_MAX 15
#define MAX_CHANNELS 3

struct dev_context {
	int panel_mode;
	int selected_channel; // channel currently displayed on the front panel
	gboolean resync;
	gboolean overcurrent;
	GMutex mutex;
};

OTC_PRIV int francaise_instrumentation_ams515_receive_data(int fd, int revents,
							  void *cb_data);

OTC_PRIV int
francaise_instrumentation_ams515_send_raw(const struct otc_dev_inst *sdi,
					  const char *cmd, char *answer,
					  gboolean echoed);

OTC_PRIV int
francaise_instrumentation_ams515_set_state(const struct otc_dev_inst *sdi,
					   char cmd, gboolean param);
OTC_PRIV int
francaise_instrumentation_ams515_set_echo(const struct otc_dev_inst *sdi,
					  gboolean param);

OTC_PRIV int
francaise_instrumentation_ams515_query_int(const struct otc_dev_inst *sdi,
					   const char cmd, int *result);
OTC_PRIV int
francaise_instrumentation_ams515_query_str(const struct otc_dev_inst *sdi,
					   const char cmd, char *result);
OTC_PRIV int
francaise_instrumentation_ams515_send_int(const struct otc_dev_inst *sdi,
					  const char cmd, int param);
OTC_PRIV int
francaise_instrumentation_ams515_send_char(const struct otc_dev_inst *sdi,
					   const char cmd, char param);

#endif
