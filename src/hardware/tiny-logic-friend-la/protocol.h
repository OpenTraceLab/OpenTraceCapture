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

#ifndef LIBOPENTRACECAPTURE_HARDWARE_TINY_LOGIC_FRIEND_LA_PROTOCOL_H
#define LIBOPENTRACECAPTURE_HARDWARE_TINY_LOGIC_FRIEND_LA_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"

#define LOG_PREFIX "tiny-logic-friend-la"

#define TLF_CHANNEL_COUNT_MAX 16
#define TLF_CHANNEL_CHAR_MAX 6

#define TRIGGER_MATCHES_COUNT 5

#define RECEIVE_BUFFER_SIZE 4096

struct dev_context {
	int channels;
	char chan_names[TLF_CHANNEL_COUNT_MAX][TLF_CHANNEL_COUNT_MAX + 1];

	uint64_t samplerate_range[3];
	uint64_t cur_samplerate;

	uint32_t max_samples;
	uint32_t cur_samples;

	int32_t trigger_matches[TRIGGER_MATCHES_COUNT];
	size_t trigger_matches_count;
	gboolean channel_state[TLF_CHANNEL_COUNT_MAX];

	char receive_buffer[RECEIVE_BUFFER_SIZE];
	gboolean data_pending;

	size_t measured_samples;
	size_t pending_samples;
	size_t num_samples;

	uint16_t last_sample;
	int32_t last_timestamp;

	uint16_t *raw_sample_buf;

	int RLE_mode;
};

OTC_PRIV int tlf_samplerates_list(const struct otc_dev_inst *sdi);
OTC_PRIV int tlf_samplerate_set(const struct otc_dev_inst *sdi, uint64_t sample_rate);
OTC_PRIV int tlf_samplerate_get(const struct otc_dev_inst *sdi, uint64_t *sample_rate);

OTC_PRIV int tlf_samples_set(const struct otc_dev_inst *sdi, int32_t samples);
OTC_PRIV int tlf_samples_get(const struct otc_dev_inst *sdi, int32_t *samples);
OTC_PRIV int tlf_maxsamples_get(const struct otc_dev_inst *sdi);

OTC_PRIV int tlf_channel_state_set(const struct otc_dev_inst *sdi, int32_t channel_index, gboolean enabled);
OTC_PRIV int tlf_channel_state_get(const struct otc_dev_inst *sdi, int32_t channel_index, gboolean *enabled);
OTC_PRIV int tlf_channels_list(struct otc_dev_inst *sdi);

OTC_PRIV int tlf_trigger_list(const struct otc_dev_inst *sdi);
OTC_PRIV int tlf_trigger_set(const struct otc_dev_inst *sdi, int32_t channel_index, char *trigger);

OTC_PRIV int tlf_RLE_mode_get(const struct otc_dev_inst *sdi);

OTC_PRIV int tlf_exec_run(const struct otc_dev_inst *sdi);
OTC_PRIV int tlf_exec_stop(const struct otc_dev_inst *sdi);

OTC_PRIV int tlf_receive_data(int fd, int revents, void *cb_data);

#endif
