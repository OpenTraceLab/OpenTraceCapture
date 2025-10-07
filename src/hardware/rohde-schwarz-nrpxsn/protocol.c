/*
 * This file is part of the libsigrok project.
 *
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

static void rohde_schwarz_nrpxsn_send_packet(
	const struct otc_dev_inst *sdi, double value, int digits)
{
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;

	devc = sdi->priv;
	if (!devc)
		return;

	otc_analog_init(&analog, &encoding, &meaning, &spec, digits);
	analog.meaning->mq = OTC_MQ_POWER;
	analog.meaning->unit = OTC_UNIT_DECIBEL_MW;
	analog.meaning->channels = sdi->channels;
	analog.num_samples = 1;
	analog.data = &value;
	analog.encoding->unitsize = sizeof(value);
	analog.encoding->is_float = TRUE;
	analog.encoding->digits = 3;

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	otc_session_send(sdi, &packet);
}

OTC_PRIV int rohde_schwarz_nrpxsn_receive_data(int fd, int revents,
		void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;
	int ret;
	double meas_value;
	int buf_count;

	meas_value = 0.0;

	(void)fd;
	(void)revents;

	sdi = cb_data;
	if (!sdi)
		return TRUE;
	scpi = sdi->conn;
	devc = sdi->priv;
	if (!scpi || !devc)
		return TRUE;

	if (devc->measurement_state == IDLE) {
		if (devc->trigger_source_changed) {
			ret = rohde_schwarz_nrpxsn_update_trigger_source(scpi, devc);
		} else if (devc->curr_freq_changed) {
			ret = rohde_schwarz_nrpxsn_update_curr_freq(scpi, devc);
		} else {
			ret = otc_scpi_send(scpi, "BUFF:CLE");
			if (ret == OTC_OK) {
				ret = otc_scpi_send(scpi, "INITiate");
				devc->measurement_state = WAITING_MEASUREMENT;
			}
		}
	} else {
		buf_count = 0;
		ret = otc_scpi_get_int(scpi, "BUFF:COUN?", &buf_count);
		if (ret == OTC_OK && buf_count > 0) {
			ret = otc_scpi_get_double(scpi, "FETCh?", &meas_value);
			if (ret == OTC_OK) {
				devc->measurement_state = IDLE;
				rohde_schwarz_nrpxsn_send_packet(sdi, meas_value, 16);
				otc_sw_limits_update_samples_read(&devc->limits, 1);
			}
		}
	}

	if (ret != OTC_OK || otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop(sdi);

	return TRUE;
}

OTC_PRIV int rohde_schwarz_nrpxsn_init(
	struct otc_scpi_dev_inst *scpi,
	struct dev_context *devc)
{
	int ret;

	devc->measurement_state = IDLE;

	ret = otc_scpi_send(scpi, "*RST");
	if (ret != OTC_OK)
		return ret;

	ret = rohde_schwarz_nrpxsn_update_trigger_source(scpi, devc);
	if (ret != OTC_OK)
		return ret;

	ret = rohde_schwarz_nrpxsn_update_curr_freq(scpi, devc);
	if (ret != OTC_OK)
		return ret;

	return otc_scpi_send(scpi, "UNIT:POW DBM");
}

OTC_PRIV int rohde_schwarz_nrpxsn_update_trigger_source(
	struct otc_scpi_dev_inst *scpi,
	struct dev_context *devc)
{
	char *cmd;

	if (!scpi || !devc)
		return OTC_ERR;

	cmd = (devc->trigger_source == 0) ? "TRIG:SOUR IMM" : "TRIG:SOUR EXT2";

	if (otc_scpi_send(scpi, cmd) == OTC_OK) {
		devc->trigger_source_changed = 0;
		return OTC_OK;
	}
	return OTC_ERR;
}

OTC_PRIV int rohde_schwarz_nrpxsn_update_curr_freq(
	struct otc_scpi_dev_inst *scpi,
	struct dev_context *devc)
{
	char cmd[32];

	if (!scpi || !devc)
		return OTC_ERR;

	sprintf(cmd, "SENS:FREQ %ld", devc->curr_freq);
	if (otc_scpi_send(scpi, cmd) == OTC_OK) {
		devc->curr_freq_changed = 0;
		return OTC_OK;
	}
	return OTC_ERR;
}
