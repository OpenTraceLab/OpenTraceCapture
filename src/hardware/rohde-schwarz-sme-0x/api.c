/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2016 Vlad Ivanov <vlad.ivanov@lab-systems.ru>
 * Copyright (C) 2024 Daniel Anselmi <danselmi@gmx.ch>
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
#include <string.h>
#include "protocol.h"

static struct otc_dev_driver rohde_schwarz_sme_0x_driver_info;

static const char *manufacturer = "ROHDE&SCHWARZ";

static const struct rs_device_model_config model_sme0x = {
	.freq_step = 0.1,
	.power_step = 0.1,
	.commands = commands_sme0x,
	.responses = responses_sme0x,
};

static const struct rs_device_model_config model_smx100 = {
	.freq_step = 0.001,
	.power_step = 0.01,
	.commands = commands_smx100,
	.responses = responses_smx100,
};

static const struct rs_device_model device_models[] = {
	{
		.model_str = "SME02",
		.model_config = &model_sme0x,
	},
	{
		.model_str = "SME03E",
		.model_config = &model_sme0x,
	},
	{
		.model_str = "SME03A",
		.model_config = &model_sme0x,
	},
	{
		.model_str = "SME03",
		.model_config = &model_sme0x,
	},
	{
		.model_str = "SME06",
		.model_config = &model_sme0x,
	},
	{
		.model_str = "SMB100A",
		.model_config = &model_smx100,
	},
	{
		.model_str = "SMBV100A",
		.model_config = &model_smx100,
	},
	{
		.model_str = "SMC100A",
		.model_config = &model_smx100,
	},
};

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_SIGNAL_GENERATOR,
};

static const uint32_t devopts[] = {
	OTC_CONF_OUTPUT_FREQUENCY      | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_AMPLITUDE             | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_ENABLED               | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_EXTERNAL_CLOCK_SOURCE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static const char *clock_sources[] = {
	"Internal",
	"External",
};

static int rs_init_device(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t model_found;
	double min_val;
	double max_val;
	size_t i;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	model_found = 0;

	for (i = 0; i < ARRAY_SIZE(device_models); i++) {
		if (!strcmp(device_models[i].model_str, sdi->model)) {
			model_found = 1;
			devc->model_config = device_models[i].model_config;
			break;
		}
	}

	if (!model_found)
		return OTC_ERR_NA;

	if (rs_sme0x_init(sdi) != OTC_OK)
		return OTC_ERR;

	if (rs_sme0x_get_minmax_freq(sdi, &min_val, &max_val) != OTC_OK)
		return OTC_ERR;
	devc->freq_min = min_val;
	devc->freq_max = max_val;

	if (rs_sme0x_get_minmax_power(sdi, &min_val, &max_val) != OTC_OK)
		return OTC_ERR;
	devc->power_min = min_val;
	devc->power_max = max_val;

	if (rs_sme0x_sync(sdi) != OTC_OK)
		return OTC_ERR;

	return OTC_OK;
}

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_scpi_hw_info *hw_info;
	const char *vendor;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK)
		goto fail;

	vendor = otc_vendor_alias(hw_info->manufacturer);
	if (strcmp(vendor, manufacturer) != 0)
		goto fail;

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &rohde_schwarz_sme_0x_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	sdi->conn = scpi;

	otc_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;

	if (rs_init_device(sdi) != OTC_OK)
		goto fail;

	if (rs_sme0x_mode_remote(sdi) != OTC_OK)
		goto fail;

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
	rs_sme0x_mode_local(sdi);
	return otc_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi || !sdi->priv)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_OUTPUT_FREQUENCY:
		*data = g_variant_new_double(devc->freq);
		break;
	case OTC_CONF_AMPLITUDE:
		*data = g_variant_new_double(devc->power);
		break;
	case OTC_CONF_ENABLED:
		*data = g_variant_new_boolean(devc->enable);
		break;
	case OTC_CONF_EXTERNAL_CLOCK_SOURCE:
		*data = g_variant_new_string(clock_sources[devc->clk_source_idx]);
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
	double value_f;
	gboolean value_b;
	int idx;

	(void)cg;

	if (!sdi || !sdi->priv)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_OUTPUT_FREQUENCY:
		value_f = g_variant_get_double(data);
		if (value_f < devc->freq_min || value_f > devc->freq_max)
			return OTC_ERR_ARG;
		return rs_sme0x_set_freq(sdi, value_f);
	case OTC_CONF_AMPLITUDE:
		value_f = g_variant_get_double(data);
		if (value_f < devc->power_min || value_f > devc->power_max)
			return OTC_ERR_ARG;
		return rs_sme0x_set_power(sdi, value_f);
	case OTC_CONF_ENABLED:
		value_b = g_variant_get_boolean(data);
		return rs_sme0x_set_enable(sdi, value_b);
	case OTC_CONF_EXTERNAL_CLOCK_SOURCE:
		idx = std_str_idx(data, ARRAY_AND_SIZE(clock_sources));
		if (idx < 0)
			return OTC_ERR_ARG;
		return rs_sme0x_set_clk_src(sdi, idx);
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case OTC_CONF_OUTPUT_FREQUENCY:
		if (!sdi || !sdi->priv)
			return OTC_ERR_ARG;
		devc = sdi->priv;
		*data = std_gvar_min_max_step(devc->freq_min, devc->freq_max,
			devc->model_config->freq_step);
		break;
	case OTC_CONF_AMPLITUDE:
		if (!sdi || !sdi->priv)
			return OTC_ERR_ARG;
		devc = sdi->priv;
		*data = std_gvar_min_max_step(devc->power_min, devc->power_max,
			devc->model_config->power_step);
		break;
	case OTC_CONF_EXTERNAL_CLOCK_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(clock_sources));
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static struct otc_dev_driver rohde_schwarz_sme_0x_driver_info = {
	.name = "rohde-schwarz-sme-0x",
	.longname = "Rohde&Schwarz SME-0x & SMx100",
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
	.dev_acquisition_start = std_dummy_dev_acquisition_start,
	.dev_acquisition_stop = std_serial_dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(rohde_schwarz_sme_0x_driver_info);
