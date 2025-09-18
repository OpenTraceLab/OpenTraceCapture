/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

#define LOG_PREFIX "input/binary"

#define CHUNK_SIZE           (4 * 1024 * 1024)
#define DEFAULT_NUM_CHANNELS 8
#define DEFAULT_SAMPLERATE   0

struct context {
	gboolean started;
	uint64_t samplerate;
	uint16_t unitsize;
};

static int init(struct otc_input *in, GHashTable *options)
{
	struct context *inc;
	int num_channels, i;
	char name[16];

	num_channels = g_variant_get_int32(g_hash_table_lookup(options, "numchannels"));
	if (num_channels < 1) {
		otc_err("Invalid value for numchannels: must be at least 1.");
		return OTC_ERR_ARG;
	}

	in->sdi = g_malloc0(sizeof(struct otc_dev_inst));
	in->priv = inc = g_malloc0(sizeof(struct context));

	inc->samplerate = g_variant_get_uint64(g_hash_table_lookup(options, "samplerate"));

	for (i = 0; i < num_channels; i++) {
		snprintf(name, sizeof(name), "%d", i);
		otc_channel_new(in->sdi, i, OTC_CHANNEL_LOGIC, TRUE, name);
	}

	inc->unitsize = (g_slist_length(in->sdi->channels) + 7) / 8;

	return OTC_OK;
}

static int process_buffer(struct otc_input *in)
{
	struct otc_datafeed_packet packet;
	struct otc_datafeed_logic logic;
	struct context *inc;
	gsize chunk_size, i;
	int chunk;

	inc = in->priv;
	if (!inc->started) {
		std_session_send_df_header(in->sdi);

		if (inc->samplerate) {
			(void)otc_session_send_meta(in->sdi, OTC_CONF_SAMPLERATE,
				g_variant_new_uint64(inc->samplerate));
		}

		inc->started = TRUE;
	}

	packet.type = OTC_DF_LOGIC;
	packet.payload = &logic;
	logic.unitsize = inc->unitsize;

	/* Cut off at multiple of unitsize. */
	chunk_size = in->buf->len / logic.unitsize * logic.unitsize;

	for (i = 0; i < chunk_size; i += chunk) {
		logic.data = in->buf->str + i;
		chunk = MIN(CHUNK_SIZE, chunk_size - i);
		chunk /= logic.unitsize;
		chunk *= logic.unitsize;
		logic.length = chunk;
		otc_session_send(in->sdi, &packet);
	}
	g_string_erase(in->buf, 0, chunk_size);

	return OTC_OK;
}

static int receive(struct otc_input *in, GString *buf)
{
	int ret;

	g_string_append_len(in->buf, buf->str, buf->len);

	if (!in->sdi_ready) {
		/* sdi is ready, notify frontend. */
		in->sdi_ready = TRUE;
		return OTC_OK;
	}

	ret = process_buffer(in);

	return ret;
}

static int end(struct otc_input *in)
{
	struct context *inc;
	int ret;

	if (in->sdi_ready)
		ret = process_buffer(in);
	else
		ret = OTC_OK;

	inc = in->priv;
	if (inc->started)
		std_session_send_df_end(in->sdi);

	return ret;
}

static int reset(struct otc_input *in)
{
	struct context *inc = in->priv;

	inc->started = FALSE;
	g_string_truncate(in->buf, 0);

	return OTC_OK;
}

static struct otc_option options[] = {
	{ "numchannels", "Number of logic channels", "The number of (logic) channels in the data", NULL, NULL },
	{ "samplerate", "Sample rate (Hz)", "The sample rate of the (logic) data in Hz", NULL, NULL },
	ALL_ZERO
};

static const struct otc_option *get_options(void)
{
	if (!options[0].def) {
		options[0].def = g_variant_ref_sink(g_variant_new_int32(DEFAULT_NUM_CHANNELS));
		options[1].def = g_variant_ref_sink(g_variant_new_uint64(DEFAULT_SAMPLERATE));
	}

	return options;
}

OTC_PRIV struct otc_input_module input_binary = {
	.id = "binary",
	.name = "Binary",
	.desc = "Raw binary logic data",
	.exts = NULL,
	.options = get_options,
	.init = init,
	.receive = receive,
	.end = end,
	.reset = reset,
};
