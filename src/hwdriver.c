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
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

/** @cond PRIVATE */
#define LOG_PREFIX "hwdriver"
/** @endcond */

/**
 * @file
 *
 * Hardware driver handling in libopentracecapture.
 */

/**
 * @defgroup grp_driver Hardware drivers
 *
 * Hardware driver handling in libopentracecapture.
 *
 * @{
 */

/* Please use the same order/grouping as in enum otc_configkey (libopentracecapture.h). */
static struct otc_key_info otc_key_info_config[] = {
	/* Device classes */
	{OTC_CONF_LOGIC_ANALYZER, OTC_T_STRING, NULL, "Logic analyzer", NULL},
	{OTC_CONF_OSCILLOSCOPE, OTC_T_STRING, NULL, "Oscilloscope", NULL},
	{OTC_CONF_MULTIMETER, OTC_T_STRING, NULL, "Multimeter", NULL},
	{OTC_CONF_DEMO_DEV, OTC_T_STRING, NULL, "Demo device", NULL},
	{OTC_CONF_SOUNDLEVELMETER, OTC_T_STRING, NULL, "Sound level meter", NULL},
	{OTC_CONF_THERMOMETER, OTC_T_STRING, NULL, "Thermometer", NULL},
	{OTC_CONF_HYGROMETER, OTC_T_STRING, NULL, "Hygrometer", NULL},
	{OTC_CONF_ENERGYMETER, OTC_T_STRING, NULL, "Energy meter", NULL},
	{OTC_CONF_DEMODULATOR, OTC_T_STRING, NULL, "Demodulator", NULL},
	{OTC_CONF_POWER_SUPPLY, OTC_T_STRING, NULL, "Power supply", NULL},
	{OTC_CONF_LCRMETER, OTC_T_STRING, NULL, "LCR meter", NULL},
	{OTC_CONF_ELECTRONIC_LOAD, OTC_T_STRING, NULL, "Electronic load", NULL},
	{OTC_CONF_SCALE, OTC_T_STRING, NULL, "Scale", NULL},
	{OTC_CONF_SIGNAL_GENERATOR, OTC_T_STRING, NULL, "Signal generator", NULL},
	{OTC_CONF_POWERMETER, OTC_T_STRING, NULL, "Power meter", NULL},
	{OTC_CONF_MULTIPLEXER, OTC_T_STRING, NULL, "Multiplexer", NULL},
	{OTC_CONF_DELAY_GENERATOR, OTC_T_STRING, NULL, "Delay generator", NULL},
	{OTC_CONF_FREQUENCY_COUNTER, OTC_T_STRING, NULL, "Frequency counter", NULL},

	/* Driver scan options */
	{OTC_CONF_CONN, OTC_T_STRING, "conn",
		"Connection", NULL},
	{OTC_CONF_SERIALCOMM, OTC_T_STRING, "serialcomm",
		"Serial communication", NULL},
	{OTC_CONF_MODBUSADDR, OTC_T_UINT64, "modbusaddr",
		"Modbus slave address", NULL},
	{OTC_CONF_FORCE_DETECT, OTC_T_STRING, "force_detect",
		"Forced detection", NULL},
	{OTC_CONF_PROBE_NAMES, OTC_T_STRING, "probe_names",
		"Names of device's probes", NULL},

	/* Device (or channel group) configuration */
	{OTC_CONF_SAMPLERATE, OTC_T_UINT64, "samplerate",
		"Sample rate", NULL},
	{OTC_CONF_CAPTURE_RATIO, OTC_T_UINT64, "captureratio",
		"Pre-trigger capture ratio", NULL},
	{OTC_CONF_PATTERN_MODE, OTC_T_STRING, "pattern",
		"Pattern", NULL},
	{OTC_CONF_RLE, OTC_T_BOOL, "rle",
		"Run length encoding", NULL},
	{OTC_CONF_TRIGGER_SLOPE, OTC_T_STRING, "triggerslope",
		"Trigger slope", NULL},
	{OTC_CONF_AVERAGING, OTC_T_BOOL, "averaging",
		"Averaging", NULL},
	{OTC_CONF_AVG_SAMPLES, OTC_T_UINT64, "avg_samples",
		"Number of samples to average over", NULL},
	{OTC_CONF_TRIGGER_SOURCE, OTC_T_STRING, "triggersource",
		"Trigger source", NULL},
	{OTC_CONF_HORIZ_TRIGGERPOS, OTC_T_FLOAT, "horiz_triggerpos",
		"Horizontal trigger position", NULL},
	{OTC_CONF_BUFFERSIZE, OTC_T_UINT64, "buffersize",
		"Buffer size", NULL},
	{OTC_CONF_TIMEBASE, OTC_T_RATIONAL_PERIOD, "timebase",
		"Time base", NULL},
	{OTC_CONF_FILTER, OTC_T_BOOL, "filter",
		"Filter", NULL},
	{OTC_CONF_VDIV, OTC_T_RATIONAL_VOLT, "vdiv",
		"Volts/div", NULL},
	{OTC_CONF_COUPLING, OTC_T_STRING, "coupling",
		"Coupling", NULL},
	{OTC_CONF_TRIGGER_MATCH, OTC_T_INT32, "triggermatch",
		"Trigger matches", NULL},
	{OTC_CONF_SAMPLE_INTERVAL, OTC_T_UINT64, "sample_interval",
		"Sample interval", NULL},
	{OTC_CONF_NUM_HDIV, OTC_T_INT32, "num_hdiv",
		"Number of horizontal divisions", NULL},
	{OTC_CONF_NUM_VDIV, OTC_T_INT32, "num_vdiv",
		"Number of vertical divisions", NULL},
	{OTC_CONF_SPL_WEIGHT_FREQ, OTC_T_STRING, "spl_weight_freq",
		"Sound pressure level frequency weighting", NULL},
	{OTC_CONF_SPL_WEIGHT_TIME, OTC_T_STRING, "spl_weight_time",
		"Sound pressure level time weighting", NULL},
	{OTC_CONF_SPL_MEASUREMENT_RANGE, OTC_T_UINT64_RANGE, "spl_meas_range",
		"Sound pressure level measurement range", NULL},
	{OTC_CONF_HOLD_MAX, OTC_T_BOOL, "hold_max",
		"Hold max", NULL},
	{OTC_CONF_HOLD_MIN, OTC_T_BOOL, "hold_min",
		"Hold min", NULL},
	{OTC_CONF_VOLTAGE_THRESHOLD, OTC_T_DOUBLE_RANGE, "voltage_threshold",
		"Voltage threshold", NULL },
	{OTC_CONF_EXTERNAL_CLOCK, OTC_T_BOOL, "external_clock",
		"External clock mode", NULL},
	{OTC_CONF_SWAP, OTC_T_BOOL, "swap",
		"Swap channel order", NULL},
	{OTC_CONF_CENTER_FREQUENCY, OTC_T_UINT64, "center_frequency",
		"Center frequency", NULL},
	{OTC_CONF_NUM_LOGIC_CHANNELS, OTC_T_INT32, "logic_channels",
		"Number of logic channels", NULL},
	{OTC_CONF_NUM_ANALOG_CHANNELS, OTC_T_INT32, "analog_channels",
		"Number of analog channels", NULL},
	{OTC_CONF_VOLTAGE, OTC_T_FLOAT, "voltage",
		"Current voltage", NULL},
	{OTC_CONF_VOLTAGE_TARGET, OTC_T_FLOAT, "voltage_target",
		"Voltage target", NULL},
	{OTC_CONF_CURRENT, OTC_T_FLOAT, "current",
		"Current current", NULL},
	{OTC_CONF_CURRENT_LIMIT, OTC_T_FLOAT, "current_limit",
		"Current limit", NULL},
	{OTC_CONF_ENABLED, OTC_T_BOOL, "enabled",
		"Channel enabled", NULL},
	{OTC_CONF_CHANNEL_CONFIG, OTC_T_STRING, "channel_config",
		"Channel modes", NULL},
	{OTC_CONF_OVER_VOLTAGE_PROTECTION_ENABLED, OTC_T_BOOL, "ovp_enabled",
		"Over-voltage protection enabled", NULL},
	{OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE, OTC_T_BOOL, "ovp_active",
		"Over-voltage protection active", NULL},
	{OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD, OTC_T_FLOAT, "ovp_threshold",
		"Over-voltage protection threshold", NULL},
	{OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED, OTC_T_BOOL, "ocp_enabled",
		"Over-current protection enabled", NULL},
	{OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE, OTC_T_BOOL, "ocp_active",
		"Over-current protection active", NULL},
	{OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD, OTC_T_FLOAT, "ocp_threshold",
		"Over-current protection threshold", NULL},
	{OTC_CONF_CLOCK_EDGE, OTC_T_STRING, "clock_edge",
		"Clock edge", NULL},
	{OTC_CONF_AMPLITUDE, OTC_T_FLOAT, "amplitude",
		"Amplitude", NULL},
	{OTC_CONF_REGULATION, OTC_T_STRING, "regulation",
		"Channel regulation", NULL},
	{OTC_CONF_OVER_TEMPERATURE_PROTECTION, OTC_T_BOOL, "otp",
		"Over-temperature protection", NULL},
	{OTC_CONF_OUTPUT_FREQUENCY, OTC_T_FLOAT, "output_frequency",
		"Output frequency", NULL},
	{OTC_CONF_OUTPUT_FREQUENCY_TARGET, OTC_T_FLOAT, "output_frequency_target",
		"Output frequency target", NULL},
	{OTC_CONF_MEASURED_QUANTITY, OTC_T_MQ, "measured_quantity",
		"Measured quantity", NULL},
	{OTC_CONF_EQUIV_CIRCUIT_MODEL, OTC_T_STRING, "equiv_circuit_model",
		"Equivalent circuit model", NULL},
	{OTC_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE, OTC_T_BOOL, "otp_active",
		"Over-temperature protection active", NULL},
	{OTC_CONF_UNDER_VOLTAGE_CONDITION, OTC_T_BOOL, "uvc",
		"Under-voltage condition", NULL},
	{OTC_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE, OTC_T_BOOL, "uvc_active",
		"Under-voltage condition active", NULL},
	{OTC_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD, OTC_T_FLOAT, "uvc_threshold",
		"Under-voltage condition threshold", NULL},
	{OTC_CONF_TRIGGER_LEVEL, OTC_T_FLOAT, "triggerlevel",
		"Trigger level", NULL},
	{OTC_CONF_EXTERNAL_CLOCK_SOURCE, OTC_T_STRING, "external_clock_source",
		"External clock source", NULL},
	{OTC_CONF_OFFSET, OTC_T_FLOAT, "offset",
		"Offset", NULL},
	{OTC_CONF_TRIGGER_PATTERN, OTC_T_STRING, "triggerpattern",
		"Trigger pattern", NULL},
	{OTC_CONF_HIGH_RESOLUTION, OTC_T_BOOL, "highresolution",
		"High resolution", NULL},
	{OTC_CONF_PEAK_DETECTION, OTC_T_BOOL, "peakdetection",
		"Peak detection", NULL},
	{OTC_CONF_LOGIC_THRESHOLD, OTC_T_STRING, "logic_threshold",
		"Logic threshold (predefined)", NULL},
	{OTC_CONF_LOGIC_THRESHOLD_CUSTOM, OTC_T_FLOAT, "logic_threshold_custom",
		"Logic threshold (custom)", NULL},
	{OTC_CONF_RANGE, OTC_T_STRING, "range",
		"Range", NULL},
	{OTC_CONF_DIGITS, OTC_T_STRING, "digits",
		"Digits", NULL},
	{OTC_CONF_PHASE, OTC_T_FLOAT, "phase",
		"Phase", NULL},
	{OTC_CONF_DUTY_CYCLE, OTC_T_FLOAT, "output_duty_cycle",
		"Duty Cycle", NULL},
	{OTC_CONF_POWER, OTC_T_FLOAT, "power",
		"Power", NULL},
	{OTC_CONF_POWER_TARGET, OTC_T_FLOAT, "power_target",
		"Power Target", NULL},
	{OTC_CONF_RESISTANCE_TARGET, OTC_T_FLOAT, "resistance_target",
		"Resistance Target", NULL},
	{OTC_CONF_OVER_CURRENT_PROTECTION_DELAY, OTC_T_FLOAT, "ocp_delay",
		"Over-current protection delay", NULL},
	{OTC_CONF_INVERTED, OTC_T_BOOL, "inverted",
		"Signal inverted", NULL},

	/* Special stuff */
	{OTC_CONF_SESSIONFILE, OTC_T_STRING, "sessionfile",
		"Session file", NULL},
	{OTC_CONF_CAPTUREFILE, OTC_T_STRING, "capturefile",
		"Capture file", NULL},
	{OTC_CONF_CAPTURE_UNITSIZE, OTC_T_UINT64, "capture_unitsize",
		"Capture unitsize", NULL},
	{OTC_CONF_POWER_OFF, OTC_T_BOOL, "power_off",
		"Power off", NULL},
	{OTC_CONF_DATA_SOURCE, OTC_T_STRING, "data_source",
		"Data source", NULL},
	{OTC_CONF_PROBE_FACTOR, OTC_T_UINT64, "probe_factor",
		"Probe factor", NULL},
	{OTC_CONF_ADC_POWERLINE_CYCLES, OTC_T_FLOAT, "nplc",
		"Number of ADC powerline cycles", NULL},

	/* Acquisition modes, sample limiting */
	{OTC_CONF_LIMIT_MSEC, OTC_T_UINT64, "limit_time",
		"Time limit", NULL},
	{OTC_CONF_LIMIT_SAMPLES, OTC_T_UINT64, "limit_samples",
		"Sample limit", NULL},
	{OTC_CONF_LIMIT_FRAMES, OTC_T_UINT64, "limit_frames",
		"Frame limit", NULL},
	{OTC_CONF_CONTINUOUS, OTC_T_BOOL, "continuous",
		"Continuous sampling", NULL},
	{OTC_CONF_DATALOG, OTC_T_BOOL, "datalog",
		"Datalog", NULL},
	{OTC_CONF_DEVICE_MODE, OTC_T_STRING, "device_mode",
		"Device mode", NULL},
	{OTC_CONF_TEST_MODE, OTC_T_STRING, "test_mode",
		"Test mode", NULL},

	{OTC_CONF_OVER_POWER_PROTECTION_ENABLED, OTC_T_BOOL, "opp_enabled",
		"Over-power protection enabled", NULL},
	{OTC_CONF_OVER_POWER_PROTECTION_ACTIVE, OTC_T_BOOL, "opp_active",
		"Over-power protection active", NULL},
	{OTC_CONF_OVER_POWER_PROTECTION_THRESHOLD, OTC_T_FLOAT, "opp_threshold",
		"Over-power protection threshold", NULL},

	{OTC_CONF_RESISTANCE, OTC_T_FLOAT, "resistance",
		"Resistance", NULL},

	{OTC_CONF_GATE_TIME, OTC_T_RATIONAL_PERIOD, "gate_time",
		"Gate time", NULL},
	ALL_ZERO
};

/* Please use the same order as in enum otc_mq (libopentracecapture.h). */
static struct otc_key_info otc_key_info_mq[] = {
	{OTC_MQ_VOLTAGE, 0, "voltage", "Voltage", NULL},
	{OTC_MQ_CURRENT, 0, "current", "Current", NULL},
	{OTC_MQ_RESISTANCE, 0, "resistance", "Resistance", NULL},
	{OTC_MQ_CAPACITANCE, 0, "capacitance", "Capacitance", NULL},
	{OTC_MQ_TEMPERATURE, 0, "temperature", "Temperature", NULL},
	{OTC_MQ_FREQUENCY, 0, "frequency", "Frequency", NULL},
	{OTC_MQ_DUTY_CYCLE, 0, "duty_cycle", "Duty cycle", NULL},
	{OTC_MQ_CONTINUITY, 0, "continuity", "Continuity", NULL},
	{OTC_MQ_PULSE_WIDTH, 0, "pulse_width", "Pulse width", NULL},
	{OTC_MQ_CONDUCTANCE, 0, "conductance", "Conductance", NULL},
	{OTC_MQ_POWER, 0, "power", "Power", NULL},
	{OTC_MQ_GAIN, 0, "gain", "Gain", NULL},
	{OTC_MQ_SOUND_PRESSURE_LEVEL, 0, "spl", "Sound pressure level", NULL},
	{OTC_MQ_CARBON_MONOXIDE, 0, "co", "Carbon monoxide", NULL},
	{OTC_MQ_RELATIVE_HUMIDITY, 0, "rh", "Relative humidity", NULL},
	{OTC_MQ_TIME, 0, "time", "Time", NULL},
	{OTC_MQ_WIND_SPEED, 0, "wind_speed", "Wind speed", NULL},
	{OTC_MQ_PRESSURE, 0, "pressure", "Pressure", NULL},
	{OTC_MQ_PARALLEL_INDUCTANCE, 0, "parallel_inductance", "Parallel inductance", NULL},
	{OTC_MQ_PARALLEL_CAPACITANCE, 0, "parallel_capacitance", "Parallel capacitance", NULL},
	{OTC_MQ_PARALLEL_RESISTANCE, 0, "parallel_resistance", "Parallel resistance", NULL},
	{OTC_MQ_SERIES_INDUCTANCE, 0, "series_inductance", "Series inductance", NULL},
	{OTC_MQ_SERIES_CAPACITANCE, 0, "series_capacitance", "Series capacitance", NULL},
	{OTC_MQ_SERIES_RESISTANCE, 0, "series_resistance", "Series resistance", NULL},
	{OTC_MQ_DISSIPATION_FACTOR, 0, "dissipation_factor", "Dissipation factor", NULL},
	{OTC_MQ_QUALITY_FACTOR, 0, "quality_factor", "Quality factor", NULL},
	{OTC_MQ_PHASE_ANGLE, 0, "phase_angle", "Phase angle", NULL},
	{OTC_MQ_DIFFERENCE, 0, "difference", "Difference", NULL},
	{OTC_MQ_COUNT, 0, "count", "Count", NULL},
	{OTC_MQ_POWER_FACTOR, 0, "power_factor", "Power factor", NULL},
	{OTC_MQ_APPARENT_POWER, 0, "apparent_power", "Apparent power", NULL},
	{OTC_MQ_MASS, 0, "mass", "Mass", NULL},
	{OTC_MQ_HARMONIC_RATIO, 0, "harmonic_ratio", "Harmonic ratio", NULL},
	{OTC_MQ_ENERGY, 0, "energy", "Energy", NULL},
	{OTC_MQ_ELECTRIC_CHARGE, 0, "electric_charge", "Electric charge", NULL},
	ALL_ZERO
};

/* Please use the same order as in enum otc_mqflag (libopentracecapture.h). */
static struct otc_key_info otc_key_info_mqflag[] = {
	{OTC_MQFLAG_AC, 0, "ac", "AC", NULL},
	{OTC_MQFLAG_DC, 0, "dc", "DC", NULL},
	{OTC_MQFLAG_RMS, 0, "rms", "RMS", NULL},
	{OTC_MQFLAG_DIODE, 0, "diode", "Diode", NULL},
	{OTC_MQFLAG_HOLD, 0, "hold", "Hold", NULL},
	{OTC_MQFLAG_MAX, 0, "max", "Max", NULL},
	{OTC_MQFLAG_MIN, 0, "min", "Min", NULL},
	{OTC_MQFLAG_AUTORANGE, 0, "auto_range", "Auto range", NULL},
	{OTC_MQFLAG_RELATIVE, 0, "relative", "Relative", NULL},
	{OTC_MQFLAG_SPL_FREQ_WEIGHT_A, 0, "spl_freq_weight_a",
		"Frequency weighted (A)", NULL},
	{OTC_MQFLAG_SPL_FREQ_WEIGHT_C, 0, "spl_freq_weight_c",
		"Frequency weighted (C)", NULL},
	{OTC_MQFLAG_SPL_FREQ_WEIGHT_Z, 0, "spl_freq_weight_z",
		"Frequency weighted (Z)", NULL},
	{OTC_MQFLAG_SPL_FREQ_WEIGHT_FLAT, 0, "spl_freq_weight_flat",
		"Frequency weighted (flat)", NULL},
	{OTC_MQFLAG_SPL_TIME_WEIGHT_S, 0, "spl_time_weight_s",
		"Time weighted (S)", NULL},
	{OTC_MQFLAG_SPL_TIME_WEIGHT_F, 0, "spl_time_weight_f",
		"Time weighted (F)", NULL},
	{OTC_MQFLAG_SPL_LAT, 0, "spl_time_average", "Time-averaged (LEQ)", NULL},
	{OTC_MQFLAG_SPL_PCT_OVER_ALARM, 0, "spl_pct_over_alarm",
		"Percentage over alarm", NULL},
	{OTC_MQFLAG_DURATION, 0, "duration", "Duration", NULL},
	{OTC_MQFLAG_AVG, 0, "average", "Average", NULL},
	{OTC_MQFLAG_REFERENCE, 0, "reference", "Reference", NULL},
	{OTC_MQFLAG_UNSTABLE, 0, "unstable", "Unstable", NULL},
	{OTC_MQFLAG_FOUR_WIRE, 0, "four_wire", "4-Wire", NULL},
	ALL_ZERO
};

/* This must handle all the keys from enum otc_datatype (libopentracecapture.h). */
/** @private */
OTC_PRIV const GVariantType *otc_variant_type_get(int datatype)
{
	switch (datatype) {
	case OTC_T_INT32:
		return G_VARIANT_TYPE_INT32;
	case OTC_T_UINT32:
		return G_VARIANT_TYPE_UINT32;
	case OTC_T_UINT64:
		return G_VARIANT_TYPE_UINT64;
	case OTC_T_STRING:
		return G_VARIANT_TYPE_STRING;
	case OTC_T_BOOL:
		return G_VARIANT_TYPE_BOOLEAN;
	case OTC_T_FLOAT:
		return G_VARIANT_TYPE_DOUBLE;
	case OTC_T_RATIONAL_PERIOD:
	case OTC_T_RATIONAL_VOLT:
	case OTC_T_UINT64_RANGE:
	case OTC_T_DOUBLE_RANGE:
		return G_VARIANT_TYPE_TUPLE;
	case OTC_T_KEYVALUE:
		return G_VARIANT_TYPE_DICTIONARY;
	case OTC_T_MQ:
		return G_VARIANT_TYPE_TUPLE;
	default:
		return NULL;
	}
}

/** @private */
OTC_PRIV int otc_variant_type_check(uint32_t key, GVariant *value)
{
	const struct otc_key_info *info;
	const GVariantType *type, *expected;
	char *expected_string, *type_string;

	info = otc_key_info_get(OTC_KEY_CONFIG, key);
	if (!info)
		return OTC_OK;

	expected = otc_variant_type_get(info->datatype);
	type = g_variant_get_type(value);
	if (!g_variant_type_equal(type, expected)
			&& !g_variant_type_is_subtype_of(type, expected)) {
		expected_string = g_variant_type_dup_string(expected);
		type_string = g_variant_type_dup_string(type);
		otc_err("Wrong variant type for key '%s': expected '%s', got '%s'",
			info->name, expected_string, type_string);
		g_free(expected_string);
		g_free(type_string);
		return OTC_ERR_ARG;
	}

	return OTC_OK;
}

/**
 * Return the list of supported hardware drivers.
 *
 * @param[in] ctx Pointer to a libopentracecapture context struct. Must not be NULL.
 *
 * @retval NULL The ctx argument was NULL, or there are no supported drivers.
 * @retval Other Pointer to the NULL-terminated list of hardware drivers.
 *               The user should NOT g_free() this list, otc_exit() will do that.
 *
 * @since 0.4.0
 */
OTC_API struct otc_dev_driver **otc_driver_list(const struct otc_context *ctx)
{
	if (!ctx)
		return NULL;

	return ctx->driver_list;
}

/**
 * Initialize a hardware driver.
 *
 * This usually involves memory allocations and variable initializations
 * within the driver, but _not_ scanning for attached devices.
 * The API call otc_driver_scan() is used for that.
 *
 * @param ctx A libopentracecapture context object allocated by a previous call to
 *            otc_init(). Must not be NULL.
 * @param driver The driver to initialize. This must be a pointer to one of
 *               the entries returned by otc_driver_list(). Must not be NULL.
 *
 * @retval OTC_OK Success
 * @retval OTC_ERR_ARG Invalid parameter(s).
 * @retval OTC_ERR_BUG Internal errors.
 * @retval other Another negative error code upon other errors.
 *
 * @since 0.2.0
 */
OTC_API int otc_driver_init(struct otc_context *ctx, struct otc_dev_driver *driver)
{
	int ret;

	if (!ctx) {
		otc_err("Invalid libopentracecapture context, can't initialize.");
		return OTC_ERR_ARG;
	}

	if (!driver) {
		otc_err("Invalid driver, can't initialize.");
		return OTC_ERR_ARG;
	}

	/* No log message here, too verbose and not very useful. */

	if ((ret = driver->init(driver, ctx)) < 0)
		otc_err("Failed to initialize the driver: %d.", ret);

	return ret;
}

/**
 * Enumerate scan options supported by this driver.
 *
 * Before calling otc_driver_scan_options_list(), the user must have previously
 * initialized the driver by calling otc_driver_init().
 *
 * @param driver The driver to enumerate options for. This must be a pointer
 *               to one of the entries returned by otc_driver_list(). Must not
 *               be NULL.
 *
 * @return A GArray * of uint32_t entries, or NULL on invalid arguments. Each
 *         entry is a configuration key that is supported as a scan option.
 *         The array must be freed by the caller using g_array_free().
 *
 * @since 0.4.0
 */
OTC_API GArray *otc_driver_scan_options_list(const struct otc_dev_driver *driver)
{
	GVariant *gvar;
	const uint32_t *opts;
	gsize num_opts;
	GArray *result;

	if (otc_config_list(driver, NULL, NULL, OTC_CONF_SCAN_OPTIONS, &gvar) != OTC_OK)
		return NULL;

	opts = g_variant_get_fixed_array(gvar, &num_opts, sizeof(uint32_t));

	result = g_array_sized_new(FALSE, FALSE, sizeof(uint32_t), num_opts);

	g_array_insert_vals(result, 0, opts, num_opts);

	g_variant_unref(gvar);

	return result;
}

static int check_options(struct otc_dev_driver *driver, GSList *options,
		uint32_t optlist_key, struct otc_dev_inst *sdi,
		struct otc_channel_group *cg)
{
	struct otc_config *src;
	const struct otc_key_info *srci;
	GVariant *gvar_opts;
	GSList *l;
	const uint32_t *opts;
	gsize num_opts, i;
	int ret;

	if (otc_config_list(driver, sdi, cg, optlist_key, &gvar_opts) != OTC_OK) {
		/* Driver publishes no options for this optlist. */
		return OTC_ERR;
	}

	ret = OTC_OK;
	opts = g_variant_get_fixed_array(gvar_opts, &num_opts, sizeof(uint32_t));
	for (l = options; l; l = l->next) {
		src = l->data;
		for (i = 0; i < num_opts; i++) {
			if (opts[i] == src->key)
				break;
		}
		if (i == num_opts) {
			if (!(srci = otc_key_info_get(OTC_KEY_CONFIG, src->key)))
				/* Shouldn't happen. */
				otc_err("Invalid option %d.", src->key);
			else
				otc_err("Invalid option '%s'.", srci->id);
			ret = OTC_ERR_ARG;
			break;
		}
		if (otc_variant_type_check(src->key, src->data) != OTC_OK) {
			ret = OTC_ERR_ARG;
			break;
		}
	}
	g_variant_unref(gvar_opts);

	return ret;
}

/**
 * Tell a hardware driver to scan for devices.
 *
 * In addition to the detection, the devices that are found are also
 * initialized automatically. On some devices, this involves a firmware upload,
 * or other such measures.
 *
 * The order in which the system is scanned for devices is not specified. The
 * caller should not assume or rely on any specific order.
 *
 * Before calling otc_driver_scan(), the user must have previously initialized
 * the driver by calling otc_driver_init().
 *
 * @param driver The driver that should scan. This must be a pointer to one of
 *               the entries returned by otc_driver_list(). Must not be NULL.
 * @param options A list of 'struct otc_hwopt' options to pass to the driver's
 *                scanner. Can be NULL/empty.
 *
 * @return A GSList * of 'struct otc_dev_inst', or NULL if no devices were
 *         found (or errors were encountered). This list must be freed by the
 *         caller using g_slist_free(), but without freeing the data pointed
 *         to in the list.
 *
 * @since 0.2.0
 */
OTC_API GSList *otc_driver_scan(struct otc_dev_driver *driver, GSList *options)
{
	GSList *l;

	if (!driver) {
		otc_err("Invalid driver, can't scan for devices.");
		return NULL;
	}

	if (!driver->context) {
		otc_err("Driver not initialized, can't scan for devices.");
		return NULL;
	}

	if (options) {
		if (check_options(driver, options, OTC_CONF_SCAN_OPTIONS, NULL, NULL) != OTC_OK)
			return NULL;
	}

	l = driver->scan(driver, options);

	otc_spew("Scan found %d devices (%s).", g_slist_length(l), driver->name);

	return l;
}

/**
 * Call driver cleanup function for all drivers.
 *
 * @param[in] ctx Pointer to a libopentracecapture context struct. Must not be NULL.
 *
 * @private
 */
OTC_PRIV void otc_hw_cleanup_all(const struct otc_context *ctx)
{
	int i;
	struct otc_dev_driver **drivers;

	if (!ctx)
		return;

	otc_dbg("Cleaning up all drivers.");

	drivers = otc_driver_list(ctx);
	for (i = 0; drivers[i]; i++) {
		if (drivers[i]->cleanup)
			drivers[i]->cleanup(drivers[i]);
		drivers[i]->context = NULL;
	}
}

/**
 * Allocate struct otc_config.
 *
 * A floating reference can be passed in for data.
 *
 * @param key The config key to use.
 * @param data The GVariant data to use.
 *
 * @return The newly allocated struct otc_config. This function is assumed
 *         to never fail.
 *
 * @private
 */
OTC_PRIV struct otc_config *otc_config_new(uint32_t key, GVariant *data)
{
	struct otc_config *src;

	src = g_malloc0(sizeof(struct otc_config));
	src->key = key;
	src->data = g_variant_ref_sink(data);

	return src;
}

/**
 * Free struct otc_config.
 *
 * @private
 */
OTC_PRIV void otc_config_free(struct otc_config *src)
{
	if (!src || !src->data) {
		otc_err("%s: invalid data!", __func__);
		return;
	}

	g_variant_unref(src->data);
	g_free(src);
}

/** @private */
OTC_PRIV int otc_dev_acquisition_start(struct otc_dev_inst *sdi)
{
	if (!sdi || !sdi->driver) {
		otc_err("%s: Invalid arguments.", __func__);
		return OTC_ERR_ARG;
	}

	if (sdi->status != OTC_ST_ACTIVE) {
		otc_err("%s: Device instance not active, can't start.",
			sdi->driver->name);
		return OTC_ERR_DEV_CLOSED;
	}

	otc_dbg("%s: Starting acquisition.", sdi->driver->name);

	return sdi->driver->dev_acquisition_start(sdi);
}

/** @private */
OTC_PRIV int otc_dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	if (!sdi || !sdi->driver) {
		otc_err("%s: Invalid arguments.", __func__);
		return OTC_ERR_ARG;
	}

	if (sdi->status != OTC_ST_ACTIVE) {
		otc_err("%s: Device instance not active, can't stop.",
			sdi->driver->name);
		return OTC_ERR_DEV_CLOSED;
	}

	otc_dbg("%s: Stopping acquisition.", sdi->driver->name);

	return sdi->driver->dev_acquisition_stop(sdi);
}

static void log_key(const struct otc_dev_inst *sdi,
	const struct otc_channel_group *cg, uint32_t key, unsigned int op,
	GVariant *data)
{
	const char *opstr;
	const struct otc_key_info *srci;
	gchar *tmp_str;

	/* Don't log OTC_CONF_DEVICE_OPTIONS, it's verbose and not too useful. */
	if (key == OTC_CONF_DEVICE_OPTIONS)
		return;

	opstr = op == OTC_CONF_GET ? "get" : op == OTC_CONF_SET ? "set" : "list";
	srci = otc_key_info_get(OTC_KEY_CONFIG, key);

	tmp_str = g_variant_print(data, TRUE);
	otc_spew("otc_config_%s(): key %d (%s) sdi %p cg %s -> %s", opstr, key,
		srci ? srci->id : "NULL", sdi, cg ? cg->name : "NULL",
		data ? tmp_str : "NULL");
	g_free(tmp_str);
}

static int check_key(const struct otc_dev_driver *driver,
		const struct otc_dev_inst *sdi, const struct otc_channel_group *cg,
		uint32_t key, unsigned int op, GVariant *data)
{
	const struct otc_key_info *srci;
	gsize num_opts, i;
	GVariant *gvar_opts;
	const uint32_t *opts;
	uint32_t pub_opt;
	const char *suffix;
	const char *opstr;

	if (sdi && cg)
		suffix = " for this device instance and channel group";
	else if (sdi)
		suffix = " for this device instance";
	else
		suffix = "";

	if (!(srci = otc_key_info_get(OTC_KEY_CONFIG, key))) {
		otc_err("Invalid key %d.", key);
		return OTC_ERR_ARG;
	}
	opstr = op == OTC_CONF_GET ? "get" : op == OTC_CONF_SET ? "set" : "list";

	switch (key) {
	case OTC_CONF_LIMIT_MSEC:
	case OTC_CONF_LIMIT_SAMPLES:
	case OTC_CONF_SAMPLERATE:
		/* Setting any of these to 0 is not useful. */
		if (op != OTC_CONF_SET || !data)
			break;
		if (g_variant_get_uint64(data) == 0) {
			otc_err("Cannot set '%s' to 0.", srci->id);
			return OTC_ERR_ARG;
		}
		break;
	case OTC_CONF_CAPTURE_RATIO:
		/* Capture ratio must always be between 0 and 100. */
		if (op != OTC_CONF_SET || !data)
			break;
		if (g_variant_get_uint64(data) > 100) {
			otc_err("Capture ratio must be 0..100.");
			return OTC_ERR_ARG;
		}
		break;
	}

	if (otc_config_list(driver, sdi, cg, OTC_CONF_DEVICE_OPTIONS, &gvar_opts) != OTC_OK) {
		/* Driver publishes no options. */
		otc_err("No options available%s.", suffix);
		return OTC_ERR_ARG;
	}
	opts = g_variant_get_fixed_array(gvar_opts, &num_opts, sizeof(uint32_t));
	pub_opt = 0;
	for (i = 0; i < num_opts; i++) {
		if ((opts[i] & OTC_CONF_MASK) == key) {
			pub_opt = opts[i];
			break;
		}
	}
	g_variant_unref(gvar_opts);
	if (!pub_opt) {
		otc_err("Option '%s' not available%s.", srci->id, suffix);
		return OTC_ERR_ARG;
	}

	if (!(pub_opt & op)) {
		otc_err("Option '%s' not available to %s%s.", srci->id, opstr, suffix);
		return OTC_ERR_ARG;
	}

	return OTC_OK;
}

/**
 * Query value of a configuration key at the given driver or device instance.
 *
 * @param[in] driver The otc_dev_driver struct to query. Must not be NULL.
 * @param[in] sdi (optional) If the key is specific to a device, this must
 *            contain a pointer to the struct otc_dev_inst to be checked.
 *            Otherwise it must be NULL. If sdi is != NULL, sdi->priv must
 *            also be != NULL.
 * @param[in] cg The channel group on the device for which to list the
 *               values, or NULL.
 * @param[in] key The configuration key (OTC_CONF_*).
 * @param[in,out] data Pointer to a GVariant where the value will be stored.
 *             Must not be NULL. The caller is given ownership of the GVariant
 *             and must thus decrease the refcount after use. However if
 *             this function returns an error code, the field should be
 *             considered unused, and should not be unreferenced.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 * @retval OTC_ERR_ARG The driver doesn't know that key, but this is not to be
 *         interpreted as an error by the caller; merely as an indication
 *         that it's not applicable.
 *
 * @since 0.3.0
 */
OTC_API int otc_config_get(const struct otc_dev_driver *driver,
		const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg,
		uint32_t key, GVariant **data)
{
	int ret;

	if (!driver || !data)
		return OTC_ERR;

	if (!driver->config_get)
		return OTC_ERR_ARG;

	if (check_key(driver, sdi, cg, key, OTC_CONF_GET, NULL) != OTC_OK)
		return OTC_ERR_ARG;

	if (sdi && !sdi->priv) {
		otc_err("Can't get config (sdi != NULL, sdi->priv == NULL).");
		return OTC_ERR;
	}

	if ((ret = driver->config_get(key, data, sdi, cg)) == OTC_OK) {
		log_key(sdi, cg, key, OTC_CONF_GET, *data);
		/* Got a floating reference from the driver. Sink it here,
		 * caller will need to unref when done with it. */
		g_variant_ref_sink(*data);
	}

	if (ret == OTC_ERR_CHANNEL_GROUP)
		otc_err("%s: No channel group specified.",
			(sdi) ? sdi->driver->name : "unknown");

	return ret;
}

/**
 * Set value of a configuration key in a device instance.
 *
 * @param[in] sdi The device instance. Must not be NULL. sdi->driver and
 *                sdi->priv must not be NULL either.
 * @param[in] cg The channel group on the device for which to list the
 *                    values, or NULL.
 * @param[in] key The configuration key (OTC_CONF_*).
 * @param data The new value for the key, as a GVariant with GVariantType
 *        appropriate to that key. A floating reference can be passed
 *        in; its refcount will be sunk and unreferenced after use.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 * @retval OTC_ERR_ARG The driver doesn't know that key, but this is not to be
 *         interpreted as an error by the caller; merely as an indication
 *         that it's not applicable.
 *
 * @since 0.3.0
 */
OTC_API int otc_config_set(const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg,
		uint32_t key, GVariant *data)
{
	int ret;

	g_variant_ref_sink(data);

	if (!sdi || !sdi->driver || !sdi->priv || !data)
		ret = OTC_ERR;
	else if (!sdi->driver->config_set)
		ret = OTC_ERR_ARG;
	else if (sdi->status != OTC_ST_ACTIVE) {
		otc_err("%s: Device instance not active, can't set config.",
			sdi->driver->name);
		ret = OTC_ERR_DEV_CLOSED;
	} else if (check_key(sdi->driver, sdi, cg, key, OTC_CONF_SET, data) != OTC_OK)
		return OTC_ERR_ARG;
	else if ((ret = otc_variant_type_check(key, data)) == OTC_OK) {
		log_key(sdi, cg, key, OTC_CONF_SET, data);
		ret = sdi->driver->config_set(key, data, sdi, cg);
	}

	g_variant_unref(data);

	if (ret == OTC_ERR_CHANNEL_GROUP)
		otc_err("%s: No channel group specified.",
			(sdi) ? sdi->driver->name : "unknown");

	return ret;
}

/**
 * Apply configuration settings to the device hardware.
 *
 * @param sdi The device instance.
 *
 * @return OTC_OK upon success or OTC_ERR in case of error.
 *
 * @since 0.3.0
 */
OTC_API int otc_config_commit(const struct otc_dev_inst *sdi)
{
	int ret;

	if (!sdi || !sdi->driver)
		ret = OTC_ERR;
	else if (!sdi->driver->config_commit)
		ret = OTC_OK;
	else if (sdi->status != OTC_ST_ACTIVE) {
		otc_err("%s: Device instance not active, can't commit config.",
			sdi->driver->name);
		ret = OTC_ERR_DEV_CLOSED;
	} else
		ret = sdi->driver->config_commit(sdi);

	return ret;
}

/**
 * List all possible values for a configuration key.
 *
 * @param[in] driver The otc_dev_driver struct to query. Must not be NULL.
 * @param[in] sdi (optional) If the key is specific to a device instance, this
 *            must contain a pointer to the struct otc_dev_inst to be checked.
 *            Otherwise it must be NULL. If sdi is != NULL, sdi->priv must
 *            also be != NULL.
 * @param[in] cg The channel group on the device instance for which to list
 *            the values, or NULL. If this device instance doesn't
 *            have channel groups, this must not be != NULL.
 *            If cg is NULL, this function will return the "common" device
 *            instance options that are channel-group independent. Otherwise
 *            it will return the channel-group specific options.
 * @param[in] key The configuration key (OTC_CONF_*).
 * @param[in,out] data A pointer to a GVariant where the list will be stored.
 *                The caller is given ownership of the GVariant and must thus
 *                unref the GVariant after use. However if this function
 *                returns an error code, the field should be considered
 *                unused, and should not be unreferenced.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 * @retval OTC_ERR_ARG The driver doesn't know that key, but this is not to be
 *         interpreted as an error by the caller; merely as an indication
 *         that it's not applicable.
 *
 * @since 0.3.0
 */
OTC_API int otc_config_list(const struct otc_dev_driver *driver,
		const struct otc_dev_inst *sdi,
		const struct otc_channel_group *cg,
		uint32_t key, GVariant **data)
{
	int ret;

	if (!driver || !data)
		return OTC_ERR;

	if (!driver->config_list)
		return OTC_ERR_ARG;

	if (key != OTC_CONF_SCAN_OPTIONS && key != OTC_CONF_DEVICE_OPTIONS) {
		if (check_key(driver, sdi, cg, key, OTC_CONF_LIST, NULL) != OTC_OK)
			return OTC_ERR_ARG;
	}

	if (sdi && !sdi->priv) {
		otc_err("Can't list config (sdi != NULL, sdi->priv == NULL).");
		return OTC_ERR;
	}

	if (key != OTC_CONF_SCAN_OPTIONS && key != OTC_CONF_DEVICE_OPTIONS && !sdi) {
		otc_err("Config keys other than OTC_CONF_SCAN_OPTIONS and "
		       "OTC_CONF_DEVICE_OPTIONS always need an sdi.");
		return OTC_ERR_ARG;
	}

	if (cg && sdi && !sdi->channel_groups) {
		otc_err("Can't list config for channel group, there are none.");
		return OTC_ERR_ARG;
	}

	if (cg && sdi && !g_slist_find(sdi->channel_groups, cg)) {
		otc_err("If a channel group is specified, it must be a valid one.");
		return OTC_ERR_ARG;
	}

	if (cg && !sdi) {
		otc_err("Need sdi when a channel group is specified.");
		return OTC_ERR_ARG;
	}

	if ((ret = driver->config_list(key, data, sdi, cg)) == OTC_OK) {
		log_key(sdi, cg, key, OTC_CONF_LIST, *data);
		g_variant_ref_sink(*data);
	}

	if (ret == OTC_ERR_CHANNEL_GROUP)
		otc_err("%s: No channel group specified.",
			(sdi) ? sdi->driver->name : "unknown");

	return ret;
}

static struct otc_key_info *get_keytable(int keytype)
{
	struct otc_key_info *table;

	switch (keytype) {
	case OTC_KEY_CONFIG:
		table = otc_key_info_config;
		break;
	case OTC_KEY_MQ:
		table = otc_key_info_mq;
		break;
	case OTC_KEY_MQFLAGS:
		table = otc_key_info_mqflag;
		break;
	default:
		otc_err("Invalid keytype %d", keytype);
		return NULL;
	}

	return table;
}

/**
 * Get information about a key, by key.
 *
 * @param[in] keytype The namespace the key is in.
 * @param[in] key The key to find.
 *
 * @return A pointer to a struct otc_key_info, or NULL if the key
 *         was not found.
 *
 * @since 0.3.0
 */
OTC_API const struct otc_key_info *otc_key_info_get(int keytype, uint32_t key)
{
	struct otc_key_info *table;
	int i;

	if (!(table = get_keytable(keytype)))
		return NULL;

	for (i = 0; table[i].key; i++) {
		if (table[i].key == key)
			return &table[i];
	}

	return NULL;
}

/**
 * Get information about a key, by name.
 *
 * @param[in] keytype The namespace the key is in.
 * @param[in] keyid The key id string.
 *
 * @return A pointer to a struct otc_key_info, or NULL if the key
 *         was not found.
 *
 * @since 0.2.0
 */
OTC_API const struct otc_key_info *otc_key_info_name_get(int keytype, const char *keyid)
{
	struct otc_key_info *table;
	int i;

	if (!(table = get_keytable(keytype)))
		return NULL;

	for (i = 0; table[i].key; i++) {
		if (!table[i].id)
			continue;
		if (!strcmp(table[i].id, keyid))
			return &table[i];
	}

	return NULL;
}

/** @} */
