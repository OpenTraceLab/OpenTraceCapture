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

#include <config.h>
#include <string.h>
#include "protocol.h"

#define IDN_RETRIES 3 /* at least 2 */

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
	OTC_CONF_CHANNEL_CONFIG | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
};

static const uint32_t devopts_cg[] = {
	OTC_CONF_VOLTAGE | OTC_CONF_GET,
	OTC_CONF_VOLTAGE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CURRENT | OTC_CONF_GET,
	OTC_CONF_CURRENT_LIMIT | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static const char *channel_modes[] = {
	"Independent",
};

static const char *gpd_serialcomms[] = {
	"9600/8n1",
	"57600/8n1",
	"115200/8n1"
};

static const struct gpd_model models[] = {
	{ GPD_2303S, "GPD-2303S",
		CHANMODE_INDEPENDENT,
		2,
		{
			/* Channel 1 */
			{ { 0, 30, 0.001 }, { 0, 3, 0.001 } },
			/* Channel 2 */
			{ { 0, 30, 0.001 }, { 0, 3, 0.001 } },
		},
	},
	{ GPD_3303S, "GPD-3303S",
		CHANMODE_INDEPENDENT,
		2,
		{
			/* Channel 1 */
			{ { 0, 32, 0.001 }, { 0, 3.2, 0.001 } },
			/* Channel 2 */
			{ { 0, 32, 0.001 }, { 0, 3.2, 0.001 } },
		},
	},
};

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	const char *conn, *serialcomm, **serialcomms;
	const struct gpd_model *model;
	const struct otc_config *src;
	struct otc_channel *ch;
	struct otc_channel_group *cg;
	GSList *l;
	struct otc_serial_dev_inst *serial;
	struct otc_dev_inst *sdi;
	char reply[100];
	unsigned int i, b, serialcomms_count;
	struct dev_context *devc;
	char channel[10];
	GRegex *regex;
	GMatchInfo *match_info;
	unsigned int cc_cv_ch1, cc_cv_ch2, track1, track2, beep, baud1, baud2;

	serial = NULL;
	match_info = NULL;
	regex = NULL;
	conn = NULL;
	serialcomm = NULL;

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
	if (serialcomm) {
		serialcomms = &serialcomm;
		serialcomms_count = 1;
	} else {
		serialcomms = gpd_serialcomms;
		serialcomms_count = sizeof(gpd_serialcomms) / sizeof(gpd_serialcomms[0]);
	}

	for( b = 0; b < serialcomms_count; b++) {
		serialcomm = serialcomms[b];
		otc_info("Probing serial port %s @ %s", conn, serialcomm);
		serial = otc_serial_dev_inst_new(conn, serialcomm);
		if (serial_open(serial, SERIAL_RDWR) != OTC_OK)
			continue;

		/*
		 * Problem: we need to clear the GPD receive buffer before we
		 * can expect it to process commands correctly.
		 *
		 * Do not just send a newline, since that may cause it to
		 * execute a currently buffered command.
		 *
		 * Solution: Send identification request a few times.
		 * The first should corrupt any previous buffered command if present
		 * and respond with "Invalid Character." or respond directly with
		 * an identification string.
		 */
		for (i = 0; i < IDN_RETRIES; i++) {
			/* Request the GPD to identify itself */
			gpd_send_cmd(serial, "*IDN?\n");
			if (gpd_receive_reply(serial, reply, sizeof(reply)) == OTC_OK) {
				if (0 == strncmp(reply, "GW INSTEK", 9)) {
					break;
				}
			}
		}
		if (i == IDN_RETRIES) {
			otc_err("Device did not reply to identification request.");
			serial_flush(serial);
			goto error;
		}

		/*
		 * Returned identification string is for example:
		 * "GW INSTEK,GPD-2303S,SN:ER915277,V2.10"
		 */
		regex = g_regex_new("GW INSTEK,(.+),SN:(.+),(V.+)", 0, 0, NULL);
		if (!g_regex_match(regex, reply, 0, &match_info)) {
			otc_err("Unsupported model '%s'.", reply);
			goto error;
		}

		model = NULL;
		for (i = 0; i < ARRAY_SIZE(models); i++) {
			if (!strcmp(g_match_info_fetch(match_info, 1), models[i].name)) {
				model = &models[i];
				break;
			}
		}
		if (!model) {
			otc_err("Unsupported model '%s'.", reply);
			goto error;
		}

		otc_info("Detected model '%s'.", model->name);

		sdi = g_malloc0(sizeof(struct otc_dev_inst));
		sdi->status = OTC_ST_INACTIVE;
		sdi->vendor = g_strdup("GW Instek");
		sdi->model = g_strdup(model->name);
		sdi->inst_type = OTC_INST_SERIAL;
		sdi->conn = serial;

		for (i = 0; i < model->num_channels; i++) {
			snprintf(channel, sizeof(channel), "CH%d", i + 1);
			ch = otc_channel_new(sdi, i, OTC_CHANNEL_ANALOG, TRUE, channel);
			cg = otc_channel_group_new(sdi, channel, NULL);
			cg->channels = g_slist_append(NULL, ch);
		}

		devc = g_malloc0(sizeof(struct dev_context));
		otc_sw_limits_init(&devc->limits);
		devc->model = model;
		devc->config = g_malloc0(sizeof(struct per_channel_config)
					 * model->num_channels);
		sdi->priv = devc;

		serial_flush(serial);
		gpd_send_cmd(serial, "STATUS?\n");
		gpd_receive_reply(serial, reply, sizeof(reply));

		if (sscanf(reply, "%1u%1u%1u%1u%1u%1u%1u%1u", &cc_cv_ch1,
				&cc_cv_ch2, &track1, &track2, &beep,
				&devc->output_enabled, &baud1, &baud2) != 8) {
			/* old firmware (< 2.00?) responds with different format */
			if (sscanf(reply, "%1u %1u %1u %1u %1u X %1u X", &cc_cv_ch1,
				   &cc_cv_ch2, &track1, &track2, &beep,
				   &devc->output_enabled) != 6) {
				otc_err("Invalid reply to STATUS: '%s'.", reply);
				goto error;
			}
			/* ignore remaining two lines of status message */
			gpd_receive_reply(serial, reply, sizeof(reply));
			gpd_receive_reply(serial, reply, sizeof(reply));
		}

		for (i = 0; i < model->num_channels; i++) {
			gpd_send_cmd(serial, "ISET%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_current_max) != 1) {
				otc_err("Invalid reply to ISETn?: '%s'.", reply);
				goto error;
			}

			gpd_send_cmd(serial, "VSET%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_voltage_max) != 1) {
				otc_err("Invalid reply to VSETn?: '%s'.", reply);
				goto error;
			}
			gpd_send_cmd(serial, "IOUT%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_current_last) != 1) {
				otc_err("Invalid reply to IOUTn?: '%s'.", reply);
				goto error;
			}
			gpd_send_cmd(serial, "VOUT%d?\n", i + 1);
			gpd_receive_reply(serial, reply, sizeof(reply));
			if (sscanf(reply, "%f", &devc->config[i].output_voltage_last) != 1) {
				otc_err("Invalid reply to VOUTn?: '%s'.", reply);
				goto error;
			}
		}

		return std_scan_complete(di, g_slist_append(NULL, sdi));

	error:
		if (match_info)
			g_match_info_free(match_info);
		if (regex)
			g_regex_unref(regex);
		if (serial)
			serial_close(serial);
	}

	return NULL;
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int channel;
	const struct dev_context *devc;
	const struct otc_channel *ch;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	if (!cg) {
		switch (key) {
		case OTC_CONF_LIMIT_SAMPLES:
		case OTC_CONF_LIMIT_MSEC:
			return otc_sw_limits_config_get(&devc->limits, key, data);
		case OTC_CONF_CHANNEL_CONFIG:
			*data = g_variant_new_string(
				channel_modes[devc->channel_mode]);
			break;
		case OTC_CONF_ENABLED:
			*data = g_variant_new_boolean(devc->output_enabled);
			break;
		default:
			return OTC_ERR_NA;
		}
	} else {
		ch = cg->channels->data;
		channel = ch->index;
		switch (key) {
		case OTC_CONF_VOLTAGE:
			*data = g_variant_new_double(
				devc->config[channel].output_voltage_last);
			break;
		case OTC_CONF_VOLTAGE_TARGET:
			*data = g_variant_new_double(
				devc->config[channel].output_voltage_max);
			break;
		case OTC_CONF_CURRENT:
			*data = g_variant_new_double(
				devc->config[channel].output_current_last);
			break;
		case OTC_CONF_CURRENT_LIMIT:
			*data = g_variant_new_double(
				devc->config[channel].output_current_max);
			break;
		default:
			return OTC_ERR_NA;
		}
	}

	return OTC_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int ret, channel;
	const struct otc_channel *ch;
	double dval;
	gboolean bval;
	struct dev_context *devc;

	devc = sdi->priv;

	ret = OTC_OK;

	switch (key) {
	case OTC_CONF_LIMIT_MSEC:
	case OTC_CONF_LIMIT_SAMPLES:
		return otc_sw_limits_config_set(&devc->limits, key, data);
	case OTC_CONF_ENABLED:
		bval = g_variant_get_boolean(data);
		gpd_send_cmd(sdi->conn, "OUT%c\n", bval ? '1' : '0');
		devc->output_enabled = bval;
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		ch = cg->channels->data;
		channel = ch->index;
		dval = g_variant_get_double(data);
		if (dval < devc->model->channels[channel].voltage[0]
		    || dval > devc->model->channels[channel].voltage[1])
			return OTC_ERR_ARG;
		gpd_send_cmd(sdi->conn, "VSET%d:%05.3lf\n", channel + 1, dval);
		devc->config[channel].output_voltage_max = dval;
		break;
	case OTC_CONF_CURRENT_LIMIT:
		ch = cg->channels->data;
		channel = ch->index;
		dval = g_variant_get_double(data);
		if (dval < devc->model->channels[channel].current[0]
		    || dval > devc->model->channels[channel].current[1])
			return OTC_ERR_ARG;
		gpd_send_cmd(sdi->conn, "ISET%d:%05.3lf\n", channel + 1, dval);
		devc->config[channel].output_current_max = dval;
		break;
	default:
		ret = OTC_ERR_NA;
		break;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	const struct dev_context *devc;
	const struct otc_channel *ch;
	int channel;

	devc = (sdi) ? sdi->priv : NULL;

	if (!cg) {
		switch (key) {
		case OTC_CONF_SCAN_OPTIONS:
		case OTC_CONF_DEVICE_OPTIONS:
			return STD_CONFIG_LIST(key, data, sdi, cg, scanopts,
					       drvopts, devopts);
		case OTC_CONF_CHANNEL_CONFIG:
			*data = g_variant_new_strv(ARRAY_AND_SIZE(channel_modes));
			break;
		default:
			return OTC_ERR_NA;
		}
	} else {
		ch = cg->channels->data;
		channel = ch->index;

		switch (key) {
		case OTC_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
			break;
		case OTC_CONF_VOLTAGE_TARGET:
			*data = std_gvar_min_max_step_array(
				devc->model->channels[channel].voltage);
			break;
		case OTC_CONF_CURRENT_LIMIT:
			*data = std_gvar_min_max_step_array(
				devc->model->channels[channel].current);
			break;
		default:
			return OTC_ERR_NA;
		}
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
	serial_source_add(sdi->session, serial, G_IO_IN, 100,
			  gpd_receive_data, (void *)sdi);

	return OTC_OK;
}

static struct otc_dev_driver gwinstek_gpd_driver_info = {
	.name = "gwinstek-gpd",
	.longname = "GW Instek GPD",
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
OTC_REGISTER_DEV_DRIVER(gwinstek_gpd_driver_info);
