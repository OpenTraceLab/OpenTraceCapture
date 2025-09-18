/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2014 Matthias Heidbrink <m-opentracelab@heidbrink.biz>
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
#include "protocol.h"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_POWER_SUPPLY,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_VOLTAGE | OTC_CONF_GET,
	OTC_CONF_VOLTAGE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CURRENT | OTC_CONF_GET,
	OTC_CONF_CURRENT_LIMIT | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
};

/* Note: All models have one power supply output only. */
static const struct hcs_model models[] = {
	{ MANSON_HCS_3100, "HCS-3100",     "3100",     { 1, 18, 0.1 }, { 0, 10,   0.10 } },
	{ MANSON_HCS_3100, "HCS-3100",     "HCS-3100", { 1, 18, 0.1 }, { 0, 10,   0.10 } },
	{ MANSON_HCS_3102, "HCS-3102",     "3102",     { 1, 36, 0.1 }, { 0,  5,   0.01 } },
	{ MANSON_HCS_3102, "HCS-3102",     "HCS-3102", { 1, 36, 0.1 }, { 0,  5,   0.01 } },
	{ MANSON_HCS_3104, "HCS-3104",     "3104",     { 1, 60, 0.1 }, { 0,  2.5, 0.01 } },
	{ MANSON_HCS_3104, "HCS-3104",     "HCS-3104", { 1, 60, 0.1 }, { 0,  2.5, 0.01 } },
	{ MANSON_HCS_3150, "HCS-3150",     "3150",     { 1, 18, 0.1 }, { 0, 15,   0.10 } },
	{ MANSON_HCS_3150, "HCS-3150",     "HCS-3150", { 1, 18, 0.1 }, { 0, 15,   0.10 } },
	{ MANSON_HCS_3200, "HCS-3200",     "3200",     { 1, 18, 0.1 }, { 0, 20,   0.10 } },
	{ MANSON_HCS_3200, "HCS-3200",     "HCS-3200", { 1, 18, 0.1 }, { 0, 20,   0.10 } },
	{ MANSON_HCS_3202, "HCS-3202",     "3202",     { 1, 36, 0.1 }, { 0, 10,   0.10 } },
	{ MANSON_HCS_3202, "HCS-3202",     "HCS-3202", { 1, 36, 0.1 }, { 0, 10,   0.10 } },
	{ MANSON_HCS_3204, "HCS-3204",     "3204",     { 1, 60, 0.1 }, { 0,  5,   0.01 } },
	{ MANSON_HCS_3204, "HCS-3204",     "HCS-3204", { 1, 60, 0.1 }, { 0,  5,   0.01 } },
	{ MANSON_HCS_3300, "HCS-3300-USB", "3300",     { 1, 16, 0.1 }, { 0, 30,   0.10 } },
	{ MANSON_HCS_3300, "HCS-3300-USB", "HCS-3300", { 1, 16, 0.1 }, { 0, 30,   0.10 } },
	{ MANSON_HCS_3302, "HCS-3302-USB", "3302",     { 1, 32, 0.1 }, { 0, 15,   0.10 } },
	{ MANSON_HCS_3302, "HCS-3302-USB", "HCS-3302", { 1, 32, 0.1 }, { 0, 15,   0.10 } },
	{ MANSON_HCS_3304, "HCS-3304-USB", "3304",     { 1, 60, 0.1 }, { 0,  8,   0.10 } },
	{ MANSON_HCS_3304, "HCS-3304-USB", "HCS-3304", { 1, 60, 0.1 }, { 0,  8,   0.10 } },
	{ MANSON_HCS_3400, "HCS-3400-USB", "3400",     { 1, 16, 0.1 }, { 0, 40,   0.10 } },
	{ MANSON_HCS_3400, "HCS-3400-USB", "HCS-3400", { 1, 16, 0.1 }, { 0, 40,   0.10 } },
	{ MANSON_HCS_3402, "HCS-3402-USB", "3402",     { 1, 32, 0.1 }, { 0, 20,   0.10 } },
	{ MANSON_HCS_3402, "HCS-3402-USB", "HCS-3402", { 1, 32, 0.1 }, { 0, 20,   0.10 } },
	{ MANSON_HCS_3404, "HCS-3404-USB", "3404",     { 1, 60, 0.1 }, { 0, 10,   0.10 } },
	{ MANSON_HCS_3404, "HCS-3404-USB", "HCS-3404", { 1, 60, 0.1 }, { 0, 10,   0.10 } },
	{ MANSON_HCS_3600, "HCS-3600-USB", "3600",     { 1, 16, 0.1 }, { 0, 60,   0.10 } },
	{ MANSON_HCS_3600, "HCS-3600-USB", "HCS-3600", { 1, 16, 0.1 }, { 0, 60,   0.10 } },
	{ MANSON_HCS_3602, "HCS-3602-USB", "3602",     { 1, 32, 0.1 }, { 0, 30,   0.10 } },
	{ MANSON_HCS_3602, "HCS-3602-USB", "HCS-3602", { 1, 32, 0.1 }, { 0, 30,   0.10 } },
	{ MANSON_HCS_3604, "HCS-3604-USB", "3604",     { 1, 60, 0.1 }, { 0, 15,   0.10 } },
	{ MANSON_HCS_3604, "HCS-3604-USB", "HCS-3604", { 1, 60, 0.1 }, { 0, 15,   0.10 } },
	ALL_ZERO
};

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	int i, model_id;
	struct dev_context *devc;
	struct otc_dev_inst *sdi;
	struct otc_config *src;
	GSList *l;
	const char *conn, *serialcomm;
	struct otc_serial_dev_inst *serial;
	char reply[50], **tokens, *dummy;

	conn = NULL;
	serialcomm = NULL;
	devc = NULL;

	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case OTC_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		default:
			otc_err("Unknown option %d, skipping.", src->key);
			break;
		}
	}

	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = "9600/8n1";

	serial = otc_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != OTC_OK)
		return NULL;

	otc_info("Probing serial port %s.", conn);

	/* Get the device model. */
	memset(&reply, 0, sizeof(reply));
	if ((hcs_send_cmd(serial, "GMOD\r") < 0) ||
	    (hcs_read_reply(serial, 2, reply, sizeof(reply)) < 0))
		return NULL;
	tokens = g_strsplit((const gchar *)&reply, "\r", 2);

	model_id = -1;
	for (i = 0; models[i].id != NULL; i++) {
		if (!strcmp(models[i].id, tokens[0]))
			model_id = i;
	}
	if (model_id < 0) {
		otc_err("Unknown model ID '%s' detected, aborting.", tokens[0]);
		g_strfreev(tokens);
		return NULL;
	}
	g_strfreev(tokens);

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("Manson");
	sdi->model = g_strdup(models[model_id].name);
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->conn = serial;

	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "CH1");

	devc = g_malloc0(sizeof(struct dev_context));
	otc_sw_limits_init(&devc->limits);
	devc->model = &models[model_id];

	sdi->priv = devc;

	/* Get current voltage, current, status. */
	if ((hcs_send_cmd(serial, "GETD\r") < 0) ||
	    (hcs_read_reply(serial, 2, reply, sizeof(reply)) < 0))
		goto exit_err;
	tokens = g_strsplit((const gchar *)&reply, "\r", 2);
	if (hcs_parse_volt_curr_mode(sdi, tokens) < 0) {
		g_strfreev(tokens);
		goto exit_err;
	}
	g_strfreev(tokens);

	/* Get max. voltage and current. */
	if ((hcs_send_cmd(serial, "GMAX\r") < 0) ||
	    (hcs_read_reply(serial, 2, reply, sizeof(reply)) < 0))
		goto exit_err;
	tokens = g_strsplit((const gchar *)&reply, "\r", 2);
	devc->current_max_device = g_strtod(&tokens[0][3], &dummy) * devc->model->current[2];
	tokens[0][3] = '\0';
	devc->voltage_max_device = g_strtod(tokens[0], &dummy) * devc->model->voltage[2];
	g_strfreev(tokens);

	serial_close(serial);

	return std_scan_complete(di, g_slist_append(NULL, sdi));

exit_err:
	otc_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_get(&devc->limits, key, data);
	case OTC_CONF_VOLTAGE:
		*data = g_variant_new_double(devc->voltage);
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		*data = g_variant_new_double(devc->voltage_max);
		break;
	case OTC_CONF_CURRENT:
		*data = g_variant_new_double(devc->current);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		*data = g_variant_new_double(devc->current_max);
		break;
	case OTC_CONF_ENABLED:
		*data = g_variant_new_boolean(devc->output_enabled);
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
	gboolean bval;
	gdouble dval;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_LIMIT_MSEC:
	case OTC_CONF_LIMIT_SAMPLES:
		return otc_sw_limits_config_set(&devc->limits, key, data);
	case OTC_CONF_VOLTAGE_TARGET:
		dval = g_variant_get_double(data);
		if (dval < devc->model->voltage[0] || dval > devc->voltage_max_device)
			return OTC_ERR_ARG;

		if ((hcs_send_cmd(sdi->conn, "VOLT%03.0f\r",
			(dval / devc->model->voltage[2])) < 0) ||
		    (hcs_read_reply(sdi->conn, 1, devc->buf, sizeof(devc->buf)) < 0))
			return OTC_ERR;
		devc->voltage_max = dval;
		break;
	case OTC_CONF_CURRENT_LIMIT:
		dval = g_variant_get_double(data);
		if (dval < devc->model->current[0] || dval > devc->current_max_device)
			return OTC_ERR_ARG;

		if ((hcs_send_cmd(sdi->conn, "CURR%03.0f\r",
			(dval / devc->model->current[2])) < 0) ||
		    (hcs_read_reply(sdi->conn, 1, devc->buf, sizeof(devc->buf)) < 0))
			return OTC_ERR;
		devc->current_max = dval;
		break;
	case OTC_CONF_ENABLED:
		bval = g_variant_get_boolean(data);

		if (hcs_send_cmd(sdi->conn, "SOUT%1d\r", !bval) < 0) {
			otc_err("Could not send OTC_CONF_ENABLED command.");
			return OTC_ERR;
		}
		if (hcs_read_reply(sdi->conn, 1, devc->buf, sizeof(devc->buf)) < 0) {
			otc_err("Could not read OTC_CONF_ENABLED reply.");
			return OTC_ERR;
		}
		devc->output_enabled = bval;
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	const double *a;
	struct dev_context *devc;

	devc = (sdi) ? sdi->priv : NULL;

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case OTC_CONF_VOLTAGE_TARGET:
		if (!devc || !devc->model)
			return OTC_ERR_ARG;
		a = devc->model->voltage;
		*data = std_gvar_min_max_step(a[0], devc->voltage_max_device, a[2]);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		if (!devc || !devc->model)
			return OTC_ERR_ARG;
		a = devc->model->current;
		*data = std_gvar_min_max_step(a[0], devc->current_max_device, a[2]);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;

	devc = sdi->priv;

	otc_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi);

	devc->reply_pending = FALSE;
	devc->req_sent_at = 0;

	serial = sdi->conn;
	serial_source_add(sdi->session, serial, G_IO_IN, 10,
			hcs_receive_data, (void *)sdi);

	return OTC_OK;
}

static struct otc_dev_driver manson_hcs_3xxx_driver_info = {
	.name = "manson-hcs-3xxx",
	.longname = "Manson HCS-3xxx",
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
OTC_REGISTER_DEV_DRIVER(manson_hcs_3xxx_driver_info);
