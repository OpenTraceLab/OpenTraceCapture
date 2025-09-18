/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2015 Uwe Hermann <uwe@hermann-uwe.de>
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
#include "../libopentracecapture-internal.h"

/** @cond PRIVATE */
#define LOG_PREFIX "transform"
/** @endcond */

/**
 * @file
 *
 * Transform module handling.
 */

/**
 * @defgroup grp_transform Transform modules
 *
 * Transform module handling.
 *
 * @{
 */

/** @cond PRIVATE */
extern OTC_PRIV struct otc_transform_module transform_nop;
extern OTC_PRIV struct otc_transform_module transform_scale;
extern OTC_PRIV struct otc_transform_module transform_invert;
/** @endcond */

static const struct otc_transform_module *transform_module_list[] = {
	&transform_nop,
	&transform_scale,
	&transform_invert,
	NULL,
};

/**
 * Returns a NULL-terminated list of all available transform modules.
 *
 * @since 0.4.0
 */
OTC_API const struct otc_transform_module **otc_transform_list(void)
{
	return transform_module_list;
}

/**
 * Returns the specified transform module's ID.
 *
 * @since 0.4.0
 */
OTC_API const char *otc_transform_id_get(const struct otc_transform_module *tmod)
{
	if (!tmod) {
		otc_err("Invalid transform module NULL!");
		return NULL;
	}

	return tmod->id;
}

/**
 * Returns the specified transform module's name.
 *
 * @since 0.4.0
 */
OTC_API const char *otc_transform_name_get(const struct otc_transform_module *tmod)
{
	if (!tmod) {
		otc_err("Invalid transform module NULL!");
		return NULL;
	}

	return tmod->name;
}

/**
 * Returns the specified transform module's description.
 *
 * @since 0.4.0
 */
OTC_API const char *otc_transform_description_get(const struct otc_transform_module *tmod)
{
	if (!tmod) {
		otc_err("Invalid transform module NULL!");
		return NULL;
	}

	return tmod->desc;
}

/**
 * Return the transform module with the specified ID, or NULL if no module
 * with that ID is found.
 *
 * @since 0.4.0
 */
OTC_API const struct otc_transform_module *otc_transform_find(const char *id)
{
	int i;

	for (i = 0; transform_module_list[i]; i++) {
		if (!strcmp(transform_module_list[i]->id, id))
			return transform_module_list[i];
	}

	return NULL;
}

/**
 * Returns a NULL-terminated array of struct otc_option, or NULL if the
 * module takes no options.
 *
 * Each call to this function must be followed by a call to
 * otc_transform_options_free().
 *
 * @since 0.4.0
 */
OTC_API const struct otc_option **otc_transform_options_get(const struct otc_transform_module *tmod)
{
	const struct otc_option *mod_opts, **opts;
	int size, i;

	if (!tmod || !tmod->options)
		return NULL;

	mod_opts = tmod->options();

	for (size = 0; mod_opts[size].id; size++)
		;
	opts = g_malloc((size + 1) * sizeof(struct otc_option *));

	for (i = 0; i < size; i++)
		opts[i] = &mod_opts[i];
	opts[i] = NULL;

	return opts;
}

/**
 * After a call to otc_transform_options_get(), this function cleans up all
 * resources returned by that call.
 *
 * @since 0.4.0
 */
OTC_API void otc_transform_options_free(const struct otc_option **options)
{
	int i;

	if (!options)
		return;

	for (i = 0; options[i]; i++) {
		if (options[i]->def) {
			g_variant_unref(options[i]->def);
			((struct otc_option *)options[i])->def = NULL;
		}

		if (options[i]->values) {
			g_slist_free_full(options[i]->values, (GDestroyNotify)g_variant_unref);
			((struct otc_option *)options[i])->values = NULL;
		}
	}
	g_free(options);
}

/**
 * Create a new transform instance using the specified transform module.
 *
 * <code>options</code> is a *GHashTable with the keys corresponding with
 * the module options' <code>id</code> field. The values should be GVariant
 * pointers with sunk * references, of the same GVariantType as the option's
 * default value.
 *
 * The otc_dev_inst passed in can be used by the instance to determine
 * channel names, samplerate, and so on.
 *
 * @since 0.4.0
 */
OTC_API const struct otc_transform *otc_transform_new(const struct otc_transform_module *tmod,
		GHashTable *options, const struct otc_dev_inst *sdi)
{
	struct otc_transform *t;
	const struct otc_option *mod_opts;
	const GVariantType *gvt;
	GHashTable *new_opts;
	GHashTableIter iter;
	gpointer key, value;
	int i;

	t = g_malloc(sizeof(struct otc_transform));
	t->module = tmod;
	t->sdi = sdi;

	new_opts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)g_variant_unref);
	if (tmod->options) {
		mod_opts = tmod->options();
		for (i = 0; mod_opts[i].id; i++) {
			if (options && g_hash_table_lookup_extended(options,
					mod_opts[i].id, &key, &value)) {
				/* Pass option along. */
				gvt = g_variant_get_type(mod_opts[i].def);
				if (!g_variant_is_of_type(value, gvt)) {
					otc_err("Invalid type for '%s' option.",
						(char *)key);
					g_free(t);
					return NULL;
				}
				g_hash_table_insert(new_opts, g_strdup(mod_opts[i].id),
						g_variant_ref(value));
			} else {
				/* Option not given: insert the default value. */
				g_hash_table_insert(new_opts, g_strdup(mod_opts[i].id),
						g_variant_ref(mod_opts[i].def));
			}
		}

		/* Make sure no invalid options were given. */
		if (options) {
			g_hash_table_iter_init(&iter, options);
			while (g_hash_table_iter_next(&iter, &key, &value)) {
				if (!g_hash_table_lookup(new_opts, key)) {
					otc_err("Transform module '%s' has no option '%s'.",
						tmod->id, (char *)key);
					g_hash_table_destroy(new_opts);
					g_free(t);
					return NULL;
				}
			}
		}
	}

	if (t->module->init && t->module->init(t, new_opts) != OTC_OK) {
		g_free(t);
		t = NULL;
	}
	if (new_opts)
		g_hash_table_destroy(new_opts);

	/* Add the transform to the session's list of transforms. */
	sdi->session->transforms = g_slist_append(sdi->session->transforms, t);

	return t;
}

/**
 * Free the specified transform instance and all associated resources.
 *
 * @since 0.4.0
 */
OTC_API int otc_transform_free(const struct otc_transform *t)
{
	int ret;

	if (!t)
		return OTC_ERR_ARG;

	ret = OTC_OK;
	if (t->module->cleanup)
		ret = t->module->cleanup((struct otc_transform *)t);
	g_free((gpointer)t);

	return ret;
}

/** @} */
