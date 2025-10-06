/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2018 James Churchill <pelrun@gmail.com>
 * Copyright (C) 2019 Frank Stettner <frank-stettner@gmx.net>
 * Copyright (C) 2021 Gerhard Sittig <gerhard.sittig@gmx.net>
 * Copyright (C) 2021 Constantin Wenger <constantin.wenger@gmail.com>
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

#include "config.h"

#include <string.h>
#include <math.h>

#include "protocol.h"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
	OTC_CONF_MODBUSADDR,
};

static const uint32_t drvopts[] = {
	OTC_CONF_POWER_SUPPLY,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_VOLTAGE | OTC_CONF_GET,
	OTC_CONF_VOLTAGE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CURRENT | OTC_CONF_GET,
	OTC_CONF_CURRENT_LIMIT | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_REGULATION | OTC_CONF_GET,
	OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
};

static const uint32_t devopts_w_range[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_VOLTAGE | OTC_CONF_GET,
	OTC_CONF_VOLTAGE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CURRENT | OTC_CONF_GET,
	OTC_CONF_CURRENT_LIMIT | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_REGULATION | OTC_CONF_GET,
	OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_RANGE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static const uint32_t devopts_etm[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_VOLTAGE | OTC_CONF_GET,
	OTC_CONF_VOLTAGE_TARGET | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_CURRENT | OTC_CONF_GET,
	OTC_CONF_CURRENT_LIMIT | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_REGULATION | OTC_CONF_GET,
	OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE | OTC_CONF_GET,
	OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE | OTC_CONF_GET,
};

/* Range name, max current/voltage/power, current/voltage/power digits. */
static const struct rdtech_dps_range ranges_dps3005[] = {
	{  "5A",  5, 30,  160, 3, 2, 0 }
};

static const struct rdtech_dps_range ranges_dps5005[] = {
	{  "5A",  5, 50,  250, 3, 2, 0 }
};

static const struct rdtech_dps_range ranges_dps5015[] = {
	{ "15A", 15, 50,  750, 2, 2, 0 }
};

static const struct rdtech_dps_range ranges_dps5020[] = {
	{ "20A", 20, 50, 1000, 2, 2, 0 }
};

static const struct rdtech_dps_range ranges_dps8005[] = {
	{  "5A",  5, 80,  408, 3, 2, 0 }
};

static const struct rdtech_dps_range ranges_rd6006[] = {
	{  "6A",  6, 60,  360, 3, 2, 0 }
};

static const struct rdtech_dps_range ranges_rd6006p[] = {
	{  "6A",  6, 60,  360, 4, 3, 0 }
};

static const struct rdtech_dps_range ranges_rd6012[] = {
	{ "12A", 12, 60,  720, 2, 2, 0 }
};

/*
 * RD6012P supports multiple current ranges with differing resolution.
 * Up to 6A with 4 digits (when RTU reg 20 == 0), up to 12A with 3 digits
 * (when RTU reg 20 == 1).
 */
static const struct rdtech_dps_range ranges_rd6012p[] = {
	{  "6A",  6, 60,  360, 4, 3, 0 },
	{ "12A", 12, 60,  720, 3, 3, 0 }
};

static const struct rdtech_dps_range ranges_rd6018[] = {
	{ "18A", 18, 60, 1080, 2, 2, 0 }
};

static const struct rdtech_dps_range ranges_rd6024[] = {
	{ "24A", 24, 60, 1440, 2, 2, 0 }
};

/* Model ID, model name, model dependent ranges. */
static const struct rdtech_dps_model supported_models[] = {
	{ MODEL_DPS, 3005, "DPS3005", ARRAY_AND_SIZE(ranges_dps3005), },
	{ MODEL_DPS, 5005, "DPS5005", ARRAY_AND_SIZE(ranges_dps5005), },
	{ MODEL_DPS, 5205, "DPH5005", ARRAY_AND_SIZE(ranges_dps5005), },
	{ MODEL_DPS, 5015, "DPS5015", ARRAY_AND_SIZE(ranges_dps5015), },
	{ MODEL_DPS, 5020, "DPS5020", ARRAY_AND_SIZE(ranges_dps5020), },
	{ MODEL_DPS, 8005, "DPS8005", ARRAY_AND_SIZE(ranges_dps8005), },
	/*
	 * Specs for models RD60nn taken from the 2020.12.2 instruction manual,
	 * specs for RD6006P from the 2021.2.26 (english) manual,
	 * specs for RD6012P from the 2021.10.26 (english) manual,
	 * and specs for RD6024P from the 2021.1.7 (english) manual.
	 */
	{ MODEL_RD, 60061, "RD6006", ARRAY_AND_SIZE(ranges_rd6006),  },
	{ MODEL_RD, 60062, "RD6006", ARRAY_AND_SIZE(ranges_rd6006),  },
	{ MODEL_RD, 60065, "RD6006P", ARRAY_AND_SIZE(ranges_rd6006p), },
	{ MODEL_RD, 60121, "RD6012", ARRAY_AND_SIZE(ranges_rd6012),  },
	{ MODEL_RD, 60125, "RD6012P", ARRAY_AND_SIZE(ranges_rd6012p), },
	{ MODEL_RD, 60181, "RD6018", ARRAY_AND_SIZE(ranges_rd6018),  },
	{ MODEL_RD, 60241, "RD6024", ARRAY_AND_SIZE(ranges_rd6024),  },
};

static const struct etommens_etm_xxxxp_model etommens_models[] = {
	{ 0x4B50, 3010, "eTM-3010P/RS310P/HM310P" },
	{ 0x4B50, 305, "etM-305P/RS305P/HM305P" },
};

static struct otc_dev_driver rdtech_dps_driver_info;
static struct otc_dev_driver rdtech_rd_driver_info;
static struct otc_dev_driver etommens_etm_xxxxp_driver_info;

static struct otc_dev_inst *probe_device(struct otc_modbus_dev_inst *modbus,
	enum rdtech_dps_model_type model_type)
{
	static const char * const type_prefix[] = {
		[MODEL_DPS] = "DPS",
		[MODEL_RD]  = "RD",
	};

	uint16_t id, version;
	uint32_t serno;
	int ret;
	const struct rdtech_dps_model *model, *supported;
	size_t i;
	struct otc_dev_inst *sdi;
	struct dev_context *devc;

	ret = rdtech_dps_get_model_version(modbus,
		model_type, &id, &version, &serno);
	otc_dbg("probe: ret %d, type %s, model %u, vers %u, snr %u.",
		ret, type_prefix[model_type], id, version, serno);
	if (ret != OTC_OK)
		return NULL;
	model = NULL;
	for (i = 0; i < ARRAY_SIZE(supported_models); i++) {
		supported = &supported_models[i];
		if (model_type != supported->model_type)
			continue;
		if (id != supported->id)
			continue;
		model = supported;
		break;
	}
	if (!model) {
		otc_err("Unknown model: %s%u.", type_prefix[model_type], id);
		return NULL;
	}

	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("RDTech");
	switch (model_type) {
	case MODEL_DPS:
		sdi->model = g_strdup(model->name);
		sdi->version = g_strdup_printf("v%u", version);
		sdi->driver = &rdtech_dps_driver_info;
		break;
	case MODEL_RD:
		sdi->model = g_strdup(model->name);
		sdi->version = g_strdup_printf("v%u.%u",
			version / 100, version % 100);
		if (serno)
			sdi->serial_num = g_strdup_printf("%u", serno);
		sdi->driver = &rdtech_rd_driver_info;
		break;
	default:
		otc_err("Programming error, unhandled DPS/DPH/RD device type.");
		g_free(sdi->vendor);
		g_free(sdi);
		return NULL;
	}
	sdi->conn = modbus;
	sdi->inst_type = OTC_INST_MODBUS;

	otc_channel_new(sdi, 0, OTC_CHANNEL_ANALOG, TRUE, "V");
	otc_channel_new(sdi, 1, OTC_CHANNEL_ANALOG, TRUE, "I");
	otc_channel_new(sdi, 2, OTC_CHANNEL_ANALOG, TRUE, "P");

	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;
	otc_sw_limits_init(&devc->limits);
	devc->model.rdtech_model = model;
	ret = rdtech_dps_update_range(sdi);
	if (ret != OTC_OK)
		return NULL;

	return sdi;
}

static struct otc_dev_inst *probe_device_dps(struct otc_modbus_dev_inst *modbus)
{
	return probe_device(modbus, MODEL_DPS);
}

static struct otc_dev_inst *probe_device_rd(struct otc_modbus_dev_inst *modbus)
{
	return probe_device(modbus, MODEL_RD);
}

static int config_compare(gconstpointer a, gconstpointer b)
{
	const struct otc_config *ac = a, *bc = b;
	return ac->key != bc->key;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options,
	enum rdtech_dps_model_type model_type)
{
	static const char *default_serialcomm_dps = "9600/8n1";
	static const char *default_serialcomm_rd = "115200/8n1";

	struct otc_config default_serialcomm = {
		.key = OTC_CONF_SERIALCOMM,
		.data = NULL,
	};
	struct otc_config default_modbusaddr = {
		.key = OTC_CONF_MODBUSADDR,
		.data = g_variant_new_uint64(1),
	};
	GSList *opts, *devices;
	const char *serialcomm;
	struct otc_dev_inst *(*probe_func)(struct otc_modbus_dev_inst *modbus);

	/* TODO See why di->context isn't available yet at this time. */
	serialcomm = NULL;
	probe_func = NULL;
	if (di->context == &rdtech_dps_driver_info || model_type == MODEL_DPS) {
		serialcomm = default_serialcomm_dps;
		probe_func = probe_device_dps;
	}
	if (di->context == &rdtech_rd_driver_info || model_type == MODEL_RD) {
		serialcomm = default_serialcomm_rd;
		probe_func = probe_device_rd;
	}
	if (!probe_func)
		return NULL;
	if (serialcomm && *serialcomm)
		default_serialcomm.data = g_variant_new_string(serialcomm);

	opts = options;
	if (!g_slist_find_custom(options, &default_serialcomm, config_compare))
		opts = g_slist_prepend(opts, &default_serialcomm);
	if (!g_slist_find_custom(options, &default_modbusaddr, config_compare))
		opts = g_slist_prepend(opts, &default_modbusaddr);

	devices = otc_modbus_scan(di->context, opts, probe_func);

	while (opts != options)
		opts = g_slist_delete_link(opts, opts);
	g_variant_unref(default_serialcomm.data);
	g_variant_unref(default_modbusaddr.data);

	return devices;
}

static GSList *scan_dps(struct otc_dev_driver *di, GSList *options)
{
	return scan(di, options, MODEL_DPS);
}

static GSList *scan_rd(struct otc_dev_driver *di, GSList *options)
{
	return scan(di, options, MODEL_RD);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	struct otc_modbus_dev_inst *modbus;
	struct rdtech_dps_state state;
	int ret;

	modbus = sdi->conn;
	if (otc_modbus_open(modbus) < 0)
		return OTC_ERR;

	memset(&state, 0, sizeof(state));
	state.lock = TRUE;
	state.mask |= STATE_LOCK;
	ret = rdtech_dps_set_state(sdi, &state);
	if (ret != OTC_OK)
		return ret;

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	struct otc_modbus_dev_inst *modbus;
	struct rdtech_dps_state state;

	modbus = sdi->conn;
	if (!modbus)
		return OTC_ERR_BUG;

	memset(&state, 0, sizeof(state));
	state.lock = FALSE;
	state.mask |= STATE_LOCK;
	(void)rdtech_dps_set_state(sdi, &state);

	return otc_modbus_close(modbus);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct rdtech_dps_state state;
	int ret;
	const char *cc_text, *range_text;

	(void)cg;

	devc = sdi->priv;

	ret = OTC_OK;
	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		ret = otc_sw_limits_config_get(&devc->limits, key, data);
		break;
	case OTC_CONF_ENABLED:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_OUTPUT_ENABLED))
			return OTC_ERR_DATA;
		*data = g_variant_new_boolean(state.output_enabled);
		break;
	case OTC_CONF_REGULATION:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_REGULATION_CC))
			return OTC_ERR_DATA;
		cc_text = state.regulation_cc ? "CC" : "CV";
		*data = g_variant_new_string(cc_text);
		break;
	case OTC_CONF_VOLTAGE:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_VOLTAGE))
			return OTC_ERR_DATA;
		*data = g_variant_new_double(state.voltage);
		break;
	case OTC_CONF_VOLTAGE_TARGET:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_VOLTAGE_TARGET))
			return OTC_ERR_DATA;
		*data = g_variant_new_double(state.voltage_target);
		break;
	case OTC_CONF_CURRENT:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_CURRENT))
			return OTC_ERR_DATA;
		*data = g_variant_new_double(state.current);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_CURRENT_LIMIT))
			return OTC_ERR_DATA;
		*data = g_variant_new_double(state.current_limit);
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_ENABLED:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_PROTECT_ENABLED))
			return OTC_ERR_DATA;
		*data = g_variant_new_boolean(state.protect_enabled);
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_PROTECT_OVP))
			return OTC_ERR_DATA;
		*data = g_variant_new_boolean(state.protect_ovp);
		break;
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_OUTPUT_ENABLED))
			return OTC_ERR_DATA;
		*data = g_variant_new_double(state.ovp_threshold);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_PROTECT_ENABLED))
			return OTC_ERR_DATA;
		*data = g_variant_new_boolean(state.protect_enabled);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_PROTECT_OCP))
			return OTC_ERR_DATA;
		*data = g_variant_new_boolean(state.protect_ocp);
		break;
	case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_OCP_THRESHOLD))
			return OTC_ERR_DATA;
		*data = g_variant_new_double(state.ocp_threshold);
		break;
	case OTC_CONF_RANGE:
		ret = rdtech_dps_get_state(sdi, &state, ST_CTX_CONFIG);
		if (ret != OTC_OK)
			return ret;
		if (!(state.mask & STATE_RANGE))
			return OTC_ERR_DATA;
		range_text = devc->model.rdtech_model->ranges[state.range].range_str;
		*data = g_variant_new_string(range_text);
		break;
	default:
		return OTC_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct rdtech_dps_state state;
	const char *range_str;
	const struct rdtech_dps_range *range;
	size_t i;

	(void)cg;

	devc = sdi->priv;
	memset(&state, 0, sizeof(state));

	switch (key) {
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_LIMIT_MSEC:
		return otc_sw_limits_config_set(&devc->limits, key, data);
	case OTC_CONF_ENABLED:
		state.output_enabled = g_variant_get_boolean(data);
		state.mask |= STATE_OUTPUT_ENABLED;
		return rdtech_dps_set_state(sdi, &state);
	case OTC_CONF_VOLTAGE_TARGET:
		state.voltage_target = g_variant_get_double(data);
		state.mask |= STATE_VOLTAGE_TARGET;
		return rdtech_dps_set_state(sdi, &state);
	case OTC_CONF_CURRENT_LIMIT:
		state.current_limit = g_variant_get_double(data);
		state.mask |= STATE_CURRENT_LIMIT;
		return rdtech_dps_set_state(sdi, &state);
	case OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD:
		state.ovp_threshold = g_variant_get_double(data);
		state.mask |= STATE_OVP_THRESHOLD;
		return rdtech_dps_set_state(sdi, &state);
	case OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD:
		state.ocp_threshold = g_variant_get_double(data);
		state.mask |= STATE_OCP_THRESHOLD;
		return rdtech_dps_set_state(sdi, &state);
	case OTC_CONF_RANGE:
		range_str = g_variant_get_string(data, NULL);
		for (i = 0; i < devc->model.rdtech_model->n_ranges; i++) {
			range = &devc->model.rdtech_model->ranges[i];
			if (g_strcmp0(range->range_str, range_str) != 0)
				continue;
			state.range = i;
			state.mask |= STATE_RANGE;
			return rdtech_dps_set_state(sdi, &state);
		}
		return OTC_ERR_NA;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	const struct rdtech_dps_range *range;
	GVariantBuilder gvb;
	size_t i;
	const char *s;

	devc = (sdi) ? sdi->priv : NULL;

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		if (devc && devc->model.rdtech_model->n_ranges > 1) {
			return STD_CONFIG_LIST(key, data, sdi, cg,
				scanopts, drvopts, devopts_w_range);
		} else {
			return STD_CONFIG_LIST(key, data, sdi, cg,
				scanopts, drvopts, devopts);
		}
	case OTC_CONF_VOLTAGE_TARGET:
		rdtech_dps_update_range(sdi);
		range = &devc->model.rdtech_model->ranges[devc->curr_range];
		*data = std_gvar_min_max_step(0.0, range->max_voltage,
			1 / devc->voltage_multiplier);
		break;
	case OTC_CONF_CURRENT_LIMIT:
		rdtech_dps_update_range(sdi);
		range = &devc->model.rdtech_model->ranges[devc->curr_range];
		*data = std_gvar_min_max_step(0.0, range->max_current,
			1 / devc->current_multiplier);
		break;
	case OTC_CONF_RANGE:
		g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);
		for (i = 0; i < devc->model.rdtech_model->n_ranges; i++) {
			s = devc->model.rdtech_model->ranges[i].range_str;
			g_variant_builder_add(&gvb, "s", s);
		}
		*data = g_variant_builder_end(&gvb);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_modbus_dev_inst *modbus;
	int ret;

	modbus = sdi->conn;
	devc = sdi->priv;

	devc->acquisition_started = TRUE;

	/* Seed internal state from current data. */
	ret = rdtech_dps_seed_receive(sdi);
	if (ret != OTC_OK) {
		devc->acquisition_started = FALSE;
		return ret;
	}

	/* Register the periodic data reception callback. */
	ret = otc_modbus_source_add(sdi->session, modbus, G_IO_IN, 10,
			rdtech_dps_receive_data, (void *)sdi);
	if (ret != OTC_OK) {
		devc->acquisition_started = FALSE;
		return ret;
	}

	otc_sw_limits_acquisition_start(&devc->limits);
	std_session_send_df_header(sdi);

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_modbus_dev_inst *modbus;

	devc = sdi->priv;

	std_session_send_df_end(sdi);
	devc->acquisition_started = FALSE;

	modbus = sdi->conn;
	otc_modbus_source_remove(sdi->session, modbus);

	return OTC_OK;
}

static struct otc_dev_driver rdtech_dps_driver_info = {
	.name = "rdtech-dps",
	.longname = "RDTech DPS/DPH series power supply",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan_dps,
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
OTC_REGISTER_DEV_DRIVER(rdtech_dps_driver_info);

static struct otc_dev_driver rdtech_rd_driver_info = {
	.name = "rdtech-rd",
	.longname = "RDTech RD series power supply",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan_rd,
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
OTC_REGISTER_DEV_DRIVER(rdtech_rd_driver_info);
