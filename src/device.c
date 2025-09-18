/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include <stdio.h>
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"
#include "scpi.h"

/** @cond PRIVATE */
#define LOG_PREFIX "device"
/** @endcond */

/**
 * @file
 *
 * Device handling in libopentracecapture.
 */

/**
 * @defgroup grp_devices Devices
 *
 * Device handling in libopentracecapture.
 *
 * @{
 */

/**
 * Allocate and initialize a new struct otc_channel and add it to sdi.
 *
 * @param[in] sdi The device instance the channel is connected to.
 *                Must not be NULL.
 * @param[in] index @copydoc otc_channel::index
 * @param[in] type @copydoc otc_channel::type
 * @param[in] enabled @copydoc otc_channel::enabled
 * @param[in] name @copydoc otc_channel::name
 *
 * @return A new struct otc_channel*.
 *
 * @private
 */
OTC_PRIV struct otc_channel *otc_channel_new(struct otc_dev_inst *sdi,
		int index, int type, gboolean enabled, const char *name)
{
	struct otc_channel *ch;

	ch = g_malloc0(sizeof(*ch));
	ch->sdi = sdi;
	ch->index = index;
	ch->type = type;
	ch->enabled = enabled;
	if (name && *name)
		ch->name = g_strdup(name);

	sdi->channels = g_slist_append(sdi->channels, ch);

	return ch;
}

/**
 * Release a previously allocated struct otc_channel.
 *
 * @param[in] ch Pointer to struct otc_channel.
 *
 * @private
 */
OTC_PRIV void otc_channel_free(struct otc_channel *ch)
{
	if (!ch)
		return;
	g_free(ch->name);
	g_free(ch->priv);
	g_free(ch);
}

/**
 * Wrapper around otc_channel_free(), suitable for glib iterators.
 *
 * @private
 */
OTC_PRIV void otc_channel_free_cb(void *p)
{
	otc_channel_free(p);
}

/**
 * Set the name of the specified channel.
 *
 * If the channel already has a different name assigned to it, it will be
 * removed, and the new name will be saved instead.
 *
 * @param[in] channel The channel whose name to set. Must not be NULL.
 * @param[in] name The new name that the specified channel should get.
 *                 A copy of the string is made.
 *
 * @return OTC_OK on success, or OTC_ERR_ARG on invalid arguments.
 *
 * @since 0.3.0
 */
OTC_API int otc_dev_channel_name_set(struct otc_channel *channel,
		const char *name)
{
	if (!channel)
		return OTC_ERR_ARG;
	if (!name || !*name)
		return OTC_ERR_ARG;

	g_free(channel->name);
	channel->name = g_strdup(name);

	return OTC_OK;
}

/**
 * Enable or disable a channel.
 *
 * @param[in] channel The channel to enable or disable. Must not be NULL.
 * @param[in] state TRUE to enable the channel, FALSE to disable.
 *
 * @return OTC_OK on success or OTC_ERR on failure. In case of invalid
 *         arguments, OTC_ERR_ARG is returned and the channel enabled state
 *         remains unchanged.
 *
 * @since 0.3.0
 */
OTC_API int otc_dev_channel_enable(struct otc_channel *channel, gboolean state)
{
	int ret;
	gboolean was_enabled;
	struct otc_dev_inst *sdi;

	if (!channel)
		return OTC_ERR_ARG;

	sdi = channel->sdi;
	was_enabled = channel->enabled;
	channel->enabled = state;
	if (!state != !was_enabled && sdi->driver
			&& sdi->driver->config_channel_set) {
		ret = sdi->driver->config_channel_set(
			sdi, channel, OTC_CHANNEL_SET_ENABLED);
		/* Roll back change if it wasn't applicable. */
		if (ret != OTC_OK)
			return ret;
	}

	return OTC_OK;
}

/**
 * Returns the next enabled channel, wrapping around if necessary.
 *
 * @param[in] sdi The device instance the channel is connected to.
 *                Must not be NULL.
 * @param[in] cur_channel The current channel.
 *
 * @return A pointer to the next enabled channel of this device.
 *
 * @private
 */
OTC_PRIV struct otc_channel *otc_next_enabled_channel(const struct otc_dev_inst *sdi,
		struct otc_channel *cur_channel)
{
	struct otc_channel *next_channel;
	GSList *l;

	next_channel = cur_channel;
	do {
		l = g_slist_find(sdi->channels, next_channel);
		if (l && l->next)
			next_channel = l->next->data;
		else
			next_channel = sdi->channels->data;
	} while (!next_channel->enabled);

	return next_channel;
}

/**
 * Compare two channels, return whether they differ.
 *
 * The channels' names and types are checked. The enabled state is not
 * considered a condition for difference. The test is motivated by the
 * desire to detect changes in the configuration of acquisition setups
 * between re-reads of an input file.
 *
 * @param[in] ch1 First channel.
 * @param[in] ch2 Second channel.
 *
 * @return TRUE upon differences or unexpected input, FALSE otherwise.
 *
 * @private
 */
OTC_PRIV gboolean otc_channels_differ(struct otc_channel *ch1, struct otc_channel *ch2)
{
	if (!ch1 || !ch2)
		return TRUE;

	if (ch1->type != ch2->type)
		return TRUE;
	if (strcmp(ch1->name, ch2->name))
		return TRUE;

	return FALSE;
}

/**
 * Compare two channel lists, return whether they differ.
 *
 * Listing the same set of channels but in a different order is considered
 * a difference in the lists.
 *
 * @param[in] l1 First channel list.
 * @param[in] l2 Second channel list.
 *
 * @return TRUE upon differences or unexpected input, FALSE otherwise.
 *
 * @private
 */
OTC_PRIV gboolean otc_channel_lists_differ(GSList *l1, GSList *l2)
{
	struct otc_channel *ch1, *ch2;

	while (l1 && l2) {
		ch1 = l1->data;
		ch2 = l2->data;
		l1 = l1->next;
		l2 = l2->next;
		if (!ch1 || !ch2)
			return TRUE;
		if (otc_channels_differ(ch1, ch2))
			return TRUE;
		if (ch1->index != ch2->index)
			return TRUE;
	}
	if (l1 || l2)
		return TRUE;

	return FALSE;
}

/**
 * Allocate and initialize a new channel group, and add it to sdi.
 *
 * @param[in] sdi The device instance the channel group is connected to.
 *                Optional, can be NULL.
 * @param[in] name @copydoc otc_channel_group::name
 * @param[in] priv @copydoc otc_channel_group::priv
 *
 * @return A pointer to a new struct otc_channel_group, NULL upon error.
 *
 * @private
 */
OTC_PRIV struct otc_channel_group *otc_channel_group_new(struct otc_dev_inst *sdi,
	const char *name, void *priv)
{
	struct otc_channel_group *cg;

	cg = g_malloc0(sizeof(*cg));
	if (name && *name)
		cg->name = g_strdup(name);
	cg->priv = priv;

	if (sdi)
		sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);

	return cg;
}

/**
 * Release a previously allocated struct otc_channel_group.
 *
 * @param[in] cg Pointer to struct otc_channel_group.
 *
 * @private
 */
OTC_PRIV void otc_channel_group_free(struct otc_channel_group *cg)
{
	if (!cg)
		return;

	g_free(cg->name);
	g_slist_free(cg->channels);
	g_free(cg->priv);
	g_free(cg);
}

/**
 * Wrapper around otc_channel_group_free(), suitable for glib iterators.
 *
 * @private
 */
OTC_PRIV void otc_channel_group_free_cb(void *cg)
{
	return otc_channel_group_free(cg);
}

/**
 * Determine whether the specified device instance has the specified
 * capability.
 *
 * @param sdi Pointer to the device instance to be checked. Must not be NULL.
 *            If the device's 'driver' field is NULL (virtual device), this
 *            function will always return FALSE (virtual devices don't have
 *            a hardware capabilities list).
 * @param[in] key The option that should be checked for is supported by the
 *            specified device.
 *
 * @retval TRUE Device has the specified option.
 * @retval FALSE Device does not have the specified option, invalid input
 *         parameters or other error conditions.
 *
 * @since 0.2.0
 */
OTC_API gboolean otc_dev_has_option(const struct otc_dev_inst *sdi, int key)
{
	GVariant *gvar;
	const int *devopts;
	gsize num_opts, i;
	int ret;

	if (!sdi || !sdi->driver || !sdi->driver->config_list)
		return FALSE;

	if (sdi->driver->config_list(OTC_CONF_DEVICE_OPTIONS,
				&gvar, sdi, NULL) != OTC_OK)
		return FALSE;

	ret = FALSE;
	devopts = g_variant_get_fixed_array(gvar, &num_opts, sizeof(int32_t));
	for (i = 0; i < num_opts; i++) {
		if ((devopts[i] & OTC_CONF_MASK) == key) {
			ret = TRUE;
			break;
		}
	}
	g_variant_unref(gvar);

	return ret;
}

/**
 * Enumerate the configuration options of the specified item.
 *
 * @param driver Pointer to the driver to be checked. Must not be NULL.
 * @param sdi Pointer to the device instance to be checked. May be NULL to
 *            check driver options.
 * @param cg Pointer to a channel group, if a specific channel group is to
 *           be checked. Must be NULL to check device-wide options.
 *
 * @return A GArray * of enum otc_configkey values, or NULL on invalid
 *         arguments. The array must be freed by the caller using
 *         g_array_free().
 *
 * @since 0.4.0
 */
OTC_API GArray *otc_dev_options(const struct otc_dev_driver *driver,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	GVariant *gvar;
	const uint32_t *opts;
	uint32_t opt;
	gsize num_opts, i;
	GArray *result;

	if (!driver || !driver->config_list)
		return NULL;

	if (sdi && sdi->driver != driver)
		return NULL;

	if (driver->config_list(OTC_CONF_DEVICE_OPTIONS, &gvar, sdi, cg) != OTC_OK)
		return NULL;

	opts = g_variant_get_fixed_array(gvar, &num_opts, sizeof(uint32_t));

	result = g_array_sized_new(FALSE, FALSE, sizeof(uint32_t), num_opts);

	for (i = 0; i < num_opts; i++) {
		opt = opts[i] & OTC_CONF_MASK;
		g_array_insert_val(result, i, opt);
	}

	g_variant_unref(gvar);

	return result;
}

/**
 * Enumerate the configuration capabilities supported by a device instance
 * for a given configuration key.
 *
 * @param sdi Pointer to the device instance to be checked. Must not be NULL.
 *            If the device's 'driver' field is NULL (virtual device), this
 *            function will always return FALSE (virtual devices don't have
 *            a hardware capabilities list).
 * @param cg Pointer to a channel group, if a specific channel group is to
 *           be checked. Must be NULL to check device-wide options.
 * @param[in] key The option that should be checked for is supported by the
 *            specified device.
 *
 * @retval A bitmask of enum otc_configcap values, which will be zero for
 *         invalid inputs or if the key is unsupported.
 *
 * @since 0.4.0
 */
OTC_API int otc_dev_config_capabilities_list(const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg, const int key)
{
	GVariant *gvar;
	const int *devopts;
	gsize num_opts, i;
	int ret;

	if (!sdi || !sdi->driver || !sdi->driver->config_list)
		return 0;

	if (sdi->driver->config_list(OTC_CONF_DEVICE_OPTIONS,
				&gvar, sdi, cg) != OTC_OK)
		return 0;

	ret = 0;
	devopts = g_variant_get_fixed_array(gvar, &num_opts, sizeof(int32_t));
	for (i = 0; i < num_opts; i++) {
		if ((devopts[i] & OTC_CONF_MASK) == key) {
			ret = devopts[i] & ~OTC_CONF_MASK;
			break;
		}
	}
	g_variant_unref(gvar);

	return ret;
}

/**
 * Allocate and init a new user-generated device instance.
 *
 * @param vendor Device vendor.
 * @param model Device model.
 * @param version Device version.
 *
 * @retval struct otc_dev_inst *. Dynamically allocated, free using
 *         otc_dev_inst_free().
 */
OTC_API struct otc_dev_inst *otc_dev_inst_user_new(const char *vendor,
		const char *model, const char *version)
{
	struct otc_dev_inst *sdi;

	sdi = g_malloc0(sizeof(*sdi));

	sdi->vendor = g_strdup(vendor);
	sdi->model = g_strdup(model);
	sdi->version = g_strdup(version);
	sdi->inst_type = OTC_INST_USER;

	return sdi;
}

/**
 * Add a new channel to the specified device instance.
 *
 * @param[in] sdi Device instance to use. Must not be NULL.
 * @param[in] index @copydoc otc_channel::index
 * @param[in] type @copydoc otc_channel::type
 * @param[in] name @copydoc otc_channel::name
 *
 * @return OTC_OK Success.
 * @return OTC_OK Invalid argument.
 */
OTC_API int otc_dev_inst_channel_add(struct otc_dev_inst *sdi, int index, int type, const char *name)
{
	if (!sdi || sdi->inst_type != OTC_INST_USER || index < 0)
		return OTC_ERR_ARG;

	if (!otc_channel_new(sdi, index, type, TRUE, name))
		return OTC_ERR_DATA;

	return OTC_OK;
}

/**
 * Free device instance struct created by otc_dev_inst().
 *
 * @param sdi Device instance to free. If NULL, the function will do nothing.
 *
 * @private
 */
OTC_PRIV void otc_dev_inst_free(struct otc_dev_inst *sdi)
{
	struct otc_channel *ch;
	GSList *l;

	if (!sdi)
		return;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		otc_channel_free(ch);
	}
	g_slist_free(sdi->channels);
	g_slist_free_full(sdi->channel_groups, otc_channel_group_free_cb);

	if (sdi->session)
		otc_session_dev_remove(sdi->session, sdi);

	g_free(sdi->vendor);
	g_free(sdi->model);
	g_free(sdi->version);
	g_free(sdi->serial_num);
	g_free(sdi->connection_id);
	g_free(sdi);
}

#ifdef HAVE_LIBUSB_1_0

/**
 * Allocate and init a struct for a USB device instance.
 *
 * @param[in] bus @copydoc otc_usb_dev_inst::bus
 * @param[in] address @copydoc otc_usb_dev_inst::address
 * @param[in] hdl @copydoc otc_usb_dev_inst::devhdl
 *
 * @return The struct otc_usb_dev_inst * for USB device instance.
 *
 * @private
 */
OTC_PRIV struct otc_usb_dev_inst *otc_usb_dev_inst_new(uint8_t bus,
			uint8_t address, struct libusb_device_handle *hdl)
{
	struct otc_usb_dev_inst *udi;

	udi = g_malloc0(sizeof(*udi));
	udi->bus = bus;
	udi->address = address;
	udi->devhdl = hdl;

	return udi;
}

/**
 * Free struct otc_usb_dev_inst * allocated by otc_usb_dev_inst().
 *
 * @param usb The struct otc_usb_dev_inst * to free. If NULL, this
 *            function does nothing.
 *
 * @private
 */
OTC_PRIV void otc_usb_dev_inst_free(struct otc_usb_dev_inst *usb)
{
	g_free(usb);
}

/**
 * Wrapper for g_slist_free_full() convenience.
 *
 * @private
 */
OTC_PRIV void otc_usb_dev_inst_free_cb(gpointer p)
{
	otc_usb_dev_inst_free(p);
}
#endif

#ifdef HAVE_SERIAL_COMM

/**
 * Allocate and init a struct for a serial device instance.
 *
 * Both parameters are copied to newly allocated strings, and freed
 * automatically by otc_serial_dev_inst_free().
 *
 * @param[in] port OS-specific serial port specification. Examples:
 *                 "/dev/ttyUSB0", "/dev/ttyACM1", "/dev/tty.Modem-0", "COM1".
 *                 Must not be NULL.
 * @param[in] serialcomm A serial communication parameters string, in the form
 *              of \<speed\>/\<data bits\>\<parity\>\<stopbits\>, for example
 *              "9600/8n1" or "600/7o2". This is an optional parameter;
 *              it may be filled in later. Can be NULL.
 *
 * @return A pointer to a newly initialized struct otc_serial_dev_inst,
 *         or NULL on error.
 *
 * @private
 */
OTC_PRIV struct otc_serial_dev_inst *otc_serial_dev_inst_new(const char *port,
		const char *serialcomm)
{
	struct otc_serial_dev_inst *serial;

	serial = g_malloc0(sizeof(*serial));
	serial->port = g_strdup(port);
	if (serialcomm)
		serial->serialcomm = g_strdup(serialcomm);

	return serial;
}

/**
 * Free struct otc_serial_dev_inst * allocated by otc_serial_dev_inst().
 *
 * @param serial The struct otc_serial_dev_inst * to free. If NULL, this
 *               function will do nothing.
 *
 * @private
 */
OTC_PRIV void otc_serial_dev_inst_free(struct otc_serial_dev_inst *serial)
{
	if (!serial)
		return;

	g_free(serial->port);
	g_free(serial->serialcomm);
	g_free(serial);
}
#endif

/** @private */
OTC_PRIV struct otc_usbtmc_dev_inst *otc_usbtmc_dev_inst_new(const char *device)
{
	struct otc_usbtmc_dev_inst *usbtmc;

	usbtmc = g_malloc0(sizeof(*usbtmc));
	usbtmc->device = g_strdup(device);
	usbtmc->fd = -1;

	return usbtmc;
}

/** @private */
OTC_PRIV void otc_usbtmc_dev_inst_free(struct otc_usbtmc_dev_inst *usbtmc)
{
	if (!usbtmc)
		return;

	g_free(usbtmc->device);
	g_free(usbtmc);
}

/**
 * Get the list of devices/instances of the specified driver.
 *
 * @param driver The driver to use. Must not be NULL.
 *
 * @return The list of devices/instances of this driver, or NULL upon errors
 *         or if the list is empty.
 *
 * @since 0.2.0
 */
OTC_API GSList *otc_dev_list(const struct otc_dev_driver *driver)
{
	if (driver && driver->dev_list)
		return driver->dev_list(driver);
	else
		return NULL;
}

/**
 * Clear the list of device instances a driver knows about.
 *
 * @param driver The driver to use. This must be a pointer to one of
 *               the entries returned by otc_driver_list(). Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid driver.
 *
 * @since 0.2.0
 */
OTC_API int otc_dev_clear(const struct otc_dev_driver *driver)
{
	if (!driver) {
		otc_err("Invalid driver.");
		return OTC_ERR_ARG;
	}

	if (!driver->context) {
		/*
		 * Driver was never initialized, nothing to do.
		 *
		 * No log message since this usually gets called for all
		 * drivers, whether they were initialized or not.
		 */
		return OTC_OK;
	}

	/* No log message here, too verbose and not very useful. */

	return driver->dev_clear(driver);
}

/**
 * Open the specified device instance.
 *
 * If the device instance is already open (sdi->status == OTC_ST_ACTIVE),
 * OTC_ERR will be returned and no re-opening of the device will be attempted.
 *
 * If opening was successful, sdi->status is set to OTC_ST_ACTIVE, otherwise
 * it will be left unchanged.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid arguments.
 * @retval OTC_ERR Device instance was already active, or other error.
 *
 * @since 0.2.0
 */
OTC_API int otc_dev_open(struct otc_dev_inst *sdi)
{
	int ret;

	if (!sdi || !sdi->driver || !sdi->driver->dev_open)
		return OTC_ERR_ARG;

	if (sdi->status == OTC_ST_ACTIVE) {
		otc_err("%s: Device instance already active, can't re-open.",
			sdi->driver->name);
		return OTC_ERR;
	}

	otc_dbg("%s: Opening device instance.", sdi->driver->name);

	ret = sdi->driver->dev_open(sdi);

	if (ret == OTC_OK)
		sdi->status = OTC_ST_ACTIVE;

	return ret;
}

/**
 * Close the specified device instance.
 *
 * If the device instance is not open (sdi->status != OTC_ST_ACTIVE),
 * OTC_ERR_DEV_CLOSED will be returned and no closing will be attempted.
 *
 * Note: sdi->status will be set to OTC_ST_INACTIVE, regardless of whether
 * there are any errors during closing of the device instance (any errors
 * will be reported via error code and log message, though).
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR_ARG Invalid arguments.
 * @retval OTC_ERR_DEV_CLOSED Device instance was not active.
 * @retval OTC_ERR Other error.
 *
 * @since 0.2.0
 */
OTC_API int otc_dev_close(struct otc_dev_inst *sdi)
{
	if (!sdi || !sdi->driver || !sdi->driver->dev_close)
		return OTC_ERR_ARG;

	if (sdi->status != OTC_ST_ACTIVE) {
		otc_err("%s: Device instance not active, can't close.",
			sdi->driver->name);
		return OTC_ERR_DEV_CLOSED;
	}

	sdi->status = OTC_ST_INACTIVE;

	otc_dbg("%s: Closing device instance.", sdi->driver->name);

	return sdi->driver->dev_close(sdi);
}

/**
 * Queries a device instances' driver.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The driver instance or NULL on error.
 */
OTC_API struct otc_dev_driver *otc_dev_inst_driver_get(const struct otc_dev_inst *sdi)
{
	if (!sdi || !sdi->driver)
		return NULL;

	return sdi->driver;
}

/**
 * Queries a device instances' vendor.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The vendor string or NULL.
 */
OTC_API const char *otc_dev_inst_vendor_get(const struct otc_dev_inst *sdi)
{
	if (!sdi)
		return NULL;

	return sdi->vendor;
}

/**
 * Queries a device instances' model.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The model string or NULL.
 */
OTC_API const char *otc_dev_inst_model_get(const struct otc_dev_inst *sdi)
{
	if (!sdi)
		return NULL;

	return sdi->model;
}

/**
 * Queries a device instances' version.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The version string or NULL.
 */
OTC_API const char *otc_dev_inst_version_get(const struct otc_dev_inst *sdi)
{
	if (!sdi)
		return NULL;

	return sdi->version;
}

/**
 * Queries a device instances' serial number.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The serial number string or NULL.
 */
OTC_API const char *otc_dev_inst_sernum_get(const struct otc_dev_inst *sdi)
{
	if (!sdi)
		return NULL;

	return sdi->serial_num;
}

/**
 * Queries a device instances' connection identifier.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return A copy of the connection ID string or NULL. The caller is responsible
 *         for g_free()ing the string when it is no longer needed.
 */
OTC_API const char *otc_dev_inst_connid_get(const struct otc_dev_inst *sdi)
{
#ifdef HAVE_LIBUSB_1_0
	struct drv_context *drvc;
	int cnt, i, a, b;
	char conn_id_usb[64];
	struct otc_usb_dev_inst *usb;
	struct libusb_device **devlist;
#endif

#ifdef HAVE_SERIAL_COMM
	struct otc_serial_dev_inst *serial;
#endif

	struct otc_scpi_dev_inst *scpi;
	char *conn_id_scpi;

	if (!sdi)
		return NULL;

#ifdef HAVE_SERIAL_COMM
	if ((!sdi->connection_id) && (sdi->inst_type == OTC_INST_SERIAL)) {
		/* connection_id isn't populated, let's do that for serial devices. */

		serial = sdi->conn;
		((struct otc_dev_inst *)sdi)->connection_id = g_strdup(serial->port);
	}
#endif

#ifdef HAVE_LIBUSB_1_0
	if ((!sdi->connection_id) && (sdi->inst_type == OTC_INST_USB)) {
		/* connection_id isn't populated, let's do that for USB devices. */

		drvc = sdi->driver->context;
		usb = sdi->conn;

		if ((cnt = libusb_get_device_list(drvc->otc_ctx->libusb_ctx, &devlist)) < 0) {
			otc_err("Failed to retrieve device list: %s.",
			       libusb_error_name(cnt));
			return NULL;
		}

		for (i = 0; i < cnt; i++) {
			/* Find the USB device by the logical address we know. */
			b = libusb_get_bus_number(devlist[i]);
			a = libusb_get_device_address(devlist[i]);
			if (b != usb->bus || a != usb->address)
				continue;

			if (usb_get_port_path(devlist[i], conn_id_usb, sizeof(conn_id_usb)) < 0)
				continue;

			((struct otc_dev_inst *)sdi)->connection_id = g_strdup(conn_id_usb);
			break;
		}

		libusb_free_device_list(devlist, 1);
	}
#endif

	if ((!sdi->connection_id) && (sdi->inst_type == OTC_INST_SCPI)) {
		/* connection_id isn't populated, let's do that for SCPI devices. */

		scpi = sdi->conn;
		/* TODO: Add SCPI support later
		otc_scpi_connection_id(scpi, &conn_id_scpi);
		((struct otc_dev_inst *)sdi)->connection_id = g_strdup(conn_id_scpi);
		g_free(conn_id_scpi);
		*/
		((struct otc_dev_inst *)sdi)->connection_id = g_strdup("scpi-stub");
	}

	return sdi->connection_id;
}

/**
 * Queries a device instances' channel list.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The GSList of channels or NULL.
 */
OTC_API GSList *otc_dev_inst_channels_get(const struct otc_dev_inst *sdi)
{
	if (!sdi)
		return NULL;

	return sdi->channels;
}

/**
 * Queries a device instances' channel groups list.
 *
 * @param sdi Device instance to use. Must not be NULL.
 *
 * @return The GSList of channel groups or NULL.
 */
OTC_API GSList *otc_dev_inst_channel_groups_get(const struct otc_dev_inst *sdi)
{
	if (!sdi)
		return NULL;

	return sdi->channel_groups;
}

/** @} */
