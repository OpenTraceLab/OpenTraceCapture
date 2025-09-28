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

#ifndef LIBSIGROK_PROTO_H
#define LIBSIGROK_PROTO_H

/**
 * @file
 *
 * Header file containing API function prototypes.
 */

/*--- analog.c --------------------------------------------------------------*/

OTC_API int otc_analog_to_float(const struct otc_datafeed_analog *analog,
		float *buf);
OTC_API const char *otc_analog_si_prefix(float *value, int *digits);
OTC_API gboolean otc_analog_si_prefix_friendly(enum otc_unit unit);
OTC_API int otc_analog_unit_to_string(const struct otc_datafeed_analog *analog,
		char **result);
OTC_API void otc_rational_set(struct otc_rational *r, int64_t p, uint64_t q);
OTC_API int otc_rational_eq(const struct otc_rational *a, const struct otc_rational *b);
OTC_API int otc_rational_mult(struct otc_rational *res, const struct otc_rational *a,
		const struct otc_rational *b);
OTC_API int otc_rational_div(struct otc_rational *res, const struct otc_rational *num,
		const struct otc_rational *div);

/*--- backend.c -------------------------------------------------------------*/

OTC_API int otc_init(struct otc_context **ctx);
OTC_API int otc_exit(struct otc_context *ctx);

OTC_API GSList *otc_buildinfo_libs_get(void);
OTC_API char *otc_buildinfo_host_get(void);
OTC_API char *otc_buildinfo_scpi_backends_get(void);

/*--- conversion.c ----------------------------------------------------------*/

OTC_API int otc_a2l_threshold(const struct otc_datafeed_analog *analog,
		float threshold, uint8_t *output, uint64_t count);
OTC_API int otc_a2l_schmitt_trigger(const struct otc_datafeed_analog *analog,
		float lo_thr, float hi_thr, uint8_t *state, uint8_t *output,
		uint64_t count);

/*--- log.c -----------------------------------------------------------------*/

typedef int (*otc_log_callback)(void *cb_data, int loglevel,
				const char *format, va_list args);

OTC_API int otc_log_loglevel_set(int loglevel);
OTC_API int otc_log_loglevel_get(void);
OTC_API int otc_log_callback_set(otc_log_callback cb, void *cb_data);
OTC_API int otc_log_callback_set_default(void);
OTC_API int otc_log_callback_get(otc_log_callback *cb, void **cb_data);

/*--- device.c --------------------------------------------------------------*/

OTC_API int otc_dev_channel_name_set(struct otc_channel *channel,
		const char *name);
OTC_API int otc_dev_channel_enable(struct otc_channel *channel,
		gboolean state);
OTC_API gboolean otc_dev_has_option(const struct otc_dev_inst *sdi, int key);
OTC_API int otc_dev_config_capabilities_list(const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg, int key);
OTC_API GArray *otc_dev_options(const struct otc_dev_driver *driver,
		const struct otc_dev_inst *sdi, const struct otc_channel_group *cg);
OTC_API GSList *otc_dev_list(const struct otc_dev_driver *driver);
OTC_API int otc_dev_clear(const struct otc_dev_driver *driver);
OTC_API int otc_dev_open(struct otc_dev_inst *sdi);
OTC_API int otc_dev_close(struct otc_dev_inst *sdi);

OTC_API struct otc_dev_driver *otc_dev_inst_driver_get(const struct otc_dev_inst *sdi);
OTC_API const char *otc_dev_inst_vendor_get(const struct otc_dev_inst *sdi);
OTC_API const char *otc_dev_inst_model_get(const struct otc_dev_inst *sdi);
OTC_API const char *otc_dev_inst_version_get(const struct otc_dev_inst *sdi);
OTC_API const char *otc_dev_inst_sernum_get(const struct otc_dev_inst *sdi);
OTC_API const char *otc_dev_inst_connid_get(const struct otc_dev_inst *sdi);
OTC_API GSList *otc_dev_inst_channels_get(const struct otc_dev_inst *sdi);
OTC_API GSList *otc_dev_inst_channel_groups_get(const struct otc_dev_inst *sdi);

OTC_API struct otc_dev_inst *otc_dev_inst_user_new(const char *vendor,
		const char *model, const char *version);
OTC_API int otc_dev_inst_channel_add(struct otc_dev_inst *sdi, int index, int type, const char *name);

/*--- hwdriver.c ------------------------------------------------------------*/

OTC_API struct otc_dev_driver **otc_driver_list(const struct otc_context *ctx);
OTC_API int otc_driver_init(struct otc_context *ctx,
		struct otc_dev_driver *driver);
OTC_API GArray *otc_driver_scan_options_list(const struct otc_dev_driver *driver);
OTC_API GSList *otc_driver_scan(struct otc_dev_driver *driver, GSList *options);
OTC_API int otc_config_get(const struct otc_dev_driver *driver,
		const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg,
		uint32_t key, GVariant **data);
OTC_API int otc_config_set(const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg,
		uint32_t key, GVariant *data);
OTC_API int otc_config_commit(const struct otc_dev_inst *sdi);
OTC_API int otc_config_list(const struct otc_dev_driver *driver,
		const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg,
		uint32_t key, GVariant **data);
OTC_API const struct otc_key_info *otc_key_info_get(int keytype, uint32_t key);
OTC_API const struct otc_key_info *otc_key_info_name_get(int keytype, const char *keyid);

/*--- session.c -------------------------------------------------------------*/

typedef void (*otc_session_stopped_callback)(void *data);
typedef void (*otc_datafeed_callback)(const struct otc_dev_inst *sdi,
		const struct otc_datafeed_packet *packet, void *cb_data);

OTC_API struct otc_trigger *otc_session_trigger_get(struct otc_session *session);

/* Session setup */
OTC_API int otc_session_load(struct otc_context *ctx, const char *filename,
	struct otc_session **session);
OTC_API int otc_session_new(struct otc_context *ctx, struct otc_session **session);
OTC_API int otc_session_destroy(struct otc_session *session);
OTC_API int otc_session_dev_remove_all(struct otc_session *session);
OTC_API int otc_session_dev_add(struct otc_session *session,
		struct otc_dev_inst *sdi);
OTC_API int otc_session_dev_remove(struct otc_session *session,
		struct otc_dev_inst *sdi);
OTC_API int otc_session_dev_list(struct otc_session *session, GSList **devlist);
OTC_API int otc_session_trigger_set(struct otc_session *session, struct otc_trigger *trig);

/* Datafeed setup */
OTC_API int otc_session_datafeed_callback_remove_all(struct otc_session *session);
OTC_API int otc_session_datafeed_callback_add(struct otc_session *session,
		otc_datafeed_callback cb, void *cb_data);

/* Session control */
OTC_API int otc_session_start(struct otc_session *session);
OTC_API int otc_session_run(struct otc_session *session);
OTC_API int otc_session_stop(struct otc_session *session);
OTC_API int otc_session_is_running(struct otc_session *session);
OTC_API int otc_session_stopped_callback_set(struct otc_session *session,
		otc_session_stopped_callback cb, void *cb_data);

OTC_API int otc_packet_copy(const struct otc_datafeed_packet *packet,
		struct otc_datafeed_packet **copy);
OTC_API void otc_packet_free(struct otc_datafeed_packet *packet);

/*--- input/input.c ---------------------------------------------------------*/

OTC_API const struct otc_input_module **otc_input_list(void);
OTC_API const char *otc_input_id_get(const struct otc_input_module *imod);
OTC_API const char *otc_input_name_get(const struct otc_input_module *imod);
OTC_API const char *otc_input_description_get(const struct otc_input_module *imod);
OTC_API const char *const *otc_input_extensions_get(
		const struct otc_input_module *imod);
OTC_API const struct otc_input_module *otc_input_find(const char *id);
OTC_API const struct otc_option **otc_input_options_get(const struct otc_input_module *imod);
OTC_API void otc_input_options_free(const struct otc_option **options);
OTC_API struct otc_input *otc_input_new(const struct otc_input_module *imod,
		GHashTable *options);
OTC_API int otc_input_scan_buffer(GString *buf, const struct otc_input **in);
OTC_API int otc_input_scan_file(const char *filename, const struct otc_input **in);
OTC_API const struct otc_input_module *otc_input_module_get(const struct otc_input *in);
OTC_API struct otc_dev_inst *otc_input_dev_inst_get(const struct otc_input *in);
OTC_API int otc_input_send(const struct otc_input *in, GString *buf);
OTC_API int otc_input_end(const struct otc_input *in);
OTC_API int otc_input_reset(const struct otc_input *in);
OTC_API void otc_input_free(const struct otc_input *in);

/*--- output/output.c -------------------------------------------------------*/

OTC_API const struct otc_output_module **otc_output_list(void);
OTC_API const char *otc_output_id_get(const struct otc_output_module *omod);
OTC_API const char *otc_output_name_get(const struct otc_output_module *omod);
OTC_API const char *otc_output_description_get(const struct otc_output_module *omod);
OTC_API const char *const *otc_output_extensions_get(
		const struct otc_output_module *omod);
OTC_API const struct otc_output_module *otc_output_find(char *id);
OTC_API const struct otc_option **otc_output_options_get(const struct otc_output_module *omod);
OTC_API void otc_output_options_free(const struct otc_option **opts);
OTC_API const struct otc_output *otc_output_new(const struct otc_output_module *omod,
		GHashTable *params, const struct otc_dev_inst *sdi,
		const char *filename);
OTC_API gboolean otc_output_test_flag(const struct otc_output_module *omod,
		uint64_t flag);
OTC_API int otc_output_send(const struct otc_output *o,
		const struct otc_datafeed_packet *packet, GString **out);
OTC_API int otc_output_free(const struct otc_output *o);

/*--- transform/transform.c -------------------------------------------------*/

OTC_API const struct otc_transform_module **otc_transform_list(void);
OTC_API const char *otc_transform_id_get(const struct otc_transform_module *tmod);
OTC_API const char *otc_transform_name_get(const struct otc_transform_module *tmod);
OTC_API const char *otc_transform_description_get(const struct otc_transform_module *tmod);
OTC_API const struct otc_transform_module *otc_transform_find(const char *id);
OTC_API const struct otc_option **otc_transform_options_get(const struct otc_transform_module *tmod);
OTC_API void otc_transform_options_free(const struct otc_option **opts);
OTC_API const struct otc_transform *otc_transform_new(const struct otc_transform_module *tmod,
		GHashTable *params, const struct otc_dev_inst *sdi);
OTC_API int otc_transform_free(const struct otc_transform *t);

/*--- trigger.c -------------------------------------------------------------*/

OTC_API struct otc_trigger *otc_trigger_new(const char *name);
OTC_API void otc_trigger_free(struct otc_trigger *trig);
OTC_API struct otc_trigger_stage *otc_trigger_stage_add(struct otc_trigger *trig);
OTC_API int otc_trigger_match_add(struct otc_trigger_stage *stage,
		struct otc_channel *ch, int trigger_match, float value);

/*--- serial.c --------------------------------------------------------------*/

OTC_API GSList *otc_serial_list(const struct otc_dev_driver *driver);
OTC_API void otc_serial_free(struct otc_serial_port *serial);

/*--- resource.c ------------------------------------------------------------*/

typedef int (*otc_resource_open_callback)(struct otc_resource *res,
		const char *name, void *cb_data);
typedef int (*otc_resource_close_callback)(struct otc_resource *res,
		void *cb_data);
typedef gssize (*otc_resource_read_callback)(const struct otc_resource *res,
		void *buf, size_t count, void *cb_data);

OTC_API GSList *otc_resourcepaths_get(int res_type);

OTC_API int otc_resource_set_hooks(struct otc_context *ctx,
		otc_resource_open_callback open_cb,
		otc_resource_close_callback close_cb,
		otc_resource_read_callback read_cb, void *cb_data);

/*--- strutil.c -------------------------------------------------------------*/

OTC_API char *otc_si_string_u64(uint64_t x, const char *unit);
OTC_API char *otc_samplerate_string(uint64_t samplerate);
OTC_API char *otc_period_string(uint64_t v_p, uint64_t v_q);
OTC_API char *otc_voltage_string(uint64_t v_p, uint64_t v_q);
OTC_API int otc_parse_sizestring(const char *sizestring, uint64_t *size);
OTC_API uint64_t otc_parse_timestring(const char *timestring);
OTC_API gboolean otc_parse_boolstring(const char *boolstring);
OTC_API int otc_parse_period(const char *periodstr, uint64_t *p, uint64_t *q);
OTC_API int otc_parse_voltage(const char *voltstr, uint64_t *p, uint64_t *q);
OTC_API char **otc_parse_probe_names(const char *spec,
	const char **dflt_names, size_t dflt_count,
	size_t max_count, size_t *ret_count);
OTC_API void otc_free_probe_names(char **names);
OTC_API int otc_sprintf_ascii(char *buf, const char *format, ...);
OTC_API int otc_vsprintf_ascii(char *buf, const char *format, va_list args);
OTC_API int otc_snprintf_ascii(char *buf, size_t buf_size,
		const char *format, ...);
OTC_API int otc_vsnprintf_ascii(char *buf, size_t buf_size,
		const char *format, va_list args);
OTC_API int otc_parse_rational(const char *str, struct otc_rational *ret);
OTC_API char *otc_text_trim_spaces(char *s);
OTC_API char *otc_text_next_line(char *s, size_t l, char **next, size_t *taken);
OTC_API char *otc_text_next_word(char *s, char **next);

OTC_API int otc_next_power_of_two(size_t value, size_t *bits, size_t *power);

/*--- version.c -------------------------------------------------------------*/

OTC_API int otc_package_version_major_get(void);
OTC_API int otc_package_version_minor_get(void);
OTC_API int otc_package_version_micro_get(void);
OTC_API const char *otc_package_version_string_get(void);

OTC_API int otc_lib_version_current_get(void);
OTC_API int otc_lib_version_revision_get(void);
OTC_API int otc_lib_version_age_get(void);
OTC_API const char *otc_lib_version_string_get(void);

/*--- error.c ---------------------------------------------------------------*/

OTC_API const char *otc_strerror(int error_code);
OTC_API const char *otc_strerror_name(int error_code);

#endif
