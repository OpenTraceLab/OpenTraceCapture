/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2016 Vlad Ivanov <vlad.ivanov@lab-systems.ru>
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
#include "../../scpi.h"

#include "protocol.h"

enum {
	RS_CMD_CONTROL_REMOTE,
	RS_CMD_SET_FREQ,
	RS_CMD_SET_POWER,
	RS_CMD_GET_FREQ,
	RS_CMD_GET_POWER,
};

static char *commands[] = {
	[RS_CMD_CONTROL_REMOTE] = "\n",
	[RS_CMD_SET_FREQ] = "FREQ %.1fHz",
	[RS_CMD_SET_POWER] = "POW %.1fdBm",
	[RS_CMD_GET_FREQ] = "FREQ?",
	[RS_CMD_GET_POWER] = "POW?",
};

OTC_PRIV int rs_sme0x_mode_remote(struct otc_scpi_dev_inst *scpi)
{
	return otc_scpi_send(scpi, commands[RS_CMD_CONTROL_REMOTE]);
}

OTC_PRIV int rs_sme0x_get_freq(const struct otc_dev_inst *sdi, double *freq)
{
	if (otc_scpi_get_double(sdi->conn, commands[RS_CMD_GET_FREQ], freq) != OTC_OK)
		return OTC_ERR;

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_get_power(const struct otc_dev_inst *sdi, double *power)
{
	if (otc_scpi_get_double(sdi->conn, commands[RS_CMD_GET_POWER], power) != OTC_OK)
		return OTC_ERR;

	return OTC_OK;
}

OTC_PRIV int rs_sme0x_set_freq(const struct otc_dev_inst *sdi, double freq)
{
	struct dev_context *devc;
	const struct rs_device_model *config;

	devc = sdi->priv;
	config = devc->model_config;

	if ((freq > config->freq_max) || (freq < config->freq_min))
		return OTC_ERR_ARG;

	return otc_scpi_send(sdi->conn, commands[RS_CMD_SET_FREQ], freq);
}

OTC_PRIV int rs_sme0x_set_power(const struct otc_dev_inst *sdi, double power)
{
	struct dev_context *devc;
	const struct rs_device_model *config;

	devc = sdi->priv;
	config = devc->model_config;

	if ((power > config->power_max) || (power < config->power_min))
		return OTC_ERR_ARG;

	return otc_scpi_send(sdi->conn, commands[RS_CMD_SET_POWER], power);
}
