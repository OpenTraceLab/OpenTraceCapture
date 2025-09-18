/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2018 Sven Bursch-Osewold <sb_git@bursch.com>
 * Copyright (C) 2019 King Kévin <kingkevin@cuvoodoo.info>
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

#ifndef LIBSIGROK_HARDWARE_ZKETECH_EBD_USB_PROTOCOL_H
#define LIBSIGROK_HARDWARE_ZKETECH_EBD_USB_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "zketech-ebd-usb"

#define MSG_MAX_LEN 19
#define MSG_FRAME_BEGIN 0xfa
#define MSG_FRAME_END 0xf8

struct dev_context {
	struct otc_sw_limits limits;
	GMutex rw_mutex;
	float current_limit;
	float uvc_threshold;
	gboolean running;
	gboolean load_activated;
};

/* Communication via serial. */
OTC_PRIV int ebd_read_message(struct otc_serial_dev_inst *serial, size_t length,
	uint8_t *buf);

/* Commands. */
OTC_PRIV int ebd_init(struct otc_serial_dev_inst *serial,
	struct dev_context *devc);
OTC_PRIV int ebd_loadstart(struct otc_serial_dev_inst *serial,
	struct dev_context *devc);
OTC_PRIV int ebd_receive_data(int fd, int revents, void *cb_data);
OTC_PRIV int ebd_stop(struct otc_serial_dev_inst *serial,
	struct dev_context *devc);
OTC_PRIV int ebd_loadtoggle(struct otc_serial_dev_inst *serial,
	struct dev_context *devc);

/* Configuration. */
OTC_PRIV int ebd_get_current_limit(const struct otc_dev_inst *sdi, float *current);
OTC_PRIV int ebd_set_current_limit(const struct otc_dev_inst *sdi, float current);
OTC_PRIV int ebd_get_uvc_threshold(const struct otc_dev_inst *sdi, float *voltage);
OTC_PRIV int ebd_set_uvc_threshold(const struct otc_dev_inst *sdi, float voltage);
OTC_PRIV gboolean ebd_current_is0(struct dev_context *devc);

#endif
