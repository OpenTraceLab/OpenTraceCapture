/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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
#include <glib.h>
#include <libopentracecapture/internal.h>
#include <stdlib.h>
#include "protocol.h"

static struct scale_info uss_scale_info = {
	{
		.name = "uss-scale",
		.longname = "USS scale",
		.api_version = 1,
		.init = std_init,
		.cleanup = std_cleanup,
		.scan = otc_scan_serial,
		.dev_list = NULL,
		.dev_clear = NULL,
		.config_get = NULL,
		.config_set = NULL,
		.config_channel_set = NULL,
		.config_commit = NULL,
		.config_list = NULL,
		.dev_open = otc_dev_open_serial,
		.dev_close = std_serial_dev_close,
		.dev_acquisition_start = std_serial_dev_acquisition_start,
		.dev_acquisition_stop = std_serial_dev_acquisition_stop,
	},
	.vendor = "U.S. Solid",
	.device = "USS-DBS28",
	.conn = "9600/8n1",
	.packet_size = 17,
	.packet_valid = otc_uss_dbs_packet_valid,
	.packet_parse = otc_uss_dbs_parse,
	.info_size = sizeof(struct uss_dbs_info),
};

OTC_PRIV int otc_hw_ussscale_init(struct otc_context *otc_ctx)
{
	int ret;

	ret = otc_hw_register(otc_ctx, &uss_scale_info);

	return ret;
}
