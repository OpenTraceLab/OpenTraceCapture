/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2021 Peter Skarpetis <peters@skarpetis.com>
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

#include <math.h>

#include <config.h>
#include "protocol.h"

#define SERIALCOMM "57600/8n1"

static struct otc_dev_driver mhinstek_mhs5200a_driver_info;

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
};

static const uint32_t drvopts[] = {
	OTC_CONF_SIGNAL_GENERATOR,
};

static const uint32_t mhs5200a_devopts[] = {
	OTC_CONF_CONTINUOUS,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_LIMIT_MSEC | OTC_CONF_GET | OTC_CONF_SET,
};

static const uint32_t mhs5200a_devopts_cg[] = {
	OTC_CONF_ENABLED | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_PATTERN_MODE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_OUTPUT_FREQUENCY | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_AMPLITUDE | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_OFFSET | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_PHASE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_DUTY_CYCLE | OTC_CONF_GET | OTC_CONF_SET,
};

#define WAVEFORM_DEFAULT WFO_FREQUENCY | WFO_AMPLITUDE | WFO_OFFSET | WFO_PHASE

static const struct waveform_spec mhs5200a_waveforms[] = {
	{ WAVEFORM_SINE,             1.0E-6,  21.0E+6, 1.0E-6, WAVEFORM_DEFAULT },
	{ WAVEFORM_SQUARE,           1.0E-6,   6.0E+6, 1.0E-6, WAVEFORM_DEFAULT | WFO_DUTY_CYCLE },
	{ WAVEFORM_TRIANGLE,         1.0E-6,   6.0E+6, 1.0E-6, WAVEFORM_DEFAULT },
	{ WAVEFORM_RISING_SAWTOOTH,  1.0E-6,   6.0E+6, 1.0E-6, WAVEFORM_DEFAULT },
	{ WAVEFORM_FALLING_SAWTOOTH, 1.0E-6,   6.0E+6, 1.0E-6, WAVEFORM_DEFAULT },
};

static const struct channel_spec mhs5200a_channels[] = {
	{ "CH1",  ARRAY_AND_SIZE(mhs5200a_waveforms) },
	{ "CH2",  ARRAY_AND_SIZE(mhs5200a_waveforms) },
};

static const double phase_min_max_step[] = { 0.0, 360.0, 1.0 };

OTC_PRIV int mhs5200a_frequency_limits(enum waveform_type wtype, double *freq_min, double *freq_max)
{
	const struct waveform_spec *wspec;
	unsigned int i;
	
	wspec = NULL;
	for (i = 0; i < ARRAY_SIZE(mhs5200a_waveforms); i++) {
		if (mhs5200a_waveforms[i].waveform == wtype) {
			wspec = &mhs5200a_waveforms[i];
			break;
		}
	}
	if (!wspec) {
		otc_err("Could not determine current pattern type");
		return OTC_ERR;
	}
	*freq_min = wspec->freq_min;
	*freq_max = wspec->freq_max;
	return OTC_OK;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	struct otc_config *src;
	const char *conn, *serialcomm;
	struct otc_dev_inst *sdi;
	struct otc_channel *ch;
	struct otc_channel_group *cg;
	GSList *l, *devices;
	unsigned int i, ch_idx;
	char buf[PROTOCOL_LEN_MAX];

	devices = NULL;
	conn = NULL;
	serialcomm = SERIALCOMM;
	for (l = options; l; l = l->next) {
		src = l->data;
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
	
	serial = otc_serial_dev_inst_new(conn, serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != OTC_OK)
		return NULL;

	otc_info("Probing serial port %s.", conn);

	
	/* Query and verify model string. */
	if (mhs5200a_get_model(serial, buf, sizeof(buf)) < 0) {
		serial_close(serial);
		return NULL;
	}
	otc_info("Found device on port %s.", conn);

	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = OTC_ST_INACTIVE;
	sdi->vendor = g_strdup("MHINSTEK");
	sdi->model = g_strdup(buf);
	/*sdi->version = g_strdup("5.04"); */
	/*sdi->serial_num = g_strdup("1234"); */
	sdi->driver = &mhinstek_mhs5200a_driver_info;

	
	devc = g_malloc0(sizeof(*devc));
	otc_sw_limits_init(&devc->limits);
	devc->max_frequency = (buf[6] - '0') * 10 + buf[7] - '0';
	devc->max_frequency *= 1.0e06;
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->conn = serial;
	sdi->priv = devc;

	/* Create channel group and channel for each device channel. */
	ch_idx = 0;
	for (i = 0; i < ARRAY_SIZE(mhs5200a_channels); i++) {
		ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, TRUE,
				mhs5200a_channels[i].name);
		cg = g_malloc0(sizeof(*cg));
		snprintf(buf, sizeof(buf), "%u", i + 1);
		cg->name = g_strdup(buf);
		cg->channels = g_slist_append(cg->channels, ch);

		sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);
	}
	
	/* Create channels for the frequency counter output. */
	ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, TRUE, "FREQ");
	ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, TRUE, "PERIOD");
	ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, TRUE, "DUTY");
	ch = otc_channel_new(sdi, ch_idx++, OTC_CHANNEL_ANALOG, TRUE, "WIDTH");

	/* Add found device to result set. */
	devices = g_slist_append(devices, sdi);

	serial_close(serial);

	return std_scan_complete(di, devices);
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct otc_channel *ch;
	const struct otc_key_info *kinfo;
	double valf;
	long vall;
	int ret;
	
	if (!sdi || !data)
		return OTC_ERR_ARG;

	devc = sdi->priv;
	ret = OTC_OK;
	kinfo = otc_key_info_get(OTC_KEY_CONFIG, key);

	if (!cg) {
		switch (key) {
		case OTC_CONF_LIMIT_SAMPLES:
		case OTC_CONF_LIMIT_MSEC:
			ret = otc_sw_limits_config_get(&devc->limits, key, data);
			break;
		default:
			otc_dbg("%s: Unsupported key: %d (%s)", __func__,
				(int)key, (kinfo ? kinfo->name : "unknown"));
			ret = OTC_ERR_NA;
			break;
		}
	} else {
		ch = cg->channels->data;

		switch (key) {
		case OTC_CONF_ENABLED:
			if ((ret = mhs5200a_get_onoff(sdi, &vall)) == OTC_OK) {
				*data = g_variant_new_boolean(vall);
			}
			break;
		case OTC_CONF_PATTERN_MODE:
			if ((ret = mhs5200a_get_waveform(sdi, ch->index + 1, &vall)) == OTC_OK) {
				*data = g_variant_new_string(mhs5200a_waveform_to_string(vall));
			}
			break;
		case OTC_CONF_OUTPUT_FREQUENCY:
			if ((ret = mhs5200a_get_frequency(sdi, ch->index + 1, &valf)) == OTC_OK)
				*data = g_variant_new_double(valf);
			break;
		case OTC_CONF_AMPLITUDE:
			if ((ret = mhs5200a_get_amplitude(sdi, ch->index + 1, &valf)) == OTC_OK)
				*data = g_variant_new_double(valf);
			break;
		case OTC_CONF_OFFSET:
			if ((ret = mhs5200a_get_offset(sdi, ch->index + 1, &valf)) == OTC_OK)
				*data = g_variant_new_double(valf);
			break;
		case OTC_CONF_PHASE:
			if ((ret = mhs5200a_get_phase(sdi, ch->index + 1, &valf)) == OTC_OK)
				*data = g_variant_new_double(valf);
			break;
		case OTC_CONF_DUTY_CYCLE:
			if ((ret = mhs5200a_get_duty_cycle(sdi, ch->index + 1, &valf)) == OTC_OK)
				*data = g_variant_new_double(valf);
			break;
		default:
			otc_dbg("%s: Unsupported (cg) key: %d (%s)", __func__,
				(int)key, (kinfo ? kinfo->name : "unknown"));
			ret = OTC_ERR_NA;
			break;
		}
	}
		
	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct otc_channel *ch;
	const struct otc_key_info *kinfo;
	int ret;
	
	if (!sdi || !data)
		return OTC_ERR_ARG;

	devc = sdi->priv;
	ret = OTC_OK;
	kinfo = otc_key_info_get(OTC_KEY_CONFIG, key);

	if (!cg) {
		switch (key) {
		case OTC_CONF_LIMIT_SAMPLES:
		case OTC_CONF_LIMIT_MSEC:
			ret = otc_sw_limits_config_set(&devc->limits, key, data);
			break;
		default:
			otc_dbg("%s: Unsupported key: %d (%s)", __func__,
				(int)key, (kinfo ? kinfo->name : "unknown"));
			ret = OTC_ERR_NA;
			break;
		}
	} else {
		ch = cg->channels->data;

		switch (key) {
		case OTC_CONF_ENABLED:
			ret = mhs5200a_set_onoff(sdi, g_variant_get_boolean(data));
			break;
		case OTC_CONF_PATTERN_MODE:
			ret = mhs5200a_set_waveform_string(sdi, ch->index + 1, g_variant_get_string(data, NULL));
			break;
		case OTC_CONF_OUTPUT_FREQUENCY:
			ret = mhs5200a_set_frequency(sdi, ch->index + 1, g_variant_get_double(data));
			break;
		case OTC_CONF_AMPLITUDE:
			ret = mhs5200a_set_amplitude(sdi, ch->index + 1, g_variant_get_double(data));
			break;
		case OTC_CONF_OFFSET:
			ret = mhs5200a_set_offset(sdi, ch->index + 1, g_variant_get_double(data));
			break;
		case OTC_CONF_PHASE:
			ret = mhs5200a_set_phase(sdi, ch->index + 1, g_variant_get_double(data));
			break;
		case OTC_CONF_DUTY_CYCLE:
			ret = mhs5200a_set_duty_cycle(sdi, ch->index + 1, g_variant_get_double(data));
			break;
		default:
			otc_dbg("%s: Unsupported (cg) key: %d (%s)", __func__,
			       (int)key, (kinfo ? kinfo->name : "unknown"));
			ret = OTC_ERR_NA;
			break;
		}
	}
		
	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	struct otc_channel *ch;
	const struct channel_spec *ch_spec;
	GVariantBuilder *b;
	unsigned int i;
	double fspec[3];

	if (sdi) {
		devc = sdi->priv;
	} else {
		devc = NULL;
	}
	
	if (!cg) {
		switch (key) {
		case OTC_CONF_SCAN_OPTIONS:
		case OTC_CONF_DEVICE_OPTIONS:
			return std_opts_config_list(key, data, sdi, cg,
						    ARRAY_AND_SIZE(scanopts),
						    ARRAY_AND_SIZE(drvopts),
						    ARRAY_AND_SIZE(mhs5200a_devopts));
		default:
			return OTC_ERR_NA;
		}
	} else {
		ch = cg->channels->data;
		ch_spec = &mhs5200a_channels[ch->index];
		switch(key) {
		case OTC_CONF_DEVICE_OPTIONS:
			*data = std_gvar_array_u32(ARRAY_AND_SIZE(mhs5200a_devopts_cg));
			break;
		case OTC_CONF_PATTERN_MODE:
			b = g_variant_builder_new(G_VARIANT_TYPE("as"));
			for (i = 0; i < ch_spec->num_waveforms; i++) {
				g_variant_builder_add(b, "s",
						      mhs5200a_waveform_to_string(ch_spec->waveforms[i].waveform));
			}
			*data = g_variant_new("as", b);
			g_variant_builder_unref(b);
			break;
		case OTC_CONF_OUTPUT_FREQUENCY:
			if (!devc) {
				return OTC_ERR;
			}
			fspec[0] = 0.1;
			fspec[1] = devc->max_frequency;
			fspec[2] = 0.1;
			*data = std_gvar_min_max_step_array(fspec);
			break;
		case OTC_CONF_PHASE:
			*data = std_gvar_min_max_step_array(phase_min_max_step);
			break;
		default:
			return OTC_ERR_NA;
		}
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	int ret;
	
	if (!sdi)
		return OTC_ERR_ARG;


	ret = mhs5200a_set_counter_function(sdi, COUNTER_MEASURE_FREQUENCY);
	if (ret < 0)
		return OTC_ERR;
	
	ret = mhs5200a_set_counter_onoff(sdi, 1);
	if (ret < 0)
		return OTC_ERR;
	
	devc = sdi->priv;

	otc_sw_limits_acquisition_start(&devc->limits);
	ret = std_session_send_df_header(sdi);
	otc_session_source_add(sdi->session, -1, 0, 1000, mhs5200a_receive_data, (void *)sdi);
	return ret;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	if (!sdi)
		return OTC_ERR_ARG;

	mhs5200a_set_counter_onoff(sdi, 0);
	otc_session_source_remove(sdi->session, -1);
	std_session_send_df_end(sdi);
	return OTC_OK;
}

static struct otc_dev_driver mhinstek_mhs5200a_driver_info = {
	.name = "mhinstek-mhs-5200a",
	.longname = "MHINSTEK MHS-5200A",
	.api_version = 1,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(mhinstek_mhs5200a_driver_info);

