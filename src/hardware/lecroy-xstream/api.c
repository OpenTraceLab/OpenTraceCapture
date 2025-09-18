/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2017 Sven Schnelle <svens@stackframe.org>
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
#include <stdlib.h>
#include "../../scpi.h"
#include "protocol.h"

static struct otc_dev_driver lecroy_xstream_driver_info;

static const char *manufacturers[] = {
	"LECROY",
};

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
};

static const uint32_t drvopts[] = {
	OTC_CONF_OSCILLOSCOPE,
};

static const uint32_t devopts[] = {
	OTC_CONF_LIMIT_FRAMES | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_SAMPLERATE | OTC_CONF_GET,
	OTC_CONF_TIMEBASE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_NUM_HDIV | OTC_CONF_GET,
	OTC_CONF_HORIZ_TRIGGERPOS | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_TRIGGER_SOURCE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_TRIGGER_SLOPE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static const uint32_t devopts_cg_analog[] = {
	OTC_CONF_NUM_VDIV | OTC_CONF_GET,
	OTC_CONF_VDIV | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_COUPLING | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
};

static struct otc_dev_inst *probe_device(struct otc_scpi_dev_inst *scpi)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_scpi_hw_info *hw_info;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (otc_scpi_get_hw_id(scpi, &hw_info) != OTC_OK) {
		otc_info("Couldn't get IDN response.");
		goto fail;
	}

	if (std_str_idx_s(hw_info->manufacturer, ARRAY_AND_SIZE(manufacturers)) < 0)
		goto fail;

	sdi = g_malloc0(sizeof(struct otc_dev_inst));
	sdi->vendor = g_strdup(hw_info->manufacturer);
	sdi->model = g_strdup(hw_info->model);
	sdi->version = g_strdup(hw_info->firmware_version);
	sdi->serial_num = g_strdup(hw_info->serial_number);
	sdi->driver = &lecroy_xstream_driver_info;
	sdi->inst_type = OTC_INST_SCPI;
	sdi->conn = scpi;

	otc_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	devc = g_malloc0(sizeof(struct dev_context));
	sdi->priv = devc;

	if (lecroy_xstream_init_device(sdi) != OTC_OK)
		goto fail;

	return sdi;

fail:
	otc_scpi_hw_info_free(hw_info);
	otc_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	return otc_scpi_scan(di->context, options, probe_device);
}

static void clear_helper(struct dev_context *devc)
{
	lecroy_xstream_state_free(devc->model_state);
	g_free(devc->analog_groups);
}

static int dev_clear(const struct otc_dev_driver *di)
{
	return std_dev_clear_with_callback(di, (std_dev_clear_callback)clear_helper);
}

static int dev_open(struct otc_dev_inst *sdi)
{
	if (otc_scpi_open(sdi->conn) != OTC_OK)
		return OTC_ERR;

	if (lecroy_xstream_state_get(sdi) != OTC_OK)
		return OTC_ERR;

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	return otc_scpi_close(sdi->conn);
}

static int config_get(uint32_t key, GVariant **data,
		const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int idx;
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	model = devc->model_config;
	state = devc->model_state;
	*data = NULL;

	switch (key) {
	case OTC_CONF_NUM_HDIV:
		*data = g_variant_new_int32(model->num_xdivs);
		break;
	case OTC_CONF_TIMEBASE:
		*data = g_variant_new("(tt)",
				(*model->timebases)[state->timebase][0],
				(*model->timebases)[state->timebase][1]);
		break;
	case OTC_CONF_NUM_VDIV:
		if (std_cg_idx(cg, devc->analog_groups, model->analog_channels) < 0)
			return OTC_ERR_ARG;
		*data = g_variant_new_int32(model->num_ydivs);
		break;
	case OTC_CONF_VDIV:
		if ((idx = std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return OTC_ERR_ARG;
		*data = g_variant_new("(tt)",
				(*model->vdivs)[state->analog_channels[idx].vdiv][0],
				(*model->vdivs)[state->analog_channels[idx].vdiv][1]);
		break;
	case OTC_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_string((*model->trigger_sources)[state->trigger_source]);
		break;
	case OTC_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_string((*model->trigger_slopes)[state->trigger_slope]);
		break;
	case OTC_CONF_HORIZ_TRIGGERPOS:
		*data = g_variant_new_double(state->horiz_triggerpos);
		break;
	case OTC_CONF_COUPLING:
		if ((idx = std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return OTC_ERR_ARG;
		*data = g_variant_new_string((*model->coupling_options)[state->analog_channels[idx].coupling]);
		break;
	case OTC_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(state->sample_rate);
		break;
	case OTC_CONF_ENABLED:
		*data = g_variant_new_boolean(FALSE);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_set(uint32_t key, GVariant *data,
		const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	int ret, idx, j;
	char command[MAX_COMMAND_SIZE];
	struct dev_context *devc;
	const struct scope_config *model;
	struct scope_state *state;
	double tmp_d;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	model = devc->model_config;
	state = devc->model_state;

	switch (key) {
	case OTC_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		ret = OTC_OK;
		break;
	case OTC_CONF_TRIGGER_SOURCE:
		if ((idx = std_str_idx(data, *model->trigger_sources, model->num_trigger_sources)) < 0)
			return OTC_ERR_ARG;
		state->trigger_source = idx;
		g_snprintf(command, sizeof(command),
				"TRIG_SELECT EDGE,SR,%s", (*model->trigger_sources)[idx]);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_VDIV:
		if ((idx = std_u64_tuple_idx(data, *model->vdivs, model->num_vdivs)) < 0)
			return OTC_ERR_ARG;
		if ((j = std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return OTC_ERR_ARG;
		state->analog_channels[j].vdiv = idx;
		g_snprintf(command, sizeof(command),
				"C%d:VDIV %E", j + 1, (float) (*model->vdivs)[idx][0] / (*model->vdivs)[idx][1]);
		if (otc_scpi_send(sdi->conn, command) != OTC_OK || otc_scpi_get_opc(sdi->conn) != OTC_OK)
			return OTC_ERR;
		ret = OTC_OK;
		break;
	case OTC_CONF_TIMEBASE:
		if ((idx = std_u64_tuple_idx(data, *model->timebases, model->num_timebases)) < 0)
			return OTC_ERR_ARG;
		state->timebase = idx;
		g_snprintf(command, sizeof(command),
				"TIME_DIV %E", (float) (*model->timebases)[idx][0] / (*model->timebases)[idx][1]);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_HORIZ_TRIGGERPOS:
		tmp_d = g_variant_get_double(data);

		if (tmp_d < 0.0 || tmp_d > 1.0)
			return OTC_ERR;

		state->horiz_triggerpos = tmp_d;
		tmp_d = -(tmp_d - 0.5) *
				((double)(*model->timebases)[state->timebase][0] /
				(*model->timebases)[state->timebase][1])
				* model->num_xdivs;

		g_snprintf(command, sizeof(command), "TRIG POS %e S", tmp_d);

		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_TRIGGER_SLOPE:
		if ((idx = std_str_idx(data, *model->trigger_slopes, model->num_trigger_slopes)) < 0)
			return OTC_ERR_ARG;
		state->trigger_slope = idx;
		g_snprintf(command, sizeof(command),
				"%s:TRIG_SLOPE %s", (*model->trigger_sources)[state->trigger_source],
				(*model->trigger_slopes)[idx]);
		ret = otc_scpi_send(sdi->conn, command);
		break;
	case OTC_CONF_COUPLING:
		if ((idx = std_str_idx(data, *model->coupling_options, model->num_coupling_options)) < 0)
			return OTC_ERR_ARG;
		if ((j = std_cg_idx(cg, devc->analog_groups, model->analog_channels)) < 0)
			return OTC_ERR_ARG;
		state->analog_channels[j].coupling = idx;
		g_snprintf(command, sizeof(command), "C%d:COUPLING %s",
				j + 1, (*model->coupling_options)[idx]);
		if (otc_scpi_send(sdi->conn, command) != OTC_OK || otc_scpi_get_opc(sdi->conn) != OTC_OK)
			return OTC_ERR;
		ret = OTC_OK;
		break;
	default:
		ret = OTC_ERR_NA;
		break;
	}

	if (ret == OTC_OK)
		ret = otc_scpi_get_opc(sdi->conn);

	return ret;
}

static int config_channel_set(const struct otc_dev_inst *sdi,
	struct otc_channel *ch, unsigned int changes)
{
	/* Currently we only handle OTC_CHANNEL_SET_ENABLED. */
	if (changes != OTC_CHANNEL_SET_ENABLED)
		return OTC_ERR_NA;

	return lecroy_xstream_channel_state_set(sdi, ch->index, ch->enabled);
}

static int config_list(uint32_t key, GVariant **data,
		const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	const struct scope_config *model;

	devc = (sdi) ? sdi->priv : NULL;
	model = (devc) ? devc->model_config : NULL;

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, NO_OPTS, NO_OPTS);
	case OTC_CONF_DEVICE_OPTIONS:
		if (!cg)
			return STD_CONFIG_LIST(key, data, sdi, cg, NO_OPTS, drvopts, devopts);
		*data = std_gvar_array_u32(ARRAY_AND_SIZE(devopts_cg_analog));
		break;
	case OTC_CONF_COUPLING:
		if (!model)
			return OTC_ERR_ARG;
		*data = g_variant_new_strv(*model->coupling_options, model->num_coupling_options);
		break;
	case OTC_CONF_TRIGGER_SOURCE:
		if (!model)
			return OTC_ERR_ARG;
		*data = g_variant_new_strv(*model->trigger_sources, model->num_trigger_sources);
		break;
	case OTC_CONF_TRIGGER_SLOPE:
		if (!model)
			return OTC_ERR_ARG;
		*data = g_variant_new_strv(*model->trigger_slopes, model->num_trigger_slopes);
		break;
	case OTC_CONF_TIMEBASE:
		if (!model)
			return OTC_ERR_ARG;
		*data = std_gvar_tuple_array(*model->timebases, model->num_timebases);
		break;
	case OTC_CONF_VDIV:
		if (!model)
			return OTC_ERR_ARG;
		*data = std_gvar_tuple_array(*model->vdivs, model->num_vdivs);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

OTC_PRIV int lecroy_xstream_request_data(const struct otc_dev_inst *sdi)
{
	char command[MAX_COMMAND_SIZE];
	struct otc_channel *ch;
	struct dev_context *devc;

	devc = sdi->priv;

	/*
	 * We may be left with an invalid current_channel if acquisition was
	 * already stopped but we are processing the last pending events.
	 */
	if (!devc->current_channel)
		return OTC_ERR_NA;

	ch = devc->current_channel->data;

	if (ch->type != OTC_CHANNEL_ANALOG)
		return OTC_ERR;

	g_snprintf(command, sizeof(command), "C%d:WAVEFORM?", ch->index + 1);
	return otc_scpi_send(sdi->conn, command);
}

static int setup_channels(const struct otc_dev_inst *sdi)
{
	GSList *l;
	char command[MAX_COMMAND_SIZE];
	struct scope_state *state;
	struct otc_channel *ch;
	struct dev_context *devc;
	struct otc_scpi_dev_inst *scpi;

	devc = sdi->priv;
	scpi = sdi->conn;
	state = devc->model_state;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case OTC_CHANNEL_ANALOG:
			if (ch->enabled == state->analog_channels[ch->index].state)
				break;
			g_snprintf(command, sizeof(command), "C%d:TRACE %s",
					ch->index + 1, ch->enabled ? "ON" : "OFF");

			if (otc_scpi_send(scpi, command) != OTC_OK)
				return OTC_ERR;

			state->analog_channels[ch->index].state = ch->enabled;
			break;
		default:
			return OTC_ERR;
		}
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	GSList *l;
	struct otc_channel *ch;
	struct dev_context *devc;
	struct scope_state *state;
	int ret;
	struct otc_scpi_dev_inst *scpi;

	devc = sdi->priv;
	scpi = sdi->conn;

	/* Preset empty results. */
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	state = devc->model_state;
	state->sample_rate = 0;

	/* Contruct the list of enabled channels. */
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (!ch->enabled)
			continue;

		devc->enabled_channels = g_slist_append(devc->enabled_channels, ch);
	}

	if (!devc->enabled_channels)
		return OTC_ERR;

	/* Configure the analog channels. */
	if (setup_channels(sdi) != OTC_OK) {
		otc_err("Failed to setup channel configuration!");
		ret = OTC_ERR;
		goto free_enabled;
	}

	/*
	 * Start acquisition on the first enabled channel. The
	 * receive routine will continue driving the acquisition.
	 */
	otc_scpi_source_add(sdi->session, scpi, G_IO_IN, 50,
			lecroy_xstream_receive_data, (void *)sdi);

	std_session_send_df_header(sdi);

	devc->current_channel = devc->enabled_channels;

	return lecroy_xstream_request_data(sdi);

free_enabled:
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;

	return ret;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_scpi_dev_inst *scpi;

	std_session_send_df_end(sdi);

	devc = sdi->priv;

	devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	scpi = sdi->conn;
	otc_scpi_source_remove(sdi->session, scpi);

	return OTC_OK;
}

static struct otc_dev_driver lecroy_xstream_driver_info = {
	.name = "lecroy-xstream",
	.longname = "LeCroy X-Stream",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_channel_set = config_channel_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(lecroy_xstream_driver_info);
