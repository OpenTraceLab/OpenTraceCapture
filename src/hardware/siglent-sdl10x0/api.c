/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2024 Timo Boettcher <timo@timoboettcher.name>
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
#include "../../scpi.h"
#include "protocol.h"

static const char *manufacturers[] = {
	"Siglent Technologies",
};

static const char *models[] = {
	"SDL1020X-E",
	"SDL1020X",
	"SDL1030X-E",
	"SDL1030X",
};

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
};

static const uint32_t drvopts[] = {
	OTC_CONF_ELECTRONIC_LOAD,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
};

static const uint32_t devopts_cg[] = {
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_REGULATION | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_VOLTAGE | OTC_CONF_GET,
	OTC_CONF_VOLTAGE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CURRENT | OTC_CONF_GET,
	OTC_CONF_CURRENT_LIMIT | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_POWER | OTC_CONF_GET,
	OTC_CONF_POWER_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_RESISTANCE | OTC_CONF_GET,
	OTC_CONF_RESISTANCE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_OVER_POWER_PROTECTION_ENABLED | OTC_CONF_GET,
	OTC_CONF_OVER_POWER_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_POWER_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED | OTC_CONF_GET,
	OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
};

static const char *regulation[] = {
	"CURRENT",
	"VOLTAGE",
	"POWER",
	"RESISTANCE"
};

static struct otc_dev_driver siglent_sdl10x0_driver_info;

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct otc_dev_inst *sdi;
	struct otc_channel_group *cg;
	struct otc_channel *ch;
	struct dev_context *devc;
	struct otc_scpi_hw_info *hw_info;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK) {
		otc_info("Couldn't get IDN response.");
		goto fail;
	}

	if (std_str_idx_s(hw_info->manufacturer, ARRAY_AND_SIZE(manufacturers)) < 0)
		goto fail;

	if (std_str_idx_s(hw_info->model, ARRAY_AND_SIZE(models)) < 0)
		goto fail;

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &siglent_sdl10x0_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	sdi->conn = scpi;
	sdi->channels = NULL;
	sdi->channel_groups = NULL;

	cg = otc_channel_group_new(sdi, "1", NULL);

	ch = otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "1");
	cg->channels = g_slist_append(cg->channels, ch);

	otc_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;

	/*
	 * modelname 'SDL1020X-E':
	 * 6th character indicates wattage:
	 * 2 => 200
	 * 3 => 300
	 */
	devc->maxpower = 200.0;
	if (g_ascii_strncasecmp(sdi->model, "SDL1030", strlen("SDL1030")) == 0) {
		devc->maxpower = 300.0;
	}

	return sdi;

fail:
	otc_scpi_hw_info_free(hw_info);
	otc_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	return otc_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	return otc_scpi_open(sdi->conn);
}

static int dev_close(struct otc_dev_inst *sdi)
{
	return otc_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int ret;
	float ival;
	gboolean bval;
	char *mode;

	(void)cg;
	struct dev_context *devc;

	devc = sdi->priv;
	if (!devc)
		return OTC_ERR;

	ret = OTC_OK;
	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_get(&devc->limits, key, data);
	case OTC_CONF_ENABLED:
		ret = otc_scpi_get_bool(sdi->conn, ":INPUT?", &bval);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_boolean(bval);
		break;
	case OTC_CONF_REGULATION:
		ret = otc_scpi_get_string(sdi->conn, ":FUNC?", &mode);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_string(mode);
		break;
	case OTC_CONF_VOLTAGE:
		ret = otc_scpi_get_float(sdi->conn, "MEAS:VOLTage?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		ret = otc_scpi_get_float(sdi->conn, ":VOLTage:LEVel?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_CURRENT:
		ret = otc_scpi_get_float(sdi->conn, "MEAS:CURRent?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		ret = otc_scpi_get_float(sdi->conn, ":CURRENT:LEVel?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_POWER:
		ret = otc_scpi_get_float(sdi->conn, "MEAS:POWer?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_POWER_TARGET:
		ret = otc_scpi_get_float(sdi->conn, ":POWer:LEVel?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_RESISTANCE:
		ret = otc_scpi_get_float(sdi->conn, "MEAS:RESistance?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_RESISTANCE_TARGET:
		ret = otc_scpi_get_float(sdi->conn, ":RESistance:LEVel?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_OVER_POWER_PROTECTION_ENABLED:
		/* Always true */
		*data = g_variant_new_boolean(TRUE);
		break;
	case OTC_CONF_OVER_POWER_PROTECTION_ACTIVE:
		ret = otc_scpi_get_bool(sdi->conn, ":POWer:PROTection:STATe?", &bval);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_boolean(bval);
		break;
	case OTC_CONF_OVER_POWER_PROTECTION_THRESHOLD:
		ret = otc_scpi_get_float(sdi->conn, ":POWer:PROTection:LEVel?", &ival);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		/* Always true */
		*data = g_variant_new_boolean(TRUE);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE:
		ret = otc_scpi_get_bool(sdi->conn, ":CURRent:PROTection:STATe?", &bval);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_boolean(bval);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		ret = otc_scpi_get_bool(sdi->conn, ":CURRent:PROTection:LEVel?", &bval);
		if (ret != OTC_OK)
			return OTC_ERR;
		*data = g_variant_new_double((double)ival);
		break;
	default:
		return OTC_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int ret;
	double ivalue;
	gboolean ena;
	char command[64];
	const char *mode_str;
	enum siglent_sdl10x0_modes mode;

	(void)cg;
	struct dev_context *devc;

	devc = sdi->priv;
	if (!devc)
		return OTC_ERR;

	ret = OTC_OK;
	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_set(&devc->limits, key, data);
	case OTC_CONF_ENABLED:
		ena = g_variant_get_boolean(data);
		g_snprintf(command, sizeof(command), ":INPUT %s", ena ? "ON" : "OFF");
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_REGULATION:
		mode_str = g_variant_get_string(data, NULL);
		if (siglent_sdl10x0_string_to_mode(mode_str, &mode) != OTC_OK) {
			ret = OTC_ERR_ARG;
			break;
		}
		g_snprintf(command, sizeof(command), ":FUNC %s", siglent_sdl10x0_mode_to_longstring(mode));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		ivalue = g_variant_get_double(data);
		g_snprintf(command, sizeof(command), ":VOLT:LEV:IMM %.3f", (ivalue));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		ivalue = g_variant_get_double(data);
		g_snprintf(command, sizeof(command), ":CURR:LEV:IMM %.3f", (ivalue));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_POWER_TARGET:
		ivalue = g_variant_get_double(data);
		g_snprintf(command, sizeof(command), ":POW:LEV:IMM %.3f", (ivalue));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_RESISTANCE_TARGET:
		ivalue = g_variant_get_double(data);
		g_snprintf(command, sizeof(command), ":RES:LEV:IMM %.3f", (ivalue));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_OVER_POWER_PROTECTION_THRESHOLD:
		ivalue = g_variant_get_double(data);
		g_snprintf(command, sizeof(command), ":POW:PROT:LEV %.3f", (ivalue));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		ivalue = g_variant_get_double(data);
		g_snprintf(command, sizeof(command), ":CURR:PROT:LEV %.3f", (ivalue));
		otc_spew("Sending '%s'.", command);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	default:
		ret = OTC_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{

	struct dev_context *devc;

	devc = sdi ? sdi->priv : NULL;

	if (!cg) {
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	} else {
		switch (key) {
		case OTC_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg));
			break;
		case OTC_CONF_REGULATION:
			*data = std_gvar_array_str(ARRAY_AND_SIZE(regulation));
			break;
		case OTC_CONF_VOLTAGE_TARGET:
			*data = std_gvar_min_max_step(0.0, 150.0, 0.001);
			break;
		case OTC_CONF_CURRENT_LIMIT:
			*data = std_gvar_min_max_step(0.0, 30.0, 0.001);
			break;
		case OTC_CONF_POWER_TARGET:
			if (!devc) {
				*data = std_gvar_min_max_step(0.0, 200.0, 0.001);
			} else {
				*data = std_gvar_min_max_step(0.0, devc->maxpower, 0.001);
			}
			break;
		case OTC_CONF_RESISTANCE_TARGET:
			*data = std_gvar_min_max_step(0.03, 10000.0, 0.01);
			break;
		case OTC_CONF_OVER_POWER_PROTECTION_THRESHOLD:
			if (!devc) {
				*data = std_gvar_min_max_step(0.0, 200.0, 0.001);
			} else {
				*data = std_gvar_min_max_step(0.0, devc->maxpower, 0.001);
			}
			break;
		case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
			*data = std_gvar_min_max_step(0.0, 30.0, 0.001);
			break;
		default:
			return OTC_ERR_NA;
		}
	}
	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;

	scpi = sdi->conn;
	devc = sdi->priv;

	otc_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi);

	/*
	 * Start acquisition. The receive routine will continue
	 * driving the acquisition.
	 */
	otc_scpi_send(scpi, "MEAS:VOLT?");
	devc->acq_state = ACQ_REQUESTED_VOLTAGE;
	return otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 100, siglent_sdl10x0_handle_events, (void *)sdi);
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;

	scpi = sdi->conn;

	otc_scpi_source_remove(sdi->session, scpi);
	std_session_send_df_end(sdi);

	return OTC_OK;
}

static struct otc_dev_driver siglent_sdl10x0_driver_info = {
	.name = "siglent-sdl10x0",
	.longname = "SIGLENT SDL10x0",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(siglent_sdl10x0_driver_info);
