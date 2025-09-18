/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Martin Lederhilger <martin.lederhilger@gmx.at>
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

#include <string.h>
#include "protocol.h"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
	OTC_CONF_LIMIT_FRAMES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_SAMPLERATE | OTC_CONF_GET,
};

static struct otc_dev_driver gwinstek_gds_800_driver_info;

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct dev_context *devc;
	struct otc_dev_inst *sdi;
	struct otc_scpi_hw_info *hw_info;
	struct otc_channel_group *cg;

	if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK) {
		otc_info("Couldn't get IDN response.");
		return NULL;
	}

	if (strcmp(hw_info->manufacturer, "GW") != 0 ||
	    strncmp(hw_info->model, "GDS-8", 5) != 0) {
		otc_scpi_hw_info_free(hw_info);
		return NULL;
	}

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->conn = scpi;
	sdi->driver = &gwinstek_gds_800_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->channels = NULL;
	sdi->channel_groups = NULL;

	otc_scpi_hw_info_free(hw_info);

	devc = g_malloc0(sizeof(struct dev_context));
	devc->frame_limit = 1;
	devc->sample_rate = 0.0;
	devc->df_started = FALSE;
	sdi->priv = devc;

	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "CH1");
	otc_channel_new(sdi, 1, OTC_CHANNEL_ANALOG, TRUE, "CH2");

	cg = otc_channel_group_new(sdi, "", NULL);
	cg->channels = g_slist_append(cg->channels, g_slist_nth_data(sdi->channels, 0));
	cg->channels = g_slist_append(cg->channels, g_slist_nth_data(sdi->channels, 1));

	return sdi;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	return otc_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	int ret;
	struct otc_scpi_dev_inst *scpi = sdi->conn;

	if ((ret = otc_scpi_open(scpi)) < 0) {
		otc_err("Failed to open SCPI device: %s.", otc_strerror(ret));
		return OTC_ERR;
	}

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;

	scpi = sdi->conn;

	if (!scpi)
		return OTC_ERR_BUG;

	return otc_scpi_close(scpi);
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
	case OTC_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->sample_rate);
		break;
	case OTC_CONF_LIMIT_FRAMES:
		*data = g_variant_new_uint64(devc->frame_limit);
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

	(void)cg;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;

	scpi = sdi->conn;
	devc = sdi->priv;

	devc->state = START_ACQUISITION;
	devc->cur_acq_frame = 0;

	otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 50,
			gwinstek_gds_800_receive_data, (void *)sdi);

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;

	scpi = sdi->conn;
	devc = sdi->priv;

	if (devc->df_started) {
		std_session_send_df_frame_end(sdi);
		std_session_send_df_end(sdi);
		devc->df_started = FALSE;
	}

	otc_scpi_source_remove(sdi->session, scpi);

	return OTC_OK;
}

static struct otc_dev_driver gwinstek_gds_800_driver_info = {
	.name = "gwinstek-gds-800",
	.longname = "GW Instek GDS-800 series",
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
OTC_REGISTER_DEV_DRIVER(gwinstek_gds_800_driver_info);
