/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2020 Martin Eitzenberger <x@cymaphore.net>
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

/**
 * @file
 * @version 1
 *
 * APPA B Interface
 *
 * Based on APPA Communication Protocol v2.8
 *
 * Driver for modern APPA meters (handheld, bench, clamp). Communication is
 * done over a serial interface using the known APPA-Frames, see below. The
 * base protocol is always the same and deviates only where the models have
 * differences in ablities, range and features.
 *
 * Supporting Live data and downloading LOG and MEM data from devices.
 * Connection is done via BLE or optical serial (USB, EA232, EA485).
 *
 * Utilizes the APPA transport protocol for packet handling.
 *
 * Support for calibration information is prepared but not implemented.
 */

#include <config.h>
#include "protocol.h"

static const uint32_t appadmm_scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
};

static const uint32_t appadmm_drvopts[] = {
	OTC_CONF_MULTIMETER,
};

static const uint32_t appadmm_devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_FRAMES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_DATA_SOURCE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static const char *appadmm_data_sources[] = {
	"Live", /**< APPADMM_DATA_SOURCE_LIVE */
	"MEM", /**< APPADMM_DATA_SOURCE_MEM */
	"LOG", /**< APPADMM_DATA_SOURCE_LOG */
};

static GSList *appadmm_scan(struct otc_dev_driver *di, GSList *options)
{
	struct drv_context *drvc;
	struct appadmm_context *devc;
	GSList *devices;
	const char *conn;
	const char *serialcomm;
	struct otc_dev_inst *sdi;
	struct otc_serial_dev_inst *serial;
	struct otc_channel_group *group;
	struct otc_channel *channel_primary;
	struct otc_channel *channel_secondary;

	int retr;

	GSList *it;
	struct otc_config *src;

	devices = NULL;
	drvc = di->context;
	drvc->instances = NULL;

	/* Device context is used instead of another ..._info struct here */
	devc = g_malloc0(sizeof(struct appadmm_context));
	appadmm_clear_context(devc);

	serialcomm = APPADMM_CONF_SERIAL;
	conn = NULL;
	for (it = options; it; it = it->next) {
		src = it->data;
		switch (src->key) {
		case OTC_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}

	if (!conn)
		return NULL;

	if (!serialcomm)
		serialcomm = APPADMM_CONF_SERIAL;

	serial = otc_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) < OTC_OK)
		return NULL;

	if (serial_flush(serial) < OTC_OK)
		return NULL;

	sdi = g_malloc0(sizeof(*sdi));

	sdi->conn = serial;
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->status = OTC_ST_INACTIVE;
	sdi->driver = di;
	sdi->priv = devc;

	otc_tp_appa_init(&devc->appa_inst, serial);

	appadmm_op_identify(sdi);

	/* If received model is invalid or nothing received, abort */
	if (devc->model_id == APPADMM_MODEL_ID_INVALID
		|| devc->model_id < APPADMM_MODEL_ID_INVALID
		|| devc->model_id > APPADMM_MODEL_ID_OVERFLOW) {
		otc_err("APPA-Device NOT FOUND or INVALID; No valid response "
			"to read_information request.");
		otc_serial_dev_inst_free(serial);
		serial_close(serial);
		return NULL;
	}

	/* Older models with the AMICCOM A8105 have troubles with higher rates
	 * over BLE, let them run without time windows
	 */
#ifdef HAVE_BLUETOOTH
	if (devc->appa_inst.serial->bt_conn_type == SER_BT_CONN_APPADMM
		&& (devc->model_id == APPADMM_MODEL_ID_208B
		|| devc->model_id == APPADMM_MODEL_ID_506B
		|| devc->model_id == APPADMM_MODEL_ID_506B_2
		|| devc->model_id == APPADMM_MODEL_ID_150B))
		devc->rate_interval = APPADMM_RATE_INTERVAL_DISABLE;
	else
#endif
		devc->rate_interval = APPADMM_RATE_INTERVAL_DEFAULT;

	otc_info("APPA-Device DETECTED; Vendor: %s, Model: %s, OEM-Model: %s, Version: %s, Serial number: %s, Model ID: %i",
		sdi->vendor,
		sdi->model,
		appadmm_model_id_name(devc->model_id),
		sdi->version,
		sdi->serial_num,
		devc->model_id);

	channel_primary = otc_channel_new(sdi,
		APPADMM_CHANNEL_DISPLAY_PRIMARY,
		OTC_CHANNEL_ANALOG,
		TRUE,
		appadmm_channel_name(APPADMM_CHANNEL_DISPLAY_PRIMARY));

	channel_secondary = otc_channel_new(sdi,
		APPADMM_CHANNEL_DISPLAY_SECONDARY,
		OTC_CHANNEL_ANALOG,
		TRUE,
		appadmm_channel_name(APPADMM_CHANNEL_DISPLAY_SECONDARY));

	group = g_malloc0(sizeof(*group));
	group->name = g_strdup("Display");
	sdi->channel_groups = g_slist_append(sdi->channel_groups, group);

	group->channels = g_slist_append(group->channels, channel_primary);
	group->channels = g_slist_append(group->channels, channel_secondary);

	devices = g_slist_append(devices, sdi);

	if ((retr = serial_close(serial)) < OTC_OK) {
		otc_err("Unable to close device after scan");
		return NULL;
	}

	return std_scan_complete(di, devices);
}

static int appadmm_config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct appadmm_context *devc;

	(void) cg;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_FRAMES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_get(&devc->limits, key, data);
	case OTC_CONF_DATA_SOURCE:
		return(*data =
			g_variant_new_string(appadmm_data_sources[devc->data_source]))
			!= NULL ? OTC_OK : OTC_ERR_ARG;
	default:
		return OTC_ERR_NA;
	}

	return OTC_ERR_ARG;
}

static int appadmm_config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct appadmm_context *devc;

	int idx;
	int retr;

	(void) cg;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	retr = OTC_OK;
	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_FRAMES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_set(&devc->limits, key, data);
	case OTC_CONF_DATA_SOURCE:
		if ((idx = std_str_idx(data, ARRAY_AND_SIZE(appadmm_data_sources))) < 0)
			return OTC_ERR_ARG;
		devc->data_source = idx;
		break;
	default:
		retr = OTC_ERR_NA;
	}

	return retr;
}

static int appadmm_config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int retr;

	retr = OTC_OK;

	if (!sdi)
		return STD_CONFIG_LIST(key, data, sdi, cg, appadmm_scanopts, appadmm_drvopts, appadmm_devopts);

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, appadmm_scanopts, appadmm_drvopts, appadmm_devopts);
	case OTC_CONF_DATA_SOURCE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(appadmm_data_sources));
		break;
	default:
		return OTC_ERR_NA;
	}

	return retr;
}

/**
 * Start Data Acquisition, for Live, LOG and MEM alike.
 *
 * For MEM and LOG entries, check if the device is capable of such a feature
 * and request the amount of data present. Otherwise acquisition will instantly
 * fail.
 *
 * @param sdi Serial Device Instance
 * @retval OTC_OK on success
 * @retval OTC_ERR_... on error
 */
static int appadmm_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct appadmm_context *devc;
	struct otc_serial_dev_inst *serial;
	enum appadmm_storage_e storage;

	int retr;

	devc = sdi->priv;
	serial = sdi->conn;

	retr = OTC_OK;

	switch (devc->data_source) {
	case APPADMM_DATA_SOURCE_LIVE:
		otc_sw_limits_acquisition_start(&devc->limits);
		if ((retr = std_session_send_df_header(sdi)) < OTC_OK)
			return retr;

		retr = serial_source_add(sdi->session, serial, G_IO_IN, 10,
			appadmm_acquire_live, (void *) sdi);
		break;

	case APPADMM_DATA_SOURCE_MEM:
	case APPADMM_DATA_SOURCE_LOG:
		if ((retr = appadmm_op_storage_info(sdi)) < OTC_OK)
			return retr;

		switch (devc->data_source) {
		case APPADMM_DATA_SOURCE_MEM:
			storage = APPADMM_STORAGE_MEM;
			break;
		case APPADMM_DATA_SOURCE_LOG:
			storage = APPADMM_STORAGE_LOG;
			break;
		default:
			return OTC_ERR_BUG;
		}

		devc->error_counter = 0;

		/* Frame limit is used for selecting the amount of data read
		 * from the device. Thhis way the user can reduce the amount
		 * of data downloaded from the device. */
		if (devc->limits.limit_frames < 1
			|| devc->limits.limit_frames > (uint64_t) devc->storage_info[storage].amount)
			devc->limits.limit_frames = devc->storage_info[storage].amount;

		otc_sw_limits_acquisition_start(&devc->limits);
		if ((retr = std_session_send_df_header(sdi)) < OTC_OK)
			return retr;

		if (devc->storage_info[storage].rate > 0) {
			otc_session_send_meta(sdi, OTC_CONF_SAMPLE_INTERVAL, g_variant_new_uint64(devc->storage_info[storage].rate * 1000));
		}

		retr = serial_source_add(sdi->session, serial, G_IO_IN, 10,
			appadmm_acquire_storage, (void *) sdi);
		break;
	}

	return retr;
}

#define APPADMM_DRIVER_ENTRY(ARG_NAME, ARG_LONGNAME) \
&((struct otc_dev_driver){ \
	.name = ARG_NAME, \
	.longname = ARG_LONGNAME, \
	.api_version = 1, \
	.init = std_init, \
	.cleanup = std_cleanup, \
	.scan = appadmm_scan, \
	.dev_list = std_dev_list, \
	.dev_clear = std_dev_clear, \
	.config_get = appadmm_config_get, \
	.config_set = appadmm_config_set, \
	.config_list = appadmm_config_list, \
	.dev_open = std_serial_dev_open, \
	.dev_close = std_serial_dev_close, \
	.dev_acquisition_start = appadmm_acquisition_start, \
	.dev_acquisition_stop = std_serial_dev_acquisition_stop, \
	.context = NULL, \
})

/**
 * List of assigned driver names
 */
OTC_REGISTER_DEV_DRIVER_LIST(appadmm_drivers,
	APPADMM_DRIVER_ENTRY("appa-dmm", "APPA 150, 170, 200, 500, A, S and sFlex-Series"),
	APPADMM_DRIVER_ENTRY("benning-dmm", "BENNING MM 10-1, MM 12, CM 9-2, CM 10-1, CM 12, -PV"),
	APPADMM_DRIVER_ENTRY("cmt-35xx", "CMT 35xx Series"),
	APPADMM_DRIVER_ENTRY("ht-8100", "HT Instruments HT8100"),
	APPADMM_DRIVER_ENTRY("iso-tech-idm50x", "ISO-TECH IDM50x Series"),
	APPADMM_DRIVER_ENTRY("rspro-dmm", "RS PRO IDM50x and S Series"),
	APPADMM_DRIVER_ENTRY("sefram-7xxx", "Sefram 7xxx Series"),
	APPADMM_DRIVER_ENTRY("voltcraft-vc930", "Voltcraft VC-930"),
	APPADMM_DRIVER_ENTRY("voltcraft-vc950", "Voltcraft VC-950"),
	);
