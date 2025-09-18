/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Bert Vermeulen <bert@biot.com>
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

#ifndef LIBSIGROK_SCPI_H
#define LIBSIGROK_SCPI_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

#define SCPI_CMD_IDN "*IDN?"
#define SCPI_CMD_OPC "*OPC?"

enum {
	SCPI_CMD_GET_TIMEBASE = 1,
	SCPI_CMD_SET_TIMEBASE,
	SCPI_CMD_GET_HORIZONTAL_DIV,
	SCPI_CMD_GET_VERTICAL_SCALE,
	SCPI_CMD_SET_VERTICAL_SCALE,
	SCPI_CMD_GET_TRIGGER_SOURCE,
	SCPI_CMD_SET_TRIGGER_SOURCE,
	SCPI_CMD_GET_TRIGGER_SLOPE,
	SCPI_CMD_SET_TRIGGER_SLOPE,
	SCPI_CMD_GET_TRIGGER_PATTERN,
	SCPI_CMD_SET_TRIGGER_PATTERN,
	SCPI_CMD_GET_HIGH_RESOLUTION,
	SCPI_CMD_SET_HIGH_RESOLUTION,
	SCPI_CMD_GET_PEAK_DETECTION,
	SCPI_CMD_SET_PEAK_DETECTION,
	SCPI_CMD_GET_COUPLING,
	SCPI_CMD_SET_COUPLING,
	SCPI_CMD_GET_HORIZ_TRIGGERPOS,
	SCPI_CMD_SET_HORIZ_TRIGGERPOS,
	SCPI_CMD_GET_ANALOG_CHAN_STATE,
	SCPI_CMD_SET_ANALOG_CHAN_STATE,
	SCPI_CMD_GET_DIG_CHAN_STATE,
	SCPI_CMD_SET_DIG_CHAN_STATE,
	SCPI_CMD_GET_VERTICAL_OFFSET,
	SCPI_CMD_GET_DIG_POD_STATE,
	SCPI_CMD_SET_DIG_POD_STATE,
	SCPI_CMD_GET_ANALOG_DATA,
	SCPI_CMD_GET_DIG_DATA,
	SCPI_CMD_GET_SAMPLE_RATE,
	SCPI_CMD_GET_PROBE_UNIT,
	SCPI_CMD_GET_DIG_POD_THRESHOLD,
	SCPI_CMD_SET_DIG_POD_THRESHOLD,
	SCPI_CMD_GET_DIG_POD_USER_THRESHOLD,
	SCPI_CMD_SET_DIG_POD_USER_THRESHOLD,
};

enum scpi_transport_layer {
	SCPI_TRANSPORT_LIBGPIB,
	SCPI_TRANSPORT_SERIAL,
	SCPI_TRANSPORT_RAW_TCP,
	SCPI_TRANSPORT_RIGOL_TCP,
	SCPI_TRANSPORT_USBTMC,
	SCPI_TRANSPORT_VISA,
	SCPI_TRANSPORT_VXI,
};

struct scpi_command {
	int command;
	const char *string;
};

struct otc_scpi_hw_info {
	char *manufacturer;
	char *model;
	char *serial_number;
	char *firmware_version;
};

struct otc_scpi_dev_inst {
	const char *name;
	const char *prefix;
	enum scpi_transport_layer transport;
	int priv_size;
	GSList *(*scan)(struct drv_context *drvc);
	int (*dev_inst_new)(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm);
	int (*open)(struct otc_scpi_dev_inst *scpi);
	int (*connection_id)(struct otc_scpi_dev_inst *scpi, char **connection_id);
	int (*source_add)(struct otc_session *session, void *priv, int events,
		int timeout, otc_receive_data_callback cb, void *cb_data);
	int (*source_remove)(struct otc_session *session, void *priv);
	int (*send)(void *priv, const char *command);
	int (*read_begin)(void *priv);
	int (*read_data)(void *priv, char *buf, int maxlen);
	int (*write_data)(void *priv, char *buf, int len);
	int (*read_complete)(void *priv);
	int (*close)(struct otc_scpi_dev_inst *scpi);
	void (*free)(void *priv);
	unsigned int read_timeout_us;
	void *priv;
	/* Only used for quirk workarounds, notably the Rigol DS1000 series. */
	uint64_t firmware_version;
	GMutex scpi_mutex;
	char *actual_channel_name;
	gboolean no_opc_command;
};

OTC_PRIV GSList *otc_scpi_scan(struct drv_context *drvc, GSList *options,
		struct otc_dev_inst *(*probe_device)(struct otc_scpi_dev_inst *scpi));
OTC_PRIV struct otc_scpi_dev_inst *scpi_dev_inst_new(struct drv_context *drvc,
		const char *resource, const char *serialcomm);
OTC_PRIV int otc_scpi_open(struct otc_scpi_dev_inst *scpi);
OTC_PRIV int otc_scpi_connection_id(struct otc_scpi_dev_inst *scpi,
		char **connection_id);
OTC_PRIV int otc_scpi_source_add(struct otc_session *session,
		struct otc_scpi_dev_inst *scpi, int events, int timeout,
		otc_receive_data_callback cb, void *cb_data);
OTC_PRIV int otc_scpi_source_remove(struct otc_session *session,
		struct otc_scpi_dev_inst *scpi);
OTC_PRIV int otc_scpi_send(struct otc_scpi_dev_inst *scpi,
		const char *format, ...);
OTC_PRIV int otc_scpi_send_variadic(struct otc_scpi_dev_inst *scpi,
		const char *format, va_list args);
OTC_PRIV int otc_scpi_read_begin(struct otc_scpi_dev_inst *scpi);
OTC_PRIV int otc_scpi_read_data(struct otc_scpi_dev_inst *scpi, char *buf, int maxlen);
OTC_PRIV int otc_scpi_write_data(struct otc_scpi_dev_inst *scpi, char *buf, int len);
OTC_PRIV int otc_scpi_read_complete(struct otc_scpi_dev_inst *scpi);
OTC_PRIV int otc_scpi_close(struct otc_scpi_dev_inst *scpi);
OTC_PRIV void otc_scpi_free(struct otc_scpi_dev_inst *scpi);

OTC_PRIV int otc_scpi_read_response(struct otc_scpi_dev_inst *scpi,
			GString *response, gint64 abs_timeout_us);
OTC_PRIV int otc_scpi_get_string(struct otc_scpi_dev_inst *scpi,
			const char *command, char **scpi_response);
OTC_PRIV int otc_scpi_get_bool(struct otc_scpi_dev_inst *scpi,
			const char *command, gboolean *scpi_response);
OTC_PRIV int otc_scpi_get_int(struct otc_scpi_dev_inst *scpi,
			const char *command, int *scpi_response);
OTC_PRIV int otc_scpi_get_float(struct otc_scpi_dev_inst *scpi,
			const char *command, float *scpi_response);
OTC_PRIV int otc_scpi_get_double(struct otc_scpi_dev_inst *scpi,
			const char *command, double *scpi_response);
OTC_PRIV int otc_scpi_get_opc(struct otc_scpi_dev_inst *scpi);
OTC_PRIV int otc_scpi_get_floatv(struct otc_scpi_dev_inst *scpi,
			const char *command, GArray **scpi_response);
OTC_PRIV int otc_scpi_get_uint8v(struct otc_scpi_dev_inst *scpi,
			const char *command, GArray **scpi_response);
OTC_PRIV int otc_scpi_get_data(struct otc_scpi_dev_inst *scpi,
			const char *command, GString **scpi_response);
OTC_PRIV int otc_scpi_get_block(struct otc_scpi_dev_inst *scpi,
			const char *command, GByteArray **scpi_response);
OTC_PRIV int otc_scpi_get_hw_id(struct otc_scpi_dev_inst *scpi,
			struct otc_scpi_hw_info **scpi_response);
OTC_PRIV void otc_scpi_hw_info_free(struct otc_scpi_hw_info *hw_info);

OTC_PRIV const char *otc_scpi_unquote_string(char *s);

OTC_PRIV const char *otc_vendor_alias(const char *raw_vendor);
OTC_PRIV const char *otc_scpi_cmd_get(const struct scpi_command *cmdtable,
		int command);
OTC_PRIV int otc_scpi_cmd(const struct otc_dev_inst *sdi,
		const struct scpi_command *cmdtable,
		int channel_command, const char *channel_name,
		int command, ...);
OTC_PRIV int otc_scpi_cmd_resp(const struct otc_dev_inst *sdi,
		const struct scpi_command *cmdtable,
		int channel_command, const char *channel_name,
		GVariant **gvar, const GVariantType *gvtype, int command, ...);

/*--- GPIB only functions ---------------------------------------------------*/

#ifdef HAVE_LIBGPIB
OTC_PRIV int otc_scpi_gpib_spoll(struct otc_scpi_dev_inst *scpi, char *buf);
#endif

#endif
