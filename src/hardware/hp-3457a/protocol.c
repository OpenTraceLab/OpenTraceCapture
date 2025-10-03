/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2016 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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
#include <math.h>
#include "../../scpi.h"
#include "protocol.h"

static int set_mq_volt(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags);
static int set_mq_amp(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags);
static int set_mq_ohm(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags);

/*
 * The source for the frequency measurement can be either AC voltage, AC+DC
 * voltage, AC current, or AC+DC current. Configuring this is not yet
 * supported. For details, see "FSOURCE" command.
 * The set_mode function is optional and can be set to NULL, but in that case
 * a cmd string must be provided.
 */
static const struct {
	enum otc_mq mq;
	enum otc_unit unit;
	const char *cmd;
	int (*set_mode)(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags);
} otc_mq_to_cmd_map[] = {
	{ OTC_MQ_VOLTAGE, OTC_UNIT_VOLT, "DCV", set_mq_volt },
	{ OTC_MQ_CURRENT, OTC_UNIT_AMPERE, "DCI", set_mq_amp },
	{ OTC_MQ_RESISTANCE, OTC_UNIT_OHM, "OHM", set_mq_ohm },
	{ OTC_MQ_FREQUENCY, OTC_UNIT_HERTZ, "FREQ", NULL },
};

static const struct rear_card_info rear_card_parameters[] = {
	{
		.type = REAR_TERMINALS,
		.card_id = 0,
		.name = "Rear terminals",
		.cg_name = "rear",
		.num_channels = 1,
	}, {
		.type = HP_44491A,
		.card_id = 44491,
		.name = "44491A Armature Relay Multiplexer",
		.cg_name = "44491a",
		.num_channels = 14,
	}, {
		.type = HP_44492A,
		.card_id = 44492,
		.name = "44492A Reed Relay Multiplexer",
		.cg_name = "44492a",
		.num_channels = 10,
	}
};

static int send_mq_ac_dc(struct otc_scpi_dev_inst *scpi, const char *mode,
			 enum otc_mqflag flags)
{
	const char *ac_flag, *dc_flag;

	if (flags & ~(OTC_MQFLAG_AC | OTC_MQFLAG_DC))
		return OTC_ERR_NA;

	ac_flag = (flags & OTC_MQFLAG_AC) ? "AC" : "";
	dc_flag = "";
	/* Must specify DC measurement when AC flag is not given. */
	if ((flags & OTC_MQFLAG_DC) || !(flags & OTC_MQFLAG_AC))
		dc_flag = "DC";

	return otc_scpi_send(scpi, "%s%s%s", ac_flag, dc_flag, mode);
}

static int set_mq_volt(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags)
{
	return send_mq_ac_dc(scpi, "V", flags);
}

static int set_mq_amp(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags)
{
	return send_mq_ac_dc(scpi, "I", flags);
}

static int set_mq_ohm(struct otc_scpi_dev_inst *scpi, enum otc_mqflag flags)
{
	const char *ohm_flag;

	if (flags & ~(OTC_MQFLAG_FOUR_WIRE))
		return OTC_ERR_NA;

	ohm_flag = (flags & OTC_MQFLAG_FOUR_WIRE) ? "F" : "";
	return otc_scpi_send(scpi, "OHM%s", ohm_flag);
}

OTC_PRIV int hp_3457a_set_mq(const struct otc_dev_inst *sdi, enum otc_mq mq,
			    enum otc_mqflag mq_flags)
{
	int ret;
	size_t i;
	struct otc_scpi_dev_inst *scpi = sdi->conn;
	struct dev_context *devc = sdi->priv;

	/* No need to send command if we're not changing measurement type. */
	if (devc->measurement_mq == mq)
		return OTC_OK;

	for (i = 0; i < ARRAY_SIZE(otc_mq_to_cmd_map); i++) {
		if (otc_mq_to_cmd_map[i].mq != mq)
			continue;
		if (otc_mq_to_cmd_map[i].set_mode) {
			ret = otc_mq_to_cmd_map[i].set_mode(scpi, mq_flags);
		} else {
			ret = otc_scpi_send(scpi, otc_mq_to_cmd_map[i].cmd);
		}
		if (ret == OTC_OK) {
			devc->measurement_mq = otc_mq_to_cmd_map[i].mq;
			devc->measurement_mq_flags = mq_flags;
			devc->measurement_unit = otc_mq_to_cmd_map[i].unit;
		}
		return ret;
	}

	return OTC_ERR_NA;
}

OTC_PRIV const struct rear_card_info *hp_3457a_probe_rear_card(struct otc_scpi_dev_inst *scpi)
{
	size_t i;
	float card_fval;
	unsigned int card_id;
	const struct rear_card_info *rear_card = NULL;

	if (otc_scpi_get_float(scpi, "OPT?", &card_fval) != OTC_OK)
		return NULL;

	card_id = (unsigned int)card_fval;

	for (i = 0; i < ARRAY_SIZE(rear_card_parameters); i++) {
		if (rear_card_parameters[i].card_id == card_id) {
			rear_card = rear_card_parameters + i;
			break;
		}
	}

	if (!rear_card)
		return NULL;

	otc_info("Found %s.", rear_card->name);

	return rear_card;
}

OTC_PRIV int hp_3457a_set_nplc(const struct otc_dev_inst *sdi, float nplc)
{
	int ret;
	struct otc_scpi_dev_inst *scpi = sdi->conn;
	struct dev_context *devc = sdi->priv;

	if ((nplc < 1E-6) || (nplc > 100))
		return OTC_ERR_ARG;

	/* Only need one digit of precision here. */
	ret = otc_scpi_send(scpi, "NPLC %.0E", nplc);

	/*
	 * The instrument only has a few valid NPLC setting, so get back the
	 * one which was selected.
	 */
	otc_scpi_get_float(scpi, "NPLC?", &devc->nplc);

	return ret;
}

OTC_PRIV int hp_3457a_select_input(const struct otc_dev_inst *sdi,
				  enum channel_conn loc)
{
	int ret;
	struct otc_scpi_dev_inst *scpi = sdi->conn;
	struct dev_context *devc = sdi->priv;

	if (devc->input_loc == loc)
		return OTC_OK;

	ret = otc_scpi_send(scpi, "TERM %s", (loc == CONN_FRONT) ? "FRONT": "REAR");
	if (ret == OTC_OK)
		devc->input_loc = loc;

	return ret;
}

OTC_PRIV int hp_3457a_send_scan_list(const struct otc_dev_inst *sdi,
				    unsigned int *channels, size_t len)
{
	size_t i;
	char chan[16], list_str[64] = "";

	for (i = 0; i < len; i++) {
		g_snprintf(chan, sizeof(chan), ",%u", channels[i]);
		g_strlcat(list_str, chan, sizeof(list_str));
	}

	return otc_scpi_send(sdi->conn, "SLIST %s", list_str);
}

/* HIRES register only contains valid data with 10 or more powerline cycles. */
static int is_highres_enabled(struct dev_context *devc)
{
	return (devc->nplc >= 10.0);
}

static void activate_next_channel(struct dev_context *devc)
{
	GSList *list_elem;
	struct otc_channel *chan;

	list_elem = g_slist_find(devc->active_channels, devc->current_channel);
	if (list_elem)
		list_elem = list_elem->next;
	if (!list_elem)
		list_elem = devc->active_channels;

	chan = list_elem->data;

	devc->current_channel = chan;
}

static void retrigger_measurement(struct otc_scpi_dev_inst *scpi,
				  struct dev_context *devc)
{
	otc_scpi_send(scpi, "?");
	devc->acq_state = ACQ_TRIGGERED_MEASUREMENT;
}

static void request_hires(struct otc_scpi_dev_inst *scpi,
			  struct dev_context *devc)
{
	otc_scpi_send(scpi, "RMATH HIRES");
	devc->acq_state = ACQ_REQUESTED_HIRES;
}

static void request_range(struct otc_scpi_dev_inst *scpi,
			  struct dev_context *devc)
{
	otc_scpi_send(scpi, "RANGE?");
	devc->acq_state = ACQ_REQUESTED_RANGE;
}

static void request_current_channel(struct otc_scpi_dev_inst *scpi,
				    struct dev_context *devc)
{
	otc_scpi_send(scpi, "CHAN?");
	devc->acq_state = ACQ_REQUESTED_CHANNEL_SYNC;
}

/*
 * Calculate the number of leading zeroes in the measurement.
 *
 * Depending on the range and measurement, a reading may not have eight digits
 * of resolution. For example, on a 30V range:
 *    : 10.000000 V has 8 significant digits
 *    :  9.999999 V has 7 significant digits
 *    :  0.999999 V has 6 significant digits
 *
 * The number of significant digits is determined based on the range in which
 * the measurement was taken:
 *    1. By taking the base 10 logarithm of the range, and converting that to
 *       an integer, we can get the minimum reading which has a full resolution
 *       reading. Raising 10 to the integer power gives the full resolution.
 *       Ex: For 30 V range, a full resolution reading is 10.000000.
 *    2. A ratio is taken between the full resolution reading and the
 *       measurement. Since the full resolution reading is a power of 10,
 *       for every leading zero, this ratio will be slightly higher than a
 *       power of 10. For example, for 10 V full resolution:
 *          : 10.000000 V, ratio = 1.0000000
 *          :  9.999999 V, ratio = 1.0000001
 *          :  0.999999 V, ratio = 10.000001
 *    3. The ratio is rounded up to prevent loss of precision in the next step.
 *    4. The base 10 logarithm of the ratio is taken, then rounded up. This
 *       gives the number of leading zeroes in the measurement.
 *       For example, for 10 V full resolution:
 *          : 10.000000 V, ceil(1.0000000) =  1, log10 = 0.00; 0 leading zeroes
 *          :  9.999999 V, ceil(1.0000001) =  2, log10 = 0.30; 1 leading zero
 *          :  0.999999 V, ceil(10.000001) = 11, log10 = 1.04, 2 leading zeroes
 *    5. The number of leading zeroes is subtracted from the maximum number of
 *       significant digits, 8, at 7 1/2 digits resolution.
 *       For a 10 V full resolution reading, this gives:
 *          : 10.000000 V, 0 leading zeroes => 8 significant digits
 *          :  9.999999 V, 1 leading zero   => 7 significant digits
 *          :  0.999999 V, 2 leading zeroes => 6 significant digits
 *
 * Single precision floating point numbers can achieve about 16 million counts,
 * but in high resolution mode we can get as much as 30 million counts. As a
 * result, these calculations must be done with double precision
 * (the HP 3457A is a very precise instrument).
 */
static int calculate_num_zero_digits(double measurement, double range)
{
	int zero_digits;
	double min_full_res_reading, log10_range, full_res_ratio;

	log10_range = log10(range);
	min_full_res_reading = pow(10, (int)log10_range);
	if (measurement > min_full_res_reading) {
		zero_digits = 0;
	} else if (measurement == 0.0) {
		zero_digits = 0;
	} else {
		full_res_ratio = min_full_res_reading / measurement;
		zero_digits = ceil(log10(ceil(full_res_ratio)));
	}

	return zero_digits;
}

/*
 * Until the output modules understand double precision data, we need to send
 * the measurement as floats instead of doubles, hence, the dance with
 * measurement_workaround double to float conversion.
 * See bug #779 for details.
 * The workaround should be removed once the output modules are fixed.
 */
static void acq_send_measurement(struct otc_dev_inst *sdi)
{
	double hires_measurement;
	float measurement_workaround;
	int zero_digits, num_digits;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	struct dev_context *devc = sdi->priv;

	hires_measurement = devc->base_measurement;
	if (is_highres_enabled(devc))
		hires_measurement += devc->hires_register;

	/* Figure out how many of the digits are significant. */
	num_digits = is_highres_enabled(devc) ? 8 : 7;
	zero_digits = calculate_num_zero_digits(hires_measurement,
						devc->measurement_range);
	num_digits = num_digits - zero_digits;

	packet.type = OTC_DF_ANALOG;
	packet.payload = &analog;

	otc_analog_init(&analog, &encoding, &meaning, &spec, num_digits);
	encoding.unitsize = sizeof(float);

	meaning.channels = g_slist_append(NULL, devc->current_channel);

	measurement_workaround = hires_measurement;
	analog.num_samples = 1;
	analog.data = &measurement_workaround;

	meaning.mq = devc->measurement_mq;
	meaning.mqflags = devc->measurement_mq_flags;
	meaning.unit = devc->measurement_unit;

	otc_session_send(sdi, &packet);

	g_slist_free(meaning.channels);
}

/*
 * The scan-advance channel sync -- call to request_current_channel() -- is not
 * necessarily needed. It is done in case we have a communication error and the
 * DMM advances the channel without having sent the reading. The DMM only
 * advances the channel when it thinks it sent the reading over HP-IB. Thus, on
 * most errors we can retrigger the measurement and still be in sync. This
 * check is done to make sure we don't fall out of sync due to obscure errors.
 */
OTC_PRIV int hp_3457a_receive_data(int fd, int revents, void *cb_data)
{
	int ret;
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;
	struct channel_context *chanc;
	struct otc_dev_inst *sdi;

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	scpi = sdi->conn;

	switch (devc->acq_state) {
	case ACQ_TRIGGERED_MEASUREMENT:
		ret = otc_scpi_get_double(scpi, NULL, &devc->base_measurement);
		if (ret != OTC_OK) {
			retrigger_measurement(scpi, devc);
			return TRUE;
		}

		if (is_highres_enabled(devc))
			request_hires(scpi, devc);
		else
			request_range(scpi, devc);

		break;
	case ACQ_REQUESTED_HIRES:
		ret = otc_scpi_get_double(scpi, NULL, &devc->hires_register);
		if (ret != OTC_OK) {
			retrigger_measurement(scpi, devc);
			return TRUE;
		}
		request_range(scpi, devc);
		break;
	case ACQ_REQUESTED_RANGE:
		ret = otc_scpi_get_double(scpi, NULL, &devc->measurement_range);
		if (ret != OTC_OK) {
			retrigger_measurement(scpi, devc);
			return TRUE;
		}
		devc->acq_state = ACQ_GOT_MEASUREMENT;
		break;
	case ACQ_REQUESTED_CHANNEL_SYNC:
		ret = otc_scpi_get_double(scpi, NULL, &devc->last_channel_sync);
		if (ret != OTC_OK) {
			otc_err("Cannot check channel synchronization.");
			otc_dev_acquisition_stop(sdi);
			return FALSE;
		}
		devc->acq_state = ACQ_GOT_CHANNEL_SYNC;
		break;
	default:
		return FALSE;
	}

	if (devc->acq_state == ACQ_GOT_MEASUREMENT) {
		acq_send_measurement(sdi);
		devc->num_samples++;
	}

	if (devc->acq_state == ACQ_GOT_CHANNEL_SYNC) {
		chanc = devc->current_channel->priv;
		if (chanc->index != devc->last_channel_sync) {
			otc_err("Current channel and scan advance out of sync.");
			otc_err("Expected channel %u, but device says %u",
			       chanc->index,
			       (unsigned int)devc->last_channel_sync);
			otc_dev_acquisition_stop(sdi);
			return FALSE;
		}
		/* All is good. Back to business. */
		retrigger_measurement(scpi, devc);
	}

	if (devc->limit_samples && (devc->num_samples >= devc->limit_samples)) {
		otc_dev_acquisition_stop(sdi);
		return FALSE;
	}

	/* Got more to go. */
	if (devc->acq_state == ACQ_GOT_MEASUREMENT) {
		activate_next_channel(devc);
		/* Retrigger, or check if scan-advance is in sync. */
		if (((devc->num_samples % 10) == 9)
		   && (devc->num_active_channels > 1)) {
			request_current_channel(scpi, devc);
		} else {
			retrigger_measurement(scpi, devc);
		}
	}

	return TRUE;
}
