/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2016 Vlad Ivanov <vlad.ivanov@lab-systems.ru>
 * Copyright (C) 2024 Daniel Anselmi <danselmi@gmx.ch>
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

#ifndef LIBOPENTRACECAPTURE_HARDWARE_ROHDE_SCHWARZ_SME_0X_PROTOCOL_H
#define LIBOPENTRACECAPTURE_HARDWARE_ROHDE_SCHWARZ_SME_0X_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <opentracecapture/libopentracecapture.h>
#include "../../libopentracecapture-internal.h"
#include "../../scpi.h"

#define LOG_PREFIX "rohde-schwarz-sme-0x"

enum {
	RS_CMD_PRESET,
	RS_CMD_RESET_STATUS,
	RS_CMD_CONTROL_REMOTE,
	RS_CMD_CONTROL_LOCAL,
	RS_CMD_CONTROL_REMOTEQM,
	RS_CMD_SET_ENABLE,
	RS_CMD_SET_FREQ,
	RS_CMD_SET_POWER,
	RS_CMD_SET_CLK_SRC,
	RS_CMD_GET_ENABLE,
	RS_CMD_GET_FREQ,
	RS_CMD_GET_POWER,
	RS_CMD_GET_CLK_SRC,
};

enum {
	RS_RESP_OUTP_ON,
	RS_RESP_OUTP_OFF,
	RS_RESP_CLK_SRC_INT,
	RS_RESP_CLK_SRC_EXT,
};

struct rs_device_model_config {
	double freq_step;
	double power_step;
	const char **commands;
	const char **responses;
};

struct rs_device_model {
	const char *model_str;
	const struct rs_device_model_config *model_config;
};

struct dev_context {
	const struct rs_device_model_config *model_config;
	double freq;
	double power;
	gboolean enable;
	int clk_source_idx;

	double freq_min;
	double freq_max;
	double power_min;
	double power_max;
};

extern const char *commands_sme0x[];
extern const char *commands_smx100[];
extern const char *responses_sme0x[];
extern const char *responses_smx100[];

OTC_PRIV int rs_sme0x_init(const struct otc_dev_inst *sdi);
OTC_PRIV int rs_sme0x_mode_remote(const struct otc_dev_inst *sdi);
OTC_PRIV int rs_sme0x_mode_local(const struct otc_dev_inst *sdi);
OTC_PRIV int rs_sme0x_sync(const struct otc_dev_inst *sdi);
OTC_PRIV int rs_sme0x_get_enable(const struct otc_dev_inst *sdi, gboolean *enable);
OTC_PRIV int rs_sme0x_get_freq(const struct otc_dev_inst *sdi, double *freq);
OTC_PRIV int rs_sme0x_get_power(const struct otc_dev_inst *sdi, double *power);
OTC_PRIV int rs_sme0x_get_clk_src_idx(const struct otc_dev_inst *sdi, int *idx);
OTC_PRIV int rs_sme0x_set_enable(const struct otc_dev_inst *sdi, gboolean enable);
OTC_PRIV int rs_sme0x_set_freq(const struct otc_dev_inst *sdi, double freq);
OTC_PRIV int rs_sme0x_set_power(const struct otc_dev_inst *sdi, double power);
OTC_PRIV int rs_sme0x_set_clk_src(const struct otc_dev_inst *sdi, int idx);

OTC_PRIV int rs_sme0x_get_minmax_freq(const struct otc_dev_inst *sdi, double *min_freq, double *max_freq);
OTC_PRIV int rs_sme0x_get_minmax_power(const struct otc_dev_inst *sdi, double *min_power, double *max_power);

#endif
