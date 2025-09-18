/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2019 Dave Buechi <db@pflutsch.ch>
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
#include "protocol.h"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_THERMOMETER,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_DATA_SOURCE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static const char *channel_names[] = {
	"T1", "T2", "T1-T2",
};

static const char *data_sources[] = {
	"Live", "Memory",
};

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	struct otc_dev_inst *sdi;
	struct otc_config *src;
	GSList *devices, *l;
	const char *conn, *serialcomm;
	uint8_t buf[2 * MASTECH_MS6514_FRAME_SIZE];
	size_t len, i;

	len = sizeof(buf);
	devices = NULL;
	conn = serialcomm = NULL;
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
		serialcomm = "9600/8n1";

	serial = otc_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDONLY) != OTC_OK)
		return NULL;

	otc_info("Probing serial port %s.", conn);

	/* Let's get a bit of data and see if we can find a packet. */
	if (serial_stream_detect(serial, buf, &len, 2 * MASTECH_MS6514_FRAME_SIZE,
			mastech_ms6514_packet_valid, NULL, NULL, 500) != OTC_OK)
		goto scan_cleanup;

	otc_info("Found device on port %s.", conn);

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("MASTECH");
	sdi->model = g_strdup("MS6514");
	devc = g_malloc0(sizeof(struct dev_context));
	devc->data_source = DEFAULT_DATA_SOURCE;
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	for (i = 0; i < ARRAY_SIZE(channel_names); i++)
		otc_channel_new(sdi, i, OTC_CHANNEL_ANALOG, TRUE, channel_names[i]);

	devices = g_slist_append(devices, sdi);

scan_cleanup:
	serial_close(serial);

	return std_scan_complete(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_get(&devc->limits, key, data);
	case OTC_CONF_DATA_SOURCE:
		*data = g_variant_new_string(data_sources[devc->data_source]);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	int idx;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_set(&devc->limits, key, data);
	case OTC_CONF_DATA_SOURCE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(data_sources))) < 0)
			return OTC_ERR_ARG;
		devc->data_source = idx;
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case OTC_CONF_DATA_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(data_sources));
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct otc_serial_dev_inst *serial;
	struct dev_context *devc;
	uint8_t command;

	serial = sdi->conn;
	devc = sdi->priv;

	otc_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi);

	if (devc->data_source == DATA_SOURCE_MEMORY) {
		command = CMD_GET_STORED;
		serial_write_blocking(serial, &command, sizeof(command), 0);
	}

	serial_source_add(sdi->session, serial, G_IO_IN, MASTECH_MS6514_BUF_SIZE,
			mastech_ms6514_receive_data, (void *)sdi);

	return OTC_OK;
}

static struct otc_dev_driver mastech_ms6514_driver_info = {
	.name = "mastech-ms6514",
	.longname = "MASTECH MS6514",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = std_serial_dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(mastech_ms6514_driver_info);
