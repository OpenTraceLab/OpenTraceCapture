/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2017 John Chajecki <subs@qcontinuum.plus.com>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "../../scpi.h"
#include "protocol.h"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t drvopts[] = {
	OTC_CONF_MULTIMETER,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_SET,
};

/* Vendor, model, number of channels, poll period */
static const struct fluke_scpi_dmm_model supported_models[] = {
	{ "FLUKE", "45", 2, 0 },
};

static struct otc_dev_driver fluke_45_driver_info;

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct dev_context *devc;
	struct otc_dev_inst *sdi;
	struct otc_scpi_hw_info *hw_info;
	const struct scpi_command *cmdset = fluke_45_cmdset;
	unsigned int i;
	const struct fluke_scpi_dmm_model *model = NULL;
	gchar *channel_name;

	/* Get device IDN. */
	if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK) {
		otc_scpi_hw_info_free(hw_info);
		otc_info("Couldn't get IDN response, retrying.");
		otc_scpi_close(scpi);
		otc_scpi_open(scpi);
		if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK) {
			otc_scpi_hw_info_free(hw_info);
			otc_info("Couldn't get IDN response.");
			return NULL;
		}
	}

	/* Check IDN. */
	for (i = 0; i < ARRAY_SIZE(supported_models); i++) {
		if (!g_ascii_strcasecmp(hw_info->manufacturer,
					supported_models[i].vendor) &&
				!strcmp(hw_info->model, supported_models[i].model)) {
			model = &supported_models[i];
			break;
		}
	}
	if (!model) {
		otc_scpi_hw_info_free(hw_info);
		return NULL;
	}

	/* Set up device parameters. */
	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(model->vendor);
	sdi->model = g_strdup(model->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->conn = scpi;
	sdi->driver = &fluke_45_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	otc_scpi_hw_info_free(hw_info);

	devc = g_malloc0(sizeof(struct dev_context));
	devc->num_channels = model->num_channels;
	devc->cmdset = cmdset;
	sdi->priv = devc;

	/* Create channels. */
	for (i = 0; i < devc->num_channels; i++) {
		channel_name = g_strdup_printf("P%d", i + 1);
		otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, channel_name);
	}

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

	if ((ret = otc_scpi_open(scpi) < 0)) {
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

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;

	(void)cg;

	return otc_sw_limits_config_get(&devc->limits, key, data);
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;
	int ret;

	scpi = sdi->conn;
	devc = sdi->priv;

	otc_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi);

	if ((ret = otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 10,
			fl45_scpi_receive_data, (void *)sdi)) != OTC_OK)
		return ret;

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct otc_scpi_dev_inst *scpi;
	double d;

	scpi = sdi->conn;

	/*
	 * A requested value is certainly on the way. Retrieve it now,
	 * to avoid leaving the device in a state where it's not expecting
	 * commands.
	 */
	otc_scpi_get_double(scpi, NULL, &d);
	otc_scpi_source_remove(sdi->session, scpi);

	std_session_send_df_end(sdi);

	return OTC_OK;
}

static struct otc_dev_driver fluke_45_driver_info = {
	.name = "fluke-45",
	.longname = "Fluke 45",
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
OTC_REGISTER_DEV_DRIVER(fluke_45_driver_info);
