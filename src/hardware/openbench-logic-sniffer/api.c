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
#include "protocol.h"

#define SERIALCOMM "115200/8n1"

static const uint32_t scanopts[] = {
	OTC_CONF_CONN,
	OTC_CONF_SERIALCOMM,
	OTC_CONF_PROBE_NAMES,
};

static const uint32_t drvopts[] = {
	OTC_CONF_LOGIC_ANALYZER,
};

static const uint32_t devopts[] = {
	OTC_CONF_CONN | OTC_CONF_GET,
	OTC_CONF_LIMIT_SAMPLES | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_SAMPLERATE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_TRIGGER_MATCH | OTC_CONF_LIST,
	OTC_CONF_CAPTURE_RATIO | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_EXTERNAL_CLOCK | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_CLOCK_EDGE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_PATTERN_MODE | OTC_CONF_GET | OTC_CONF_SET | OTC_CONF_LIST,
	OTC_CONF_SWAP | OTC_CONF_SET,
	OTC_CONF_RLE | OTC_CONF_GET | OTC_CONF_SET,
};

static const int32_t trigger_matches[] = {
	OTC_TRIGGER_ZERO,
	OTC_TRIGGER_ONE,
};

static const char *external_clock_edges[] = {
	"rising", /* positive edge */
	"falling" /* negative edge */
};

#define STR_PATTERN_NONE     "None"
#define STR_PATTERN_EXTERNAL "External"
#define STR_PATTERN_INTERNAL "Internal"

/* Supported methods of test pattern outputs */
enum {
	/**
	 * Capture pins 31:16 (unbuffered wing) output a test pattern
	 * that can captured on pins 0:15.
	 */
	PATTERN_EXTERNAL,

	/** Route test pattern internally to capture buffer. */
	PATTERN_INTERNAL,
};

static const char *patterns[] = {
	STR_PATTERN_NONE,
	STR_PATTERN_EXTERNAL,
	STR_PATTERN_INTERNAL,
};

/* Channels are numbered 0-31 (on the PCB silkscreen). */
OTC_PRIV const char *ols_channel_names[] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
	"13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23",
	"24", "25", "26", "27", "28", "29", "30", "31",
};

/* Default supported samplerates, can be overridden by device metadata. */
static const uint64_t samplerates[] = {
	OTC_HZ(10),
	OTC_MHZ(200),
	OTC_HZ(1),
};

#define RESPONSE_DELAY_US (20 * 1000)

static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
	struct otc_config *src;
	struct otc_dev_inst *sdi;
	struct otc_serial_dev_inst *serial;
	GSList *l;
	int num_read;
	unsigned int i;
	const char *conn, *serialcomm, *probe_names;
	char buf[4] = { 0, 0, 0, 0 };
	struct dev_context *devc;
	size_t ch_max;

	conn = serialcomm = NULL;
	probe_names = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case OTC_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		case OTC_CONF_PROBE_NAMES:
			probe_names = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;

	if (!serialcomm)
		serialcomm = SERIALCOMM;

	serial = otc_serial_dev_inst_new(conn, serialcomm);

	/* The discovery procedure is like this: first send the Reset
	 * command (0x00) 5 times, since the device could be anywhere
	 * in a 5-byte command. Then send the ID command (0x02).
	 * If the device responds with 4 bytes ("OLS1" or "SLA1"), we
	 * have a match.
	 */
	otc_info("Probing %s.", conn);
	if (serial_open(serial, SERIAL_RDWR) != OTC_OK)
		return NULL;

	if (ols_send_reset(serial) != OTC_OK) {
		serial_close(serial);
		otc_err("Could not use port %s. Quitting.", conn);
		return NULL;
	}
	send_shortcommand(serial, CMD_ID);

	g_usleep(RESPONSE_DELAY_US);

	if (serial_has_receive_data(serial) == 0) {
		serial_close(serial);
		otc_dbg("Didn't get any ID reply.");
		return NULL;
	}

	num_read =
		serial_read_blocking(serial, buf, 4, serial_timeout(serial, 4));
	if (num_read < 0) {
		serial_close(serial);
		otc_err("Getting ID reply failed (%d).", num_read);
		return NULL;
	}

	if (strncmp(buf, "1SLO", 4) && strncmp(buf, "1ALS", 4)) {
		serial_close(serial);
		GString *id = otc_hexdump_new((uint8_t *)buf, num_read);

		otc_err("Invalid ID reply (got %s).", id->str);

		otc_hexdump_free(id);
		return NULL;
	} else {
		otc_dbg("Successful detection, got '%c%c%c%c' (0x%02x 0x%02x 0x%02x 0x%02x).",
		       buf[0], buf[1], buf[2], buf[3],
		       buf[0], buf[1], buf[2], buf[3]);
	}

	/*
	 * Create common data structures (sdi, devc) here in the common
	 * code path. These further get filled in either from metadata
	 * which is gathered from the device, or from open coded generic
	 * fallback data which is kept in the driver source code.
	 */
	sdi = g_malloc0(sizeof(*sdi));
	sdi->status = OTC_ST_INACTIVE;
	sdi->inst_type = OTC_INST_SERIAL;
	sdi->conn = serial;
	sdi->connection_id = g_strdup(serial->port);
	devc = g_malloc0(sizeof(*devc));
	sdi->priv = devc;
	devc->trigger_at_smpl = OLS_NO_TRIGGER;
	devc->channel_names = otc_parse_probe_names(probe_names,
		ols_channel_names, ARRAY_SIZE(ols_channel_names),
		ARRAY_SIZE(ols_channel_names), &ch_max);

	/*
	 * Definitely using the OLS protocol, check if it supports
	 * the metadata command. Otherwise assign generic values.
	 * Create as many opentracelab channels as was determined when
	 * the device was probed.
	 */
	send_shortcommand(serial, CMD_METADATA);
	g_usleep(RESPONSE_DELAY_US);
	if (serial_has_receive_data(serial) != 0) {
		/* Got metadata. */
		(void)ols_get_metadata(sdi);
	} else {
		/* Not an OLS -- some other board using the SUMP protocol. */
		otc_info("Device does not support metadata.");
		sdi->vendor = g_strdup("Sump");
		sdi->model = g_strdup("Logic Analyzer");
		sdi->version = g_strdup("v1.0");
		devc->max_channels = ch_max;
	}
	if (devc->max_channels && ch_max > devc->max_channels)
		ch_max = devc->max_channels;
	for (i = 0; i < ch_max; i++) {
		otc_channel_new(sdi, i, OTC_CHANNEL_LOGIC, TRUE,
			devc->channel_names[i]);
	}
	/* Configure samplerate and divider. */
	if (ols_set_samplerate(sdi, DEFAULT_SAMPLERATE) != OTC_OK)
		otc_dbg("Failed to set default samplerate (%" PRIu64 ").",
		       DEFAULT_SAMPLERATE);

	serial_close(serial);

	return std_scan_complete(di, g_slist_append(NULL, sdi));
}

static int config_get(uint32_t key, GVariant **data,
		      const struct otc_dev_inst *sdi,
		      const struct otc_channel_group *cg)
{
	struct dev_context *devc;

	(void)cg;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_CONN:
		if (!sdi->conn || !sdi->connection_id)
			return OTC_ERR_NA;
		*data = g_variant_new_string(sdi->connection_id);
		break;
	case OTC_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case OTC_CONF_CAPTURE_RATIO:
		*data = g_variant_new_uint64(devc->capture_ratio);
		break;
	case OTC_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case OTC_CONF_PATTERN_MODE:
		if (devc->capture_flags & CAPTURE_FLAG_EXTERNAL_TEST_MODE)
			*data = g_variant_new_string(STR_PATTERN_EXTERNAL);
		else if (devc->capture_flags & CAPTURE_FLAG_INTERNAL_TEST_MODE)
			*data = g_variant_new_string(STR_PATTERN_INTERNAL);
		else
			*data = g_variant_new_string(STR_PATTERN_NONE);
		break;
	case OTC_CONF_RLE:
		*data = g_variant_new_boolean(
			devc->capture_flags & CAPTURE_FLAG_RLE ? TRUE : FALSE);
		break;
	case OTC_CONF_EXTERNAL_CLOCK:
		*data = g_variant_new_boolean(
			devc->capture_flags & CAPTURE_FLAG_CLOCK_EXTERNAL
			? TRUE : FALSE);
		break;
	case OTC_CONF_CLOCK_EDGE:
		*data = g_variant_new_string(external_clock_edges[
			devc->capture_flags & CAPTURE_FLAG_INVERT_EXT_CLOCK
			? 1 : 0]);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_set(uint32_t key, GVariant *data,
		      const struct otc_dev_inst *sdi,
		      const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	uint16_t flag;
	uint64_t tmp_u64;
	const char *stropt;

	(void)cg;

	devc = sdi->priv;

	switch (key) {
	case OTC_CONF_SAMPLERATE:
		tmp_u64 = g_variant_get_uint64(data);
		if (tmp_u64 < samplerates[0] || tmp_u64 > samplerates[1])
			return OTC_ERR_SAMPLERATE;
		return ols_set_samplerate(sdi, g_variant_get_uint64(data));
	case OTC_CONF_LIMIT_SAMPLES:
		tmp_u64 = g_variant_get_uint64(data);
		if (tmp_u64 < MIN_NUM_SAMPLES)
			return OTC_ERR;
		devc->limit_samples = tmp_u64;
		break;
	case OTC_CONF_CAPTURE_RATIO:
		devc->capture_ratio = g_variant_get_uint64(data);
		break;
	case OTC_CONF_EXTERNAL_CLOCK:
		if (g_variant_get_boolean(data)) {
			otc_info("Enabling external clock.");
			devc->capture_flags |= CAPTURE_FLAG_CLOCK_EXTERNAL;
		} else {
			otc_info("Disabled external clock.");
			devc->capture_flags &= ~CAPTURE_FLAG_CLOCK_EXTERNAL;
		}
		break;
	case OTC_CONF_CLOCK_EDGE:
		stropt = g_variant_get_string(data, NULL);
		if (!strcmp(stropt, external_clock_edges[1])) {
			otc_info("Triggering on falling edge of external clock.");
			devc->capture_flags |= CAPTURE_FLAG_INVERT_EXT_CLOCK;
		} else {
			otc_info("Triggering on rising edge of external clock.");
			devc->capture_flags &= ~CAPTURE_FLAG_INVERT_EXT_CLOCK;
		}
		break;
	case OTC_CONF_PATTERN_MODE:
		stropt = g_variant_get_string(data, NULL);
		if (!strcmp(stropt, STR_PATTERN_NONE)) {
			otc_info("Disabling test modes.");
			flag = 0x0000;
		} else if (!strcmp(stropt, STR_PATTERN_INTERNAL)) {
			otc_info("Enabling internal test mode.");
			flag = CAPTURE_FLAG_INTERNAL_TEST_MODE;
		} else if (!strcmp(stropt, STR_PATTERN_EXTERNAL)) {
			otc_info("Enabling external test mode.");
			flag = CAPTURE_FLAG_EXTERNAL_TEST_MODE;
		} else {
			return OTC_ERR;
		}
		devc->capture_flags &= ~CAPTURE_FLAG_INTERNAL_TEST_MODE;
		devc->capture_flags &= ~CAPTURE_FLAG_EXTERNAL_TEST_MODE;
		devc->capture_flags |= flag;
		break;
	case OTC_CONF_SWAP:
		if (g_variant_get_boolean(data)) {
			otc_info("Enabling channel swapping.");
			devc->capture_flags |= CAPTURE_FLAG_SWAP_CHANNELS;
		} else {
			otc_info("Disabling channel swapping.");
			devc->capture_flags &= ~CAPTURE_FLAG_SWAP_CHANNELS;
		}
		break;
	case OTC_CONF_RLE:
		if (g_variant_get_boolean(data)) {
			otc_info("Enabling RLE.");
			devc->capture_flags |= CAPTURE_FLAG_RLE;
		} else {
			otc_info("Disabling RLE.");
			devc->capture_flags &= ~CAPTURE_FLAG_RLE;
		}
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
		       const struct otc_dev_inst *sdi,
		       const struct otc_channel_group *cg)
{
	struct dev_context *devc;
	int num_ols_changrp, i;
	uint64_t samplerates_ovrd[3];

	devc = (sdi) ? sdi->priv : NULL;

	switch (key) {
	case OTC_CONF_SCAN_OPTIONS:
	case OTC_CONF_DEVICE_OPTIONS:
		return STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts,
				       devopts);
	case OTC_CONF_SAMPLERATE:
		if (!devc)
			return OTC_ERR_ARG;
		samplerates_ovrd[0] = samplerates[0];
		samplerates_ovrd[1] = samplerates[1];
		samplerates_ovrd[2] = samplerates[2];
		if (devc->max_samplerate)
			samplerates_ovrd[1] = devc->max_samplerate;
		*data = std_gvar_samplerates_steps(ARRAY_AND_SIZE(samplerates_ovrd));
		break;
	case OTC_CONF_TRIGGER_MATCH:
		*data = std_gvar_array_i32(ARRAY_AND_SIZE(trigger_matches));
		break;
	case OTC_CONF_CLOCK_EDGE:
		*data = std_gvar_array_str(ARRAY_AND_SIZE(external_clock_edges));
		break;
	case OTC_CONF_PATTERN_MODE:
		*data = g_variant_new_strv(ARRAY_AND_SIZE(patterns));
		break;
	case OTC_CONF_LIMIT_SAMPLES:
		if (!sdi)
			return OTC_ERR_ARG;
		devc = sdi->priv;
		if (devc->max_samples == 0)
			/* Device didn't specify sample memory size in metadata. */
			return OTC_ERR_NA;
		/*
		 * Channel groups are turned off if no channels in that group are
		 * enabled, making more room for samples for the enabled group.
		*/
		uint32_t channel_mask = ols_channel_mask(sdi);
		num_ols_changrp = 0;
		for (i = 0; i < 4; i++) {
			if (channel_mask & (0xff << (i * 8)))
				num_ols_changrp++;
		}

		*data = std_gvar_tuple_u64(MIN_NUM_SAMPLES, (num_ols_changrp)
			? devc->max_samples / num_ols_changrp : MIN_NUM_SAMPLES);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	int ret;
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;

	devc = sdi->priv;
	serial = sdi->conn;

	ret = ols_prepare_acquisition(sdi);
	if (ret != OTC_OK)
		return ret;

	/* Start acquisition on the device. */
	if (send_shortcommand(serial, CMD_ARM_BASIC_TRIGGER) != OTC_OK)
		return OTC_ERR;

	/* Reset all operational states. */
	devc->rle_count = devc->num_transfers = 0;
	devc->num_samples = devc->num_bytes = 0;
	devc->cnt_bytes = devc->cnt_samples = devc->cnt_samples_rle = 0;
	memset(devc->sample, 0, 4);

	std_session_send_df_header(sdi);

	/* If the device stops sending for longer than it takes to send a byte,
	 * that means it's finished. But wait at least 100 ms to be safe.
	 */
	return serial_source_add(sdi->session, serial, G_IO_IN, 100,
				 ols_receive_data, (struct otc_dev_inst *)sdi);
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	abort_acquisition(sdi);

	return OTC_OK;
}

static struct otc_dev_driver ols_driver_info = {
	.name = "ols",
	.longname = "Openbench Logic Sniffer & SUMP compatibles",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
OTC_REGISTER_DEV_DRIVER(ols_driver_info);
