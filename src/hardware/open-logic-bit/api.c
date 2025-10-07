/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2021 Ultra-Embedded <admin@ultra-embedded.com>
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

#define USB_VENDOR_ID			0x0403
#define USB_DEVICE_ID			0x6014

static const uint32_t drvopts[] = {
	OTC_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_SAMPLERATE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_TRIGGER_MATCH | OTC_CONF_LIST,
};

static const char *channel_names[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15"
};

static const uint64_t samplerates[] = {
	OTC_MHZ(1), OTC_MHZ(10), OTC_MHZ(25), OTC_MHZ(50), OTC_MHZ(100)
};

static const int32_t trigger_matches[] = {
	OTC_TRIGGER_ZERO,
	OTC_TRIGGER_ONE,
	OTC_TRIGGER_RISING,
	OTC_TRIGGER_FALLING,
};

static void clear_helper(struct dev_context *devc)
{
	ftdi_free(devc->ftdic);
	g_free(devc->data_buf);
}

static int dev_clear(const struct otc_dev_driver *di)
{
	return std_dev_clear_with_callback(di, (std_dev_clear_callback)clear_helper);
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	unsigned int i;
	int ret;

	(void)options;

	devc = g_malloc0(sizeof(struct dev_context));

	/* Allocate memory for the incoming data. */
	devc->data_buf    = g_malloc0(DATA_BUF_SIZE * sizeof(uint16_t));
	devc->data_pos    = 0;
	devc->num_samples = 0;
	devc->sample_rate = OTC_MHZ(100);

	if (!(devc->ftdic = ftdi_new())) {
		otc_err("Failed to initialize libftdi.");
		goto err_free_sample_buf;
	}

	ret = ftdi_usb_open_desc(devc->ftdic, USB_VENDOR_ID, USB_DEVICE_ID,
				 NULL, NULL);
	if (ret < 0) {
		/* Log errors, except for -3 ("device not found"). */
		if (ret != -3)
			otc_err("Failed to open device (%d): %s", ret,
			       ftdi_get_error_string(devc->ftdic));
		goto err_free_ftdic;
	}

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("OpenLogicBit");
	sdi->model  = NULL;
	sdi->priv   = devc;

	for (i = 0; i < ARRAY_SIZE(channel_names); i++)
		otc_channel_new(sdi, i, OTC_CHANNEL_LOGIC, TRUE, channel_names[i]);

	openlb_close(devc);

	return std_scan_complete(di, g_slist_append(NULL, sdi));

	openlb_close(devc);
err_free_ftdic:
	ftdi_free(devc->ftdic);
err_free_sample_buf:
	g_free(devc->data_buf);
	g_free(devc);

	return NULL;
}

static int dev_open(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;

	devc = sdi->priv;

	ret = ftdi_set_interface(devc->ftdic, INTERFACE_A);
	if (ret < 0) {
		otc_err("Failed to set FTDI interface A (%d): %s", ret,
		       ftdi_get_error_string(devc->ftdic));
		return OTC_ERR;
	}

	ret = ftdi_usb_open_desc(devc->ftdic, USB_VENDOR_ID, USB_DEVICE_ID,
				 NULL, NULL);
	if (ret < 0) {
		otc_err("Failed to open device (%d): %s", ret,
		       ftdi_get_error_string(devc->ftdic));
		return OTC_ERR;
	}

	if ((ret = ftdi_usb_purge_buffers(devc->ftdic)) < 0) {
		otc_err("Failed to purge FTDI RX/TX buffers (%d): %s.",
		       ret, ftdi_get_error_string(devc->ftdic));
		goto err_dev_open_close_ftdic;
	}

	ret = ftdi_set_bitmode(devc->ftdic, 0xff, BITMODE_RESET);
	if (ret < 0) {
		otc_err("Failed to reset the FTDI chip bitmode (%d): %s.",
		       ret, ftdi_get_error_string(devc->ftdic));
		goto err_dev_open_close_ftdic;
	}

	ret = ftdi_set_bitmode(devc->ftdic, 0xff, BITMODE_SYNCFF);
	if (ret < 0) {
		otc_err("Failed to put FTDI chip into sync FIFO mode (%d): %s.",
		       ret, ftdi_get_error_string(devc->ftdic));
		goto err_dev_open_close_ftdic;
	}

	ret = ftdi_set_latency_timer(devc->ftdic, 2);
	if (ret < 0) {
		otc_err("Failed to set FTDI latency timer (%d): %s.",
		       ret, ftdi_get_error_string(devc->ftdic));
		goto err_dev_open_close_ftdic;
	}

	ret = ftdi_read_data_set_chunksize(devc->ftdic, 64 * 1024);
	if (ret < 0) {
		otc_err("Failed to set FTDI read data chunk size (%d): %s.",
		       ret, ftdi_get_error_string(devc->ftdic));
		goto err_dev_open_close_ftdic;
	}

	return OTC_OK;

err_dev_open_close_ftdic:
	openlb_close(devc);

	return OTC_ERR;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	return openlb_close(devc);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc = sdi->priv;
	(void)cg;

	switch (key) {
	case OTC_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->sample_rate);
		break;
	case OTC_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
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

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_SAMPLERATE:
		devc->sample_rate = g_variant_get_uint64(data);
		break;
	case OTC_CONF_LIMIT_SAMPLES:
		devc->limit_samples = g_variant_get_uint64(data);
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
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, NO_OPTS, drvopts, devopts);
	case OTC_CONF_SAMPLERATE:
		*data = std_gvar_samplerates(ARRAY_AND_SIZE(samplerates));
		break;
	case OTC_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	int ret;
	struct dev_context *devc = sdi->priv;

	if (!devc->ftdic)
		return OTC_ERR_BUG;

	/* Reset some device structure state. */
	devc->seq_num     = 1;
	devc->data_pos    = 0;
	devc->num_samples = 0;

	if (openlb_convert_triggers(sdi) != OTC_OK) {
		otc_err("Failed to configure trigger.");
		return OTC_ERR;
	}

	if ((ret = openlb_start_acquisition(devc)) < 0)
		return ret;

	std_session_send_df_header(sdi);

	/* Hook up a dummy handler to receive data from the device. */
	otc_session_source_add(sdi->session, -1, 0, 0, openlb_receive_data, (void *)sdi);

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	otc_session_source_remove(sdi->session, -1);
	std_session_send_df_end(sdi);

	return OTC_OK;
}

static struct otc_dev_driver openlb_driver_info = {
	.name = "openlb",
	.longname = "Open-Logic-Bit",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(openlb_driver_info);
