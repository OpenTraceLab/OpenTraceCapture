/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

#define LOG_PREFIX "transform/scale"

struct context {
	struct otc_rational factor;
};

static int init(struct otc_transform *t, GHashTable *options)
{
	struct context *ctx;

	if (!t || !t->sdi || !options)
		return OTC_ERR_ARG;

	t->priv = ctx = g_malloc0(sizeof(struct context));

	g_variant_get(g_hash_table_lookup(options, "factor"), "(xt)",
			&ctx->factor.p, &ctx->factor.q);

	return OTC_OK;
}

static int receive(const struct otc_transform *t,
		struct otc_datafeed_packet *packet_in,
		struct otc_datafeed_packet **packet_out)
{
	struct context *ctx;
	const struct otc_datafeed_analog *analog;

	if (!t || !t->sdi || !packet_in || !packet_out)
		return OTC_ERR_ARG;
	ctx = t->priv;

	switch (packet_in->type) {
	case OTC_DF_ANALOG:
		analog = packet_in->payload;
		analog->encoding->scale.p *= ctx->factor.p;
		analog->encoding->scale.q *= ctx->factor.q;
		break;
	default:
		otc_spew("Unsupported packet type %d, ignoring.", packet_in->type);
		break;
	}

	/* Return the in-place-modified packet. */
	*packet_out = packet_in;

	return OTC_OK;
}

static int cleanup(struct otc_transform *t)
{
	struct context *ctx;

	if (!t || !t->sdi)
		return OTC_ERR_ARG;
	ctx = t->priv;

	g_free(ctx);
	t->priv = NULL;

	return OTC_OK;
}

static struct otc_option options[] = {
	{ "factor", "Factor", "Factor by which to scale the analog values", NULL, NULL },
	ALL_ZERO
};

static const struct otc_option *get_options(void)
{
	int64_t p = 1;
	uint64_t q = 1;

	/* Default to a scaling factor of 1.0. */
	if (!options[0].def)
		options[0].def = g_variant_ref_sink(g_variant_new("(xt)", &p, &q));

	return options;
}

OTC_PRIV struct otc_transform_module transform_scale = {
	.id = "scale",
	.name = "Scale",
	.desc = "Scale analog values by a specified factor",
	.options = get_options,
	.init = init,
	.receive = receive,
	.cleanup = cleanup,
};
