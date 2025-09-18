/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Bert Vermeulen <bert@biot.com>
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
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"

/** @cond PRIVATE */
#define LOG_PREFIX "input"
/** @endcond */

/** @cond PRIVATE */
#define CHUNK_SIZE	(4 * 1024 * 1024)
/** @endcond */

/**
 * @file
 *
 * Input module handling.
 */

/**
 * @defgroup grp_input Input modules
 *
 * Input file/data module handling.
 *
 * libopentracecapture can process acquisition data in several different ways.
 * Aside from acquiring data from a hardware device, it can also take it
 * from a file in various formats (binary, CSV, VCD, and so on).
 *
 * Like all libopentracecapture data handling, processing is done in a streaming
 * manner: input should be supplied a chunk at a time. This way anything
 * that processes data can do so in real time, without the user having
 * to wait for the whole thing to be finished.
 *
 * Every input module is "pluggable", meaning it's handled as being separate
 * from the main libopentracecapture, but linked in to it statically. To keep things
 * modular and separate like this, functions within an input module should be
 * declared static, with only the respective 'struct otc_input_module' being
 * exported for use into the wider libopentracecapture namespace.
 *
 * @{
 */

/** @cond PRIVATE */
extern OTC_PRIV struct otc_input_module input_binary;
extern OTC_PRIV struct otc_input_module input_chronovu_la8;
extern OTC_PRIV struct otc_input_module input_csv;
extern OTC_PRIV struct otc_input_module input_logicport;
extern OTC_PRIV struct otc_input_module input_null;
extern OTC_PRIV struct otc_input_module input_protocoldata;
extern OTC_PRIV struct otc_input_module input_raw_analog;
extern OTC_PRIV struct otc_input_module input_saleae;
extern OTC_PRIV struct otc_input_module input_stf;
extern OTC_PRIV struct otc_input_module input_trace32_ad;
extern OTC_PRIV struct otc_input_module input_vcd;
extern OTC_PRIV struct otc_input_module input_wav;
extern OTC_PRIV struct otc_input_module input_isf;
/** @endcond */

static const struct otc_input_module *input_module_list[] = {
	&input_binary,
	&input_chronovu_la8,
	&input_csv,
	&input_logicport,
	&input_null,
	&input_protocoldata,
	&input_raw_analog,
	&input_saleae,
#if defined HAVE_INPUT_STF && HAVE_INPUT_STF
	&input_stf,
#endif
	&input_trace32_ad,
	&input_vcd,
	&input_wav,
	&input_isf,
	NULL,
};

/**
 * Returns a NULL-terminated list of all available input modules.
 *
 * @since 0.4.0
 */
OTC_API const struct otc_input_module **otc_input_list(void)
{
	return input_module_list;
}

/**
 * Returns the specified input module's ID.
 *
 * @since 0.4.0
 */
OTC_API const char *otc_input_id_get(const struct otc_input_module *imod)
{
	if (!imod) {
		otc_err("Invalid input module NULL!");
		return NULL;
	}

	return imod->id;
}

/**
 * Returns the specified input module's name.
 *
 * @since 0.4.0
 */
OTC_API const char *otc_input_name_get(const struct otc_input_module *imod)
{
	if (!imod) {
		otc_err("Invalid input module NULL!");
		return NULL;
	}

	return imod->name;
}

/**
 * Returns the specified input module's description.
 *
 * @since 0.4.0
 */
OTC_API const char *otc_input_description_get(const struct otc_input_module *imod)
{
	if (!imod) {
		otc_err("Invalid input module NULL!");
		return NULL;
	}

	return imod->desc;
}

/**
 * Returns the specified input module's file extensions typical for the file
 * format, as a NULL terminated array, or returns a NULL pointer if there is
 * no preferred extension.
 * @note these are a suggestions only.
 *
 * @since 0.4.0
 */
OTC_API const char *const *otc_input_extensions_get(
		const struct otc_input_module *imod)
{
	if (!imod) {
		otc_err("Invalid input module NULL!");
		return NULL;
	}

	return imod->exts;
}

/**
 * Return the input module with the specified ID, or NULL if no module
 * with that id is found.
 *
 * @since 0.4.0
 */
OTC_API const struct otc_input_module *otc_input_find(const char *id)
{
	int i;

	for (i = 0; input_module_list[i]; i++) {
		if (!strcmp(input_module_list[i]->id, id))
			return input_module_list[i];
	}

	return NULL;
}

/**
 * Returns a NULL-terminated array of struct otc_option, or NULL if the
 * module takes no options.
 *
 * Each call to this function must be followed by a call to
 * otc_input_options_free().
 *
 * @since 0.4.0
 */
OTC_API const struct otc_option **otc_input_options_get(const struct otc_input_module *imod)
{
	const struct otc_option *mod_opts, **opts;
	int size, i;

	if (!imod || !imod->options)
		return NULL;

	mod_opts = imod->options();

	for (size = 0; mod_opts[size].id; size++)
		;
	opts = g_malloc((size + 1) * sizeof(struct otc_option *));

	for (i = 0; i < size; i++)
		opts[i] = &mod_opts[i];
	opts[i] = NULL;

	return opts;
}

/**
 * After a call to otc_input_options_get(), this function cleans up all
 * resources returned by that call.
 *
 * @since 0.4.0
 */
OTC_API void otc_input_options_free(const struct otc_option **options)
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
 * Create a new input instance using the specified input module.
 *
 * This function is used when a client wants to use a specific input
 * module to parse a stream. No effort is made to identify the format.
 *
 * @param imod The input module to use. Must not be NULL.
 * @param options GHashTable consisting of keys corresponding with
 * the module options @c id field. The values should be GVariant
 * pointers with sunk references, of the same GVariantType as the option's
 * default value.
 *
 * @since 0.4.0
 */
OTC_API struct otc_input *otc_input_new(const struct otc_input_module *imod,
		GHashTable *options)
{
	struct otc_input *in;
	const struct otc_option *mod_opts;
	const GVariantType *gvt;
	GHashTable *new_opts;
	GHashTableIter iter;
	gpointer key, value;
	int i;

	in = g_malloc0(sizeof(struct otc_input));
	in->module = imod;

	new_opts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)g_variant_unref);
	if (imod->options) {
		mod_opts = imod->options();
		for (i = 0; mod_opts[i].id; i++) {
			if (options && g_hash_table_lookup_extended(options,
					mod_opts[i].id, &key, &value)) {
				/* Option not given: insert the default value. */
				gvt = g_variant_get_type(mod_opts[i].def);
				if (!g_variant_is_of_type(value, gvt)) {
					otc_err("Invalid type for '%s' option.",
						(char *)key);
					g_free(in);
					return NULL;
				}
				g_hash_table_insert(new_opts, g_strdup(mod_opts[i].id),
						g_variant_ref(value));
			} else {
				/* Pass option along. */
				g_hash_table_insert(new_opts, g_strdup(mod_opts[i].id),
						g_variant_ref(mod_opts[i].def));
			}
		}

		/* Make sure no invalid options were given. */
		if (options) {
			g_hash_table_iter_init(&iter, options);
			while (g_hash_table_iter_next(&iter, &key, &value)) {
				if (!g_hash_table_lookup(new_opts, key)) {
					otc_err("Input module '%s' has no option '%s'",
						imod->id, (char *)key);
					g_hash_table_destroy(new_opts);
					g_free(in);
					return NULL;
				}
			}
		}
	}

	if (in->module->init && in->module->init(in, new_opts) != OTC_OK) {
		g_free(in);
		in = NULL;
	} else {
		in->buf = g_string_sized_new(128);
	}

	if (new_opts)
		g_hash_table_destroy(new_opts);

	return in;
}

/* Returns TRUE if all required meta items are available. */
static gboolean check_required_metadata(const uint8_t *metadata, uint8_t *avail)
{
	int m, a;
	uint8_t reqd;

	for (m = 0; metadata[m]; m++) {
		if (!(metadata[m] & OTC_INPUT_META_REQUIRED))
			continue;
		reqd = metadata[m] & ~OTC_INPUT_META_REQUIRED;
		for (a = 0; avail[a]; a++) {
			if (avail[a] == reqd)
				break;
		}
		if (!avail[a])
			/* Found a required meta item that isn't available. */
			return FALSE;
	}

	return TRUE;
}

/**
 * Try to find an input module that can parse the given buffer.
 *
 * The buffer must contain enough of the beginning of the file for
 * the input modules to find a match. This is format-dependent. When
 * magic strings get checked, 128 bytes normally could be enough. Note
 * that some formats try to parse larger header sections, and benefit
 * from seeing a larger scope.
 *
 * If an input module is found, an instance is created into *in.
 * Otherwise, *in contains NULL. When multiple input moduless claim
 * support for the format, the one with highest confidence takes
 * precedence. Applications will see at most one input module spec.
 *
 * If an instance is created, it has the given buffer used for scanning
 * already submitted to it, to be processed before more data is sent.
 * This allows a frontend to submit an initial chunk of a non-seekable
 * stream, such as stdin, without having to keep it around and submit
 * it again later.
 *
 */
OTC_API int otc_input_scan_buffer(GString *buf, const struct otc_input **in)
{
	const struct otc_input_module *imod, *best_imod;
	GHashTable *meta;
	unsigned int m, i;
	unsigned int conf, best_conf;
	int ret;
	uint8_t mitem, avail_metadata[8];

	/* No more metadata to be had from a buffer. */
	avail_metadata[0] = OTC_INPUT_META_HEADER;
	avail_metadata[1] = 0;

	*in = NULL;
	best_imod = NULL;
	best_conf = ~0;
	for (i = 0; input_module_list[i]; i++) {
		imod = input_module_list[i];
		if (!imod->metadata[0]) {
			/* Module has no metadata for matching so will take
			 * any input. No point in letting it try to match. */
			continue;
		}
		if (!check_required_metadata(imod->metadata, avail_metadata))
			/* Cannot satisfy this module's requirements. */
			continue;

		meta = g_hash_table_new(NULL, NULL);
		for (m = 0; m < sizeof(imod->metadata); m++) {
			mitem = imod->metadata[m] & ~OTC_INPUT_META_REQUIRED;
			if (mitem == OTC_INPUT_META_HEADER)
				g_hash_table_insert(meta, GINT_TO_POINTER(mitem), buf);
		}
		if (g_hash_table_size(meta) == 0) {
			/* No metadata for this module, so nothing to match. */
			g_hash_table_destroy(meta);
			continue;
		}
		otc_spew("Trying module %s.", imod->id);
		ret = imod->format_match(meta, &conf);
		g_hash_table_destroy(meta);
		if (ret == OTC_ERR_DATA) {
			/* Module recognized this buffer, but cannot handle it. */
			continue;
		} else if (ret == OTC_ERR) {
			/* Module didn't recognize this buffer. */
			continue;
		} else if (ret != OTC_OK) {
			/* Can be OTC_ERR_NA. */
			continue;
		}

		/* Found a matching module. */
		otc_spew("Module %s matched, confidence %u.", imod->id, conf);
		if (conf >= best_conf)
			continue;
		best_imod = imod;
		best_conf = conf;
	}

	if (best_imod) {
		*in = otc_input_new(best_imod, NULL);
		g_string_insert_len((*in)->buf, 0, buf->str, buf->len);
		return OTC_OK;
	}

	return OTC_ERR;
}

/**
 * Try to find an input module that can parse the given file.
 *
 * If an input module is found, an instance is created into *in.
 * Otherwise, *in contains NULL. When multiple input moduless claim
 * support for the format, the one with highest confidence takes
 * precedence. Applications will see at most one input module spec.
 *
 */
OTC_API int otc_input_scan_file(const char *filename, const struct otc_input **in)
{
	int64_t filesize;
	FILE *stream;
	const struct otc_input_module *imod, *best_imod;
	GHashTable *meta;
	GString *header;
	size_t count;
	unsigned int midx, i;
	unsigned int conf, best_conf;
	int ret;
	uint8_t avail_metadata[8];

	*in = NULL;

	if (!filename || !filename[0]) {
		otc_err("Invalid filename.");
		return OTC_ERR_ARG;
	}
	stream = g_fopen(filename, "rb");
	if (!stream) {
		otc_err("Failed to open %s: %s", filename, g_strerror(errno));
		return OTC_ERR;
	}
	filesize = otc_file_get_size(stream);
	if (filesize < 0) {
		otc_err("Failed to get size of %s: %s",
			filename, g_strerror(errno));
		fclose(stream);
		return OTC_ERR;
	}
	header = g_string_sized_new(CHUNK_SIZE);
	count = fread(header->str, 1, header->allocated_len - 1, stream);
	if (count < 1 || ferror(stream)) {
		otc_err("Failed to read %s: %s", filename, g_strerror(errno));
		fclose(stream);
		g_string_free(header, TRUE);
		return OTC_ERR;
	}
	fclose(stream);
	g_string_set_size(header, count);

	meta = g_hash_table_new(NULL, NULL);
	g_hash_table_insert(meta, GINT_TO_POINTER(OTC_INPUT_META_FILENAME),
			(char *)filename);
	g_hash_table_insert(meta, GINT_TO_POINTER(OTC_INPUT_META_FILESIZE),
			GSIZE_TO_POINTER(MIN(filesize, G_MAXSSIZE)));
	g_hash_table_insert(meta, GINT_TO_POINTER(OTC_INPUT_META_HEADER),
			header);
	midx = 0;
	avail_metadata[midx++] = OTC_INPUT_META_FILENAME;
	avail_metadata[midx++] = OTC_INPUT_META_FILESIZE;
	avail_metadata[midx++] = OTC_INPUT_META_HEADER;
	avail_metadata[midx] = 0;
	/* TODO: MIME type */

	best_imod = NULL;
	best_conf = ~0;
	for (i = 0; input_module_list[i]; i++) {
		imod = input_module_list[i];
		if (!imod->metadata[0]) {
			/* Module has no metadata for matching so will take
			 * any input. No point in letting it try to match. */
			continue;
		}
		if (!check_required_metadata(imod->metadata, avail_metadata))
			/* Cannot satisfy this module's requirements. */
			continue;

		otc_dbg("Trying module %s.", imod->id);

		ret = imod->format_match(meta, &conf);
		if (ret == OTC_ERR) {
			/* Module didn't recognize this buffer. */
			continue;
		} else if (ret != OTC_OK) {
			/* Module recognized this buffer, but cannot handle it. */
			continue;
		}
		/* Found a matching module. */
		otc_dbg("Module %s matched, confidence %u.", imod->id, conf);
		if (conf >= best_conf)
			continue;
		best_imod = imod;
		best_conf = conf;
	}
	g_hash_table_destroy(meta);
	g_string_free(header, TRUE);

	if (best_imod) {
		*in = otc_input_new(best_imod, NULL);
		return OTC_OK;
	}

	return OTC_ERR;
}

/**
 * Return the input instance's module "class". This can be used to find out
 * which input module handles a specific input file. This is especially
 * useful when an application did not create the input stream by specifying
 * an input module, but instead some shortcut or convenience wrapper did.
 *
 * @since 0.6.0
 */
OTC_API const struct otc_input_module *otc_input_module_get(const struct otc_input *in)
{
	if (!in)
		return NULL;

	return in->module;
}

/**
 * Return the input instance's (virtual) device instance. This can be
 * used to find out the number of channels and other information.
 *
 * If the device instance has not yet been fully populated by the input
 * module, NULL is returned. This indicates the module needs more data
 * to identify the number of channels and so on.
 *
 * @since 0.4.0
 */
OTC_API struct otc_dev_inst *otc_input_dev_inst_get(const struct otc_input *in)
{
	if (in->sdi_ready)
		return in->sdi;
	else
		return NULL;
}

/**
 * Send data to the specified input instance.
 *
 * When an input module instance is created with otc_input_new(), this
 * function is used to feed data to the instance.
 *
 * As enough data gets fed into this function to completely populate
 * the device instance associated with this input instance, this is
 * guaranteed to return the moment it's ready. This gives the caller
 * the chance to examine the device instance, attach session callbacks
 * and so on.
 *
 * @since 0.4.0
 */
OTC_API int otc_input_send(const struct otc_input *in, GString *buf)
{
	size_t len;

	len = buf ? buf->len : 0;
	otc_spew("Sending %zu bytes to %s module.", len, in->module->id);
	return in->module->receive((struct otc_input *)in, buf);
}

/**
 * Signal the input module no more data will come.
 *
 * This will cause the module to process any data it may have buffered.
 * The OTC_DF_END packet will also typically be sent at this time.
 *
 * @since 0.4.0
 */
OTC_API int otc_input_end(const struct otc_input *in)
{
	otc_spew("Calling end() on %s module.", in->module->id);
	return in->module->end((struct otc_input *)in);
}

/**
 * Reset the input module's input handling structures.
 *
 * Causes the input module to reset its internal state so that we can re-send
 * the input data from the beginning without having to re-create the entire
 * input module.
 *
 * @since 0.5.0
 */
OTC_API int otc_input_reset(const struct otc_input *in_ro)
{
	struct otc_input *in;
	int rc;

	in = (struct otc_input *)in_ro;	/* "un-const" */
	if (!in || !in->module)
		return OTC_ERR_ARG;

	/*
	 * Run the optional input module's .reset() method. This shall
	 * take care of the context (kept in the 'inc' variable).
	 */
	if (in->module->reset) {
		otc_spew("Resetting %s module.", in->module->id);
		rc = in->module->reset(in);
	} else {
		otc_spew("Tried to reset %s module but no reset handler found.",
			in->module->id);
		rc = OTC_OK;
	}

	/*
	 * Handle input module status (kept in the 'in' variable) here
	 * in common logic. This agrees with how input module's receive()
	 * and end() routines "amend but never seed" the 'in' information.
	 *
	 * Void potentially accumulated receive() buffer content, and
	 * clear the sdi_ready flag. This makes sure that subsequent
	 * processing will scan the header again before sample data gets
	 * interpreted, and stale content from previous calls won't affect
	 * the result.
	 *
	 * This common logic does not harm when the input module implements
	 * .reset() and contains identical assignments. In the absence of
	 * an individual .reset() method, simple input modules can completely
	 * rely on common code and keep working across resets.
	 */
	if (in->buf)
		g_string_truncate(in->buf, 0);
	in->sdi_ready = FALSE;

	return rc;
}

/**
 * Free the specified input instance and all associated resources.
 *
 * @since 0.4.0
 */
OTC_API void otc_input_free(const struct otc_input *in)
{
	if (!in)
		return;

	/*
	 * Run the input module's optional .cleanup() routine. This
	 * takes care of the context (kept in the 'inc' variable).
	 */
	if (in->module->cleanup)
		in->module->cleanup((struct otc_input *)in);

	/*
	 * Common code releases the input module's state (kept in the
	 * 'in' variable). Release the device instance, the receive()
	 * buffer, the shallow 'in->priv' block which is 'inc' (after
	 * .cleanup() released potentially nested resources under 'inc').
	 */
	otc_dev_inst_free(in->sdi);
	if (in->buf->len > 64) {
		/* That seems more than just some sub-unitsize leftover... */
		otc_warn("Found %" G_GSIZE_FORMAT
			" unprocessed bytes at free time.", in->buf->len);
	}
	g_string_free(in->buf, TRUE);
	g_free(in->priv);
	g_free((gpointer)in);
}

/** @} */
