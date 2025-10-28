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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif

#ifndef OPENTRACECAPTURE_LIBSIGROK_H
#define OPENTRACECAPTURE_LIBSIGROK_H

#include <stdio.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * The public libopentracecapture header file to be used by frontends.
 *
 * This is the only file that libopentracecapture users (frontends) are supposed to
 * use and \#include. There are other header files which get installed with
 * libopentracecapture, but those are not meant to be used directly by frontends.
 *
 * The correct way to get/use the libopentracecapture API functions is:
 *
 * @code{.c}
 *   #include <libopentracecapture/libopentracecapture.h>
 * @endcode
 */

/*
 * All possible return codes of libopentracecapture functions must be listed here.
 * Functions should never return hardcoded numbers as status, but rather
 * use these enum values. All error codes are negative numbers.
 *
 * The error codes are globally unique in libopentracecapture, i.e. if one of the
 * libopentracecapture functions returns a "malloc error" it must be exactly the same
 * return value as used by all other functions to indicate "malloc error".
 * There must be no functions which indicate two different errors via the
 * same return code.
 *
 * Also, for compatibility reasons, no defined return codes are ever removed
 * or reused for different errors later. You can only add new entries and
 * return codes, but never remove or redefine existing ones.
 */

/** Status/error codes returned by libopentracecapture functions. */
enum otc_error_code {
	OTC_OK                =  0, /**< No error. */
	OTC_ERR               = -1, /**< Generic/unspecified error. */
	OTC_ERR_MALLOC        = -2, /**< Malloc/calloc/realloc error. */
	OTC_ERR_ARG           = -3, /**< Function argument error. */
	OTC_ERR_BUG           = -4, /**< Errors hinting at internal bugs. */
	OTC_ERR_SAMPLERATE    = -5, /**< Incorrect samplerate. */
	OTC_ERR_NA            = -6, /**< Not applicable. */
	OTC_ERR_DEV_CLOSED    = -7, /**< Device is closed, but must be open. */
	OTC_ERR_TIMEOUT       = -8, /**< A timeout occurred. */
	OTC_ERR_CHANNEL_GROUP = -9, /**< A channel group must be specified. */
	OTC_ERR_DATA          =-10, /**< Data is invalid.  */
	OTC_ERR_IO            =-11, /**< Input/output error. */

	/* Update otc_strerror()/otc_strerror_name() (error.c) upon changes! */
};

/** Ternary return type for DMM/LCR/etc packet parser validity checks. */
enum otc_valid_code {
	OTC_PACKET_INVALID = -1,	/**< Certainly invalid. */
	OTC_PACKET_VALID = 0,	/**< Certainly valid. */
	OTC_PACKET_NEED_RX = +1,	/**< Need more RX data. */
};

#define OTC_MAX_CHANNELNAME_LEN 32

/* Handy little macros */
#define OTC_HZ(n)  (n)
#define OTC_KHZ(n) ((n) * UINT64_C(1000))
#define OTC_MHZ(n) ((n) * UINT64_C(1000000))
#define OTC_GHZ(n) ((n) * UINT64_C(1000000000))

#define OTC_HZ_TO_NS(n) (UINT64_C(1000000000) / (n))

/** libopentracecapture loglevels. */
enum otc_loglevel {
	OTC_LOG_NONE = 0, /**< Output no messages at all. */
	OTC_LOG_ERR  = 1, /**< Output error messages. */
	OTC_LOG_WARN = 2, /**< Output warnings. */
	OTC_LOG_INFO = 3, /**< Output informational messages. */
	OTC_LOG_DBG  = 4, /**< Output debug messages. */
	OTC_LOG_SPEW = 5, /**< Output very noisy debug messages. */
};

/*
 * Use OTC_API to mark public API symbols, and OTC_PRIV for private symbols.
 *
 * Variables and functions marked 'static' are private already and don't
 * need OTC_PRIV. However, functions which are not static (because they need
 * to be used in other libopentracecapture-internal files) but are also not meant to
 * be part of the public libopentracecapture API, must use OTC_PRIV.
 *
 * This uses the 'visibility' feature of gcc (requires gcc >= 4.0).
 *
 * This feature is not available on MinGW/Windows, as it is a feature of
 * ELF files and MinGW/Windows uses PE files.
 *
 * Details: http://gcc.gnu.org/wiki/Visibility
 */

/* Marks public libopentracecapture API symbols. */
#ifdef _MSC_VER
#ifdef OPENTRACECAPTURE_BUILD
#define OTC_API __declspec(dllexport)
#else
#define OTC_API __declspec(dllimport)
#endif
#elif !defined(_WIN32)
#define OTC_API __attribute__((visibility("default")))
#else
#define OTC_API
#endif

/* Marks private, non-public libopentracecapture symbols (not part of the API). */
#ifdef _MSC_VER
#define OTC_PRIV
#elif !defined(_WIN32)
#define OTC_PRIV __attribute__((visibility("hidden")))
#else
#define OTC_PRIV
#endif

/** Type definition for callback function for data reception. */
typedef int (*otc_receive_data_callback)(int fd, int revents, void *cb_data);

/** Data types used by otc_config_info(). */
enum otc_datatype {
	OTC_T_UINT64 = 10000,
	OTC_T_STRING,
	OTC_T_BOOL,
	OTC_T_FLOAT,
	OTC_T_RATIONAL_PERIOD,
	OTC_T_RATIONAL_VOLT,
	OTC_T_KEYVALUE,
	OTC_T_UINT64_RANGE,
	OTC_T_DOUBLE_RANGE,
	OTC_T_INT32,
	OTC_T_MQ,
	OTC_T_UINT32,

	/* Update otc_variant_type_get() (hwdriver.c) upon changes! */
};

/** Value for otc_datafeed_packet.type. */
enum otc_packettype {
	/** Payload is otc_datafeed_header. */
	OTC_DF_HEADER = 10000,
	/** End of stream (no further data). */
	OTC_DF_END,
	/** Payload is struct otc_datafeed_meta */
	OTC_DF_META,
	/** The trigger matched at this point in the data feed. No payload. */
	OTC_DF_TRIGGER,
	/** Payload is struct otc_datafeed_logic. */
	OTC_DF_LOGIC,
	/** Beginning of frame. No payload. */
	OTC_DF_FRAME_BEGIN,
	/** End of frame. No payload. */
	OTC_DF_FRAME_END,
	/** Payload is struct otc_datafeed_analog. */
	OTC_DF_ANALOG,

	/* Update datafeed_dump() (session.c) upon changes! */
};

/** Measured quantity, otc_analog_meaning.mq. */
enum otc_mq {
	OTC_MQ_VOLTAGE = 10000,
	OTC_MQ_CURRENT,
	OTC_MQ_RESISTANCE,
	OTC_MQ_CAPACITANCE,
	OTC_MQ_TEMPERATURE,
	OTC_MQ_FREQUENCY,
	/** Duty cycle, e.g. on/off ratio. */
	OTC_MQ_DUTY_CYCLE,
	/** Continuity test. */
	OTC_MQ_CONTINUITY,
	OTC_MQ_PULSE_WIDTH,
	OTC_MQ_CONDUCTANCE,
	/** Electrical power, usually in W, or dBm. */
	OTC_MQ_POWER,
	/** Gain (a transistor's gain, or hFE, for example). */
	OTC_MQ_GAIN,
	/** Logarithmic representation of sound pressure relative to a
	 * reference value. */
	OTC_MQ_SOUND_PRESSURE_LEVEL,
	/** Carbon monoxide level */
	OTC_MQ_CARBON_MONOXIDE,
	/** Humidity */
	OTC_MQ_RELATIVE_HUMIDITY,
	/** Time */
	OTC_MQ_TIME,
	/** Wind speed */
	OTC_MQ_WIND_SPEED,
	/** Pressure */
	OTC_MQ_PRESSURE,
	/** Parallel inductance (LCR meter model). */
	OTC_MQ_PARALLEL_INDUCTANCE,
	/** Parallel capacitance (LCR meter model). */
	OTC_MQ_PARALLEL_CAPACITANCE,
	/** Parallel resistance (LCR meter model). */
	OTC_MQ_PARALLEL_RESISTANCE,
	/** Series inductance (LCR meter model). */
	OTC_MQ_SERIES_INDUCTANCE,
	/** Series capacitance (LCR meter model). */
	OTC_MQ_SERIES_CAPACITANCE,
	/** Series resistance (LCR meter model). */
	OTC_MQ_SERIES_RESISTANCE,
	/** Dissipation factor. */
	OTC_MQ_DISSIPATION_FACTOR,
	/** Quality factor. */
	OTC_MQ_QUALITY_FACTOR,
	/** Phase angle. */
	OTC_MQ_PHASE_ANGLE,
	/** Difference from reference value. */
	OTC_MQ_DIFFERENCE,
	/** Count. */
	OTC_MQ_COUNT,
	/** Power factor. */
	OTC_MQ_POWER_FACTOR,
	/** Apparent power */
	OTC_MQ_APPARENT_POWER,
	/** Mass */
	OTC_MQ_MASS,
	/** Harmonic ratio */
	OTC_MQ_HARMONIC_RATIO,
	/** Energy. */
	OTC_MQ_ENERGY,
	/** Electric charge. */
	OTC_MQ_ELECTRIC_CHARGE,

	/* Update otc_key_info_mq[] (hwdriver.c) upon changes! */
};

/** Unit of measured quantity, otc_analog_meaning.unit. */
enum otc_unit {
	/** Volt */
	OTC_UNIT_VOLT = 10000,
	/** Ampere (current). */
	OTC_UNIT_AMPERE,
	/** Ohm (resistance). */
	OTC_UNIT_OHM,
	/** Farad (capacity). */
	OTC_UNIT_FARAD,
	/** Kelvin (temperature). */
	OTC_UNIT_KELVIN,
	/** Degrees Celsius (temperature). */
	OTC_UNIT_CELSIUS,
	/** Degrees Fahrenheit (temperature). */
	OTC_UNIT_FAHRENHEIT,
	/** Hertz (frequency, 1/s, [Hz]). */
	OTC_UNIT_HERTZ,
	/** Percent value. */
	OTC_UNIT_PERCENTAGE,
	/** Boolean value. */
	OTC_UNIT_BOOLEAN,
	/** Time in seconds. */
	OTC_UNIT_SECOND,
	/** Unit of conductance, the inverse of resistance. */
	OTC_UNIT_SIEMENS,
	/**
	 * An absolute measurement of power, in decibels, referenced to
	 * 1 milliwatt (dBm).
	 */
	OTC_UNIT_DECIBEL_MW,
	/** Voltage in decibel, referenced to 1 volt (dBV). */
	OTC_UNIT_DECIBEL_VOLT,
	/**
	 * Measurements that intrinsically do not have units attached, such
	 * as ratios, gains, etc. Specifically, a transistor's gain (hFE) is
	 * a unitless quantity, for example.
	 */
	OTC_UNIT_UNITLESS,
	/** Sound pressure level, in decibels, relative to 20 micropascals. */
	OTC_UNIT_DECIBEL_SPL,
	/**
	 * Normalized (0 to 1) concentration of a substance or compound with 0
	 * representing a concentration of 0%, and 1 being 100%. This is
	 * represented as the fraction of number of particles of the substance.
	 */
	OTC_UNIT_CONCENTRATION,
	/** Revolutions per minute. */
	OTC_UNIT_REVOLUTIONS_PER_MINUTE,
	/** Apparent power [VA]. */
	OTC_UNIT_VOLT_AMPERE,
	/** Real power [W]. */
	OTC_UNIT_WATT,
	/** Energy (consumption) in watt hour [Wh]. */
	OTC_UNIT_WATT_HOUR,
	/** Wind speed in meters per second. */
	OTC_UNIT_METER_SECOND,
	/** Pressure in hectopascal */
	OTC_UNIT_HECTOPASCAL,
	/** Relative humidity assuming air temperature of 293 Kelvin (%rF). */
	OTC_UNIT_HUMIDITY_293K,
	/** Plane angle in 1/360th of a full circle. */
	OTC_UNIT_DEGREE,
	/** Henry (inductance). */
	OTC_UNIT_HENRY,
	/** Mass in gram [g]. */
	OTC_UNIT_GRAM,
	/** Mass in carat [ct]. */
	OTC_UNIT_CARAT,
	/** Mass in ounce [oz]. */
	OTC_UNIT_OUNCE,
	/** Mass in troy ounce [oz t]. */
	OTC_UNIT_TROY_OUNCE,
	/** Mass in pound [lb]. */
	OTC_UNIT_POUND,
	/** Mass in pennyweight [dwt]. */
	OTC_UNIT_PENNYWEIGHT,
	/** Mass in grain [gr]. */
	OTC_UNIT_GRAIN,
	/** Mass in tael (variants: Hong Kong, Singapore/Malaysia, Taiwan) */
	OTC_UNIT_TAEL,
	/** Mass in momme. */
	OTC_UNIT_MOMME,
	/** Mass in tola. */
	OTC_UNIT_TOLA,
	/** Pieces (number of items). */
	OTC_UNIT_PIECE,
	/** Energy in joule. */
	OTC_UNIT_JOULE,
	/** Electric charge in coulomb. */
	OTC_UNIT_COULOMB,
	/** Electric charge in ampere hour [Ah]. */
	OTC_UNIT_AMPERE_HOUR,
	/** Mass in dram [dr]. */
	OTC_UNIT_DRAM,
	/** Area density in g/m^2. */
	OTC_UNIT_GRAMMAGE,

	/*
	 * Update unit_strings[] (analog.c) and fancyprint() (output/analog.c)
	 * upon changes!
	 */
};

/** Values for otc_analog_meaning.mqflags. */
enum otc_mqflag {
	/** Voltage measurement is alternating current (AC). */
	OTC_MQFLAG_AC = 0x01,
	/** Voltage measurement is direct current (DC). */
	OTC_MQFLAG_DC = 0x02,
	/** This is a true RMS measurement. */
	OTC_MQFLAG_RMS = 0x04,
	/** Value is voltage drop across a diode, or NAN. */
	OTC_MQFLAG_DIODE = 0x08,
	/** Device is in "hold" mode (repeating the last measurement). */
	OTC_MQFLAG_HOLD = 0x10,
	/** Device is in "max" mode, only updating upon a new max value. */
	OTC_MQFLAG_MAX = 0x20,
	/** Device is in "min" mode, only updating upon a new min value. */
	OTC_MQFLAG_MIN = 0x40,
	/** Device is in autoranging mode. */
	OTC_MQFLAG_AUTORANGE = 0x80,
	/** Device is in relative mode. */
	OTC_MQFLAG_RELATIVE = 0x100,
	/** Sound pressure level is A-weighted in the frequency domain,
	 * according to IEC 61672:2003. */
	OTC_MQFLAG_SPL_FREQ_WEIGHT_A = 0x200,
	/** Sound pressure level is C-weighted in the frequency domain,
	 * according to IEC 61672:2003. */
	OTC_MQFLAG_SPL_FREQ_WEIGHT_C = 0x400,
	/** Sound pressure level is Z-weighted (i.e. not at all) in the
	 * frequency domain, according to IEC 61672:2003. */
	OTC_MQFLAG_SPL_FREQ_WEIGHT_Z = 0x800,
	/** Sound pressure level is not weighted in the frequency domain,
	 * albeit without standards-defined low and high frequency limits. */
	OTC_MQFLAG_SPL_FREQ_WEIGHT_FLAT = 0x1000,
	/** Sound pressure level measurement is S-weighted (1s) in the
	 * time domain. */
	OTC_MQFLAG_SPL_TIME_WEIGHT_S = 0x2000,
	/** Sound pressure level measurement is F-weighted (125ms) in the
	 * time domain. */
	OTC_MQFLAG_SPL_TIME_WEIGHT_F = 0x4000,
	/** Sound pressure level is time-averaged (LAT), also known as
	 * Equivalent Continuous A-weighted Sound Level (LEQ). */
	OTC_MQFLAG_SPL_LAT = 0x8000,
	/** Sound pressure level represented as a percentage of measurements
	 * that were over a preset alarm level. */
	OTC_MQFLAG_SPL_PCT_OVER_ALARM = 0x10000,
	/** Time is duration (as opposed to epoch, ...). */
	OTC_MQFLAG_DURATION = 0x20000,
	/** Device is in "avg" mode, averaging upon each new value. */
	OTC_MQFLAG_AVG = 0x40000,
	/** Reference value shown. */
	OTC_MQFLAG_REFERENCE = 0x80000,
	/** Unstable value (hasn't settled yet). */
	OTC_MQFLAG_UNSTABLE = 0x100000,
	/** Measurement is four wire (e.g. Kelvin connection). */
	OTC_MQFLAG_FOUR_WIRE = 0x200000,
	/** Tael measurement (Taiwan variant, 37.50 g/tael). */
	OTC_MQFLAG_TAEL_TAIWAN = 0x400000,
	/** Tael measurement (Hong Kong/Troy variant, 37.43 g/tael). */
	OTC_MQFLAG_TAEL_HONGKONG_TROY = 0x800000,
	/** Tael measurement (Japan variant, 37.80 g/tael). */
	OTC_MQFLAG_TAEL_JAPAN = 0x1000000,

	/*
	 * Update mq_strings[] (analog.c) and fancyprint() (output/analog.c)
	 * upon changes!
	 */
};

enum otc_trigger_matches {
	OTC_TRIGGER_ZERO = 1,
	OTC_TRIGGER_ONE,
	OTC_TRIGGER_RISING,
	OTC_TRIGGER_FALLING,
	OTC_TRIGGER_EDGE,
	OTC_TRIGGER_OVER,
	OTC_TRIGGER_UNDER,
};

/** The representation of a trigger, consisting of one or more stages
 * containing one or more matches on a channel.
 */
struct otc_trigger {
	/** A name for this trigger. This may be NULL if none is needed. */
	char *name;
	/** List of pointers to struct otc_trigger_stage. */
	GSList *stages;
};

/** A trigger stage. */
struct otc_trigger_stage {
	/** Starts at 0. */
	int stage;
	/** List of pointers to struct otc_trigger_match. */
	GSList *matches;
};

/** A channel to match and what to match it on. */
struct otc_trigger_match {
	/** The channel to trigger on. */
	struct otc_channel *channel;
	/** The trigger match to use.
	 * For logic channels, only the following matches may be used:
	 * OTC_TRIGGER_ZERO
	 * OTC_TRIGGER_ONE
	 * OTC_TRIGGER_RISING
	 * OTC_TRIGGER_FALLING
	 * OTC_TRIGGER_EDGE
	 *
	 * For analog channels, only these matches may be used:
	 * OTC_TRIGGER_RISING
	 * OTC_TRIGGER_FALLING
	 * OTC_TRIGGER_OVER
	 * OTC_TRIGGER_UNDER
	 *
	 */
	int match;
	/** If the trigger match is one of OTC_TRIGGER_OVER or OTC_TRIGGER_UNDER,
	 * this contains the value to compare against. */
	float value;
};

/**
 * @struct otc_context
 * Opaque structure representing a libopentracecapture context.
 *
 * None of the fields of this structure are meant to be accessed directly.
 *
 * @see otc_init(), otc_exit().
 */
struct otc_context;

/**
 * @struct otc_session
 * Opaque structure representing a libopentracecapture session.
 *
 * None of the fields of this structure are meant to be accessed directly.
 *
 * @see otc_session_new(), otc_session_destroy().
 */
struct otc_session;

struct otc_rational {
	/** Numerator of the rational number. */
	int64_t p;
	/** Denominator of the rational number. */
	uint64_t q;
};

/** Packet in a opentracelab data feed. */
struct otc_datafeed_packet {
	uint16_t type;
	const void *payload;
};

/** Header of a opentracelab data feed. */
struct otc_datafeed_header {
	int feed_version;
	struct timeval starttime;
};

/** Datafeed payload for type OTC_DF_META. */
struct otc_datafeed_meta {
	GSList *config;
};

/** Logic datafeed payload for type OTC_DF_LOGIC. */
struct otc_datafeed_logic {
	uint64_t length;
	uint16_t unitsize;
	void *data;
};

/** Analog datafeed payload for type OTC_DF_ANALOG. */
struct otc_datafeed_analog {
	void *data;
	uint32_t num_samples;
	struct otc_analog_encoding *encoding;
	struct otc_analog_meaning *meaning;
	struct otc_analog_spec *spec;
};

struct otc_analog_encoding {
	uint8_t unitsize;
	gboolean is_signed;
	gboolean is_float;
	gboolean is_bigendian;
	/**
	 * Number of significant digits after the decimal point, if positive.
	 * When negative, exponent with reversed polarity that is necessary to
	 * express the value with all digits without a decimal point.
	 * Refers to the value we actually read on the wire.
	 *
	 * Examples:
	 *
	 * | Disp. value | Exp. notation       | Exp. not. normalized | digits |
	 * |-------------|---------------------|----------------------|--------|
	 * |  12.34 MOhm |  1.234 * 10^7   Ohm |    1234 * 10^4   Ohm |     -4 |
	 * | 1.2345 MOhm | 1.2345 * 10^6   Ohm |   12345 * 10^2   Ohm |     -2 |
	 * |  123.4 kOhm |  1.234 * 10^5   Ohm |    1234 * 10^2   Ohm |     -2 |
	 * |   1234  Ohm |  1.234 * 10^3   Ohm |    1234 * 10^0   Ohm |      0 |
	 * |  12.34  Ohm |  1.234 * 10^1   Ohm |    1234 * 10^-2  Ohm |      2 |
	 * | 0.0123  Ohm |   1.23 * 10^-2  Ohm |     123 * 10^-4  Ohm |      4 |
	 * |  1.234 pF   |  1.234 * 10^-12 F   |    1234 * 10^-15 F   |     15 |
	 */
	int8_t digits;
	gboolean is_digits_decimal;
	struct otc_rational scale;
	struct otc_rational offset;
};

struct otc_analog_meaning {
	enum otc_mq mq;
	enum otc_unit unit;
	enum otc_mqflag mqflags;
	GSList *channels;
};

struct otc_analog_spec {
	/**
	 * Number of significant digits after the decimal point, if positive.
	 * When negative, exponent with reversed polarity that is necessary to
	 * express the value with all digits without a decimal point.
	 * Refers to vendor specifications/datasheet or actual device display.
	 *
	 * Examples:
	 *
	 * | On the wire | Exp. notation       | Exp. not. normalized | spec_digits |
	 * |-------------|---------------------|----------------------|-------------|
	 * |  12.34 MOhm |  1.234 * 10^7   Ohm |    1234 * 10^4   Ohm |          -4 |
	 * | 1.2345 MOhm | 1.2345 * 10^6   Ohm |   12345 * 10^2   Ohm |          -2 |
	 * |  123.4 kOhm |  1.234 * 10^5   Ohm |    1234 * 10^2   Ohm |          -2 |
	 * |   1234  Ohm |  1.234 * 10^3   Ohm |    1234 * 10^0   Ohm |           0 |
	 * |  12.34  Ohm |  1.234 * 10^1   Ohm |    1234 * 10^-2  Ohm |           2 |
	 * | 0.0123  Ohm |   1.23 * 10^-2  Ohm |     123 * 10^-4  Ohm |           4 |
	 * |  1.234 pF   |  1.234 * 10^-12 F   |    1234 * 10^-15 F   |          15 |
	 */
	int8_t spec_digits;
};

/** Generic option struct used by various subsystems. */
struct otc_option {
	/* Short name suitable for commandline usage, [a-z0-9-]. */
	const char *id;
	/* Short name suitable for GUI usage, can contain UTF-8. */
	const char *name;
	/* Description of the option, in a sentence. */
	const char *desc;
	/* Default value for this option. */
	GVariant *def;
	/* List of possible values, if this is an option with few values. */
	GSList *values;
};

/** Resource type.
 * @since 0.4.0
 */
enum otc_resource_type {
	OTC_RESOURCE_FIRMWARE = 1,
};

/** Resource descriptor.
 * @since 0.4.0
 */
struct otc_resource {
	/** Size of resource in bytes; set by resource open callback. */
	uint64_t size;
	/** File handle or equivalent; set by resource open callback. */
	void *handle;
	/** Resource type (OTC_RESOURCE_FIRMWARE, ...) */
	int type;
};

/** Output module flags. */
enum otc_output_flag {
	/** If set, this output module writes the output itself. */
	OTC_OUTPUT_INTERNAL_IO_HANDLING = 0x01,
};

struct otc_input;
struct otc_input_module;
struct otc_output;
struct otc_output_module;
struct otc_transform;
struct otc_transform_module;

/** Constants for channel type. */
enum otc_channeltype {
	/** Channel type is logic channel. */
	OTC_CHANNEL_LOGIC = 10000,
	/** Channel type is analog channel. */
	OTC_CHANNEL_ANALOG,
};

/** Information on single channel. */
struct otc_channel {
	/** The device this channel is attached to. */
	struct otc_dev_inst *sdi;
	/** The index of this channel, starting at 0. Logic channels will
	 * be encoded according to this index in OTC_DF_LOGIC packets. */
	int index;
	/** Channel type (OTC_CHANNEL_LOGIC, ...) */
	int type;
	/** Is this channel enabled? */
	gboolean enabled;
	/** Name of channel. */
	char *name;
	/** Private data for driver use. */
	void *priv;
};

/** Structure for groups of channels that have common properties. */
struct otc_channel_group {
	/** Name of the channel group. */
	char *name;
	/** List of otc_channel structs of the channels belonging to this group. */
	GSList *channels;
	/** Private data for driver use. */
	void *priv;
};

/** Used for setting or getting value of a config item. */
struct otc_config {
	/** Config key like OTC_CONF_CONN, etc. */
	uint32_t key;
	/** Key-specific data. */
	GVariant *data;
};

enum otc_keytype {
	OTC_KEY_CONFIG,
	OTC_KEY_MQ,
	OTC_KEY_MQFLAGS,
};

/** Information about a key. */
struct otc_key_info {
	/** Config key like OTC_CONF_CONN, MQ value like OTC_MQ_VOLTAGE, etc. */
	uint32_t key;
	/** Data type like OTC_T_STRING, etc if applicable. */
	int datatype;
	/** Short, lowercase ID string, e.g. "serialcomm", "voltage". */
	const char *id;
	/** Full capitalized name, e.g. "Serial communication". */
	const char *name;
	/** Verbose description (unused currently). */
	const char *description;
};

/** Configuration capabilities. */
enum otc_configcap {
	/** Value can be read. */
	OTC_CONF_GET = (1UL << 31),
	/** Value can be written. */
	OTC_CONF_SET = (1UL << 30),
	/** Possible values can be enumerated. */
	OTC_CONF_LIST = (1UL << 29),
};

/** Configuration keys */
enum otc_configkey {
	/*--- Device classes ------------------------------------------------*/

	/** The device can act as logic analyzer. */
	OTC_CONF_LOGIC_ANALYZER = 10000,

	/** The device can act as an oscilloscope. */
	OTC_CONF_OSCILLOSCOPE,

	/** The device can act as a multimeter. */
	OTC_CONF_MULTIMETER,

	/** The device is a demo device. */
	OTC_CONF_DEMO_DEV,

	/** The device can act as a sound level meter. */
	OTC_CONF_SOUNDLEVELMETER,

	/** The device can measure temperature. */
	OTC_CONF_THERMOMETER,

	/** The device can measure humidity. */
	OTC_CONF_HYGROMETER,

	/** The device can measure energy consumption. */
	OTC_CONF_ENERGYMETER,

	/** The device can act as a signal demodulator. */
	OTC_CONF_DEMODULATOR,

	/** The device can act as a programmable power supply. */
	OTC_CONF_POWER_SUPPLY,

	/** The device can act as an LCR meter. */
	OTC_CONF_LCRMETER,

	/** The device can act as an electronic load. */
	OTC_CONF_ELECTRONIC_LOAD,

	/** The device can act as a scale. */
	OTC_CONF_SCALE,

	/** The device can act as a function generator. */
	OTC_CONF_SIGNAL_GENERATOR,

	/** The device can measure power. */
	OTC_CONF_POWERMETER,

	/**
	 * The device can switch between multiple sources, e.g. a relay actuator
	 * or multiplexer.
	 */
	OTC_CONF_MULTIPLEXER,

	/**
	 * The device can act as a digital delay generator.
	 */
	OTC_CONF_DELAY_GENERATOR,

	/**
	 * The device can act as a frequency counter.
	 */
	OTC_CONF_FREQUENCY_COUNTER,

	/* Update otc_key_info_config[] (hwdriver.c) upon changes! */

	/*--- Driver scan options -------------------------------------------*/

	/**
	 * Specification on how to connect to a device.
	 *
	 * In combination with OTC_CONF_SERIALCOMM, this is a serial port in
	 * the form which makes sense to the OS (e.g., /dev/ttyS0).
	 * Otherwise this specifies a USB device, either in the form of
	 * @verbatim <bus>.<address> @endverbatim (decimal, e.g. 1.65) or
	 * @verbatim <vendorid>.<productid> @endverbatim
	 * (hexadecimal, e.g. 1d6b.0001).
	 */
	OTC_CONF_CONN = 20000,

	/**
	 * Serial communication specification, in the form:
	 *
	 *   @verbatim <baudrate>/<databits><parity><stopbits> @endverbatim
	 *
	 * Example: 9600/8n1
	 *
	 * The string may also be followed by one or more special settings,
	 * in the form "/key=value". Supported keys and their values are:
	 *
	 * rts    0,1    set the port's RTS pin to low or high
	 * dtr    0,1    set the port's DTR pin to low or high
	 * flow   0      no flow control
	 *        1      hardware-based (RTS/CTS) flow control
	 *        2      software-based (XON/XOFF) flow control
	 *
	 * This is always an optional parameter, since a driver typically
	 * knows the speed at which the device wants to communicate.
	 */
	OTC_CONF_SERIALCOMM,

	/**
	 * Modbus slave address specification.
	 *
	 * This is always an optional parameter, since a driver typically
	 * knows the default slave address of the device.
	 */
	OTC_CONF_MODBUSADDR,

	/**
	 * User specified forced driver attachment to unknown devices.
	 *
	 * By design the interpretation of the string depends on the
	 * specific driver. It typically would be either a replacement
	 * '*IDN?' response value, or a sub-driver name. But could also
	 * be anything else and totally arbitrary.
	 */
	OTC_CONF_FORCE_DETECT,

	/**
	 * Override builtin probe names from user specs.
	 *
	 * Users may want to override the names which are assigned to
	 * probes during scan (these usually match the vendor's labels
	 * on the device). This avoids the interactive tedium of
	 * changing channel names after device creation and before
	 * protocol decoder attachment. Think of IEEE488 recorders or
	 * parallel computer bus loggers. The scan option eliminates
	 * the issue of looking up previously assigned names before
	 * renaming a channel (see opentracelab-cli -C), which depends on
	 * the device as well as the application, and is undesirable.
	 * The scan option is limited to those drivers which implement
	 * support for it, but works identically across those drivers.
	 *
	 * The value is a string, either a comma separated list of
	 * probe names, or an alias for a typical set of names.
	 */
	OTC_CONF_PROBE_NAMES,

	/* Update otc_key_info_config[] (hwdriver.c) upon changes! */

	/*--- Device (or channel group) configuration -----------------------*/

	/** The device supports setting its samplerate, in Hz. */
	OTC_CONF_SAMPLERATE = 30000,

	/** The device supports setting a pre/post-trigger capture ratio. */
	OTC_CONF_CAPTURE_RATIO,

	/** The device supports setting a pattern (pattern generator mode). */
	OTC_CONF_PATTERN_MODE,

	/** The device supports run-length encoding (RLE). */
	OTC_CONF_RLE,

	/** The device supports setting trigger slope. */
	OTC_CONF_TRIGGER_SLOPE,

	/** The device supports averaging. */
	OTC_CONF_AVERAGING,

	/**
	 * The device supports setting number of samples to be
	 * averaged over.
	 */
	OTC_CONF_AVG_SAMPLES,

	/** Trigger source. */
	OTC_CONF_TRIGGER_SOURCE,

	/** Horizontal trigger position. */
	OTC_CONF_HORIZ_TRIGGERPOS,

	/** Buffer size. */
	OTC_CONF_BUFFERSIZE,

	/** Time base. */
	OTC_CONF_TIMEBASE,

	/** Filter. */
	OTC_CONF_FILTER,

	/** Volts/div. */
	OTC_CONF_VDIV,

	/** Coupling. */
	OTC_CONF_COUPLING,

	/** Trigger matches. */
	OTC_CONF_TRIGGER_MATCH,

	/** The device supports setting its sample interval, in ms. */
	OTC_CONF_SAMPLE_INTERVAL,

	/** Number of horizontal divisions, as related to OTC_CONF_TIMEBASE. */
	OTC_CONF_NUM_HDIV,

	/** Number of vertical divisions, as related to OTC_CONF_VDIV. */
	OTC_CONF_NUM_VDIV,

	/** Sound pressure level frequency weighting. */
	OTC_CONF_SPL_WEIGHT_FREQ,

	/** Sound pressure level time weighting. */
	OTC_CONF_SPL_WEIGHT_TIME,

	/** Sound pressure level measurement range. */
	OTC_CONF_SPL_MEASUREMENT_RANGE,

	/** Max hold mode. */
	OTC_CONF_HOLD_MAX,

	/** Min hold mode. */
	OTC_CONF_HOLD_MIN,

	/** Logic low-high threshold range. */
	OTC_CONF_VOLTAGE_THRESHOLD,

	/** The device supports using an external clock. */
	OTC_CONF_EXTERNAL_CLOCK,

	/**
	 * The device supports swapping channels. Typical this is between
	 * buffered and unbuffered channels.
	 */
	OTC_CONF_SWAP,

	/** Center frequency.
	 * The input signal is downmixed by this frequency before the ADC
	 * anti-aliasing filter.
	 */
	OTC_CONF_CENTER_FREQUENCY,

	/** The device supports setting the number of logic channels. */
	OTC_CONF_NUM_LOGIC_CHANNELS,

	/** The device supports setting the number of analog channels. */
	OTC_CONF_NUM_ANALOG_CHANNELS,

	/**
	 * Current voltage.
	 * @arg type: double
	 * @arg get: get measured voltage
	 */
	OTC_CONF_VOLTAGE,

	/**
	 * Maximum target voltage.
	 * @arg type: double
	 * @arg get: get target voltage
	 * @arg set: change target voltage
	 */
	OTC_CONF_VOLTAGE_TARGET,

	/**
	 * Current current.
	 * @arg type: double
	 * @arg get: get measured current
	 */
	OTC_CONF_CURRENT,

	/**
	 * Current limit.
	 * @arg type: double
	 * @arg get: get current limit
	 * @arg set: change current limit
	 */
	OTC_CONF_CURRENT_LIMIT,

	/**
	 * Enabling/disabling channel.
	 * @arg type: boolean
	 * @arg get: @b true if currently enabled
	 * @arg set: enable/disable
	 */
	OTC_CONF_ENABLED,

	/**
	 * Channel configuration.
	 * @arg type: string
	 * @arg get: get current setting
	 * @arg set: change current setting
	 * @arg list: array of possible values
	 */
	OTC_CONF_CHANNEL_CONFIG,

	/**
	 * Over-voltage protection (OVP) feature
	 * @arg type: boolean
	 * @arg get: @b true if currently enabled
	 * @arg set: enable/disable
	 */
	OTC_CONF_OVER_VOLTAGE_PROTECTION_ENABLED,

	/**
	 * Over-voltage protection (OVP) active
	 * @arg type: boolean
	 * @arg get: @b true if device has activated OVP, i.e. the output voltage
	 *      exceeds the over-voltage protection threshold.
	 */
	OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE,

	/**
	 * Over-voltage protection (OVP) threshold
	 * @arg type: double (voltage)
	 * @arg get: get current threshold
	 * @arg set: set new threshold
	 */
	OTC_CONF_OVER_VOLTAGE_PROTECTION_THRESHOLD,

	/**
	 * Over-current protection (OCP) feature
	 * @arg type: boolean
	 * @arg get: @b true if currently enabled
	 * @arg set: enable/disable
	 */
	OTC_CONF_OVER_CURRENT_PROTECTION_ENABLED,

	/**
	 * Over-current protection (OCP) active
	 * @arg type: boolean
	 * @arg get: @b true if device has activated OCP, i.e. the current current
	 *      exceeds the over-current protection threshold.
	 */
	OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE,

	/**
	 * Over-current protection (OCP) threshold
	 * @arg type: double (current)
	 * @arg get: get current threshold
	 * @arg set: set new threshold
	 */
	OTC_CONF_OVER_CURRENT_PROTECTION_THRESHOLD,

	/** Choice of clock edge for external clock ("r" or "f"). */
	OTC_CONF_CLOCK_EDGE,

	/** Amplitude of a source without strictly-defined MQ. */
	OTC_CONF_AMPLITUDE,

	/**
	 * Channel regulation
	 * get: "CV", "CC" or "UR", denoting constant voltage, constant current
	 *      or unregulated.
	 *      "CC-" denotes a power supply in current sink mode (e.g. HP 66xxB).
	 *      "" is used when there is no regulation, e.g. the output is disabled.
	 */
	OTC_CONF_REGULATION,

	/** Over-temperature protection (OTP) */
	OTC_CONF_OVER_TEMPERATURE_PROTECTION,

	/** Output frequency in Hz. */
	OTC_CONF_OUTPUT_FREQUENCY,

	/** Output frequency target in Hz. */
	OTC_CONF_OUTPUT_FREQUENCY_TARGET,

	/** Measured quantity. */
	OTC_CONF_MEASURED_QUANTITY,

	/** Equivalent circuit model. */
	OTC_CONF_EQUIV_CIRCUIT_MODEL,

	/** Over-temperature protection (OTP) active. */
	OTC_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE,

	/** Under-voltage condition. */
	OTC_CONF_UNDER_VOLTAGE_CONDITION,

	/** Under-voltage condition active. */
	OTC_CONF_UNDER_VOLTAGE_CONDITION_ACTIVE,

	/** Trigger level. */
	OTC_CONF_TRIGGER_LEVEL,

	/** Under-voltage condition threshold. */
	OTC_CONF_UNDER_VOLTAGE_CONDITION_THRESHOLD,

	/**
	 * Which external clock source to use if the device supports
	 * multiple external clock channels.
	 */
	OTC_CONF_EXTERNAL_CLOCK_SOURCE,

	/** Offset of a source without strictly-defined MQ. */
	OTC_CONF_OFFSET,

	/** The device supports setting a pattern for the logic trigger. */
	OTC_CONF_TRIGGER_PATTERN,

	/** High resolution mode. */
	OTC_CONF_HIGH_RESOLUTION,

	/** Peak detection. */
	OTC_CONF_PEAK_DETECTION,

	/** Logic threshold: predefined levels (TTL, ECL, CMOS, etc). */
	OTC_CONF_LOGIC_THRESHOLD,

	/** Logic threshold: custom numerical value. */
	OTC_CONF_LOGIC_THRESHOLD_CUSTOM,

	/** The measurement range of a DMM or the output range of a power supply. */
	OTC_CONF_RANGE,

	/** The number of digits (e.g. for a DMM). */
	OTC_CONF_DIGITS,

	/** Phase of a source signal. */
	OTC_CONF_PHASE,

	/** Duty cycle of a source signal. */
	OTC_CONF_DUTY_CYCLE,

	/**
	 * Current power.
	 * @arg type: double
	 * @arg get: get measured power
	 */
	OTC_CONF_POWER,

	/**
	 * Power target.
	 * @arg type: double
	 * @arg get: get power target
	 * @arg set: change power target
	 */
	OTC_CONF_POWER_TARGET,

	/**
	 * Resistance target.
	 * @arg type: double
	 * @arg get: get resistance target
	 * @arg set: change resistance target
	 */
	OTC_CONF_RESISTANCE_TARGET,

	/**
	 * Over-current protection (OCP) delay
	 * @arg type: double (time)
	 * @arg get: get current delay
	 * @arg set: set new delay
	 */
	OTC_CONF_OVER_CURRENT_PROTECTION_DELAY,

	/**
	 * Signal inversion.
	 * @arg type: boolean
	 * @arg get: @b true if the signal is inverted or has negative polarity,
	 *           @b false otherwise
	 * @arg set: set @b true to invert the signal
	 */
	OTC_CONF_INVERTED,

	/* Update otc_key_info_config[] (hwdriver.c) upon changes! */

	/*--- Special stuff -------------------------------------------------*/

	/** Session filename. */
	OTC_CONF_SESSIONFILE = 40000,

	/** The device supports specifying a capturefile to inject. */
	OTC_CONF_CAPTUREFILE,

	/** The device supports specifying the capturefile unit size. */
	OTC_CONF_CAPTURE_UNITSIZE,

	/** Power off the device. */
	OTC_CONF_POWER_OFF,

	/**
	 * Data source for acquisition. If not present, acquisition from
	 * the device is always "live", i.e. acquisition starts when the
	 * frontend asks and the results are sent out as soon as possible.
	 *
	 * If present, it indicates that either the device has no live
	 * acquisition capability (for example a pure data logger), or
	 * there is a choice. otc_config_list() returns those choices.
	 *
	 * In any case if a device has live acquisition capabilities, it
	 * is always the default.
	 */
	OTC_CONF_DATA_SOURCE,

	/** The device supports setting a probe factor. */
	OTC_CONF_PROBE_FACTOR,

	/** Number of powerline cycles for ADC integration time. */
	OTC_CONF_ADC_POWERLINE_CYCLES,

	/* Update otc_key_info_config[] (hwdriver.c) upon changes! */

	/*--- Acquisition modes, sample limiting ----------------------------*/

	/**
	 * The device supports setting a sample time limit (how long
	 * the sample acquisition should run, in ms).
	 */
	OTC_CONF_LIMIT_MSEC = 50000,

	/**
	 * The device supports setting a sample number limit (how many
	 * samples should be acquired).
	 */
	OTC_CONF_LIMIT_SAMPLES,

	/**
	 * The device supports setting a frame limit (how many
	 * frames should be acquired).
	 */
	OTC_CONF_LIMIT_FRAMES,

	/**
	 * The device supports continuous sampling. Neither a time limit
	 * nor a sample number limit has to be supplied, it will just acquire
	 * samples continuously, until explicitly stopped by a certain command.
	 */
	OTC_CONF_CONTINUOUS,

	/** The device has internal storage, into which data is logged. This
	 * starts or stops the internal logging. */
	OTC_CONF_DATALOG,

	/** Device mode for multi-function devices. */
	OTC_CONF_DEVICE_MODE,

	/** Self test mode. */
	OTC_CONF_TEST_MODE,

	/**
	 * Over-power protection (OPP) feature
	 * @arg type: boolean
	 * @arg get: @b true if currently enabled
	 * @arg set: enable/disable
	 */
	OTC_CONF_OVER_POWER_PROTECTION_ENABLED,

	/**
	 * Over-power protection (OPP) active
	 * @arg type: boolean
	 * @arg get: @b true if device has activated OPP, i.e. the current power
	 *      exceeds the over-power protection threshold.
	 */
	OTC_CONF_OVER_POWER_PROTECTION_ACTIVE,

	/**
	 * Over-power protection (OPP) threshold
	 * @arg type: double (current)
	 * @arg get: get current threshold
	 * @arg set: set new threshold
	 */
	OTC_CONF_OVER_POWER_PROTECTION_THRESHOLD,

	/**
	 * Current Resistance.
	 * @arg type: double
	 * @arg get: get measured resistance
	 */
	OTC_CONF_RESISTANCE,

	/**
	 * Gate time.
	 */
	OTC_CONF_GATE_TIME,

	/* Update otc_key_info_config[] (hwdriver.c) upon changes! */
};

/**
 * Opaque structure representing a libopentracecapture device instance.
 *
 * None of the fields of this structure are meant to be accessed directly.
 */
struct otc_dev_inst;

/** Types of device instance, struct otc_dev_inst.type */
enum otc_dev_inst_type {
	/** Device instance type for USB devices. */
	OTC_INST_USB = 10000,
	/** Device instance type for serial port devices. */
	OTC_INST_SERIAL,
	/** Device instance type for SCPI devices. */
	OTC_INST_SCPI,
	/** Device-instance type for user-created "devices". */
	OTC_INST_USER,
	/** Device instance type for Modbus devices. */
	OTC_INST_MODBUS,
};

/** Device instance status, struct otc_dev_inst.status */
enum otc_dev_inst_status {
	/** The device instance was not found. */
	OTC_ST_NOT_FOUND = 10000,
	/** The device instance was found, but is still booting. */
	OTC_ST_INITIALIZING,
	/** The device instance is live, but not in use. */
	OTC_ST_INACTIVE,
	/** The device instance is actively in use in a session. */
	OTC_ST_ACTIVE,
	/** The device is winding down its session. */
	OTC_ST_STOPPING,
};

/** Device driver data. See also http://opentracelab.org/wiki/Hardware_driver_API . */
struct otc_dev_driver {
	/* Driver-specific */
	/** Driver name. Lowercase a-z, 0-9 and dashes (-) only. */
	const char *name;
	/** Long name. Verbose driver name shown to user. */
	const char *longname;
	/** API version (currently 1).	*/
	int api_version;
	/** Called when driver is loaded, e.g. program startup. */
	int (*init) (struct otc_dev_driver *driver, struct otc_context *otc_ctx);
	/** Called before driver is unloaded.
	 *  Driver must free all resources held by it. */
	int (*cleanup) (const struct otc_dev_driver *driver);
	/** Scan for devices. Driver should do all initialisation required.
	 *  Can be called several times, e.g. with different port options.
	 *  @retval NULL Error or no devices found.
	 *  @retval other GSList of a struct otc_dev_inst for each device.
	 *                Must be freed by caller!
	 */
	GSList *(*scan) (struct otc_dev_driver *driver, GSList *options);
	/** Get list of device instances the driver knows about.
	 *  @returns NULL or GSList of a struct otc_dev_inst for each device.
	 *           Must not be freed by caller!
	 */
	GSList *(*dev_list) (const struct otc_dev_driver *driver);
	/** Clear list of devices the driver knows about. */
	int (*dev_clear) (const struct otc_dev_driver *driver);
	/** Query value of a configuration key in driver or given device instance.
	 *  @see otc_config_get().
	 */
	int (*config_get) (uint32_t key, GVariant **data,
			const struct otc_dev_inst *sdi,
			const struct otc_channel_group *cg);
	/** Set value of a configuration key in driver or a given device instance.
	 *  @see otc_config_set(). */
	int (*config_set) (uint32_t key, GVariant *data,
			const struct otc_dev_inst *sdi,
			const struct otc_channel_group *cg);
	/** Channel status change.
	 *  @see otc_dev_channel_enable(). */
	int (*config_channel_set) (const struct otc_dev_inst *sdi,
			struct otc_channel *ch, unsigned int changes);
	/** Apply configuration settings to the device hardware.
	 *  @see otc_config_commit().*/
	int (*config_commit) (const struct otc_dev_inst *sdi);
	/** List all possible values for a configuration key in a device instance.
	 *  @see otc_config_list().
	 */
	int (*config_list) (uint32_t key, GVariant **data,
			const struct otc_dev_inst *sdi,
			const struct otc_channel_group *cg);

	/* Device-specific */
	/** Open device */
	int (*dev_open) (struct otc_dev_inst *sdi);
	/** Close device */
	int (*dev_close) (struct otc_dev_inst *sdi);
	/** Begin data acquisition on the specified device. */
	int (*dev_acquisition_start) (const struct otc_dev_inst *sdi);
	/** End data acquisition on the specified device. */
	int (*dev_acquisition_stop) (struct otc_dev_inst *sdi);

	/* Dynamic */
	/** Device driver context, considered private. Initialized by init(). */
	void *context;
};

/** Serial port descriptor. */
struct otc_serial_port {
	/** The OS dependent name of the serial port. */
	char *name;
	/** An end user friendly description for the serial port. */
	char *description;
};

#include <opentracecapture/proto.h>
#include <opentracecapture/version.h>

#ifdef __cplusplus
}
#endif

#endif /* OPENTRACECAPTURE_LIBSIGROK_H */
