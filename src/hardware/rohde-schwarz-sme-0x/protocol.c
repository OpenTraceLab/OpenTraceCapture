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

#include <config.h>
#include "protocol.h"

const char *commands_sme0x[] = {
	[RS_CMD_PRESET]           = "*RST",
	[RS_CMD_RESET_STATUS]     = "*CLS",
	[RS_CMD_CONTROL_REMOTE]   = "SYST:REM",
	[RS_CMD_CONTROL_LOCAL]    = "SYST:LOC",
	[RS_CMD_CONTROL_REMOTEQM] = "SYST:RWL?",
	[RS_CMD_SET_ENABLE]       = ":OUTP %s",
	[RS_CMD_SET_FREQ]         = ":FREQ %.1lf",
	[RS_CMD_SET_POWER]        = ":POW %.1lf",
	[RS_CMD_SET_CLK_SRC]      = ":ROSC:SOUR %s",
	[RS_CMD_GET_ENABLE]       = ":OUTP?",
	[RS_CMD_GET_FREQ]         = ":FREQ?",
	[RS_CMD_GET_POWER]        = ":POW?",
	[RS_CMD_GET_CLK_SRC]      = ":ROSC:SOUR?",
};

const char *responses_sme0x[] = {
	[RS_RESP_OUTP_ON]       = "1",
	[RS_RESP_OUTP_OFF]      = "0",
	[RS_RESP_CLK_SRC_INT]   = "INT",
	[RS_RESP_CLK_SRC_EXT]   = "EXT",
};

const char *commands_smx100[] = {
	[RS_CMD_PRESET]           = "*RST",
	[RS_CMD_RESET_STATUS]     = "*CLS",
	[RS_CMD_CONTROL_REMOTE]   = NULL,
	[RS_CMD_CONTROL_LOCAL]    = NULL,
	[RS_CMD_CONTROL_REMOTEQM] = NULL,
	[RS_CMD_SET_ENABLE]       = ":OUTP %s",
	[RS_CMD_SET_FREQ]         = ":FREQ %.3lf",
	[RS_CMD_SET_POWER]        = ":POW %.2lf",
	[RS_CMD_SET_CLK_SRC]      = ":ROSC:SOUR %s",
	[RS_CMD_GET_ENABLE]       = ":OUTP?",
	[RS_CMD_GET_FREQ]         = ":FREQ?",
	[RS_CMD_GET_POWER]        = ":POW?",
	[RS_CMD_GET_CLK_SRC]      = ":ROSC:SOUR?",
};

const char *responses_smx100[] = {
	[RS_RESP_OUTP_ON]       = "1",
	[RS_RESP_OUTP_OFF]      = "0",
	[RS_RESP_CLK_SRC_INT]   = "INT",
	[RS_RESP_CLK_SRC_EXT]   = "EXT",
};

OTC_PRIV int rs_sme0x_init(const struct otc_dev_inst *sdi)
{
	int ret;
	struct dev_context *devc;
	const char *cmd;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd = devc->model_config->commands[RS_CMD_PRESET];
	if (cmd && (ret = otc_scpi_send(sdi->conn, cmd)) != OTC_OK)
		return ret;

	cmd = devc->model_config->commands[RS_CMD_RESET_STATUS];
	if (cmd && (ret = otc_scpi_send(sdi->conn, cmd)) != OTC_OK)
		return ret;

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_mode_remote(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	const char *cmd;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd = devc->model_config->commands[RS_CMD_CONTROL_REMOTE];
	if (cmd)
		return otc_scpi_send(sdi->conn, cmd);

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_mode_local(const struct otc_dev_inst *sdi)
{
	int ret, resp_dlock;
	struct dev_context *devc;
	const char *cmd_set;
	const char *cmd_get;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd_set = devc->model_config->commands[RS_CMD_CONTROL_LOCAL];
	cmd_get = devc->model_config->commands[RS_CMD_CONTROL_REMOTEQM];
	if (!cmd_set)
		return OTC_OK;

	ret = OTC_OK;
	resp_dlock = 0;
	do {
		ret = otc_scpi_send(sdi->conn, cmd_set);
		if (ret == OTC_OK) {
			if (cmd_get)
				ret = otc_scpi_get_int(sdi->conn, cmd_get, &resp_dlock);
			else
				break;
		}
	} while (ret == OTC_OK && resp_dlock == 1);

	return ret;
}

OTC_PRIV int rs_sme0x_sync(const struct otc_dev_inst *sdi)
{
	int ret;
	struct dev_context *devc;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	if ((ret = rs_sme0x_get_enable(sdi, &devc->enable)) != OTC_OK)
		return ret;
	if ((ret = rs_sme0x_get_freq(sdi, &devc->freq)) != OTC_OK)
		return ret;
	if ((ret = rs_sme0x_get_power(sdi, &devc->power)) != OTC_OK)
		return ret;
	if ((ret = rs_sme0x_get_clk_src_idx(sdi, &devc->clk_source_idx)) != OTC_OK)
		return ret;

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_get_enable(const struct otc_dev_inst *sdi, gboolean *enable)
{
	int ret;
	char *buf;
	struct dev_context *devc;
	const char *resp_on;
	const char *resp_off;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	ret = otc_scpi_get_string(sdi->conn,
		devc->model_config->commands[RS_CMD_GET_ENABLE], &buf);
	if (ret != OTC_OK)
		return ret;

	resp_on = devc->model_config->responses[RS_RESP_OUTP_ON];
	resp_off = devc->model_config->responses[RS_RESP_OUTP_OFF];
	if (strcmp(buf, resp_on) == 0) {
		ret = OTC_OK;
		*enable = TRUE;
	} else if (strcmp(buf, resp_off) == 0) {
		ret = OTC_OK;
		*enable = FALSE;
	} else {
		ret = OTC_ERR;
	}

	g_free(buf);
	return ret;
}

OTC_PRIV int rs_sme0x_get_freq(const struct otc_dev_inst *sdi, double *freq)
{
	int ret;
	struct dev_context *devc;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	ret = otc_scpi_get_double(sdi->conn,
		devc->model_config->commands[RS_CMD_GET_FREQ], freq);

	if (ret != OTC_OK)
		return OTC_ERR;

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_get_power(const struct otc_dev_inst *sdi, double *power)
{
	int ret;
	struct dev_context *devc;
	const char *cmd;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd = devc->model_config->commands[RS_CMD_GET_POWER];
	ret = otc_scpi_get_double(sdi->conn, cmd, power);
	if (ret != OTC_OK)
		return OTC_ERR;

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_get_clk_src_idx(const struct otc_dev_inst *sdi, int *idx)
{
	char *buf;
	int ret;
	struct dev_context *devc;
	const char *cmd;
	const char *resp_int;
	const char *resp_ext;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd = devc->model_config->commands[RS_CMD_GET_CLK_SRC];
	ret = otc_scpi_get_string(sdi->conn, cmd, &buf);
	if (ret != OTC_OK || !buf)
		return OTC_ERR;

	resp_int = devc->model_config->responses[RS_RESP_CLK_SRC_INT];
	resp_ext = devc->model_config->responses[RS_RESP_CLK_SRC_EXT];
	if (strcmp(buf, resp_int) == 0) {
		ret = OTC_OK;
		*idx = 0;
	} else if (strcmp(buf, resp_ext) == 0) {
		ret = OTC_OK;
		*idx = 1;
	} else {
		ret = OTC_ERR;
	}

	g_free(buf);
	return ret;
}

OTC_PRIV int rs_sme0x_set_enable(const struct otc_dev_inst *sdi, gboolean enable)
{
	int ret;
	char *cmd;
	struct dev_context *devc;
	const char *cmd_fmt;
	const char *param;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd_fmt = devc->model_config->commands[RS_CMD_SET_ENABLE];
	param = enable ? "ON" : "OFF";
	cmd = g_strdup_printf(cmd_fmt, param);
	ret = otc_scpi_send(sdi->conn, cmd);
	g_free(cmd);

	if (ret == OTC_OK)
		devc->enable = enable;

	return ret;
}

OTC_PRIV int rs_sme0x_set_freq(const struct otc_dev_inst *sdi, double freq)
{
	int ret;
	char *cmd;
	struct dev_context *devc;
	const char *cmd_fmt;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd_fmt = devc->model_config->commands[RS_CMD_SET_FREQ];
	cmd = g_strdup_printf(cmd_fmt, freq);
	ret = otc_scpi_send(sdi->conn, cmd);
	g_free(cmd);

	if (ret == OTC_OK)
		devc->freq = freq;

	return ret;
}

OTC_PRIV int rs_sme0x_set_power(const struct otc_dev_inst *sdi, double power)
{
	int ret;
	char *cmd;
	struct dev_context *devc;
	const char *cmd_fmt;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd_fmt = devc->model_config->commands[RS_CMD_SET_POWER];
	cmd = g_strdup_printf(cmd_fmt, power);
	ret = otc_scpi_send(sdi->conn, cmd);
	g_free(cmd);

	if (ret == OTC_OK)
		devc->power = power;

	return ret;
}

OTC_PRIV int rs_sme0x_set_clk_src(const struct otc_dev_inst *sdi, int idx)
{
	int ret;
	char *cmd;
	struct dev_context *devc;
	const char *cmd_fmt;
	const char *param;

	if (!sdi || !sdi->priv)
		return OTC_ERR;
	devc = sdi->priv;

	cmd_fmt = devc->model_config->commands[RS_CMD_SET_CLK_SRC];
	param = (idx == 0) ? "INT" : "EXT";
	cmd = g_strdup_printf(cmd_fmt, param);
	ret = otc_scpi_send(sdi->conn, cmd);
	g_free(cmd);

	if (ret == OTC_OK)
		devc->clk_source_idx = idx;

	return ret;
}

OTC_PRIV int rs_sme0x_get_minmax_freq(const struct otc_dev_inst *sdi,
	double *min_freq, double *max_freq)
{
	int ret;

	ret = otc_scpi_get_double(sdi->conn, ":FREQ? MIN", min_freq);
	if (ret != OTC_OK)
		return ret;

	ret = otc_scpi_get_double(sdi->conn, ":FREQ? MAX", max_freq);
	return ret;
}

OTC_PRIV int rs_sme0x_get_minmax_power(const struct otc_dev_inst *sdi,
	double *min_power, double *max_power)
{
	int ret;

	ret = otc_scpi_get_double(sdi->conn, ":POW? MIN", min_power);
	if (ret != OTC_OK)
		return ret;

	ret = otc_scpi_get_double(sdi->conn, ":POW? MAX", max_power);
	return ret;
}
