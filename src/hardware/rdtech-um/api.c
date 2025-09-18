/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2018-2020 Andreas Sandberg <andreas@sandberg.pp.se>
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

#include <config.h>

#include <glib.h>
#include <fcntl.h>
#include <opentracecapture/libopentracecapture.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "../../libopentracecapture-internal.h"
#include "protocol.h"

#define RDTECH_UM_SERIALCOMM "115200/8n1"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_ENERGYMETER,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_FRAMES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
};

static GSList *rdtech_um_scan(struct otc_dev_driver *di,
	const char *conn, const char *serialcomm)
{
	struct otc_serial_dev_inst *serial;
	const struct rdtech_um_profile *p;
	GSList *devices;
	struct dev_context *devc;
	struct otc_dev_inst *sdi;
	size_t ch_idx;
	const struct rdtech_um_channel_desc *pch;
	struct otc_channel *ch;
	struct feed_queue_analog *feed;

	serial = otc_serial_dev_inst_new(conn, serialcomm);
	if (serial_open(serial, SERIAL_RDWR) != OTC_OK)
		goto err_out;

	p = rdtech_um_probe(serial);
	if (!p) {
		otc_err("Failed to find a supported RDTech UM device.");
		goto err_out_serial;
	}

	devc = g_malloc0(sizeof(*devc));
	otc_sw_limits_init(&devc->limits);
	devc->profile = p;

	sdi = g_malloc0(sizeof(*sdi));
	sdi->priv = devc;
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("RDTech");
	sdi->model = g_strdup(p->model_name);
	sdi->version = NULL;
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->conn = serial;

	devc->feeds = g_malloc0(p->channel_count * sizeof(devc->feeds[0]));
	for (ch_idx = 0; ch_idx < p->channel_count; ch_idx++) {
		pch = &p->channels[ch_idx];
		ch = otc_channel_new(sdi, ch_idx,
			OTC_CHANNEL_ANALOG, TRUE, pch->name);
		feed = feed_queue_analog_alloc(sdi, 1, pch->digits, ch);
		feed_queue_analog_mq_unit(feed, pch->mq, 0, pch->unit);
		feed_queue_analog_scale_offset(feed, &pch->scale, NULL);
		devc->feeds[ch_idx] = feed;
	}

	devices = g_slist_append(NULL, sdi);
	serial_close(serial);
	if (!devices)
		otc_serial_dev_inst_free(serial);

	return std_scan_complete(di, devices);

err_out_serial:
	serial_close(serial);
err_out:
	otc_serial_dev_inst_free(serial);

	return NULL;
}

static void clear_helper(struct dev_context *devc)
{
	const struct rdtech_um_profile *p;
	size_t ch_idx;

	if (!devc)
		return;

	p = devc->profile;
	if (p && devc->feeds) {
		for (ch_idx = 0; ch_idx < p->channel_count; ch_idx++)
			feed_queue_analog_free(devc->feeds[ch_idx]);
		g_free(devc->feeds);
	}
}

static int dev_clear(const struct otc_dev_driver *di)
{
	return std_dev_clear_with_callback(di, (std_dev_clear_callback)clear_helper);
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	const char *conn;
	const char *serialcomm;

	conn = NULL;
	serialcomm = RDTECH_UM_SERIALCOMM;
	(void)otc_serial_extract_options(options, &conn, &serialcomm);
	if (!conn)
		return NULL;

	return rdtech_um_scan(di, conn, serialcomm);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	return otc_sw_limits_config_get(&devc->limits, key, data);
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	return otc_sw_limits_config_set(&devc->limits, key, data);
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;

	devc = sdi->priv;
	otc_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi);

	serial = sdi->conn;
	serial_source_add(sdi->session, serial, G_IO_IN, 50,
		rdtech_um_receive_data, (void *)sdi);

	return rdtech_um_poll(sdi, TRUE);
}

static struct otc_dev_driver rdtech_um_driver_info = {
	.name = "rdtech-um",
	.longname = "RDTech UMxx USB power meter",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = std_serial_dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(rdtech_um_driver_info);
