/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Christer Ekholm <christerekholm@gmail.com>
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

OTC_PRIV int hantek_6xxx_open(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct drv_context *drvc = sdi->driver->context;
	struct otc_usb_dev_inst *usb;
	struct libusb_device_descriptor des;
	libusb_device **devlist;
	int err = OTC_ERR, i;
	char connection_id[64];

	devc = sdi->priv;
	usb = sdi->conn;

	libusb_get_device_list(drvc->otc_ctx->libusb_ctx, &devlist);
	for (i = 0; devlist[i]; i++) {
		libusb_get_device_descriptor(devlist[i], &des);

		if (des.idVendor != devc->profile->fw_vid
		    || des.idProduct != devc->profile->fw_pid)
			continue;

		if ((sdi->status == OTC_ST_INITIALIZING) ||
				(sdi->status == OTC_ST_INACTIVE)) {
			/*
			 * Check device by its physical USB bus/port address.
			 */
			if (usb_get_port_path(devlist[i], connection_id, sizeof(connection_id)) < 0)
				continue;

			if (strcmp(sdi->connection_id, connection_id))
				/* This is not the one. */
				continue;
		}

		if (!(err = libusb_open(devlist[i], &usb->devhdl))) {
			if (usb->address == 0xff) {
				/*
				 * First time we touch this device after firmware upload,
				 * so we don't know the address yet.
				 */
				usb->address = libusb_get_device_address(devlist[i]);
			}

			otc_info("Opened device on %d.%d (logical) / "
					"%s (physical) interface %d.",
				usb->bus, usb->address,
				sdi->connection_id, USB_INTERFACE);

			err = OTC_OK;
		} else {
			otc_err("Failed to open device: %s.",
			       libusb_error_name(err));
			err = OTC_ERR;
		}

		/* If we made it here, we handled the device (somehow). */
		break;
	}

	libusb_free_device_list(devlist, 1);

	return err;
}

OTC_PRIV void hantek_6xxx_close(struct otc_dev_inst *sdi)
{
	struct otc_usb_dev_inst *usb;

	usb = sdi->conn;

	if (!usb->devhdl)
		return;

	otc_info("Closing device on %d.%d (logical) / %s (physical) interface %d.",
		usb->bus, usb->address, sdi->connection_id, USB_INTERFACE);
	libusb_release_interface(usb->devhdl, USB_INTERFACE);
	libusb_close(usb->devhdl);
	usb->devhdl = NULL;
	sdi->status = OTC_ST_INACTIVE;
}

OTC_PRIV int hantek_6xxx_get_channeldata(const struct otc_dev_inst *sdi,
		libusb_transfer_cb_fn cb, uint32_t data_amount)
{
	struct otc_usb_dev_inst *usb;
	struct libusb_transfer *transfer;
	int ret;
	unsigned char *buf;

	otc_dbg("Request channel data.");

	usb = sdi->conn;

	if (!(buf = g_try_malloc(data_amount))) {
		otc_err("Failed to malloc USB endpoint buffer.");
		return OTC_ERR_MALLOC;
	}
	transfer = libusb_alloc_transfer(0);
	libusb_fill_bulk_transfer(transfer, usb->devhdl, HANTEK_EP_IN, buf,
			data_amount, cb, (void *)sdi, 4000);
	if ((ret = libusb_submit_transfer(transfer)) < 0) {
		otc_err("Failed to submit transfer: %s.",
			libusb_error_name(ret));
		/* TODO: Free them all. */
		libusb_free_transfer(transfer);
		g_free(buf);
		return OTC_ERR;
	}

	return OTC_OK;
}

static uint8_t samplerate_to_reg(uint64_t samplerate)
{
	const uint64_t samplerate_values[] = {SAMPLERATE_VALUES};
	const uint8_t samplerate_regs[] = {SAMPLERATE_REGS};
	uint32_t i;

	for (i = 0; i < ARRAY_SIZE(samplerate_values); i++) {
		if (samplerate_values[i] == samplerate)
			return samplerate_regs[i];
	}

	otc_err("Failed to convert samplerate: %" PRIu64 ".", samplerate);

	return samplerate_regs[ARRAY_SIZE(samplerate_values) - 1];
}

static uint8_t voltage_to_reg(uint8_t state)
{
	const uint8_t vdiv_reg[] = {VDIV_REG};

	if (state < ARRAY_SIZE(vdiv_reg)) {
		return vdiv_reg[state];
	} else {
		otc_err("Failed to convert vdiv: %d.", state);
		return vdiv_reg[ARRAY_SIZE(vdiv_reg) - 1];
	}
}

static int write_control(const struct otc_dev_inst *sdi,
		enum control_requests reg, uint8_t value)
{
	struct otc_usb_dev_inst *usb = sdi->conn;
	int ret;

	otc_spew("hantek_6xxx_write_control: 0x%x 0x%x", reg, value);

	if ((ret = libusb_control_transfer(usb->devhdl,
			LIBUSB_REQUEST_TYPE_VENDOR, (uint8_t)reg,
			0, 0, &value, 1, 100)) <= 0) {
		otc_err("Failed to control transfer: 0x%x: %s.", reg,
			libusb_error_name(ret));
		return ret;
	}

	return 0;
}

OTC_PRIV int hantek_6xxx_start_data_collecting(const struct otc_dev_inst *sdi)
{
	otc_dbg("trigger");
	return write_control(sdi, TRIGGER_REG, 1);
}

OTC_PRIV int hantek_6xxx_stop_data_collecting(const struct otc_dev_inst *sdi)
{
	return write_control(sdi, TRIGGER_REG, 0);
}

OTC_PRIV int hantek_6xxx_update_samplerate(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	otc_dbg("update samplerate %d", samplerate_to_reg(devc->samplerate));

	return write_control(sdi, SAMPLERATE_REG, samplerate_to_reg(devc->samplerate));
}

OTC_PRIV int hantek_6xxx_update_vdiv(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	int ret1, ret2;

	otc_dbg("update vdiv %d %d", voltage_to_reg(devc->voltage[0]),
		voltage_to_reg(devc->voltage[1]));

	ret1 = write_control(sdi, VDIV_CH1_REG, voltage_to_reg(devc->voltage[0]));
	ret2 = write_control(sdi, VDIV_CH2_REG, voltage_to_reg(devc->voltage[1]));

	return MIN(ret1, ret2);
}

OTC_PRIV int hantek_6xxx_update_coupling(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	uint8_t coupling = 0xFF & ((devc->coupling[1] << 4) | devc->coupling[0]);

	if (devc->has_coupling) {
		otc_dbg("update coupling 0x%x", coupling);
		return write_control(sdi, COUPLING_REG, coupling);
	} else {
		otc_dbg("coupling not supported");
		return OTC_OK;
	}
}

OTC_PRIV int hantek_6xxx_update_channels(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	uint8_t chan = devc->ch_enabled[1] ? 2 : 1;
	otc_dbg("update channels amount %d", chan);

	return write_control(sdi, CHANNELS_REG, chan);
}

OTC_PRIV int hantek_6xxx_init(const struct otc_dev_inst *sdi)
{
	otc_dbg("Initializing");

	hantek_6xxx_update_samplerate(sdi);
	hantek_6xxx_update_vdiv(sdi);
	hantek_6xxx_update_coupling(sdi);
	// hantek_6xxx_update_channels(sdi); /* Only 2 channel mode supported. */

	return OTC_OK;
}
