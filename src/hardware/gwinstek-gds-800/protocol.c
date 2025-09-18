/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2015 Martin Lederhilger <martin.lederhilger@gmx.at>
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

#include "protocol.h"
#include <math.h>
#include <string.h>

#define ANALOG_CHANNELS 2
#define VERTICAL_DIVISIONS 10

static int read_data(struct otc_dev_inst *sdi,
		struct otc_scpi_dev_inst *scpi, struct dev_context *devc,
		int data_size)
{
	int len;

	len = otc_scpi_read_data(scpi,
		&devc->rcv_buffer[devc->cur_rcv_buffer_position],
		data_size - devc->cur_rcv_buffer_position);
	if (len < 0) {
		otc_err("Read data error.");
		otc_dev_acquisition_stop(sdi);
		devc->cur_rcv_buffer_position = 0;
		return OTC_ERR;
	}

	devc->cur_rcv_buffer_position += len;

	/* Handle the case where otc_scpi_read_data stopped at the newline. */
	if (len < data_size && otc_scpi_read_complete(scpi)) {
		devc->rcv_buffer[devc->cur_rcv_buffer_position] = '\n';
		devc->cur_rcv_buffer_position++;
	}

	if (devc->cur_rcv_buffer_position < data_size)
		return OTC_ERR; /* Not finished yet. */
	else if (devc->cur_rcv_buffer_position == data_size) {
		 devc->cur_rcv_buffer_position = 0;
		return OTC_OK;
	} else {
		otc_err("Too many bytes read.");
		otc_dev_acquisition_stop(sdi);
		devc->cur_rcv_buffer_position = 0;
		return OTC_ERR;
	}
}

OTC_PRIV int gwinstek_gds_800_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct otc_scpi_dev_inst *scpi;
	struct dev_context *devc;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	char command[32];
	char *response;
	float volts_per_division;
	int num_samples, i;
	float samples[MAX_SAMPLES];
	uint32_t sample_rate;
	char *end_ptr;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	scpi = sdi->conn;

	if (!(revents == G_IO_IN || revents == 0))
		return TRUE;

	switch (devc->state) {
	case START_ACQUISITION:
		if (otc_scpi_send(scpi, ":TRIG:MOD 3") != OTC_OK) {
			otc_err("Failed to set trigger mode to SINGLE.");
			otc_dev_acquisition_stop(sdi);
			return TRUE;
		}
		if (otc_scpi_send(scpi, ":STOP") != OTC_OK) {
			otc_err("Failed to put the trigger system into STOP state.");
			otc_dev_acquisition_stop(sdi);
			return TRUE;
		}
		if (otc_scpi_send(scpi, ":RUN") != OTC_OK) {
			otc_err("Failed to put the trigger system into RUN state.");
			otc_dev_acquisition_stop(sdi);
			return TRUE;
		}

		devc->cur_acq_channel = 0;
		devc->state = START_TRANSFER_OF_CHANNEL_DATA;
		break;
	case START_TRANSFER_OF_CHANNEL_DATA:
		if (((struct otc_channel *)g_slist_nth_data(sdi->channels, devc->cur_acq_channel))->enabled) {
			if (otc_scpi_send(scpi, ":ACQ%d:MEM?", devc->cur_acq_channel+1) != OTC_OK) {
				otc_err("Failed to acquire memory.");
				otc_dev_acquisition_stop(sdi);
				return TRUE;
			}
			if (otc_scpi_read_begin(scpi) != OTC_OK) {
				otc_err("Could not begin reading SCPI response.");
				otc_dev_acquisition_stop(sdi);
				return TRUE;
			}
			devc->state = WAIT_FOR_TRANSFER_OF_BEGIN_TRANSMISSION_COMPLETE;
			devc->cur_rcv_buffer_position = 0;
		} else {
			/* All channels acquired. */
			if (devc->cur_acq_channel == ANALOG_CHANNELS - 1) {
				otc_spew("All channels acquired.");

				if (devc->cur_acq_frame == devc->frame_limit - 1) {
					/* All frames accquired. */
					otc_spew("All frames acquired.");

					otc_dev_acquisition_stop(sdi);
					return TRUE;
				} else {
					/* Start acquiring next frame. */
					if (devc->df_started) {
						std_session_send_df_frame_end(sdi);
						std_session_send_df_frame_begin(sdi);
					}

					devc->cur_acq_frame++;
					devc->state = START_ACQUISITION;
				}
			} else {
				/* Start acquiring next channel. */
				devc->cur_acq_channel++;
			}
		}
		break;
	case WAIT_FOR_TRANSFER_OF_BEGIN_TRANSMISSION_COMPLETE:
		if (read_data(sdi, scpi, devc, 1) < 0)
			break;
		if (devc->rcv_buffer[0] == '#')
			devc->state = WAIT_FOR_TRANSFER_OF_DATA_SIZE_DIGIT_COMPLETE;
		break;
	case WAIT_FOR_TRANSFER_OF_DATA_SIZE_DIGIT_COMPLETE:
		if (read_data(sdi, scpi, devc, 1) < 0)
			break;
		if (devc->rcv_buffer[0] != '4' &&
			devc->rcv_buffer[0] != '5' &&
			devc->rcv_buffer[0] != '6') {
			otc_err("Data size digits is not 4, 5 or 6 but "
			       "'%c'.", devc->rcv_buffer[0]);
			otc_dev_acquisition_stop(sdi);
			return TRUE;
		} else {
			devc->data_size_digits = devc->rcv_buffer[0] - '0';
			devc->state = WAIT_FOR_TRANSFER_OF_DATA_SIZE_COMPLETE;
		}
		break;
	case WAIT_FOR_TRANSFER_OF_DATA_SIZE_COMPLETE:
		if (read_data(sdi, scpi, devc, devc->data_size_digits) < 0)
			break;
		devc->rcv_buffer[devc->data_size_digits] = 0;
		if (otc_atoi(devc->rcv_buffer, &devc->data_size) != OTC_OK) {
			otc_err("Could not parse data size '%s'", devc->rcv_buffer);
			otc_dev_acquisition_stop(sdi);
			return TRUE;
		} else
			devc->state = WAIT_FOR_TRANSFER_OF_SAMPLE_RATE_COMPLETE;
		break;
	case WAIT_FOR_TRANSFER_OF_SAMPLE_RATE_COMPLETE:
		if (read_data(sdi, scpi, devc, sizeof(float)) < 0)
			break;
		/*
		 * Contrary to the documentation, this field is
		 * transfered with most significant byte first!
		 */
		sample_rate = RB32(devc->rcv_buffer);
		memcpy(&devc->sample_rate, &sample_rate, sizeof(float));
		devc->state = WAIT_FOR_TRANSFER_OF_CHANNEL_INDICATOR_COMPLETE;

		if (!devc->df_started) {
			std_session_send_df_header(sdi);
			std_session_send_df_frame_begin(sdi);
			devc->df_started = TRUE;
		}
		break;
	case WAIT_FOR_TRANSFER_OF_CHANNEL_INDICATOR_COMPLETE:
		if (read_data(sdi, scpi, devc, 1) == OTC_OK)
			devc->state = WAIT_FOR_TRANSFER_OF_RESERVED_DATA_COMPLETE;
		break;
	case WAIT_FOR_TRANSFER_OF_RESERVED_DATA_COMPLETE:
		if (read_data(sdi, scpi, devc, 3) == OTC_OK)
			devc->state = WAIT_FOR_TRANSFER_OF_CHANNEL_DATA_COMPLETE;
		break;
	case WAIT_FOR_TRANSFER_OF_CHANNEL_DATA_COMPLETE:
		if (read_data(sdi, scpi, devc, devc->data_size - 8) < 0)
			break;

		/* Fetch data needed for conversion from device. */
		snprintf(command, sizeof(command), ":CHAN%d:SCAL?",
				devc->cur_acq_channel + 1);
		if (otc_scpi_get_string(scpi, command, &response) != OTC_OK) {
			otc_err("Failed to get volts per division.");
			otc_dev_acquisition_stop(sdi);
			return TRUE;
		}
		volts_per_division = g_ascii_strtod(response, &end_ptr);
		if (!strcmp(end_ptr, "mV"))
			volts_per_division *= 1.e-3;
		g_free(response);

		num_samples = (devc->data_size - 8) / 2;
		otc_spew("Received %d number of samples from channel "
			"%d.", num_samples, devc->cur_acq_channel + 1);

		float vbit = volts_per_division * VERTICAL_DIVISIONS / 256.0;
		float vbitlog = log10f(vbit);
		int digits = -(int)vbitlog + (vbitlog < 0.0);

		/* Convert data. */
		for (i = 0; i < num_samples; i++)
			samples[i] = ((float) ((int16_t) (RB16(&devc->rcv_buffer[i*2])))) * vbit;

		/* Fill frame. */
		otc_analog_init(&analog, &encoding, &meaning, &spec, digits);
		analog.meaning->channels = g_slist_append(NULL, g_slist_nth_data(sdi->channels, devc->cur_acq_channel));
		analog.num_samples = num_samples;
		analog.data = samples;
		analog.meaning->mq = OTC_MQ_VOLTAGE;
		analog.meaning->unit = OTC_UNIT_VOLT;
		analog.meaning->mqflags = 0;
		packet.type = OTC_DF_ANALOG;
		packet.payload = &analog;
		otc_session_send(sdi, &packet);
		g_slist_free(analog.meaning->channels);

		/* All channels acquired. */
		if (devc->cur_acq_channel == ANALOG_CHANNELS - 1) {
			otc_spew("All channels acquired.");

			if (devc->cur_acq_frame == devc->frame_limit - 1) {
				/* All frames acquired. */
				otc_spew("All frames acquired.");
				otc_dev_acquisition_stop(sdi);
				return TRUE;
			} else {
				/* Start acquiring next frame. */
				if (devc->df_started) {
					std_session_send_df_frame_end(sdi);
					std_session_send_df_frame_begin(sdi);
				}
				devc->cur_acq_frame++;
				devc->state = START_ACQUISITION;
			}
		} else {
			/* Start acquiring next channel. */
			devc->state = START_TRANSFER_OF_CHANNEL_DATA;
			devc->cur_acq_channel++;
			return TRUE;
		}
		break;
	}

	return TRUE;
}
