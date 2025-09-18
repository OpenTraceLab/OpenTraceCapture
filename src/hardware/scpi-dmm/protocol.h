/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2018 Gerhard Sittig <gerhard.sittig@gmx.net>
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

#ifndef LIBSIGROK_HARDWARE_SCPI_DMM_PROTOCOL_H
#define LIBSIGROK_HARDWARE_SCPI_DMM_PROTOCOL_H

#include <config.h>
#include <glib.h>
#include <stdint.h>
#include <stdlib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "../../scpi.h"

#define LOG_PREFIX "scpi-dmm"

#define SCPI_DMM_MAX_CHANNELS	1

enum scpi_dmm_cmdcode {
	DMM_CMD_SETUP_REMOTE,
	DMM_CMD_SETUP_FUNC,
	DMM_CMD_QUERY_FUNC,
	DMM_CMD_START_ACQ,
	DMM_CMD_STOP_ACQ,
	DMM_CMD_QUERY_VALUE,
	DMM_CMD_QUERY_PREC,
	DMM_CMD_SETUP_LOCAL,
	DMM_CMD_QUERY_RANGE_AUTO,
	DMM_CMD_QUERY_RANGE,
	DMM_CMD_SETUP_RANGE_AUTO,
	DMM_CMD_SETUP_RANGE,
};

struct mqopt_item {
	enum otc_mq mq;
	enum otc_mqflag mqflag;
	const char *scpi_func_setup;
	const char *scpi_func_query;
	int default_precision;
	uint32_t drv_flags;
};
#define NO_DFLT_PREC	-99
#define FLAGS_NONE	0
#define FLAG_NO_RANGE	(1 << 0)
#define FLAG_CONF_DELAY	(1 << 1)
#define FLAG_MEAS_DELAY	(1 << 2)

struct scpi_dmm_model {
	const char *vendor;
	const char *model;
	size_t num_channels;
	ssize_t digits;
	const struct scpi_command *cmdset;
	const struct mqopt_item *mqopts;
	size_t mqopt_size;
	int (*get_measurement)(const struct otc_dev_inst *sdi, size_t ch);
	const uint32_t *devopts;
	size_t devopts_size;
	unsigned int read_timeout_us; /* If zero, use default from src/scpi/scpi.c. */
	unsigned int conf_delay_us;
	unsigned int meas_delay_us;
	float infinity_limit; /* If zero, use default from protocol.c */
	gboolean check_opc;
	const char *(*get_range_text)(const struct otc_dev_inst *sdi);
	int (*set_range_from_text)(const struct otc_dev_inst *sdi,
		const char *range);
	GVariant *(*get_range_text_list)(const struct otc_dev_inst *sdi);
};

struct dev_context {
	size_t num_channels;
	const struct scpi_command *cmdset;
	const struct scpi_dmm_model *model;
	struct otc_sw_limits limits;
	struct {
		enum otc_mq curr_mq;
		enum otc_mqflag curr_mqflag;
	} start_acq_mq;
	struct scpi_dmm_acq_info {
		float f_value;
		double d_value;
		struct otc_datafeed_packet packet;
		struct otc_datafeed_analog analog[SCPI_DMM_MAX_CHANNELS];
		struct otc_analog_encoding encoding[SCPI_DMM_MAX_CHANNELS];
		struct otc_analog_meaning meaning[SCPI_DMM_MAX_CHANNELS];
		struct otc_analog_spec spec[SCPI_DMM_MAX_CHANNELS];
	} run_acq_info;
	gchar *precision;
	char range_text[32];
};

OTC_PRIV void scpi_dmm_cmd_delay(struct otc_scpi_dev_inst *scpi);
OTC_PRIV const struct mqopt_item *scpi_dmm_lookup_mq_number(
	const struct otc_dev_inst *sdi, enum otc_mq mq, enum otc_mqflag flag);
OTC_PRIV const struct mqopt_item *scpi_dmm_lookup_mq_text(
	const struct otc_dev_inst *sdi, const char *text);
OTC_PRIV int scpi_dmm_get_mq(const struct otc_dev_inst *sdi,
	enum otc_mq *mq, enum otc_mqflag *flag, char **rsp,
	const struct mqopt_item **mqitem);
OTC_PRIV int scpi_dmm_set_mq(const struct otc_dev_inst *sdi,
	enum otc_mq mq, enum otc_mqflag flag);
OTC_PRIV const char *scpi_dmm_get_range_text(const struct otc_dev_inst *sdi);
OTC_PRIV int scpi_dmm_set_range_from_text(const struct otc_dev_inst *sdi,
	const char *range);
OTC_PRIV GVariant *scpi_dmm_get_range_text_list(const struct otc_dev_inst *sdi);
OTC_PRIV int scpi_dmm_get_meas_agilent(const struct otc_dev_inst *sdi, size_t ch);
OTC_PRIV int scpi_dmm_get_meas_gwinstek(const struct otc_dev_inst *sdi, size_t ch);
OTC_PRIV int scpi_dmm_receive_data(int fd, int revents, void *cb_data);

#endif
