/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 abraxa (Soeren Apel) <soeren@apelpie.net>
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
#include "protocol_wrappers.h"

#define MAX_COMMAND_SIZE 64

/*
 * DLM2000 comm spec:
 * https://www.yokogawa.com/pdf/provide/E/GW/IM/0000022842/0/IM710105-17E.pdf
 */

int dlm_timebase_get(struct otc_scpi_dev_inst *scpi,
		gchar **response)
{
	return otc_scpi_get_string(scpi, ":TIMEBASE:TDIV?", response);
}

int dlm_timebase_set(struct otc_scpi_dev_inst *scpi,
		const gchar *value)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":TIMEBASE:TDIV %s", value);
	return otc_scpi_send(scpi, cmd);
}

int dlm_horiz_trigger_pos_get(struct otc_scpi_dev_inst *scpi,
		float *response)
{
	return otc_scpi_get_float(scpi, ":TRIGGER:DELAY:TIME?", response);
}

int dlm_horiz_trigger_pos_set(struct otc_scpi_dev_inst *scpi,
		const gchar *value)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":TRIGGER:DELAY:TIME %s", value);
	return otc_scpi_send(scpi, cmd);
}

int dlm_trigger_source_get(struct otc_scpi_dev_inst *scpi,
		gchar **response)
{
	return otc_scpi_get_string(scpi, ":TRIGGER:ATRIGGER:SIMPLE:SOURCE?", response);
}

int dlm_trigger_source_set(struct otc_scpi_dev_inst *scpi,
		const gchar *value)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":TRIGGER:ATRIGGER:SIMPLE:SOURCE %s", value);
	return otc_scpi_send(scpi, cmd);
}

int dlm_trigger_slope_get(struct otc_scpi_dev_inst *scpi,
		int *response)
{
	gchar *resp;
	int result;

	result = OTC_ERR;

	if (otc_scpi_get_string(scpi, ":TRIGGER:ATRIGGER:SIMPLE:SLOPE?", &resp) != OTC_OK) {
		g_free(resp);
		return OTC_ERR;
	}

	if (strcmp("RISE", resp) == 0) {
		*response = SLOPE_POSITIVE;
		result = OTC_OK;
	}

	if (strcmp("FALL", resp) == 0) {
		*response = SLOPE_NEGATIVE;
		result = OTC_OK;
	}

	g_free(resp);

	return result;
}

int dlm_trigger_slope_set(struct otc_scpi_dev_inst *scpi,
		const int value)
{
	if (value == SLOPE_POSITIVE)
		return otc_scpi_send(scpi, ":TRIGGER:ATRIGGER:SIMPLE:SLOPE RISE");

	if (value == SLOPE_NEGATIVE)
		return otc_scpi_send(scpi, ":TRIGGER:ATRIGGER:SIMPLE:SLOPE FALL");

	return OTC_ERR_ARG;
}

int dlm_analog_chan_state_get(struct otc_scpi_dev_inst *scpi, int channel,
		gboolean *response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:DISPLAY?", channel);
	return otc_scpi_get_bool(scpi, cmd, response);
}

int dlm_analog_chan_state_set(struct otc_scpi_dev_inst *scpi, int channel,
		const gboolean value)
{
	gchar cmd[MAX_COMMAND_SIZE];

	if (value)
		g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:DISPLAY ON", channel);
	else
		g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:DISPLAY OFF", channel);

	return otc_scpi_send(scpi, cmd);
}

int dlm_analog_chan_vdiv_get(struct otc_scpi_dev_inst *scpi, int channel,
		gchar **response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:VDIV?", channel);
	return otc_scpi_get_string(scpi, cmd, response);
}

int dlm_analog_chan_vdiv_set(struct otc_scpi_dev_inst *scpi, int channel,
		const gchar *value)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:VDIV %s", channel, value);
	return otc_scpi_send(scpi, cmd);
}

int dlm_analog_chan_voffs_get(struct otc_scpi_dev_inst *scpi, int channel,
		float *response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:POSITION?", channel);
	return otc_scpi_get_float(scpi, cmd, response);
}

int dlm_analog_chan_srate_get(struct otc_scpi_dev_inst *scpi, int channel,
		float *response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:TRACE %d", channel);

	if (otc_scpi_send(scpi, cmd) != OTC_OK)
		return OTC_ERR;

	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:RECORD 0");
	if (otc_scpi_send(scpi, cmd) != OTC_OK)
		return OTC_ERR;

	return otc_scpi_get_float(scpi, ":WAVEFORM:SRATE?", response);
}

int dlm_analog_chan_coupl_get(struct otc_scpi_dev_inst *scpi, int channel,
		gchar **response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:COUPLING?", channel);
	return otc_scpi_get_string(scpi, cmd, response);
}

int dlm_analog_chan_coupl_set(struct otc_scpi_dev_inst *scpi, int channel,
		const gchar *value)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":CHANNEL%d:COUPLING %s", channel, value);
	return otc_scpi_send(scpi, cmd);
}

int dlm_analog_chan_wrange_get(struct otc_scpi_dev_inst *scpi, int channel,
		float *response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	int result;

	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:TRACE %d", channel);
	result = otc_scpi_send(scpi, cmd);
	result &= otc_scpi_get_float(scpi, ":WAVEFORM:RANGE?", response);
	return result;
}

int dlm_analog_chan_woffs_get(struct otc_scpi_dev_inst *scpi, int channel,
		float *response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	int result;

	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:TRACE %d", channel);
	result = otc_scpi_send(scpi, cmd);
	result &= otc_scpi_get_float(scpi, ":WAVEFORM:OFFSET?", response);
	return result;
}

int dlm_digital_chan_state_get(struct otc_scpi_dev_inst *scpi, int channel,
		gboolean *response)
{
	gchar cmd[MAX_COMMAND_SIZE];
	g_snprintf(cmd, sizeof(cmd), ":LOGIC:PODA:BIT%d:DISPLAY?", channel);
	return otc_scpi_get_bool(scpi, cmd, response);
}

int dlm_digital_chan_state_set(struct otc_scpi_dev_inst *scpi, int channel,
		const gboolean value)
{
	gchar cmd[MAX_COMMAND_SIZE];

	if (value)
		g_snprintf(cmd, sizeof(cmd), ":LOGIC:PODA:BIT%d:DISPLAY ON", channel);
	else
		g_snprintf(cmd, sizeof(cmd), ":LOGIC:PODA:BIT%d:DISPLAY OFF", channel);

	return otc_scpi_send(scpi, cmd);
}

int dlm_digital_pod_state_get(struct otc_scpi_dev_inst *scpi, int pod,
		gboolean *response)
{
	gchar cmd[MAX_COMMAND_SIZE];

	/* TODO: pod currently ignored as DLM2000 only has pod A. */
	(void)pod;

	g_snprintf(cmd, sizeof(cmd), ":LOGIC:MODE?");
	return otc_scpi_get_bool(scpi, cmd, response);
}

int dlm_digital_pod_state_set(struct otc_scpi_dev_inst *scpi, int pod,
		const gboolean value)
{
	/* TODO: pod currently ignored as DLM2000 only has pod A. */
	(void)pod;

	if (value)
		return otc_scpi_send(scpi, ":LOGIC:MODE ON");
	else
		return otc_scpi_send(scpi, ":LOGIC:MODE OFF");
}

int dlm_response_headers_set(struct otc_scpi_dev_inst *scpi,
		const gboolean value)
{
	if (value)
		return otc_scpi_send(scpi, ":COMMUNICATE:HEADER ON");
	else
		return otc_scpi_send(scpi, ":COMMUNICATE:HEADER OFF");
}

int dlm_acquisition_stop(struct otc_scpi_dev_inst *scpi)
{
	return otc_scpi_send(scpi, ":STOP");
}

int dlm_acq_length_get(struct otc_scpi_dev_inst *scpi,
		uint32_t *response)
{
	int ret;
	char *s;
	long tmp;

	if (otc_scpi_get_string(scpi, ":WAVEFORM:LENGTH?", &s) != OTC_OK)
		if (!s)
			return OTC_ERR;

	if (otc_atol(s, &tmp) == OTC_OK)
		ret = OTC_OK;
	else
		ret = OTC_ERR;

	g_free(s);
	*response = tmp;

	return ret;
}

int dlm_chunks_per_acq_get(struct otc_scpi_dev_inst *scpi, int *response)
{
	int result, acq_len;

	/*
	 * Data retrieval queries such as :WAVEFORM:SEND? will only return
	 * up to 12500 samples at a time. If the oscilloscope operates in a
	 * mode where more than 12500 samples fit on screen (i.e. in one
	 * acquisition), data needs to be retrieved multiple times.
	 */

	result = otc_scpi_get_int(scpi, ":WAVEFORM:LENGTH?", &acq_len);
	*response = MAX(acq_len / DLM_MAX_FRAME_LENGTH, 1);

	return result;
}

int dlm_start_frame_set(struct otc_scpi_dev_inst *scpi, int value)
{
	gchar cmd[MAX_COMMAND_SIZE];

	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:START %d",
			value * DLM_MAX_FRAME_LENGTH);

	return otc_scpi_send(scpi, cmd);
}

int dlm_data_get(struct otc_scpi_dev_inst *scpi, int acquisition_num)
{
	gchar cmd[MAX_COMMAND_SIZE];

	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:ALL:SEND? %d", acquisition_num);
	return otc_scpi_send(scpi, cmd);
}

int dlm_analog_data_get(struct otc_scpi_dev_inst *scpi, int channel)
{
	gchar cmd[MAX_COMMAND_SIZE];
	int result;

	result = otc_scpi_send(scpi, ":WAVEFORM:FORMAT BYTE");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:RECORD 0");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:START 0");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:END 124999999");

	g_snprintf(cmd, sizeof(cmd), ":WAVEFORM:TRACE %d", channel);
	if (result == OTC_OK) result = otc_scpi_send(scpi, cmd);

	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:SEND? 1");

	return result;
}

int dlm_digital_data_get(struct otc_scpi_dev_inst *scpi)
{
	int result;

	result = otc_scpi_send(scpi, ":WAVEFORM:FORMAT BYTE");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:RECORD 0");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:START 0");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:END 124999999");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:TRACE LOGIC");
	if (result == OTC_OK) result = otc_scpi_send(scpi, ":WAVEFORM:SEND? 1");

	return result;
}
