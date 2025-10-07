/*
 * This file is part of the libsigrok project.
 *
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
#include "protocol.h"

static struct otc_dev_driver rohde_schwarz_nrpxsn_driver_info;

static const char *manufacturer = "ROHDE&SCHWARZ";

static const char *rs_trigger_sources[] = {
	"INT",
	"EXT",
};

static const struct rohde_schwarz_nrpxsn_device_model device_models[] = {
	{
		.model_str = "NRP8S",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(8),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
	{
		.model_str = "NRP8SN",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(8),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
	{
		.model_str = "NRP18S",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(18),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
	{
		.model_str = "NRP18SN",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(18),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
	{
		.model_str = "NRP33S",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(33),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
	{
		.model_str = "NRP33SN",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(33),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
	{
		.model_str = "NRP40S",
		.freq_min = OTC_MHZ(50),
		.freq_max = OTC_GHZ(40),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 100e-3,	/* 20dBm */
	},
	{
		.model_str = "NRP40SN",
		.freq_min = OTC_MHZ(50),
		.freq_max = OTC_GHZ(40),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 100e-3,	/* 20dBm */
	},
	{
		.model_str = "NRP50S",
		.freq_min = OTC_MHZ(50),
		.freq_max = OTC_GHZ(50),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 100e-3,	/* 20dBm */
	},
	{
		.model_str = "NRP50SN",
		.freq_min = OTC_MHZ(50),
		.freq_max = OTC_GHZ(50),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 100e-3,	/* 20dBm */
	},
	{
		.model_str = "NRP18S-10",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(18),
		.power_min = 1e-9,	/* -60dBm */
		.power_max = 2.0,	/* 33dBm */
	},
	{
		.model_str = "NRP18S-20",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(18),
		.power_min = 10e-9,	/* -50dBm */
		.power_max = 15.0,	/* 42dBm */
	},
	{
		.model_str = "NRP18S-25",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(18),
		.power_min = 30e-9,	/* -45dBm */
		.power_max = 30.0,	/* 45dBm */
	},
	{
		.model_str = "NRP33SN-V",
		.freq_min = OTC_MHZ(10),
		.freq_max = OTC_GHZ(33),
		.power_min = 100e-12,	/* -70dBm */
		.power_max = 200e-3,	/* 23dBm */
	},
};

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_POWERMETER,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_CONN | OTC_CONF_GET,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_CENTER_FREQUENCY | OTC_CONF_GET | OTC_CONF_SET,
};

static int init_device(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t model_found;
	size_t i;

	devc = sdi->priv;
	model_found = 0;

	for (i = 0; i < ARRAY_SIZE(device_models); i++) {
		if (!strcmp(device_models[i].model_str, sdi->model)) {
			model_found = 1;
			devc->model_config = &device_models[i];
			break;
		}
	}

	if (!model_found)
		return OTC_ERR_NA;

	return OTC_OK;
}

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct otc_scpi_hw_info *hw_info;
	int ret;
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	const char *vendor;

	ret = otc_scpi_get_hw_id(scpi, &hw_info);
	if (ret != OTC_OK) {
		otc_info("Could not get IDN response.");
		return NULL;
	}

	vendor = otc_vendor_alias(hw_info->manufacturer);
	if (strcmp(vendor, manufacturer) != 0) {
		otc_info("not an R&S device.");
		otc_scpi_hw_info_free(hw_info);
		return NULL;
	}

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->conn = scpi;
	sdi->driver = &rohde_schwarz_nrpxsn_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	ret = otc_scpi_connection_id(scpi, &sdi->connection_id);
	if (ret != OTC_OK) {
		g_free(sdi->connection_id);
		sdi->connection_id = NULL;
	}
	otc_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;

	if (init_device(sdi) != OTC_OK) {
		otc_dev_inst_free(sdi);
		g_free(devc);
		return NULL;
	}
	devc->trigger_source = 0;
	devc->trigger_source_changed = 0;
	devc->curr_freq = OTC_MHZ(50);
	devc->curr_freq_changed = 0;
	devc->measurement_state = IDLE;

	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "P1");

	return sdi;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	return otc_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	int ret;

	scpi = sdi->conn;
	ret = otc_scpi_open(scpi);
	if (ret < 0) {
		otc_err("Failed to open SCPI device: %s.", otc_strerror(ret));
		return OTC_ERR;
	}

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;

	if (!sdi || !(sdi->conn))
		return OTC_ERR_BUG;

	scpi = sdi->conn;

	if (sdi->status <= OTC_ST_INACTIVE)
		return OTC_OK;

	return otc_scpi_close(scpi);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	const struct rohde_schwarz_nrpxsn_device_model *model;
	(void)cg;

	devc = NULL;
	model = NULL;
	if (sdi) {
		devc = sdi->priv;
		model = devc->model_config;
	}

	switch (key) {
	case OTC_CONF_CONN:
		if (!sdi || !sdi->connection_id)
			return OTC_ERR_NA;
		*data = g_variant_new_string(sdi->connection_id);
		return OTC_OK;
	case OTC_CONF_CENTER_FREQUENCY:
		if (!model)
			return OTC_ERR;
		*data = g_variant_new_uint64(devc->curr_freq);
		return OTC_OK;
	case OTC_CONF_LIMIT_SAMPLES:
		if (!devc)
			return OTC_ERR_BUG;
		return otc_sw_limits_config_get(&devc->limits, key, data);
	case OTC_CONF_TRIGGER_SOURCE:
		if (!model)
			return OTC_ERR_ARG;
		*data = g_variant_new_string(rs_trigger_sources[devc->trigger_source]);
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
	struct otc_scpi_dev_inst *scpi;
	const struct rohde_schwarz_nrpxsn_device_model *model;
	int idx;
	double freq;
	(void)cg;

	devc = NULL;
	model = NULL;
	if (sdi) {
		devc = sdi->priv;
		scpi = sdi->conn;
		model = devc->model_config;
	}

	switch (key) {
	case OTC_CONF_TRIGGER_SOURCE:
		if (!model || !scpi)
			return OTC_ERR;
		idx = std_str_idx(data, ARRAY_AND_SIZE(rs_trigger_sources));
		if (idx < 0)
			return OTC_ERR_ARG;
		devc->trigger_source = idx;
		devc->trigger_source_changed = 1;
		break;
	case OTC_CONF_CENTER_FREQUENCY:
		if (!model || !scpi)
			return OTC_ERR;
		freq = g_variant_get_uint64(data);
		if (freq < model->freq_min || freq > model->freq_max)
			return OTC_ERR_ARG;
		devc->curr_freq = freq;
		devc->curr_freq_changed = 1;
		break;
	case OTC_CONF_LIMIT_SAMPLES:
		if (!devc)
			return OTC_ERR_BUG;
		return otc_sw_limits_config_set(&devc->limits, key, data);
	default:
		return OTC_ERR_NA;
	}
	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	const struct rohde_schwarz_nrpxsn_device_model *model;

	model = NULL;
	if (sdi) {
		devc = sdi->priv;
		model = devc->model_config;
	}

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case OTC_CONF_TRIGGER_SOURCE:
		if (!model)
			return OTC_ERR_ARG;
		*data = g_variant_new_strv(ARRAY_AND_SIZE(rs_trigger_sources));
		break;
	default:
		return OTC_ERR_NA;
	}
	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;
	int ret;

	scpi = sdi->conn;
	devc = sdi->priv;

	ret = rohde_schwarz_nrpxsn_init(scpi, devc);
	if (ret != OTC_OK)
		return ret;

	otc_sw_limits_acquisition_start(&devc->limits);
	ret = std_session_send_df_header(sdi);
	if (ret != OTC_OK)
		return ret;

	ret = otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 10,
		rohde_schwarz_nrpxsn_receive_data, (void *)sdi);

	return ret;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;

	scpi = sdi->conn;

	(void)otc_scpi_send(scpi, "ABORT");

	otc_scpi_source_remove(sdi->session, scpi);

	std_session_send_df_end(sdi);

	return OTC_OK;
}

static struct otc_dev_driver rohde_schwarz_nrpxsn_driver_info = {
	.name = "rohde-schwarz-nrpxsn",
	.longname = "Rohde&Schwarz NRPxxS(N)",
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

OTC_REGISTER_DEV_DRIVER(rohde_schwarz_nrpxsn_driver_info);
