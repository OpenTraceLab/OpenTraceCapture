/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Aurelien Jacobs <aurel@gnuage.org>
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

OTC_PRIV int maynuo_m97_get_bit(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_coil address, int *value)
{
	uint8_t coil;
	int ret = otc_modbus_read_coils(modbus, address, 1, &coil);
	*value = coil & 1;
	return ret;
}

OTC_PRIV int maynuo_m97_set_bit(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_coil address, int value)
{
	return otc_modbus_write_coil(modbus, address, value);
}

OTC_PRIV int maynuo_m97_get_float(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_register address, float *value)
{
	uint16_t registers[2];
	int ret = otc_modbus_read_holding_registers(modbus, address, 2, registers);
	if (ret == OTC_OK)
		*value = RBFL(registers);
	return ret;
}

OTC_PRIV int maynuo_m97_set_float(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_register address, float value)
{
	uint16_t registers[2];
	WBFL(registers, value);
	return otc_modbus_write_multiple_registers(modbus, address, 2, registers);
}


static int maynuo_m97_cmd(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_mode cmd)
{
	uint16_t registers[1];
	WB16(registers, cmd);
	return otc_modbus_write_multiple_registers(modbus, CMD, 1, registers);
}

OTC_PRIV int maynuo_m97_get_mode(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_mode *mode)
{
	uint16_t registers[1];
	int ret;
	ret = otc_modbus_read_holding_registers(modbus, SETMODE, 1, registers);
	*mode = RB16(registers) & 0xFF;
	return ret;
}

OTC_PRIV int maynuo_m97_set_mode(struct otc_modbus_dev_inst *modbus,
		enum maynuo_m97_mode mode)
{
	return maynuo_m97_cmd(modbus, mode);
}

OTC_PRIV int maynuo_m97_set_input(struct otc_modbus_dev_inst *modbus, int enable)
{
	enum maynuo_m97_mode mode;
	int ret;
	if ((ret = maynuo_m97_get_mode(modbus, &mode)) != OTC_OK)
		return ret;
	if ((ret = maynuo_m97_cmd(modbus, enable ? INPUT_ON : INPUT_OFF)) != OTC_OK)
		return ret;
	return maynuo_m97_set_mode(modbus, mode);
}

OTC_PRIV int maynuo_m97_get_model_version(struct otc_modbus_dev_inst *modbus,
		uint16_t *model, uint16_t *version)
{
	uint16_t registers[2];
	int ret;
	ret = otc_modbus_read_holding_registers(modbus, MODEL, 2, registers);
	*model   = RB16(registers + 0);
	*version = RB16(registers + 1);
	return ret;
}


OTC_PRIV const char *maynuo_m97_mode_to_str(enum maynuo_m97_mode mode)
{
	switch (mode) {
	case CC:             return "CC";
	case CV:             return "CV";
	case CW:             return "CP";
	case CR:             return "CR";
	case CC_SOFT_START:  return "CC Soft Start";
	case DYNAMIC:        return "Dynamic";
	case SHORT_CIRCUIT:  return "Short Circuit";
	case LIST:           return "List Mode";
	case CC_L_AND_UL:    return "CC Loading and Unloading";
	case CV_L_AND_UL:    return "CV Loading and Unloading";
	case CW_L_AND_UL:    return "CP Loading and Unloading";
	case CR_L_AND_UL:    return "CR Loading and Unloading";
	case CC_TO_CV:       return "CC + CV";
	case CR_TO_CV:       return "CR + CV";
	case BATTERY_TEST:   return "Battery Test";
	case CV_SOFT_START:  return "CV Soft Start";
	default:             return "UNKNOWN";
	}
}


static void maynuo_m97_session_send_value(const struct otc_dev_inst *sdi, struct otc_channel *ch, float value, enum otc_mq mq, enum otc_unit unit, int digits)
{
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;

	otc_analog_init(&analog, &encoding, &meaning, &spec, digits);
	analog.meaning->channels = g_slist_append(NULL, ch);
	analog.num_samples = 1;
	analog.data = &value;
	analog.meaning->mq = mq;
	analog.meaning->unit = unit;
	analog.meaning->mqflags = OTC_MQFLAG_DC;

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;
	otc_session_send(sdi, &packet);
	g_slist_free(analog.meaning->channels);
}

OTC_PRIV int maynuo_m97_capture_start(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_modbus_dev_inst *modbus;
	int ret;

	modbus = sdi->conn;
	devc = sdi->priv;

	if ((ret = otc_modbus_read_holding_registers(modbus, U, 4, NULL)) == OTC_OK)
		devc->expecting_registers = 4;
	return ret;
}

OTC_PRIV int maynuo_m97_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_modbus_dev_inst *modbus;
	uint16_t registers[4];

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	modbus = sdi->conn;
	devc = sdi->priv;

	devc->expecting_registers = 0;
	if (otc_modbus_read_holding_registers(modbus, -1, 4, registers) == OTC_OK) {
		std_session_send_df_frame_begin(sdi);

		maynuo_m97_session_send_value(sdi, sdi->channels->data,
		                              RBFL(registers + 0),
		                              OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, 3);
		maynuo_m97_session_send_value(sdi, sdi->channels->next->data,
		                              RBFL(registers + 2),
		                              OTC_MQ_CURRENT, OTC_UNIT_AMPERE, 4);

		std_session_send_df_frame_end(sdi);
		otc_sw_limits_update_samples_read(&devc->limits, 1);
	}

	if (otc_sw_limits_check(&devc->limits)) {
		otc_dev_acquisition_stop(sdi);
		return TRUE;
	}

	maynuo_m97_capture_start(sdi);
	return TRUE;
}
