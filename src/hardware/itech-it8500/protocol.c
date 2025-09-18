/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2020 Timo Kokkonen <tjko@iki.fi>
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
#include <string.h>
#include "protocol.h"

OTC_PRIV uint8_t itech_it8500_checksum(const uint8_t *packet)
{
	const uint8_t *p;
	uint8_t checksum;
	size_t i;

	if (!packet)
		return 0xff;

	checksum = 0;
	p = packet;
	for (i = 0; i < IT8500_PACKET_LEN - 1; i++)
		checksum += *p++;

	return checksum;
}

OTC_PRIV const char *itech_it8500_mode_to_string(enum itech_it8500_modes mode)
{
	switch (mode) {
	case CC:
		return "CC";
	case CV:
		return "CV";
	case CW:
		return "CW";
	case CR:
		return "CR";
	default:
		return "Unknown";
	}
}

OTC_PRIV int itech_it8500_string_to_mode(const char *modename,
		enum itech_it8500_modes *mode)
{
	size_t i;
	const char *s;

	for (i = 0; i < IT8500_MODES; i++) {
		s = itech_it8500_mode_to_string(i);
		if (strncmp(modename, s, strlen(s)) == 0) {
			*mode = i;
			return OTC_OK;
		}
	}

	return OTC_ERR;
}

OTC_PRIV int itech_it8500_send_cmd(struct otc_serial_dev_inst *serial,
		struct itech_it8500_cmd_packet *cmd,
		struct itech_it8500_cmd_packet **response)
{
	struct itech_it8500_cmd_packet *resp;
	uint8_t *cmd_buf, *resp_buf, checksum;
	int ret, read_len;

	if (!serial || !cmd || !response)
		return OTC_ERR_ARG;

	cmd_buf = g_malloc0(IT8500_PACKET_LEN);
	resp_buf = g_malloc0(IT8500_PACKET_LEN);
	resp = g_malloc0(sizeof(*resp));
	if (!cmd_buf || !resp_buf || !resp)
		return OTC_ERR_MALLOC;

	/*
	 * Construct request from: preamble, address, command, data,
	 * and checksum.
	 */
	cmd_buf[0] = IT8500_PREAMBLE;
	cmd_buf[1] = cmd->address;
	cmd_buf[2] = cmd->command;
	memcpy(&cmd_buf[3], cmd->data, IT8500_DATA_LEN);
	cmd_buf[IT8500_PACKET_LEN - 1] = itech_it8500_checksum(cmd_buf);

	otc_spew("%s: Sending command: %02x", __func__, cmd->command);
	ret = serial_write_blocking(serial, cmd_buf, IT8500_PACKET_LEN,
			serial_timeout(serial, IT8500_PACKET_LEN));
	if (ret < IT8500_PACKET_LEN) {
		otc_dbg("%s: Error sending command 0x%02x: %d", __func__,
			cmd->command, ret);
		ret = OTC_ERR;
		goto error;
	}

	ret = OTC_ERR;
	read_len = serial_read_blocking(serial, resp_buf, IT8500_PACKET_LEN,
			100);
	if (read_len < IT8500_PACKET_LEN) {
		otc_dbg("%s: Timeout waiting response to command: %d",
			__func__, read_len);
		goto error;
	}

	if (resp_buf[0] != IT8500_PREAMBLE) {
		otc_dbg("%s: Invalid packet received (first byte: %02x)",
			__func__, resp_buf[0]);
		goto error;
	}

	checksum = itech_it8500_checksum(resp_buf);
	if (resp_buf[IT8500_PACKET_LEN - 1] != checksum) {
		otc_dbg("%s: Invalid packet received: checksum mismatch",
			__func__);
		goto error;
	}

	resp->address = resp_buf[1];
	resp->command = resp_buf[2];
	memcpy(resp->data, &resp_buf[3], IT8500_DATA_LEN);
	otc_spew("%s: Response packet received: cmd=%02x", __func__,
		resp->command);

	if (resp->command == CMD_RESPONSE) {
		if (resp->data[0] != STS_COMMAND_SUCCESSFUL) {
			otc_dbg("%s: Command (%02x) failed: status=%02x",
				__func__, cmd->command, resp->data[0]);
			goto error;
		}
	} else {
		if (resp->command != cmd->command) {
			otc_dbg("%s: Invalid response received: %02x"
				" (expected: %02x)",
				__func__, resp->command, cmd->command);
			goto error;
		}
	}

	if (*response)
		g_free(*response);
	*response = resp;
	resp = NULL;
	ret = OTC_OK;

error:
	g_free(cmd_buf);
	g_free(resp_buf);
	g_free(resp);

	return ret;
}

OTC_PRIV int itech_it8500_cmd(const struct otc_dev_inst *sdi,
		struct itech_it8500_cmd_packet *cmd,
		struct itech_it8500_cmd_packet **response)
{
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	int ret;

	if (!sdi)
		return OTC_ERR_ARG;
	devc = sdi->priv;
	serial = sdi->conn;
	if (!devc || !serial)
		return OTC_ERR_NA;

	g_mutex_lock(&devc->mutex);
	ret = itech_it8500_send_cmd(serial, cmd, response);
	g_mutex_unlock(&devc->mutex);

	return ret;
}

OTC_PRIV void itech_it8500_status_change(const struct otc_dev_inst *sdi,
		uint8_t old_os, uint8_t new_os,
		uint16_t old_ds, uint16_t new_ds,
		enum itech_it8500_modes old_m, enum itech_it8500_modes new_m)
{
	gboolean old_bit, new_bit;
	const char *mode;

	/* Check it output status has changed. */
	old_bit = old_os & OS_OUT_FLAG;
	new_bit = new_os & OS_OUT_FLAG;
	if (old_bit != new_bit)
		otc_session_send_meta(sdi,
			OTC_CONF_ENABLED,
			g_variant_new_boolean(new_bit));

	/* Check if OVP status has changed. */
	old_bit = old_ds & DS_OV_FLAG;
	new_bit = new_ds & DS_OV_FLAG;
	if (old_bit != new_bit)
		otc_session_send_meta(sdi,
			OTC_CONF_OVER_VOLTAGE_PROTECTION_ACTIVE,
			g_variant_new_boolean(new_bit));

	/* Check if OCP status has changed. */
	old_bit = old_ds & DS_OC_FLAG;
	new_bit = new_ds & DS_OC_FLAG;
	if (old_bit != new_bit)
		otc_session_send_meta(sdi,
			OTC_CONF_OVER_CURRENT_PROTECTION_ACTIVE,
			g_variant_new_boolean(new_bit));

	/* Check if OTP status has changed. */
	old_bit = old_ds & DS_OT_FLAG;
	new_bit = new_ds & DS_OT_FLAG;
	if (old_bit != new_bit)
		otc_session_send_meta(sdi,
			OTC_CONF_OVER_TEMPERATURE_PROTECTION_ACTIVE,
			g_variant_new_boolean(new_bit));

	/* Check if operating mode has changed. */
	if (old_m != new_m) {
		mode = itech_it8500_mode_to_string(new_m);
		otc_session_send_meta(sdi, OTC_CONF_REGULATION,
			g_variant_new_string(mode));
	}
}

OTC_PRIV int itech_it8500_get_status(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct itech_it8500_cmd_packet *cmd;
	struct itech_it8500_cmd_packet *resp;
	double voltage, current, power;
	uint8_t operation_state;
	uint16_t demand_state;
	enum itech_it8500_modes mode;
	gboolean load_on;
	const uint8_t *p;
	int ret;

	if (!sdi)
		return OTC_ERR_ARG;

	devc = sdi->priv;
	if (!devc)
		return OTC_ERR_NA;

	cmd = g_malloc0(sizeof(*cmd));
	if (!cmd)
		return OTC_ERR_MALLOC;
	cmd->address = devc->address;
	cmd->command = CMD_GET_STATE;
	resp = NULL;

	ret = itech_it8500_cmd(sdi, cmd, &resp);
	if (ret == OTC_OK) {
		p = resp->data;
		voltage = read_u32le_inc(&p) / 1000.0;
		current = read_u32le_inc(&p) / 10000.0;
		power = read_u32le_inc(&p) / 1000.0;
		operation_state = read_u8_inc(&p);
		demand_state = read_u16le_inc(&p);

		if (demand_state & DS_CC_MODE_FLAG)
			mode = CC;
		else if (demand_state & DS_CV_MODE_FLAG)
			mode = CV;
		else if (demand_state & DS_CW_MODE_FLAG)
			mode = CW;
		else if (demand_state & DS_CR_MODE_FLAG)
			mode = CR;
		else
			mode = CC;
		load_on = operation_state & OS_OUT_FLAG;

		otc_dbg("Load status: V=%.4f, I=%.4f, P=%.3f, State=%s, "
			"Mode=%s (op=0x%02x, demand=0x%04x)",
			voltage, current, power, (load_on ? "ON": "OFF"),
			itech_it8500_mode_to_string(mode),
			operation_state, demand_state);

		/* Check for status change only after scan() has completed. */
		if (sdi->model) {
			itech_it8500_status_change(sdi, devc->operation_state,
				operation_state, devc->demand_state,
				demand_state, devc->mode, mode);
		}
		devc->voltage = voltage;
		devc->current = current;
		devc->power = power;
		devc->operation_state = operation_state;
		devc->demand_state = demand_state;
		devc->mode = mode;
		devc->load_on = load_on;
	}

	g_free(cmd);
	g_free(resp);

	return ret;
}

OTC_PRIV int itech_it8500_get_int(const struct otc_dev_inst *sdi,
		enum itech_it8500_command command, int *result)
{
	struct dev_context *devc;
	struct itech_it8500_cmd_packet *cmd;
	struct itech_it8500_cmd_packet *resp;
	int ret;

	if (!sdi || !result)
		return OTC_ERR_ARG;

	devc = sdi->priv;
	cmd = g_malloc0(sizeof(*cmd));
	if (!cmd)
		return OTC_ERR_MALLOC;
	cmd->address = devc->address;
	cmd->command = command;
	resp = NULL;

	ret = itech_it8500_cmd(sdi, cmd, &resp);
	if (ret == OTC_OK)
		*result = RL32(&resp->data[0]);

	g_free(cmd);
	g_free(resp);

	return ret;
}

OTC_PRIV void itech_it8500_channel_send_value(const struct otc_dev_inst *sdi,
		struct otc_channel *ch, double value, enum otc_mq mq,
		enum otc_mqflag mqflags, enum otc_unit unit, int digits)
{
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	double val;

	val = value;
	otc_analog_init(&analog, &encoding, &meaning, &spec, digits);
	analog.meaning->channels = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &val;
	analog.encoding->unitsize = sizeof(val);
	analog.encoding->is_float = TRUE;
	analog.meaning->mq = mq;
	analog.meaning->unit = unit;
	analog.meaning->mqflags = mqflags;

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	otc_session_send(sdi, &packet);
	g_slist_free(analog.meaning->channels);
}

OTC_PRIV int itech_it8500_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	GSList *l;

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (itech_it8500_get_status(sdi) != OTC_OK)
		return TRUE;

	std_session_send_df_frame_begin(sdi);

	l = g_slist_nth(sdi->channels, 0);
	itech_it8500_channel_send_value(sdi, l->data, devc->voltage,
		OTC_MQ_VOLTAGE, OTC_MQFLAG_DC, OTC_UNIT_VOLT, 5);

	l = g_slist_nth(sdi->channels, 1);
	itech_it8500_channel_send_value(sdi, l->data, devc->current,
		OTC_MQ_CURRENT, OTC_MQFLAG_DC, OTC_UNIT_AMPERE, 5);

	l = g_slist_nth(sdi->channels, 2);
	itech_it8500_channel_send_value(sdi, l->data, devc->power,
		OTC_MQ_POWER, 0, OTC_UNIT_WATT, 5);

	std_session_send_df_frame_end(sdi);

	otc_sw_limits_update_samples_read(&devc->limits, 1);
	if (otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop(sdi);

	return TRUE;
}
