/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Aurelien Jacobs <aurel@gnuage.org>
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
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

#define LOG_PREFIX "modbus"

OTC_PRIV extern const struct otc_modbus_dev_inst modbus_serial_rtu_dev;

static const struct otc_modbus_dev_inst *modbus_devs[] = {
#ifdef HAVE_SERIAL_COMM
	&modbus_serial_rtu_dev, /* Must be last as it matches any resource. */
#endif
};

static const unsigned int modbus_devs_size = ARRAY_SIZE(modbus_devs);

static struct otc_dev_inst *otc_modbus_scan_resource(const char *resource,
	const char *serialcomm, int modbusaddr,
	struct otc_dev_inst *(*probe_device)(struct otc_modbus_dev_inst *modbus))
{
	struct otc_modbus_dev_inst *modbus;
	struct otc_dev_inst *sdi;

	if (!(modbus = modbus_dev_inst_new(resource, serialcomm, modbusaddr)))
		return NULL;

	if (otc_modbus_open(modbus) != OTC_OK) {
		otc_info("Couldn't open Modbus device.");
		otc_modbus_free(modbus);
		return NULL;
	};

	sdi = probe_device(modbus);

	otc_modbus_close(modbus);

	if (!sdi)
		otc_modbus_free(modbus);

	return sdi;
}

/**
 * Scan for Modbus devices which match a probing function.
 *
 * @param drvc The driver context doing the scan.
 * @param options The scan options to find devies.
 * @param probe_device The callback function that will be called for each
 *                     found device to validate whether this device matches
 *                     what we are scanning for.
 *
 * @return A list of the devices found or NULL if no devices were found.
 */
OTC_PRIV GSList *otc_modbus_scan(struct drv_context *drvc, GSList *options,
	struct otc_dev_inst *(*probe_device)(struct otc_modbus_dev_inst *modbus))
{
	GSList *resources, *l, *devices;
	struct otc_dev_inst *sdi;
	const char *resource = NULL;
	const char *serialcomm = NULL;
	int modbusaddr = 1;
	gchar **res;
	unsigned int i;

	for (l = options; l; l = l->next) {
		struct otc_config *src = l->data;
		switch (src->key) {
		case OTC_CONF_CONN:
			resource = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_MODBUSADDR:
			modbusaddr = g_variant_get_uint64(src->data);
			break;
		}
	}

	devices = NULL;
	for (i = 0; i < modbus_devs_size; i++) {
		if ((resource && strcmp(resource, modbus_devs[i]->prefix))
		    || !modbus_devs[i]->scan)
			continue;
		resources = modbus_devs[i]->scan(modbusaddr);
		for (l = resources; l; l = l->next) {
			res = g_strsplit(l->data, ":", 2);
			if (res[0] && (sdi = otc_modbus_scan_resource(res[0],
					serialcomm ? serialcomm : res[1],
					modbusaddr, probe_device))) {
				devices = g_slist_append(devices, sdi);
				sdi->connection_id = g_strdup(l->data);
			}
			g_strfreev(res);
		}
		g_slist_free_full(resources, g_free);
	}

	if (!devices && resource) {
		sdi = otc_modbus_scan_resource(resource, serialcomm, modbusaddr,
		                              probe_device);
		if (sdi)
			devices = g_slist_append(NULL, sdi);
	}

	/* Tack a copy of the newly found devices onto the driver list. */
	if (devices)
		drvc->instances = g_slist_concat(drvc->instances, g_slist_copy(devices));

	return devices;
}

/**
 * Allocate and initialize a struct for a Modbus device instance.
 *
 * @param resource The resource description string.
 * @param serialcomm Additionnal parameters for serial port resources.
 *
 * @return The allocated otc_modbus_dev_inst structure or NULL on failure.
 */
OTC_PRIV struct otc_modbus_dev_inst *modbus_dev_inst_new(const char *resource,
		const char *serialcomm, int modbusaddr)
{
	struct otc_modbus_dev_inst *modbus = NULL;
	const struct otc_modbus_dev_inst *modbus_dev;
	gchar **params;
	unsigned int i;

	for (i = 0; i < modbus_devs_size; i++) {
		modbus_dev = modbus_devs[i];
		if (!strncmp(resource, modbus_dev->prefix, strlen(modbus_dev->prefix))) {
			otc_dbg("Opening %s device %s.", modbus_dev->name, resource);
			modbus = g_malloc(sizeof(*modbus));
			*modbus = *modbus_dev;
			modbus->priv = g_malloc0(modbus->priv_size);
			modbus->read_timeout_ms = 1000;
			params = g_strsplit(resource, "/", 0);
			if (modbus->dev_inst_new(modbus->priv, resource,
			                         params, serialcomm, modbusaddr) != OTC_OK) {
				otc_modbus_free(modbus);
				modbus = NULL;
			}
			g_strfreev(params);
			break;
		}
	}

	return modbus;
}

/**
 * Open the specified Modbus device.
 *
 * @param modbus Previously initialized Modbus device structure.
 *
 * @return OTC_OK on success, OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_open(struct otc_modbus_dev_inst *modbus)
{
	return modbus->open(modbus->priv);
}

/**
 * Add an event source for a Modbus device.
 *
 * @param session The session to add the event source to.
 * @param modbus Previously initialized Modbus device structure.
 * @param events Events to check for.
 * @param timeout Max time to wait before the callback is called, ignored if 0.
 * @param cb Callback function to add. Must not be NULL.
 * @param cb_data Data for the callback function. Can be NULL.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments, or
 *         OTC_ERR_MALLOC upon memory allocation errors.
 */
OTC_PRIV int otc_modbus_source_add(struct otc_session *session,
		struct otc_modbus_dev_inst *modbus, int events, int timeout,
		otc_receive_data_callback cb, void *cb_data)
{
	return modbus->source_add(session, modbus->priv, events, timeout, cb, cb_data);
}

/**
 * Remove event source for a Modbus device.
 *
 * @param session The session to remove the event source from.
 * @param modbus Previously initialized Modbus device structure.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments, or
 *         OTC_ERR_MALLOC upon memory allocation errors, OTC_ERR_BUG upon
 *         internal errors.
 */
OTC_PRIV int otc_modbus_source_remove(struct otc_session *session,
		struct otc_modbus_dev_inst *modbus)
{
	return modbus->source_remove(session, modbus->priv);
}

/**
 * Send a Modbus command.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param request Buffer containing the Modbus command to send.
 * @param request_size The size of the request buffer.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments, or
 *         OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_request(struct otc_modbus_dev_inst *modbus,
		uint8_t *request, int request_size)
{
	if (!request || request_size < 1)
		return OTC_ERR_ARG;

	return modbus->send(modbus->priv, request, request_size);
}

/**
 * Receive a Modbus reply.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param reply Buffer to store the received Modbus reply.
 * @param reply_size The size of the reply buffer.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments, or
 *         OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_reply(struct otc_modbus_dev_inst *modbus,
		uint8_t *reply, int reply_size)
{
	int len, ret;
	gint64 laststart;
	unsigned int elapsed_ms;

	if (!reply || reply_size < 2)
		return OTC_ERR_ARG;

	laststart = g_get_monotonic_time();

	ret = modbus->read_begin(modbus->priv, reply);
	if (ret != OTC_OK)
		return ret;
	if (*reply & 0x80)
		reply_size = 2;

	reply++;
	reply_size--;

	while (reply_size > 0) {
		len = modbus->read_data(modbus->priv, reply, reply_size);
		if (len < 0) {
			otc_err("Incompletely read Modbus response.");
			return OTC_ERR;
		} else if (len > 0) {
			laststart = g_get_monotonic_time();
		}
		reply += len;
		reply_size -= len;
		elapsed_ms = (g_get_monotonic_time() - laststart) / 1000;
		if (elapsed_ms >= modbus->read_timeout_ms) {
			otc_err("Timed out waiting for Modbus response.");
			return OTC_ERR;
		}
	}

	ret = modbus->read_end(modbus->priv);
	if (ret != OTC_OK)
		return ret;

	return OTC_OK;
}

/**
 * Send a Modbus command and receive the corresponding reply.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param request Buffer containing the Modbus command to send.
 * @param request_size The size of the request buffer.
 * @param reply Buffer to store the received Modbus reply.
 * @param reply_size The size of the reply buffer.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments, or
 *         OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_request_reply(struct otc_modbus_dev_inst *modbus,
		uint8_t *request, int request_size, uint8_t *reply, int reply_size)
{
	int ret;
	ret = otc_modbus_request(modbus, request, request_size);
	if (ret != OTC_OK)
		return ret;
	return otc_modbus_reply(modbus, reply, reply_size);
}

enum {
	MODBUS_READ_COILS = 0x01,
	MODBUS_READ_HOLDING_REGISTERS = 0x03,
	MODBUS_WRITE_COIL = 0x05,
	MODBUS_WRITE_MULTIPLE_REGISTERS = 0x10,
};

static int otc_modbus_error_check(const uint8_t *reply)
{
	const char *function = "UNKNOWN";
	const char *error = NULL;
	char buf[8];

	if (!(reply[0] & 0x80))
		return FALSE;

	switch (reply[0] & ~0x80) {
	case MODBUS_READ_COILS:
		function = "MODBUS_READ_COILS";
		break;
	case MODBUS_READ_HOLDING_REGISTERS:
		function = "READ_HOLDING_REGISTERS";
		break;
	case MODBUS_WRITE_COIL:
		function = "WRITE_COIL";
		break;
	case MODBUS_WRITE_MULTIPLE_REGISTERS:
		function = "WRITE_MULTIPLE_REGISTERS";
		break;
	}

	switch (reply[1]) {
	case 0x01:
		error = "ILLEGAL FUNCTION";
		break;
	case 0x02:
		error = "ILLEGAL DATA ADDRESS";
		break;
	case 0x03:
		error = "ILLEGAL DATA VALUE";
		break;
	case 0x04:
		error = "SLAVE DEVICE FAILURE";
		break;
	case 0x05:
		error = "ACKNOWLEDGE";
		break;
	case 0x06:
		error = "SLAVE DEVICE BUSY";
		break;
	case 0x08:
		error = "MEMORY PARITY ERROR";
		break;
	case 0x0A:
		error = "GATEWAY PATH UNAVAILABLE";
		break;
	case 0x0B:
		error = "GATEWAY TARGET DEVICE FAILED TO RESPOND";
		break;
	}
	if (!error) {
		snprintf(buf, sizeof(buf), "0x%X", reply[1]);
		error = buf;
	}

	otc_err("%s error executing %s function.", error, function);

	return TRUE;
}

/**
 * Send a Modbus read coils command and receive the corresponding coils values.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param address The Modbus address of the first coil to read, or -1 to read
 *                the reply of a previouly sent read coils command.
 * @param nb_coils The number of coils to read.
 * @param coils Buffer to store all the received coils values (1 bit per coil),
 *              or NULL to send the read coil command without reading the reply.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments,
 *         OTC_ERR_DATA upon invalid data, or OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_read_coils(struct otc_modbus_dev_inst *modbus,
		int address, int nb_coils, uint8_t *coils)
{
	uint8_t request[5], reply[2 + (nb_coils + 7) / 8];
	int ret;

	if (address < -1 || address > 0xFFFF || nb_coils < 1 || nb_coils > 2000)
		return OTC_ERR_ARG;

	W8(request + 0, MODBUS_READ_COILS);
	WB16(request + 1, address);
	WB16(request + 3, nb_coils);

	if (address >= 0) {
		ret = otc_modbus_request(modbus, request, sizeof(request));
		if (ret != OTC_OK)
			return ret;
	}

	if (coils) {
		ret = otc_modbus_reply(modbus, reply, sizeof(reply));
		if (ret != OTC_OK)
			return ret;
		if (otc_modbus_error_check(reply))
			return OTC_ERR_DATA;
		if (reply[0] != request[0] || R8(reply + 1) != (uint8_t)((nb_coils + 7) / 8))
			return OTC_ERR_DATA;
		memcpy(coils, reply + 2, (nb_coils + 7) / 8);
	}

	return OTC_OK;
}

/**
 * Send a Modbus read holding registers command and receive the corresponding
 * registers values.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param address The Modbus address of the first register to read, or -1 to
 *                read the reply of a previouly sent read registers command.
 * @param nb_registers The number of registers to read.
 * @param registers Buffer to store all the received registers values,
 *                  or NULL to send the read holding registers command
 *                  without reading the reply.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments,
 *         OTC_ERR_DATA upon invalid data, or OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_read_holding_registers(struct otc_modbus_dev_inst *modbus,
		int address, int nb_registers, uint16_t *registers)
{
	uint8_t request[5], reply[2 + (2 * nb_registers)];
	int ret;

	if (address < -1 || address > 0xFFFF
	    || nb_registers < 1 || nb_registers > 125)
		return OTC_ERR_ARG;

	W8(request + 0, MODBUS_READ_HOLDING_REGISTERS);
	WB16(request + 1, address);
	WB16(request + 3, nb_registers);

	if (address >= 0) {
		ret = otc_modbus_request(modbus, request, sizeof(request));
		if (ret != OTC_OK)
			return ret;
	}

	if (registers) {
		ret = otc_modbus_reply(modbus, reply, sizeof(reply));
		if (ret != OTC_OK)
			return ret;
		if (otc_modbus_error_check(reply))
			return OTC_ERR_DATA;
		if (reply[0] != request[0] || R8(reply + 1) != (uint8_t)(2 * nb_registers))
			return OTC_ERR_DATA;
		memcpy(registers, reply + 2, 2 * nb_registers);
	}

	return OTC_OK;
}

/**
 * Send a Modbus write coil command.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param address The Modbus address of the coil to write.
 * @param value The new value to assign to this coil.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments,
 *         OTC_ERR_DATA upon invalid data, or OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_write_coil(struct otc_modbus_dev_inst *modbus,
		int address, int value)
{
	uint8_t request[5], reply[5];
	int ret;

	if (address < 0 || address > 0xFFFF)
		return OTC_ERR_ARG;

	W8(request + 0, MODBUS_WRITE_COIL);
	WB16(request + 1, address);
	WB16(request + 3, value ? 0xFF00 : 0);

	ret = otc_modbus_request_reply(modbus, request, sizeof(request),
				      reply, sizeof(reply));
	if (ret != OTC_OK)
		return ret;
	if (otc_modbus_error_check(reply))
		return OTC_ERR_DATA;
	if (memcmp(request, reply, sizeof(reply)))
		return OTC_ERR_DATA;

	return OTC_OK;
}

/**
 * Send a Modbus write multiple registers command.
 *
 * @param modbus Previously initialized Modbus device structure.
 * @param address The Modbus address of the first register to write.
 * @param nb_registers The number of registers to write.
 * @param registers Buffer holding all the registers values to write.
 *
 * @return OTC_OK upon success, OTC_ERR_ARG upon invalid arguments,
 *         OTC_ERR_DATA upon invalid data, or OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_write_multiple_registers(struct otc_modbus_dev_inst*modbus,
		int address, int nb_registers, uint16_t *registers)
{
	uint8_t request[6 + (2 * nb_registers)], reply[5];
	int ret;

	if (address < 0 || address > 0xFFFF
	    || nb_registers < 1 || nb_registers > 123 || !registers)
		return OTC_ERR_ARG;

	W8(request + 0, MODBUS_WRITE_MULTIPLE_REGISTERS);
	WB16(request + 1, address);
	WB16(request + 3, nb_registers);
	W8(request + 5, 2 * nb_registers);
	memcpy(request + 6, registers, 2 * nb_registers);

	ret = otc_modbus_request_reply(modbus, request, sizeof(request),
				      reply, sizeof(reply));
	if (ret != OTC_OK)
		return ret;
	if (otc_modbus_error_check(reply))
		return OTC_ERR_DATA;
	if (memcmp(request, reply, sizeof(reply)))
		return OTC_ERR_DATA;

	return OTC_OK;
}

/**
 * Close Modbus device.
 *
 * @param modbus Previously initialized Modbus device structure.
 *
 * @return OTC_OK on success, OTC_ERR on failure.
 */
OTC_PRIV int otc_modbus_close(struct otc_modbus_dev_inst *modbus)
{
	return modbus->close(modbus->priv);
}

/**
 * Free Modbus device.
 *
 * @param modbus Previously initialized Modbus device structure.
 *
 * @return OTC_OK on success, OTC_ERR on failure.
 */
OTC_PRIV void otc_modbus_free(struct otc_modbus_dev_inst *modbus)
{
	modbus->free(modbus->priv);
	g_free(modbus->priv);
	g_free(modbus);
}
