/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2024 Daniel Anselmi <danselmi@gmx.ch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"

OTC_PRIV int aim_tti_dps_set_value(struct otc_scpi_dev_inst *scpi,
					struct dev_context *devc, int param, size_t channel)
{
	int ret;

	switch (param) {
	default:
	case AIM_TTI_CURRENT:
	case AIM_TTI_VOLTAGE:
	case AIM_TTI_STATUS:
		otc_err("Read only parameter %d.", param);
		return OTC_ERR;
	case AIM_TTI_CURRENT_LIMIT:
		ret = otc_scpi_send(scpi, "I%1zu %01.2f", channel+1,
					devc->config[channel].current_limit);
		break;
	case AIM_TTI_VOLTAGE_TARGET:
		ret = otc_scpi_send(scpi, "V%1zu %01.2f", channel+1,
					devc->config[channel].voltage_target);
		break;
	case AIM_TTI_OUTPUT_ENABLE:
		ret = otc_scpi_send(scpi, "OP%1zu %d", channel+1,
					(devc->config[channel].output_enabled) ? 1 : 0);
		break;
	case AIM_TTI_OCP_THRESHOLD:
		ret = otc_scpi_send(scpi, "OCP%1zu %01.2f", channel+1,
					devc->config[channel].over_current_protection_threshold);
		break;
	case AIM_TTI_OVP_THRESHOLD:
		ret = otc_scpi_send(scpi, "OVP%1zu %01.2f", channel+1,
					devc->config[channel].over_voltage_protection_threshold);
		break;
	case AIM_TTI_OUTPUT_ENABLE_ALL:
		if (devc->config[0].output_enabled) {
			ret = otc_scpi_send(scpi, "OPALL 1");
		} else {
			ret = otc_scpi_send(scpi, "OPALL 0");
		}
		break;
	case AIM_TTI_TRACKING_ENABLE:
		if (devc->tracking_enabled) {
			ret = otc_scpi_send(scpi, "CONFIG 0");
		} else {
			ret = otc_scpi_send(scpi, "CONFIG 2");
		}
		break;
	}

	return ret;
}

OTC_PRIV int aim_tti_dps_get_value(struct otc_scpi_dev_inst *scpi,
					struct dev_context *devc, int param, size_t channel)
{
	int ret, status_byte, new_mode;
	char *response;
	float val;
	gboolean bval;

	switch (param) {
	case AIM_TTI_VOLTAGE:
		ret = otc_scpi_send(scpi, "V%1zuO?", channel+1);
		break;
	case AIM_TTI_CURRENT:
		ret = otc_scpi_send(scpi, "I%1zuO?", channel+1);
		break;
	case AIM_TTI_VOLTAGE_TARGET:
		ret = otc_scpi_send(scpi, "V%1zu?", channel+1);
		break;
	case AIM_TTI_CURRENT_LIMIT:
		ret = otc_scpi_send(scpi, "I%1zu?", channel+1);
		break;
	case AIM_TTI_OUTPUT_ENABLE:
		ret = otc_scpi_send(scpi, "OP%1zu?", channel+1);
		break;
	case AIM_TTI_OCP_THRESHOLD:
		ret = otc_scpi_send(scpi, "OCP%1zu?", channel+1);
		break;
	case AIM_TTI_OVP_THRESHOLD:
		ret = otc_scpi_send(scpi, "OVP%1zu?", channel+1);
		break;
	case AIM_TTI_STATUS:
		ret = otc_scpi_send(scpi, "LSR%1zu?", channel+1);
		break;
	case AIM_TTI_TRACKING_ENABLE:
		ret = otc_scpi_send(scpi, "CONFIG?");
		break;
	default:
		otc_err("Don't know how to query %d.", param);
		return OTC_ERR;
	}
	if (ret != OTC_OK)
		return ret;

	ret = otc_scpi_get_string(scpi, NULL, &response);
	if (ret != OTC_OK || !response)
		return OTC_ERR;

	switch (param) {
	case AIM_TTI_VOLTAGE:
		val = atof(&(response[0]));
		devc->config[channel].actual_voltage = val;
		break;
	case AIM_TTI_CURRENT:
		val = atof(&(response[0]));
		devc->config[channel].actual_current = val;
		break;
	case AIM_TTI_VOLTAGE_TARGET:
		val = atof(&(response[3]));
		devc->config[channel].voltage_target = val;
		break;
	case AIM_TTI_CURRENT_LIMIT:
		val = atof(&(response[3]));
		devc->config[channel].current_limit = val;
		break;
	case AIM_TTI_OUTPUT_ENABLE:
		devc->config[channel].output_enabled =
			(response[0] == '1');
		break;
	case AIM_TTI_OCP_THRESHOLD:
		val = atof(&(response[4]));
		devc->config[channel].over_current_protection_threshold = val;
		break;
	case AIM_TTI_OVP_THRESHOLD:
		val = atof(&(response[4]));
		devc->config[channel].over_voltage_protection_threshold = val;
		break;
	case AIM_TTI_STATUS:
		status_byte = atoi(&(response[0]));

		new_mode = AIM_TTI_CV;
		if (status_byte & 0x02) {
			new_mode = AIM_TTI_CC;
		} else if (status_byte & 0x10) {
			new_mode = AIM_TTI_UR;
		}
		if (devc->config[channel].mode != new_mode)
			devc->config[channel].mode_changed = TRUE;
		devc->config[channel].mode = new_mode;

		bval = ((status_byte & 0x04) != 0);
		if (devc->config[channel].ovp_active != bval)
			devc->config[channel].ovp_active_changed = TRUE;
		devc->config[channel].ovp_active = bval;

		bval = ((status_byte & 0x08) != 0);
		if (devc->config[channel].ocp_active != bval)
			devc->config[channel].ocp_active_changed = TRUE;
		devc->config[channel].ocp_active = bval;
		break;
	case AIM_TTI_TRACKING_ENABLE:
		devc->tracking_enabled = response[0] == '0';
		break;
	default:
		otc_err("Don't know how to query %d.", param);
		return OTC_ERR;
	}

	return ret;
}

OTC_PRIV int aim_tti_dps_sync_state(struct otc_scpi_dev_inst *scpi,
									struct dev_context *devc)
{
	int ret, channel, param;

	ret = OTC_OK;

	for (channel = 0; channel < devc->model_config->channels; ++channel) {
		for (param = AIM_TTI_VOLTAGE;
			param < AIM_TTI_LAST_CHANNEL_PARAM && ret >= OTC_OK;
			++param) {
			ret = aim_tti_dps_get_value(scpi, devc, param, channel);
		}
		devc->config[channel].mode_changed = TRUE;
		devc->config[channel].ocp_active_changed = TRUE;
		devc->config[channel].ovp_active_changed = TRUE;
	}

	if (ret == OTC_OK)
		ret = aim_tti_dps_get_value(scpi, devc, AIM_TTI_TRACKING_ENABLE, 0);

	devc->acquisition_param = AIM_TTI_VOLTAGE;
	devc->acquisition_channel = 0;

	return ret;
}

OTC_PRIV void aim_tti_dps_next_acquisition(struct dev_context *devc)
{
	if (devc->acquisition_param == AIM_TTI_VOLTAGE) {
		devc->acquisition_param = AIM_TTI_CURRENT;
	} else if (devc->acquisition_param == AIM_TTI_CURRENT) {
		devc->acquisition_param = AIM_TTI_STATUS;
	} else if (devc->acquisition_param == AIM_TTI_STATUS) {
		devc->acquisition_param = AIM_TTI_VOLTAGE;
		if (devc->acquisition_channel < 0) {
			devc->acquisition_channel = 0;
		} else {
			devc->acquisition_channel++;
			if (devc->acquisition_channel >= devc->model_config->channels)
				devc->acquisition_channel = 0;
		}
	} else {
		devc->acquisition_param = AIM_TTI_VOLTAGE;
		devc->acquisition_channel = 0;
	}
}

OTC_PRIV int aim_tti_dps_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	GSList *l;
	int channel;
	GVariant *data;

	(void)fd;
	(void)revents;

	if (!cb_data)
		return TRUE;
	sdi = cb_data;

	if (!sdi->priv || !sdi->conn)
		return TRUE;
	devc = sdi->priv;
	scpi = sdi->conn;

	aim_tti_dps_get_value(scpi, devc, devc->acquisition_param,
						devc->acquisition_channel);

	otc_analog_init(&analog, &encoding, &meaning, &spec, 0);

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	analog.num_samples = 1;

	channel = devc->acquisition_channel;
	l = g_slist_copy(sdi->channels);
	if (devc->acquisition_param == AIM_TTI_VOLTAGE) {
		analog.meaning->channels = g_slist_nth(l, 2*channel);
		l = g_slist_remove_link(l, analog.meaning->channels);
		analog.meaning->mq = OTC_MQ_VOLTAGE;
		analog.meaning->unit = OTC_UNIT_VOLT;
		analog.meaning->mqflags = OTC_MQFLAG_DC;
		analog.encoding->digits = 2;
		analog.spec->spec_digits = 2;
		analog.data = &devc->config[channel].actual_voltage;
		otc_session_send(sdi, &packet);
	} else if (devc->acquisition_param == AIM_TTI_CURRENT) {
		analog.meaning->channels = g_slist_nth(l, 2*channel+1);
		l = g_slist_remove_link(l, analog.meaning->channels);
		analog.meaning->mq = OTC_MQ_CURRENT;
		analog.meaning->unit = OTC_UNIT_AMPERE;
		analog.meaning->mqflags = OTC_MQFLAG_DC;
		analog.encoding->digits = 3;
		analog.spec->spec_digits = 3;
		analog.data = &devc->config[channel].actual_current;
		otc_session_send(sdi, &packet);
		if (devc->acquisition_channel + 1 == devc->model_config->channels)
			otc_sw_limits_update_samples_read(&devc->limits, 1);
	} else if (devc->acquisition_param == AIM_TTI_STATUS) {
		if (devc->config[channel].mode_changed) {
			if (devc->config[channel].output_enabled == FALSE) {
				data = g_variant_new_string("");
			} else if (devc->config[channel].mode == AIM_TTI_CC) {
				data = g_variant_new_string("CC");
			} else if (devc->config[channel].mode == AIM_TTI_CV) {
				data = g_variant_new_string("CV");
			} else {
				data = g_variant_new_string("UR");
			}
			otc_session_send_meta(sdi, OTC_CONF_REGULATION, data);
			devc->config[channel].mode_changed = FALSE;
		}
		if (devc->config[channel].ocp_active_changed) {
			otc_session_send_meta(sdi, OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE,
				g_variant_new_boolean(devc->config[channel].ocp_active));
			devc->config[channel].ocp_active_changed = FALSE;
		}
		if (devc->config[channel].ovp_active_changed) {
			otc_session_send_meta(sdi, OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE,
				g_variant_new_boolean(devc->config[channel].ovp_active));
			devc->config[channel].ovp_active_changed = FALSE;
		}
	}
	g_slist_free(l);

	aim_tti_dps_next_acquisition(devc);

	if (otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop(sdi);

	return TRUE;
}
