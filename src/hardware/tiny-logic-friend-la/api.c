/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2021 Kevin Matocha <kmatocha@icloud.com>
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
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "../../scpi.h"
#include "protocol.h"

static struct otc_dev_driver tiny_logic_friend_la_driver_info;

static const uint32_t scanopts[] = {
};

static const uint32_t drvopts[] = {
	OTC_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	OTC_CONF_RLE | OTC_CONF_GET,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_SAMPLERATE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_TRIGGER_MATCH | OTC_CONF_LIST,
};

static int tlf_get_lists(struct otc_dev_inst *sdi)
{
	uint8_t model_found;

	otc_spew("-> Enter tlf_init_device");

	model_found = 0;

	otc_spew("-> Enter tlf_init_device 1");
	if (g_ascii_strcasecmp(sdi->model, "tiny") &&
		g_ascii_strcasecmp(sdi->model, "Logic") &&
		g_ascii_strcasecmp(sdi->model, "Friend")) {
		model_found = 1;
	}

	otc_spew("-> Enter tlf_init_device 2");
	if (!model_found) {
		otc_dbg("Device %s is not supported by this driver.", sdi->model);
		return OTC_ERR_NA;
	}

	otc_spew("-> Enter tlf_init_device 3");

	if (!(tlf_channels_list(sdi) == OTC_OK)) {
		return OTC_ERR_NA;
	}
	otc_spew("-> Enter tlf_init_device 4");

	if (!(tlf_samplerates_list(sdi) == OTC_OK)) {
		return OTC_ERR_NA;
	}
	otc_spew("-> Enter tlf_init_device 5");
	if (!(tlf_trigger_list(sdi) == OTC_OK)) {
		return OTC_ERR_NA;
	}

	if (!(tlf_RLE_mode_get(sdi) == OTC_OK)) {
		return OTC_ERR_NA;
	}
	otc_spew("-> Enter tlf_init_device 6");

	return OTC_OK;
}

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_scpi_hw_info *hw_info;

	otc_spew("-> Enter probe_device");

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK)
		goto fail;

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &tiny_logic_friend_la_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	sdi->conn = scpi;

	otc_spew("Vendor: %s\n", sdi->vendor);
	otc_spew("Model: %s\n", sdi->model);
	otc_spew("Version: %s\n", sdi->version);
	otc_spew("Serial number: %s\n", sdi->serial_num);

	otc_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;
	devc->trigger_matches_count = TRIGGER_MATCHES_COUNT;

	if (tlf_get_lists(sdi) != OTC_OK)
		goto fail;

	GSList *l;
	for (l = sdi->channels; l; l = l->next) {
		otc_err("** probe_device channel found");
	}

	return sdi;

fail:
	otc_scpi_hw_info_free(hw_info);
	otc_dev_inst_free(sdi);
	g_free(devc);
	return NULL;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	otc_spew("-> Enter scan");

	return otc_scpi_scan(di->context, options, probe_device);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	int ret;

	otc_spew("-> Enter dev_open");

	if ((ret = otc_scpi_open(sdi->conn)) < 0) {
		otc_err("Failed to open SCPI device: %s.", otc_strerror(ret));
		return OTC_ERR;
	}

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	otc_spew("-> Enter dev_close");
	return otc_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct otc_channel *ch;
	uint64_t buf_int;
	int32_t buf_32;
	gboolean enable_status;
	enable_status = FALSE;

	otc_spew("-> Enter config_get");

	if (!sdi) {
		otc_err("Must call `scan` prior to calling `config_get`.");
		return OTC_ERR_ARG;
	}

	if (!cg) {
		switch (key) {
		case OTC_CONF_SAMPLERATE:
			otc_spew("  -> OTC_CONF_SAMPLERATE");
			if (tlf_samplerate_get(sdi, &buf_int) != OTC_OK) {
				return OTC_ERR;
			}
			*data = g_variant_new_uint64(buf_int);
			otc_spew("config_get: returning samplerate");
			break;
		case OTC_CONF_NUM_LOGIC_CHANNELS:
			otc_spew("  -> OTC_CONF_NUM_LOGIC_CHANNELS");
			*data = g_variant_new_uint32(g_slist_length(sdi->channels));
			break;
		case OTC_CONF_LIMIT_SAMPLES:
			otc_spew("  -> OTC_CONF_LIMIT_SAMPLES");
			if (tlf_samples_get(sdi, &buf_32) != OTC_OK) {
				return OTC_ERR;
			}
			*data = g_variant_new_uint64(buf_32);
			break;
		case OTC_CONF_RLE:
			break;
		default:
			otc_dbg("(1) Unsupported key: %d ", key);
			return OTC_ERR_NA;
			break;
		}
	} else {
		switch (key) {
		case OTC_CONF_ENABLED:
			otc_spew("  -> OTC_CONF_ENABLED");
			ch = cg->channels->data;
			if (tlf_channel_state_get(sdi, ch->index, &enable_status) != OTC_OK) {
				return OTC_ERR;
			}
			*data = g_variant_new_boolean(enable_status);
			break;
		case OTC_CONF_NUM_LOGIC_CHANNELS:
			otc_spew("  -> OTC_CONF_NUM_LOGIC_CHANNELS");
			*data = g_variant_new_uint32(g_slist_length(sdi->channels));
			break;
		case OTC_CONF_LIMIT_SAMPLES:
			otc_spew("  -> OTC_CONF_LIMIT_SAMPLES");
			if (tlf_samples_get(sdi, &buf_32) != OTC_OK) {
				return OTC_ERR;
			}
			*data = g_variant_new_uint64(buf_32);
			break;
		default:
			otc_dbg("(2) Unsupported key: %d ", key);
			return OTC_ERR_NA;
			break;
		}
	}

	return OTC_OK;
}

static int config_channel_set(const struct otc_dev_inst *sdi,
	struct otc_channel *ch, unsigned int changes)
{
	otc_spew("-> Enter config_channel_set");

	switch (changes) {
	case OTC_CHANNEL_SET_ENABLED:
		otc_spew("  -> OTC_CHANNEL_SET_ENABLED");
		return tlf_channel_state_set(sdi, ch->index, ch->enabled);
		break;
	default:
		return OTC_ERR_NA;
	}
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct otc_channel *ch;
	uint64_t value;

	otc_spew("-> Enter config_set");

	if (!sdi) {
		otc_err("Must call `scan` prior to calling `config_set`.");
		return OTC_ERR_NA;
	}

	devc = sdi->priv;

	if (!sdi)
		return OTC_ERR_NA;

	if (!cg) {
		switch (key) {
		case OTC_CONF_SAMPLERATE:
			otc_spew("  -> OTC_CONF_SAMPLERATE");
			value = g_variant_get_uint64(data);
			if ((value < devc->samplerate_range[0]) ||
				(value > devc->samplerate_range[1])) {
				return OTC_ERR_SAMPLERATE;
			}
			return tlf_samplerate_set(sdi, value);
			break;
		case OTC_CONF_LIMIT_SAMPLES:
			otc_spew("  -> OTC_CONF_LIMIT_SAMPLES");
			if (tlf_samples_set(sdi, g_variant_get_uint64(data)) != OTC_OK) {
				return OTC_ERR;
			}
			break;
		default:
			otc_dbg("Unsupported key: %d ", key);
			return OTC_ERR_NA;
		}
	} else {
		switch (key) {
		case OTC_CONF_ENABLED:
			ch = cg->channels->data;
			otc_spew("  -> OTC_CONF_ENABLED");
			if (tlf_channel_state_set(sdi, ch->index, g_variant_get_boolean(data)) != OTC_OK) {
				return OTC_ERR;
			}
			break;
		case OTC_CONF_LIMIT_SAMPLES:
			otc_spew("  -> OTC_CONF_LIMIT_SAMPLES");
			if (tlf_samples_set(sdi, g_variant_get_uint64(data)) != OTC_OK) {
				return OTC_ERR;
			}
			break;
		default:
			otc_dbg("Unsupported key: %d ", key);
			return OTC_ERR_NA;
		}
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	otc_spew("-> config_list");
	struct dev_context *devc;

	otc_spew("-> Enter config_list");

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
		otc_spew("  -> OTC_CONF_SCAN_OPTIONS");
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case OTC_CONF_DEVICE_OPTIONS:
		otc_spew("  -> OTC_CONF_DEVICE_OPTIONS");
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts);
	case OTC_CONF_SAMPLERATE:
		otc_spew("  -> OTC_CONF_SAMPLERATE");
		if (!sdi) {
			otc_err("Must call `scan` prior to calling `config_list`.");
			return OTC_ERR_NA;
		}
		devc = sdi->priv;
		*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(devc->samplerate_range));
		break;
	case OTC_CONF_TRIGGER_MATCH:
		otc_spew("  -> OTC_CONF_TRIGGER_MATCH");
		if (!sdi) {
			otc_err("Must call `scan` prior to calling `config_list`.");
			return OTC_ERR_NA;
		}
		devc = sdi->priv;
		tlf_trigger_list(sdi);
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(devc->trigger_matches));
		break;
	case OTC_CONF_LIMIT_SAMPLES:
		if (!sdi)
			return OTC_ERR_ARG;
		else {
			devc = sdi->priv;
			if (tlf_maxsamples_get(sdi) != OTC_OK) {
				return OTC_ERR;
			}
			*data = std_gvar_tuple_u64(100, devc->max_samples);
			otc_dbg("max_samples: %u", devc->max_samples);
			break;
		}
	default:
		otc_dbg("Unsupported key: %d ", key);
		return OTC_ERR_NA;
	}
	otc_spew("<- Leaving config_list");

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_scpi_dev_inst *scpi;

	otc_spew("->dev_acquisition_start");

	scpi = sdi->conn;
	devc = sdi->priv;
	devc->data_pending = TRUE;
	devc->measured_samples = 0;
	devc->last_sample = 0;
	devc->last_timestamp = 0;

	otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 50,
			tlf_receive_data, (void *)sdi);

	std_session_send_df_header(sdi);

	otc_spew("Go RUN");
	std_session_send_df_frame_begin(sdi);

	return tlf_exec_run(sdi);
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	otc_spew("-> Enter dev_acquisition_stop");
	std_session_send_df_frame_end(sdi);
	otc_scpi_source_remove(sdi->session, sdi->conn);
	tlf_exec_stop(sdi);

	return OTC_OK;
}

static struct otc_dev_driver tiny_logic_friend_la_driver_info = {
	.name = "tiny-logic-friend-la",
	.longname = "Tiny Logic Friend-la",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_channel_set = config_channel_set,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(tiny_logic_friend_la_driver_info);
