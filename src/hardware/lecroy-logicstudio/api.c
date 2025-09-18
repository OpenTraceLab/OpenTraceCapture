/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Tilman Sauerbeck <tilman@code-monkey.de>
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "protocol.h"

#define LOGICSTUDIO16_VID 0x05ff
#define LOGICSTUDIO16_PID_LACK_FIRMWARE 0xa001
#define LOGICSTUDIO16_PID_HAVE_FIRMWARE 0xa002

#define USB_INTERFACE 0
#define USB_CONFIGURATION 0
#define FX2_FIRMWARE "lecroy-logicstudio16-fx2lp.fw"

#define UNKNOWN_ADDRESS 0xff
#define MAX_RENUM_DELAY_MS 3000

#define NUM_CHANNELS 16

static const uint32_t drvopts[] = {
	OTC_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	OTC_CONF_SAMPLERATE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CAPTURE_RATIO | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_TRIGGER_MATCH | OTC_CONF_LIST,
};

static const int32_t trigger_matches[] = {
	OTC_TRIGGER_ZERO,
	OTC_TRIGGER_ONE,
	OTC_TRIGGER_RISING,
	OTC_TRIGGER_FALLING,
	OTC_TRIGGER_EDGE,
};

static const uint64_t samplerates[] = {
	OTC_HZ(1000),
	OTC_HZ(2500),
	OTC_KHZ(5),
	OTC_KHZ(10),
	OTC_KHZ(25),
	OTC_KHZ(50),
	OTC_KHZ(100),
	OTC_KHZ(250),
	OTC_KHZ(500),
	OTC_KHZ(1000),
	OTC_KHZ(2500),
	OTC_MHZ(5),
	OTC_MHZ(10),
	OTC_MHZ(25),
	OTC_MHZ(50),
	OTC_MHZ(100),
	OTC_MHZ(250),
	OTC_MHZ(500),
};

static struct otc_dev_inst *create_device(struct otc_usb_dev_inst *usb,
		enum otc_dev_inst_status status, int64_t fw_updated)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	char channel_name[8];
	int i;

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->status = status;
	sdi->vendor = g_strdup("LeCroy");
	sdi->model = g_strdup("LogicStudio16");
	sdi->inst_type = OTC_INST_USB;
	sdi->conn = usb;

	for (i = 0; i < NUM_CHANNELS; i++) {
		snprintf(channel_name, sizeof(channel_name), "D%i", i);
		otc_channel_new(sdi, i, OTC_CHANNEL_LOGIC, TRUE, channel_name);
	}

	devc = g_malloc0(sizeof(struct dev_context));

	sdi->priv = devc;

	devc->fw_updated = fw_updated;
	devc->capture_ratio = 50;

	lls_set_samplerate(sdi, OTC_MHZ(500));

	return sdi;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	struct otc_dev_inst *sdi;
	struct drv_context *drvc;
	struct otc_usb_dev_inst *usb;
	struct libusb_device_descriptor des;
	libusb_device **devlist;
	GSList *devices;
	char connection_id[64];
	size_t i;
	int r;

	(void)options;

	drvc = di->context;

	devices = NULL;

	libusb_get_device_list(drvc->otc_ctx->libusb_ctx, &devlist);

	for (i = 0; devlist[i]; i++) {
		libusb_get_device_descriptor(devlist[i], &des);

		if (des.idVendor != LOGICSTUDIO16_VID)
			continue;

		if (usb_get_port_path(devlist[i], connection_id, sizeof(connection_id)) < 0)
			continue;

		usb = NULL;

		switch (des.idProduct) {
		case LOGICSTUDIO16_PID_HAVE_FIRMWARE:
			usb = otc_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
				libusb_get_device_address(devlist[i]), NULL);

			sdi = create_device(usb, OTC_ST_INACTIVE, 0);
			break;
		case LOGICSTUDIO16_PID_LACK_FIRMWARE:
			r = ezusb_upload_firmware(drvc->otc_ctx, devlist[i],
				USB_CONFIGURATION, FX2_FIRMWARE);
			if (r != OTC_OK) {
				/*
				 * An error message has already been logged by
				 * ezusb_upload_firmware().
				 */
				continue;
			}

			/*
			 * Put unknown as the address so that we know we still
			 * need to get the proper address after the device
			 * renumerates.
			 */
			usb = otc_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
				UNKNOWN_ADDRESS, NULL);

			sdi = create_device(usb, OTC_ST_INITIALIZING,
				g_get_monotonic_time());
			break;
		default:
			break;
		}

		/* Cannot handle this device? */
		if (!usb)
			continue;

		sdi->connection_id = g_strdup(connection_id);

		devices = g_slist_append(devices, sdi);
	}

	libusb_free_device_list(devlist, 1);

	return std_scan_complete(di, devices);
}

static int open_device(struct otc_dev_inst *sdi)
{
	struct drv_context *drvc;
	struct otc_usb_dev_inst *usb;
	struct libusb_device_descriptor des;
	libusb_device **devlist;
	char connection_id[64];
	bool is_opened;
	size_t i;
	int r;

	drvc = sdi->driver->context;
	usb = sdi->conn;

	is_opened = FALSE;

	libusb_get_device_list(drvc->otc_ctx->libusb_ctx, &devlist);

	for (i = 0; devlist[i]; i++) {
		libusb_get_device_descriptor(devlist[i], &des);

		if (des.idVendor != LOGICSTUDIO16_VID ||
			des.idProduct != LOGICSTUDIO16_PID_HAVE_FIRMWARE)
			continue;

		if (usb_get_port_path(devlist[i], connection_id, sizeof(connection_id)) < 0)
			continue;

		/*
		 * Check if this device is the same one that we associated
		 * with this sdi in scan() and bail if it isn't.
		 */
		if (strcmp(sdi->connection_id, connection_id))
			continue;

		r = libusb_open(devlist[i], &usb->devhdl);

		if (r) {
			otc_err("Failed to open device: %s.",
				libusb_error_name(r));
			break;
		}

		/* Fix up address after firmware upload. */
		if (usb->address == UNKNOWN_ADDRESS)
			usb->address = libusb_get_device_address(devlist[i]);

		is_opened = TRUE;

		break;
	}

	libusb_free_device_list(devlist, 1);

	if (!is_opened)
		return OTC_ERR;

	if ((r = libusb_claim_interface(usb->devhdl, USB_INTERFACE))) {
		otc_err("Failed to claim interface: %s.",
			libusb_error_name(r));
		return OTC_ERR;
	}

	sdi->status = OTC_ST_ACTIVE;

	return OTC_OK;
}

static int dev_open(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	int64_t timediff_us, timediff_ms;
	int ret;

	devc = sdi->priv;

	/*
	 * If we didn't need to upload FX2 firmware in scan(), open the device
	 * right away. Otherwise, wait up to MAX_RENUM_DELAY_MS ms for the
	 * FX2 to renumerate.
	 */
	if (!devc->fw_updated) {
		ret = open_device(sdi);
	} else {
		otc_info("Waiting for device to reset.");

		/* Takes >= 300ms for the FX2 to be gone from the USB bus. */
		g_usleep(300 * 1000);
		timediff_ms = 0;

		while (timediff_ms < MAX_RENUM_DELAY_MS) {
			ret = open_device(sdi);

			if (ret == OTC_OK)
				break;

			g_usleep(100 * 1000);

			timediff_us = g_get_monotonic_time() - devc->fw_updated;
			timediff_ms = timediff_us / 1000;

			otc_spew("Waited %" PRIi64 "ms.", timediff_ms);
		}

		if (ret != OTC_OK) {
			otc_err("Device failed to renumerate.");
			return OTC_ERR;
		}

		otc_info("Device came back after %" PRIi64 "ms.", timediff_ms);
	}

	if (ret != OTC_OK) {
		otc_err("Unable to open device.");
		return ret;
	}

	/*
	 * Only allocate the sample buffer now since it's rather large.
	 * Don't want to allocate it before we know we are going to use it.
	 */
	devc->fetched_samples = g_malloc(SAMPLE_BUF_SIZE);

	devc->conv8to16 = g_malloc(CONV_8TO16_BUF_SIZE);

	devc->intr_xfer = libusb_alloc_transfer(0);
	devc->bulk_xfer = libusb_alloc_transfer(0);

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct otc_usb_dev_inst *usb;
	struct dev_context *devc;

	usb = sdi->conn;
	devc = sdi->priv;

	g_free(devc->fetched_samples);
	devc->fetched_samples = NULL;

	g_free(devc->conv8to16);
	devc->conv8to16 = NULL;

	if (devc->intr_xfer) {
		devc->intr_xfer->buffer = NULL; /* Points into devc. */
		libusb_free_transfer(devc->intr_xfer);
		devc->intr_xfer = NULL;
	}

	if (devc->bulk_xfer) {
		devc->bulk_xfer->buffer = NULL; /* Points into devc. */
		libusb_free_transfer(devc->bulk_xfer);
		devc->bulk_xfer = NULL;
	}

	if (!usb->devhdl)
		return OTC_ERR_BUG;

	libusb_release_interface(usb->devhdl, 0);

	libusb_close(usb->devhdl);
	usb->devhdl = NULL;

	return OTC_OK;
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
		*data = g_variant_new_uint64(lls_get_samplerate(sdi));
		break;
	case OTC_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
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
	case OTC_CONF_SAMPLERATE:
		return lls_set_samplerate(sdi, g_variant_get_uint64(data));
	case OTC_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
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

static int config_commit(const struct otc_dev_inst *sdi)
{
	return lls_setup_acquisition(sdi);
}

static int receive_usb_data(int fd, int revents, void *cb_data)
{
	struct drv_context *drvc;
	struct timeval tv;

	(void)fd;
	(void)revents;

	drvc = (struct drv_context *)cb_data;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	libusb_handle_events_timeout_completed(drvc->otc_ctx->libusb_ctx,
		&tv, NULL);

	return TRUE;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct drv_context *drvc;
	int ret;

	drvc = sdi->driver->context;

	if ((ret = lls_start_acquisition(sdi)) < 0)
		return ret;

	std_session_send_df_header(sdi);

	return usb_source_add(sdi->session, drvc->otc_ctx, 100,
		receive_usb_data, drvc);
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	return lls_stop_acquisition(sdi);
}

static struct otc_dev_driver lecroy_logicstudio_driver_info = {
	.name = "lecroy-logicstudio",
	.longname = "LeCroy LogicStudio",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.config_commit = config_commit,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(lecroy_logicstudio_driver_info);
