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

#include <stdio.h>
#include <stdlib.h>
#include <config.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "../../scpi.h"
#include "protocol.h"

size_t samples_sent = 0;

OTC_PRIV int tlf_samplerates_list(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	devc = sdi->priv;

	int32_t sample_rate_min, sample_rate_max, sample_rate_step;

	if (otc_scpi_get_int(sdi->conn, "RATE:MIN?", &sample_rate_min) != OTC_OK) {
		otc_spew("Sent \"RATE:MIN?\", ERROR on response\n");
		return OTC_ERR;
	}

	if (otc_scpi_get_int(sdi->conn, "RATE:MAX?", &sample_rate_max) != OTC_OK) {
		otc_spew("Sent \"RATE:MAX?\", ERROR on response\n");
		return OTC_ERR;
	}

	if (otc_scpi_get_int(sdi->conn, "RATE:STEP?", &sample_rate_step) != OTC_OK) {
		otc_spew("Sent \"RATE:STEP?\", ERROR on response\n");
		return OTC_ERR;
	}

	devc->samplerate_range[0] = (uint64_t)sample_rate_min;
	devc->samplerate_range[1] = (uint64_t)sample_rate_max;
	devc->samplerate_range[2] = (uint64_t)sample_rate_step;

	otc_spew("Sample rate MIN: %d Hz, MAX: %d Hz, STEP: %d Hz\n",
				sample_rate_min, sample_rate_max, sample_rate_step);

	return OTC_OK;
}

OTC_PRIV int tlf_samplerate_set(const struct otc_dev_inst *sdi, uint64_t sample_rate)
{
	struct dev_context *devc;
	devc = sdi->priv;

	if (otc_scpi_send(sdi->conn, "RATE %ld", sample_rate) != OTC_OK) {
		otc_spew("Sent \"RATE %lu\", ERROR on response\n", sample_rate);
		return OTC_ERR;
	}

	devc->cur_samplerate = sample_rate;

	return OTC_OK;
}

OTC_PRIV int tlf_samplerate_get(const struct otc_dev_inst *sdi, uint64_t *sample_rate)
{
	struct dev_context *devc;
	int return_buf;

	devc = sdi->priv;

	if (otc_scpi_get_int(sdi->conn, "RATE?", &return_buf) != OTC_OK) {
		otc_spew("Sent \"RATE?\", ERROR on response\n");
		return OTC_ERR;
	}
	devc->cur_samplerate = (uint64_t)return_buf;
	*sample_rate = (uint64_t)return_buf;

	return OTC_OK;
}

OTC_PRIV int tlf_samples_set(const struct otc_dev_inst *sdi, int32_t samples)
{
	struct dev_context *devc;
	devc = sdi->priv;

	if (otc_scpi_send(sdi->conn, "SAMPles %ld", samples) != OTC_OK) {
		otc_dbg("tlf_samples_set Sent \"SAMPLes %d\", ERROR on response\n", samples);
		return OTC_ERR;
	}
	otc_spew("tlf_samples_set sent \"SAMPLes %d\"", samples);

	devc->cur_samples = samples;

	return OTC_OK;
}

OTC_PRIV int tlf_maxsamples_get(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	uint32_t max_samples_buf;
	devc = sdi->priv;

	if (otc_scpi_get_int(sdi->conn, "SAMPles:MAX?", &max_samples_buf) != OTC_OK) {
		otc_dbg("tlf_samples_get Sent \"SAMPLes?\", ERROR on response\n");
		return OTC_ERR;
	}
	otc_spew("tlf_samples_get Samples = %u", max_samples_buf);

	devc->max_samples = max_samples_buf;

	return OTC_OK;
}

OTC_PRIV int tlf_samples_get(const struct otc_dev_inst *sdi, int32_t *samples)
{
	struct dev_context *devc;
	devc = sdi->priv;

	if (otc_scpi_get_int(sdi->conn, "SAMPles?", samples) != OTC_OK) {
		otc_dbg("tlf_samples_get Sent \"SAMPLes?\", ERROR on response\n");
		return OTC_ERR;
	}
	otc_spew("tlf_samples_get Samples = %d", *samples);

	devc->cur_samples = *samples;

	return OTC_OK;
}

OTC_PRIV int tlf_channel_state_set(const struct otc_dev_inst *sdi, int32_t channel_index, gboolean enabled)
{
	char command[64];
	struct dev_context *devc;

	devc = sdi->priv;

	if ((channel_index < 0) || (channel_index >= devc->channels)) {
		return OTC_ERR;
	}

	if (enabled == TRUE) {
		sprintf(command, "CHANnel%d:STATus %s", channel_index + 1, "ON");
	} else if (enabled == FALSE) {
		sprintf(command, "CHANnel%d:STATus %s", channel_index + 1, "OFF");
	} else {
		return OTC_ERR;
	}

	if (otc_scpi_send(sdi->conn, command) != OTC_OK) {
		return OTC_ERR;
	}

	devc->channel_state[channel_index] = enabled;

	otc_spew("tlf_channel_state_set Channel: %d set ON", channel_index + 1);
	return OTC_OK;
}

OTC_PRIV int tlf_channel_state_get(const struct otc_dev_inst *sdi, int32_t channel_index, gboolean *enabled)
{
	struct dev_context *devc;

	devc = sdi->priv;

	if ((channel_index < 0) || (channel_index >= devc->channels)) {
		return OTC_ERR;
	}

	*enabled = devc->channel_state[channel_index];

	return OTC_OK;
}

OTC_PRIV int tlf_channels_list(struct otc_dev_inst *sdi)
{
	otc_spew("tlf_channels_list 0");

	char *buf;
	char command[25];
	int32_t j;
	int32_t channel_count;
	struct dev_context *devc;
	struct otc_channel_group *cg;
	struct otc_channel *ch;

	devc = sdi->priv;

	otc_spew("tlf_channels_list 1");

	if (otc_scpi_get_int(sdi->conn, "CHANnel:COUNT?", &channel_count) != OTC_OK) {
		otc_dbg("Sent \"CHANnel:COUNT?\", ERROR on response\n");
		return OTC_ERR;
	}

	otc_spew("tlf_channels_list 2");

	if ((channel_count < 0) || (channel_count > TLF_CHANNEL_COUNT_MAX)) {
		otc_spew("Sent \"CHANnel:COUNT?\", received %d", channel_count);
		otc_dbg("ERROR: Out of channel range between 0 and %d (TLF_CHANNEL_COUNT_MAX)", TLF_CHANNEL_COUNT_MAX);
		return OTC_ERR;
	}

	otc_spew("channel_count = %d", channel_count);
	devc->channels = channel_count;

	otc_spew("tlf_channels_list 3");

	for (int i = 0; i < channel_count; i++) {
		sprintf(command, "CHANnel%d:NAME?", i + 1);
		if (otc_scpi_get_string(sdi->conn, command, &buf) != OTC_OK) {
			otc_dbg("Sent \"%s\", ERROR on response\n", command);
			return OTC_ERR;
		}
		otc_spew("send: %s, chan #: %d, channel name: %s", command, i + 1, buf);
		if (strlen(buf) > TLF_CHANNEL_CHAR_MAX) {
			buf[TLF_CHANNEL_CHAR_MAX] = '\0';
		}
		strcpy(devc->chan_names[i], buf);
	}

	if (channel_count < TLF_CHANNEL_COUNT_MAX) {
		for (int i = channel_count; i < TLF_CHANNEL_COUNT_MAX; i++) {
			strcpy(devc->chan_names[i], "");
		}
	}

	for (int i = 0; i < TLF_CHANNEL_COUNT_MAX; i++) {
		otc_spew("Channel index: %d Channel name: %s", i, devc->chan_names[i]);
	}

	otc_dbg("Setting all channels on, configuring channels");

	cg = g_malloc0(sizeof(struct otc_channel_group));
	cg->name = g_strdup("Logic");

	for (j = 0; j < channel_count; j++) {
		if (tlf_channel_state_set(sdi, j, TRUE) != OTC_OK) {
			return OTC_ERR;
		}
		otc_spew("Adding channel %d: %s", j, devc->chan_names[j]);
		ch = otc_channel_new(sdi, j, OTC_CHANNEL_LOGIC, TRUE, devc->chan_names[j]);
		cg->channels = g_slist_append(cg->channels, ch);
	}

	sdi->channel_groups = g_slist_append(NULL, cg);

	return OTC_OK;
}

OTC_PRIV int tlf_trigger_list(const struct otc_dev_inst *sdi)
{
	char *buf;
	char command[25];
	char *token;
	int32_t trigger_option_count;
	struct dev_context *devc;

	devc = sdi->priv;

	sprintf(command, "TRIGger:OPTions?");
	if (otc_scpi_get_string(sdi->conn, command, &buf) != OTC_OK) {
		return OTC_ERR;
	}
	otc_spew("send: %s, TRIGGER options: %s", command, buf);

	trigger_option_count = 0;
	token = strtok(buf, ",");

	for (size_t i = 0; i < devc->trigger_matches_count; i++) {
		devc->trigger_matches[i] = 0;
	}

	while (token != NULL) {
		if (!g_ascii_strcasecmp(token, "0")) {
			devc->trigger_matches[trigger_option_count] = OTC_TRIGGER_ZERO;
			trigger_option_count++;
			otc_spew("Trigger token: %s, Accept ZERO trigger", token);
		} else if (!g_ascii_strcasecmp(token, "1")) {
			devc->trigger_matches[trigger_option_count] = OTC_TRIGGER_ONE;
			trigger_option_count++;
			otc_spew("Trigger token: %s, Accept ONE trigger", token);
		} else if (!g_ascii_strcasecmp(token, "R")) {
			devc->trigger_matches[trigger_option_count] = OTC_TRIGGER_RISING;
			trigger_option_count++;
			otc_spew("Trigger token: %s, Accept RISING trigger", token);
		} else if (!g_ascii_strcasecmp(token, "F")) {
			devc->trigger_matches[trigger_option_count] = OTC_TRIGGER_FALLING;
			trigger_option_count++;
			otc_spew("Trigger token: %s, Accept FALLING trigger", token);
		} else if (!g_ascii_strcasecmp(token, "E")) {
			devc->trigger_matches[trigger_option_count] = OTC_TRIGGER_EDGE;
			trigger_option_count++;
			otc_spew("Trigger token: %s, Accept EDGE trigger", token);
		} else if (!g_ascii_strcasecmp(token, "X")) {
		} else {
			otc_spew("Error on token: %s", token);
			return OTC_ERR;
		}

		token = strtok(NULL, ",");
	}

	return OTC_OK;
}

OTC_PRIV int tlf_RLE_mode_get(const struct otc_dev_inst *sdi)
{
	char *buf;
	char command[25];
	struct dev_context *devc;
	devc = sdi->priv;

	sprintf(command, "MODE?");
	if (otc_scpi_get_string(sdi->conn, command, &buf) != OTC_OK) {
		return OTC_ERR;
	}
	otc_spew("send: %s, Response: %s", command, buf);

	if (!g_ascii_strcasecmp(buf, "RLE")) {
		devc->RLE_mode = TRUE;
		otc_spew("Mode is Run Length Encoded (RLE)");
		return OTC_OK;
	}

	if (!g_ascii_strcasecmp(buf, "CLOCK")) {
		devc->RLE_mode = FALSE;
		otc_spew("Mode is Clock sampling (CLOCK)");
		return OTC_OK;
	}

	return OTC_ERR;
}

OTC_PRIV int tlf_exec_run(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;

	if (!(devc = sdi->priv)) {
		return OTC_ERR;
	}

	devc->measured_samples = 0;
	samples_sent = 0;
	otc_spew("reset devc->measured_samples, samples_sent");

	return otc_scpi_send(sdi->conn, "RUN");
}

OTC_PRIV int tlf_exec_stop(const struct otc_dev_inst *sdi)
{
	return otc_scpi_send(sdi->conn, "STOP");
}

OTC_PRIV int tlf_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	int chunk_len;
	static GArray *data = NULL;

	struct otc_datafeed_packet packet;
	struct otc_datafeed_logic logic;

	unsigned int rle_buffer_size = 12;
	uint32_t timestamp;
	uint16_t value;

	(void)revents;
	(void)fd;

	otc_spew("---> Entering tlf_receive_data");

	if (!(sdi = cb_data))
		return TRUE;
	otc_spew("---> tlf_receive_data - A");

	if (!(devc = sdi->priv))
		return TRUE;

	otc_spew("---> tlf_receive_data - B");
	if (!devc->data_pending)
		return TRUE;

	otc_spew("---> tlf_receive_data - ask for DATA?");
	if (otc_scpi_send(sdi->conn, "DATA?") != OTC_OK) {
		otc_spew("---> tlf_receive_data - going to close");
		goto close;
	}

	if (!data) {
		otc_spew("---> tlf_receive_data -> read_begin A");

		if (otc_scpi_read_begin(sdi->conn) == OTC_OK) {
			data = g_array_sized_new(FALSE, FALSE, sizeof(uint8_t), 32);
			otc_spew("read_begin");
		} else
			return TRUE;
	} else {
		otc_spew("---> tlf_receive_data -> read_begin 2");
		otc_scpi_read_begin(sdi->conn);
	}

	otc_spew("get a chunk");

	chunk_len = otc_scpi_read_data(sdi->conn, devc->receive_buffer, RECEIVE_BUFFER_SIZE);
	if (chunk_len < 0) {
		otc_dbg("Finished reading data, chunk_len: %d", chunk_len);
		goto close;
	}

	otc_spew("Received data, chunk_len: %d", chunk_len);

	packet.type = OTC_DF_LOGIC;
	packet.payload = &logic;
	logic.unitsize = 2;

	if (devc->RLE_mode) {
		for (int i = 0; i < chunk_len; i = i + 4) {
			timestamp = (((uint8_t)devc->receive_buffer[i + 1]) << 8) | ((uint8_t)devc->receive_buffer[i]);
			value = (((uint8_t)devc->receive_buffer[i + 3]) << 8) | ((uint8_t)devc->receive_buffer[i + 2]);
		}

		if (devc->measured_samples == 0) {
			devc->raw_sample_buf = g_try_malloc(rle_buffer_size * 2);
			if (!devc->raw_sample_buf) {
				otc_err("Sample buffer malloc failed.");
				return FALSE;
			}
		}

		logic.data = devc->raw_sample_buf;

		for (int i = 0; i < chunk_len; i = i + 4) {
			timestamp = (((uint8_t)devc->receive_buffer[i + 1]) << 8) | ((uint8_t)devc->receive_buffer[i]);
			value = (((uint8_t)devc->receive_buffer[i + 3]) << 8) | ((uint8_t)devc->receive_buffer[i + 2]);
			samples_sent++;

			otc_spew("devc->measured_samples: %zu", devc->measured_samples);

			for (int32_t tick = devc->last_timestamp; tick < (int32_t)timestamp; tick++) {
				((uint16_t *)devc->raw_sample_buf)[devc->pending_samples] = devc->last_sample;
				devc->measured_samples++;
				devc->pending_samples++;

				if (devc->pending_samples == rle_buffer_size) {
					logic.length = devc->pending_samples * 2;
					if (logic.data == NULL) {
						otc_spew("tlf_receive_data: payload is NULL");
					}
					otc_session_send(sdi, &packet);

					devc->pending_samples = 0;
				}
			}

			devc->last_sample = value;
			if (timestamp == 65535) {
				devc->last_timestamp = -1;
			} else {
				devc->last_timestamp = timestamp;
			}
		}

		otc_spew("About to flush...");
		if (devc->pending_samples > 0) {
			logic.length = devc->pending_samples * 2;
			otc_session_send(sdi, &packet);
		}

		otc_spew("Finished flush.");
	} else {
		for (int i = 0; i < chunk_len * 2; i = i + 2) {
			devc->raw_sample_buf = g_try_malloc(chunk_len * 4);
			if (!devc->raw_sample_buf) {
				otc_err("Sample buffer malloc failed.");
				return FALSE;
			}
			devc->raw_sample_buf[i] = (uint16_t)(((uint8_t)devc->receive_buffer[i + 1]) << 8) | ((uint8_t)devc->receive_buffer[i]);
		}
		logic.data = devc->raw_sample_buf;
		logic.length = chunk_len * 2;

		otc_session_send(sdi, &packet);
	}

	otc_warn("Sent samples %zu", samples_sent);

	if (samples_sent >= devc->cur_samples) {
		goto close;
	}

	otc_spew("<- returning from tlf_receive_data");

	return TRUE;

	otc_spew("freeing data");
	g_array_free(data, TRUE);
	data = NULL;
	return TRUE;

close:
	if (data) {
		std_session_send_df_frame_end(sdi);
		std_session_send_df_end(sdi);
		otc_dbg("read is complete");
		devc->data_pending = FALSE;

		g_array_free(data, TRUE);
		data = NULL;
		otc_dev_acquisition_stop(sdi);
	}

	return FALSE;
}
