/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2012 Peter Stuge <peter@stuge.se>
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
#include <glib.h>
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"
#include "minilzo/minilzo.h"

/** @cond PRIVATE */
#define LOG_PREFIX "backend"
/** @endcond */

/**
 * @mainpage libopentracecapture API
 *
 * @section sec_intro Introduction
 *
 * The <a href="http://opentracelab.org">opentracelab</a> project aims at creating a
 * portable, cross-platform, Free/Libre/Open-Source signal analysis software
 * suite that supports various device types (such as logic analyzers,
 * oscilloscopes, multimeters, and more).
 *
 * <a href="http://opentracelab.org/wiki/Libopentracelab">libopentracecapture</a> is a shared
 * library written in C which provides the basic API for talking to
 * <a href="http://opentracelab.org/wiki/Supported_hardware">supported hardware</a>
 * and reading/writing the acquired data into various
 * <a href="http://opentracelab.org/wiki/Input_output_formats">input/output
 * file formats</a>.
 *
 * @section sec_api API reference
 *
 * See the "Modules" page for an introduction to various libopentracecapture
 * related topics and the detailed API documentation of the respective
 * functions.
 *
 * You can also browse the API documentation by file, or review all
 * data structures.
 *
 * @section sec_mailinglists Mailing lists
 *
 * There is one mailing list for opentracelab/libopentracecapture: <a href="https://lists.sourceforge.net/lists/listinfo/opentracelab-devel">opentracelab-devel</a>.
 *
 * @section sec_irc IRC
 *
 * You can find the opentracelab developers in the
 * <a href="ircs://irc.libera.chat/#opentracelab">\#opentracelab</a>
 * IRC channel on Libera.Chat.
 *
 * @section sec_website Website
 *
 * <a href="http://opentracelab.org/wiki/Libopentracelab">opentracelab.org/wiki/Libopentracelab</a>
 */

/**
 * @file
 *
 * Initializing and shutting down libopentracecapture.
 */

/**
 * @defgroup grp_init Initialization
 *
 * Initializing and shutting down libopentracecapture.
 *
 * Before using any of the libopentracecapture functionality (except for
 * otc_log_loglevel_set()), otc_init() must be called to initialize the
 * library, which will return a struct otc_context when the initialization
 * was successful.
 *
 * When libopentracecapture functionality is no longer needed, otc_exit() should be
 * called, which will (among other things) free the struct otc_context.
 *
 * Example for a minimal program using libopentracecapture:
 *
 * @code{.c}
 *   #include <stdio.h>
 *   #include <opentracecapture/libopentracecapture.h>
 *
 *   int main(int argc, char **argv)
 *   {
 *   	int ret;
 *   	struct otc_context *otc_ctx;
 *
 *   	if ((ret = otc_init(&otc_ctx)) != OTC_OK) {
 *   		printf("Error initializing libopentracecapture (%s): %s.\n",
 *   		       otc_strerror_name(ret), otc_strerror(ret));
 *   		return 1;
 *   	}
 *
 *   	// Use libopentracecapture functions here...
 *
 *   	if ((ret = otc_exit(otc_ctx)) != OTC_OK) {
 *   		printf("Error shutting down libopentracecapture (%s): %s.\n",
 *   		       otc_strerror_name(ret), otc_strerror(ret));
 *   		return 1;
 *   	}
 *
 *   	return 0;
 *   }
 * @endcode
 *
 * @{
 */

OTC_API GSList *otc_buildinfo_libs_get(void)
{
	GSList *l = NULL, *m = NULL;
#if defined(HAVE_LIBUSB_1_0) && !defined(__FreeBSD__)
	const struct libusb_version *lv;
#endif

	m = g_slist_append(NULL, g_strdup("glib"));
	m = g_slist_append(m, g_strdup_printf("%d.%d.%d (rt: %d.%d.%d/%d:%d)",
		GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
		glib_major_version, glib_minor_version, glib_micro_version,
		glib_binary_age, glib_interface_age));
	l = g_slist_append(l, m);

#ifdef HAVE_ZLIB
	m = g_slist_append(NULL, g_strdup("zlib"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_ZLIB_VERSION));
	l = g_slist_append(l, m);
#endif

	m = g_slist_append(NULL, g_strdup("libzip"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBZIP_VERSION));
	l = g_slist_append(l, m);

	m = g_slist_append(NULL, g_strdup("minilzo"));
	m = g_slist_append(m, g_strdup_printf("%s", lzo_version_string()));
	l = g_slist_append(l, m);

#ifdef HAVE_LIBSERIALPORT
	m = g_slist_append(NULL, g_strdup("libserialport"));
	m = g_slist_append(m, g_strdup_printf("%s/%s (rt: %s/%s)",
		SP_PACKAGE_VERSION_STRING, SP_LIB_VERSION_STRING,
		sp_get_package_version_string(), sp_get_lib_version_string()));
	l = g_slist_append(l, m);
#endif
#ifdef HAVE_LIBUSB_1_0
	m = g_slist_append(NULL, g_strdup("libusb-1.0"));
#ifdef __FreeBSD__
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBUSB_1_0_VERSION));
#else
	lv = libusb_get_version();
	m = g_slist_append(m, g_strdup_printf("%d.%d.%d.%d%s API 0x%08x",
		lv->major, lv->minor, lv->micro, lv->nano, lv->rc,
#if defined(LIBUSB_API_VERSION)
		LIBUSB_API_VERSION
#elif defined(LIBUSBX_API_VERSION)
		LIBUSBX_API_VERSION
#endif
		));
#endif
	l = g_slist_append(l, m);
#endif
#ifdef HAVE_LIBHIDAPI
	m = g_slist_append(NULL, g_strdup("hidapi"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBHIDAPI_VERSION));
	l = g_slist_append(l, m);
#endif
#ifdef HAVE_LIBBLUEZ
	m = g_slist_append(NULL, g_strdup("bluez"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBBLUEZ_VERSION));
	l = g_slist_append(l, m);
#endif
#ifdef HAVE_LIBFTDI
	m = g_slist_append(NULL, g_strdup("libftdi"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBFTDI_VERSION));
	l = g_slist_append(l, m);
#endif
#ifdef HAVE_LIBGPIB
	m = g_slist_append(NULL, g_strdup("libgpib"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBGPIB_VERSION));
	l = g_slist_append(l, m);
#endif
#ifdef HAVE_LIBREVISA
	m = g_slist_append(NULL, g_strdup("librevisa"));
	m = g_slist_append(m, g_strdup_printf("%s", CONF_LIBREVISA_VERSION));
	l = g_slist_append(l, m);
#endif

	return l;
}

OTC_API char *otc_buildinfo_host_get(void)
{
	return g_strdup_printf("%s, %s-endian", CONF_HOST,
#ifdef WORDS_BIGENDIAN
	"big"
#else
	"little"
#endif
	);
}

OTC_API char *otc_buildinfo_scpi_backends_get(void)
{
	GString *s;
	char *str;

	s = g_string_sized_new(200);

	g_string_append_printf(s, "TCP, ");
#if HAVE_RPC
	g_string_append_printf(s, "RPC, ");
#endif
#ifdef HAVE_SERIAL_COMM
	g_string_append_printf(s, "serial, ");
#endif
#ifdef HAVE_LIBREVISA
	g_string_append_printf(s, "VISA, ");
#endif
#ifdef HAVE_LIBGPIB
	g_string_append_printf(s, "GPIB, ");
#endif
#ifdef HAVE_LIBUSB_1_0
	g_string_append_printf(s, "USBTMC, ");
#endif
	s->str[s->len - 2] = '\0';

	str = g_strdup(s->str);
	g_string_free(s, TRUE);

	return str;
}

static void print_versions(void)
{
	GString *s;
	GSList *l, *l_orig, *m;
	char *str;
	const char *lib, *version;

	otc_dbg("libopentracecapture %s/%s.",
		otc_package_version_string_get(), otc_lib_version_string_get());

	s = g_string_sized_new(200);
	g_string_append(s, "Libs: ");
	l_orig = otc_buildinfo_libs_get();
	for (l = l_orig; l; l = l->next) {
		m = l->data;
		lib = m->data;
		version = m->next->data;
		g_string_append_printf(s, "%s %s, ", lib, version);
		g_slist_free_full(m, g_free);
	}
	g_slist_free(l_orig);
	s->str[s->len - 2] = '.';
	s->str[s->len - 1] = '\0';
	otc_dbg("%s", s->str);
	g_string_free(s, TRUE);

	str = otc_buildinfo_host_get();
	otc_dbg("Host: %s.", str);
	g_free(str);

	str = otc_buildinfo_scpi_backends_get();
	otc_dbg("SCPI backends: %s.", str);
	g_free(str);
}

static void print_resourcepaths(void)
{
	GSList *l, *l_orig;

	otc_dbg("Firmware search paths:");
	l_orig = otc_resourcepaths_get(OTC_RESOURCE_FIRMWARE);
	for (l = l_orig; l; l = l->next)
		otc_dbg(" - %s", (const char *)l->data);
	g_slist_free_full(l_orig, g_free);
}

/**
 * Sanity-check all libopentracecapture drivers.
 *
 * @param[in] ctx Pointer to a libopentracecapture context struct. Must not be NULL.
 *
 * @retval OTC_OK All drivers are OK
 * @retval OTC_ERR One or more drivers have issues.
 * @retval OTC_ERR_ARG Invalid argument.
 */
static int sanity_check_all_drivers(const struct otc_context *ctx)
{
	int i, errors, ret = OTC_OK;
	struct otc_dev_driver **drivers;
	const char *d;

	if (!ctx)
		return OTC_ERR_ARG;

	otc_spew("Sanity-checking all drivers.");

	drivers = otc_driver_list(ctx);
	for (i = 0; drivers[i]; i++) {
		errors = 0;

		d = (drivers[i]->name) ? drivers[i]->name : "NULL";

		if (!drivers[i]->name) {
			otc_err("No name in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->longname) {
			otc_err("No longname in driver %d ('%s').", i, d);
			errors++;
		}
		if (drivers[i]->api_version < 1) {
			otc_err("API version in driver %d ('%s') < 1.", i, d);
			errors++;
		}
		if (!drivers[i]->init) {
			otc_err("No init in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->cleanup) {
			otc_err("No cleanup in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->scan) {
			otc_err("No scan in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_list) {
			otc_err("No dev_list in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_clear) {
			otc_err("No dev_clear in driver %d ('%s').", i, d);
			errors++;
		}
		/* Note: config_get() is optional. */
		if (!drivers[i]->config_set) {
			otc_err("No config_set in driver %d ('%s').", i, d);
			errors++;
		}
		/* Note: config_channel_set() is optional. */
		/* Note: config_commit() is optional. */
		if (!drivers[i]->config_list) {
			otc_err("No config_list in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_open) {
			otc_err("No dev_open in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_close) {
			otc_err("No dev_close in driver %d ('%s').", i, d);
			errors++;
		}
		if (!drivers[i]->dev_acquisition_start) {
			otc_err("No dev_acquisition_start in driver %d ('%s').",
			       i, d);
			errors++;
		}
		if (!drivers[i]->dev_acquisition_stop) {
			otc_err("No dev_acquisition_stop in driver %d ('%s').",
			       i, d);
			errors++;
		}

		/* Note: 'priv' is allowed to be NULL. */

		if (errors == 0)
			continue;

		ret = OTC_ERR;
	}

	return ret;
}

/**
 * Sanity-check all libopentracecapture input modules.
 *
 * @retval OTC_OK All modules are OK
 * @retval OTC_ERR One or more modules have issues.
 */
static int sanity_check_all_input_modules(void)
{
	int i, errors, ret = OTC_OK;
	const struct otc_input_module **inputs;
	const char *d;

	otc_spew("Sanity-checking all input modules.");

	inputs = otc_input_list();
	for (i = 0; inputs[i]; i++) {
		errors = 0;

		d = (inputs[i]->id) ? inputs[i]->id : "NULL";

		if (!inputs[i]->id) {
			otc_err("No ID in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->name) {
			otc_err("No name in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->desc) {
			otc_err("No description in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->init) {
			otc_err("No init in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->receive) {
			otc_err("No receive in module %d ('%s').", i, d);
			errors++;
		}
		if (!inputs[i]->end) {
			otc_err("No end in module %d ('%s').", i, d);
			errors++;
		}

		if (errors == 0)
			continue;

		ret = OTC_ERR;
	}

	return ret;
}

/**
 * Sanity-check all libopentracecapture output modules.
 *
 * @retval OTC_OK All modules are OK
 * @retval OTC_ERR One or more modules have issues.
 */
static int sanity_check_all_output_modules(void)
{
	int i, errors, ret = OTC_OK;
	const struct otc_output_module **outputs;
	const char *d;

	otc_spew("Sanity-checking all output modules.");

	outputs = otc_output_list();
	for (i = 0; outputs[i]; i++) {
		errors = 0;

		d = (outputs[i]->id) ? outputs[i]->id : "NULL";

		if (!outputs[i]->id) {
			otc_err("No ID in module %d ('%s').", i, d);
			errors++;
		}
		if (!outputs[i]->name) {
			otc_err("No name in module %d ('%s').", i, d);
			errors++;
		}
		if (!outputs[i]->desc) {
			otc_err("No description in module '%s'.", d);
			errors++;
		}
		if (!outputs[i]->receive) {
			otc_err("No receive in module '%s'.", d);
			errors++;
		}

		if (errors == 0)
			continue;

		ret = OTC_ERR;
	}

	return ret;
}

/**
 * Sanity-check all libopentracecapture transform modules.
 *
 * @retval OTC_OK All modules are OK
 * @retval OTC_ERR One or more modules have issues.
 */
static int sanity_check_all_transform_modules(void)
{
	int i, errors, ret = OTC_OK;
	const struct otc_transform_module **transforms;
	const char *d;

	otc_spew("Sanity-checking all transform modules.");

	transforms = otc_transform_list();
	for (i = 0; transforms[i]; i++) {
		errors = 0;

		d = (transforms[i]->id) ? transforms[i]->id : "NULL";

		if (!transforms[i]->id) {
			otc_err("No ID in module %d ('%s').", i, d);
			errors++;
		}
		if (!transforms[i]->name) {
			otc_err("No name in module %d ('%s').", i, d);
			errors++;
		}
		if (!transforms[i]->desc) {
			otc_err("No description in module '%s'.", d);
			errors++;
		}
		/* Note: options() is optional. */
		/* Note: init() is optional. */
		if (!transforms[i]->receive) {
			otc_err("No receive in module '%s'.", d);
			errors++;
		}
		/* Note: cleanup() is optional. */

		if (errors == 0)
			continue;

		ret = OTC_ERR;
	}

	return ret;
}

/**
 * Initialize libopentracecapture.
 *
 * This function must be called before any other libopentracecapture function.
 *
 * @param ctx Pointer to a libopentracecapture context struct pointer. Must not be NULL.
 *            This will be a pointer to a newly allocated libopentracecapture context
 *            object upon success, and is undefined upon errors.
 *
 * @return OTC_OK upon success, a (negative) error code otherwise. Upon errors
 *         the 'ctx' pointer is undefined and should not be used. Upon success,
 *         the context will be free'd by otc_exit() as part of the libopentracecapture
 *         shutdown.
 *
 * @since 0.2.0
 */
OTC_API int otc_init(struct otc_context **ctx)
{
	int ret = OTC_ERR;
	struct otc_context *context;
#ifdef _WIN32
	WSADATA wsadata;
#endif

	print_versions();

	print_resourcepaths();

	if (!ctx) {
		otc_err("%s(): libopentracecapture context was NULL.", __func__);
		return OTC_ERR;
	}

	context = g_malloc0(sizeof(struct otc_context));

	otc_drivers_init(context);

	if (sanity_check_all_drivers(context) < 0) {
		otc_err("Internal driver error(s), aborting.");
		goto done;
	}

	if (sanity_check_all_input_modules() < 0) {
		otc_err("Internal input module error(s), aborting.");
		goto done;
	}

	if (sanity_check_all_output_modules() < 0) {
		otc_err("Internal output module error(s), aborting.");
		goto done;
	}

	if (sanity_check_all_transform_modules() < 0) {
		otc_err("Internal transform module error(s), aborting.");
		goto done;
	}

#ifdef _WIN32
	if ((ret = WSAStartup(MAKEWORD(2, 2), &wsadata)) != 0) {
		otc_err("WSAStartup failed with error code %d.", ret);
		ret = OTC_ERR;
		goto done;
	}
#endif

	if ((ret = lzo_init()) != LZO_E_OK) {
		otc_err("lzo_init() failed with return code %d.", ret);
		otc_err("This usually indicates a compiler bug. Recompile without");
		otc_err("optimizations, and enable '-DLZO_DEBUG' for diagnostics.");
		ret = OTC_ERR;
		goto done;
	}

#ifdef HAVE_LIBUSB_1_0
	ret = libusb_init(&context->libusb_ctx);
	if (LIBUSB_SUCCESS != ret) {
		otc_err("libusb_init() returned %s.", libusb_error_name(ret));
		ret = OTC_ERR;
		goto done;
	}
#endif
#ifdef HAVE_LIBHIDAPI
	/*
	 * According to <hidapi.h>, the hid_init() routine just returns
	 * zero or non-zero, and hid_error() appears to relate to calls
	 * for a specific device after hid_open(). Which means that there
	 * is no more detailled information available beyond success/fail
	 * at this point in time.
	 */
	if (hid_init() != 0) {
		otc_err("HIDAPI hid_init() failed.");
		ret = OTC_ERR;
		goto done;
	}
#endif
	otc_resource_set_hooks(context, NULL, NULL, NULL, NULL);

	*ctx = context;
	context = NULL;
	ret = OTC_OK;

done:
	g_free(context);
	return ret;
}

/**
 * Shutdown libopentracecapture.
 *
 * @param ctx Pointer to a libopentracecapture context struct. Must not be NULL.
 *
 * @retval OTC_OK Success
 * @retval other Error code OTC_ERR, ...
 *
 * @since 0.2.0
 */
OTC_API int otc_exit(struct otc_context *ctx)
{
	if (!ctx) {
		otc_err("%s(): libopentracecapture context was NULL.", __func__);
		return OTC_ERR;
	}

	otc_hw_cleanup_all(ctx);

#ifdef _WIN32
	WSACleanup();
#endif

#ifdef HAVE_LIBHIDAPI
	hid_exit();
#endif
#ifdef HAVE_LIBUSB_1_0
	libusb_exit(ctx->libusb_ctx);
#endif

	g_free(otc_driver_list(ctx));
	g_free(ctx);

	return OTC_OK;
}

/** @} */
