/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2013 Aurelien Jacobs <aurel@gnuage.org>
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
#include <stdlib.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "protocol.h"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_ENERGYMETER,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_SET,
};

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	struct otc_dev_inst *sdi;
	GSList *devices = NULL, *l;
	const char *conn = NULL, *serialcomm = NULL;
	uint8_t buf[292];
	size_t len;
	struct otc_config *src;

	len = sizeof(buf);

	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case OTC_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = "1200/7e1";

	serial = otc_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDONLY) != OTC_OK)
		return NULL;

	otc_info("Probing serial port %s.", conn);

	/* Let's get a bit of data and see if we can find a packet. */
	if (serial_stream_detect(serial, buf, &len, len,
			teleinfo_packet_valid, NULL, NULL, 3000) != OTC_OK)
		goto scan_cleanup;

	otc_info("Found device on port %s.", conn);

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("EDF");
	sdi->model = g_strdup("Teleinfo");
	devc = g_malloc0(sizeof(struct dev_context));
	devc->optarif = teleinfo_get_optarif(buf);
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "P");

	if (devc->optarif == OPTARIF_BASE) {
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "BASE");
	} else if (devc->optarif == OPTARIF_HC) {
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HP");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HC");
	} else if (devc->optarif == OPTARIF_EJP) {
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HN");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HPM");
	} else if (devc->optarif == OPTARIF_BBR) {
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HPJB");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HPJW");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HPJR");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HCJB");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HCJW");
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "HCJR");
	}

	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "IINST");
	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "PAPP");

	devices = g_slist_append(devices, sdi);

scan_cleanup:
	serial_close(serial);

	return std_scan_complete(di, devices);
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	devc = sdi->priv;

	return otc_sw_limits_config_set(&devc->sw_limits, key, data);
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct otc_serial_dev_inst *serial = sdi->conn;
	struct dev_context *devc;

	devc = sdi->priv;

	otc_sw_limits_acquisition_start(&devc->sw_limits);

	std_session_send_df_header(sdi);

	serial_source_add(sdi->session, serial, G_IO_IN, 50,
			teleinfo_receive_data, (void *)sdi);

	return OTC_OK;
}

static struct otc_dev_driver teleinfo_driver_info = {
	.name = "teleinfo",
	.longname = "Teleinfo",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = NULL,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = std_serial_dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(teleinfo_driver_info);
