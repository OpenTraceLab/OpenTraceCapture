/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2021 Ultra-Embedded <admin@ultra-embedded.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include "protocol.h"

/** Memory/Register read or write request command structure. */
struct ob_command_s
{
	uint8_t  command;
	uint8_t  length;
	uint16_t seq_num;
	uint32_t addr;
};

/** Memory/Register read or write status response structure. */
struct ob_status_s
{
	uint16_t seq_num;
	uint16_t status;
};

/* Interface commands */
#define CMD_ID_ECHO       0x01
#define CMD_ID_DRAIN      0x02
#define CMD_ID_READ       0x10
#define CMD_ID_WRITE8_NP  0x20 /* 8-bit write (with response) */
#define CMD_ID_WRITE16_NP 0x21 /* 16-bit write (with response) */
#define CMD_ID_WRITE_NP   0x22 /* 32-bit write (with response) */
#define CMD_ID_WRITE8     0x30 /* 8-bit write */
#define CMD_ID_WRITE16    0x31 /* 16-bit write */
#define CMD_ID_WRITE      0x32 /* 32-bit write */
#define CMD_ID_GPIO_WR    0x40
#define CMD_ID_GPIO_RD    0x41

#define MAX_WR_CHUNKS     32
#define MAX_RD_CHUNKS     32
#define MAX_CHUNK_SIZE    512

/* Memory layout */
#define CFG_BASE_ADDR     0x80000000
#define MEM_BASE_ADDR     0x00000000

/* Register definitions - auto generated*/
#define LA_BUFFER_CFG     0x0
	#define LA_BUFFER_CFG_CONT                   31
	#define LA_BUFFER_CFG_CONT_SHIFT             31
	#define LA_BUFFER_CFG_CONT_MASK              0x1

	#define LA_BUFFER_CFG_WIDTH                  7
	#define LA_BUFFER_CFG_WIDTH_SHIFT            7
	#define LA_BUFFER_CFG_WIDTH_MASK             0x1

	#define LA_BUFFER_CFG_CLK_DIV_SHIFT          2
	#define LA_BUFFER_CFG_CLK_DIV_MASK           0xf

	#define LA_BUFFER_CFG_CLK_SRC                1
	#define LA_BUFFER_CFG_CLK_SRC_SHIFT          1
	#define LA_BUFFER_CFG_CLK_SRC_MASK           0x1

	#define LA_BUFFER_CFG_ENABLED                0
	#define LA_BUFFER_CFG_ENABLED_SHIFT          0
	#define LA_BUFFER_CFG_ENABLED_MASK           0x1

#define LA_BUFFER_STS     0x4
	#define LA_BUFFER_STS_DATA_LOSS              2
	#define LA_BUFFER_STS_DATA_LOSS_SHIFT        2
	#define LA_BUFFER_STS_DATA_LOSS_MASK         0x1

	#define LA_BUFFER_STS_WRAPPED                1
	#define LA_BUFFER_STS_WRAPPED_SHIFT          1
	#define LA_BUFFER_STS_WRAPPED_MASK           0x1

	#define LA_BUFFER_STS_TRIG                   0
	#define LA_BUFFER_STS_TRIG_SHIFT             0
	#define LA_BUFFER_STS_TRIG_MASK              0x1

#define LA_BUFFER_BASE    0x8
	#define LA_BUFFER_BASE_ADDR_SHIFT            0
	#define LA_BUFFER_BASE_ADDR_MASK             0xffffffff

#define LA_BUFFER_END     0xc
	#define LA_BUFFER_END_ADDR_SHIFT             0
	#define LA_BUFFER_END_ADDR_MASK              0xffffffff

#define LA_BUFFER_CURRENT  0x10
	#define LA_BUFFER_CURRENT_ADDR_SHIFT         0
	#define LA_BUFFER_CURRENT_ADDR_MASK          0xffffffff

#define LA_BUFFER_SAMPLES  0x14
	#define LA_BUFFER_SAMPLES_COUNT_SHIFT        0
	#define LA_BUFFER_SAMPLES_COUNT_MASK         0xffffffff

#define LA_BUFFER_TRIG_ENABLE  0x18
	#define LA_BUFFER_TRIG_ENABLE_VALUE_SHIFT    0
	#define LA_BUFFER_TRIG_ENABLE_VALUE_MASK     0xffffffff

#define LA_BUFFER_TRIG_SENSE  0x1c
	#define LA_BUFFER_TRIG_SENSE_VALUE_SHIFT     0
	#define LA_BUFFER_TRIG_SENSE_VALUE_MASK      0xffffffff

#define LA_BUFFER_TRIG_LEVEL  0x20
	#define LA_BUFFER_TRIG_LEVEL_VALUE_SHIFT     0
	#define LA_BUFFER_TRIG_LEVEL_VALUE_MASK      0xffffffff

/**
 * Perform a low level device write operation (with completion checks).
 *
 * @param[in] devc   The device context. Must not be NULL.
 * @param[in] buf    Buffer to write data from. Must not be NULL.
 * @param[in] size   Number of bytes to write.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 */
static int openlb_write(struct dev_context *devc, uint8_t *buf, int size)
{
	int i, bytes_written;
	GString *s;

	/* Note: Caller checks devc, devc->ftdic, buf, size. */

	s = g_string_sized_new(100);
	g_string_printf(s, "Writing %d bytes: ", size);
	for (i = 0; i < size; i++)
		g_string_append_printf(s, "0x%02x ", buf[i]);
	otc_spew("%s", s->str);
	g_string_free(s, TRUE);

	bytes_written = ftdi_write_data(devc->ftdic, buf, size);
	if (bytes_written < 0) {
		otc_err("Failed to write FTDI data (%d): %s.",
			   bytes_written, ftdi_get_error_string(devc->ftdic));
	} else if (bytes_written != size) {
		otc_err("FTDI write error, only %d/%d bytes written: %s.",
			   bytes_written, size, ftdi_get_error_string(devc->ftdic));
		bytes_written = -1;
	}

	return bytes_written;
}

/**
 * Perform a low level read operation (with retries until complete data returned).
 *
 * @param[in] devc   The device context. Must not be NULL.
 * @param[in] buf    Buffer to read data into.
 * @param[in] size   Number of bytes to read.
 *
 * @retval Number of bytes read or -1 on error.
 */
static int openlb_read(struct dev_context *devc, uint8_t *buf, int size)
{
	int total = 0;
	int retries = 0;

	if (size <= 0)
		return 0;

	do {
		int remain = size - total;
		int l = ftdi_read_data(devc->ftdic, buf, remain);
		if (l < 0) {
			otc_err("Failed to read FTDI data (%d): %s.", l, ftdi_get_error_string(devc->ftdic));
			return -1;
		}
		total += l;

		if (retries++ == 10) {
			otc_err("Failed to get total expected read data\n");
			return -1;
		}
	}
	while (total < size);

	return size;
}

/**
 * Perform a 32-bit write to the target using the open-logic-bit read / write protocol.
 *
 * This is used for accessing device registers to control the capture.
 *
 * @param[in] devc   The device context. Must not be NULL.
 * @param[in] addr   Address to write to.
 * @param[in] data   Data word to write.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 */
static int openlb_write32(struct dev_context *devc, uint32_t addr, uint32_t data)
{
	uint8_t wr_buf[sizeof(struct ob_command_s) + 4];
	uint8_t rd_buf[sizeof(struct ob_status_s)];

	struct ob_command_s *cmd = (struct ob_command_s*)wr_buf;
	cmd->command = CMD_ID_WRITE_NP;
	cmd->length  = 1;
	cmd->seq_num = devc->seq_num;
	cmd->addr    = addr;

	uint8_t *p_wr = &wr_buf[sizeof(struct ob_command_s)];
	*p_wr++ = data >> 0;
	*p_wr++ = data >> 8;
	*p_wr++ = data >> 16;
	*p_wr++ = data >> 24;

	if (openlb_write(devc, wr_buf, sizeof(struct ob_command_s) + 4) < 0)
		return OTC_ERR;

	if (openlb_read(devc, rd_buf, sizeof(rd_buf)) < 0)
		return OTC_ERR;

	struct ob_status_s *sts = (struct ob_status_s *)&rd_buf[0];
	if (sts->seq_num != devc->seq_num) {
		otc_err("ERROR: Sequence number: %04x != %04x\n", sts->seq_num, devc->seq_num);
		return OTC_ERR;
	}

	devc->seq_num++;
	return OTC_OK;
}

/**
 * Perform a 32-bit read from the target using the open-logic-bit read / write protocol.
 *
 * This is used for accessing device registers to read the capture status.
 *
 * @param[in] devc   The device context. Must not be NULL.
 * @param[in] addr   Address to read.
 * @param[in] data   Pointer to data word read. Must not be NULL.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 */
static int openlb_read32(struct dev_context *devc, uint32_t addr, uint32_t *data)
{
	uint8_t wr_buf[sizeof(struct ob_command_s)];
	uint8_t rd_buf[sizeof(struct ob_status_s)+4];

	struct ob_command_s *cmd = (struct ob_command_s*)wr_buf;
	cmd->command = CMD_ID_READ;
	cmd->length  = 1;
	cmd->seq_num = devc->seq_num;
	cmd->addr    = addr;

	if (openlb_write(devc, wr_buf, sizeof(struct ob_command_s)) < 0)
		return OTC_ERR;

	if (openlb_read(devc, rd_buf, sizeof(rd_buf)) < 0)
		return OTC_ERR;

	struct ob_status_s *sts = (struct ob_status_s *)&rd_buf[4];
	if (sts->seq_num != devc->seq_num) {
		otc_err("ERROR: Sequence number: %04x != %04x\n", sts->seq_num, devc->seq_num);
		return OTC_ERR;
	}

	(*data) = rd_buf[3];
	(*data) <<= 8;
	(*data)|= rd_buf[2];
	(*data) <<= 8;
	(*data)|= rd_buf[1];
	(*data) <<= 8;
	(*data)|= rd_buf[0];

	devc->seq_num++;
	return OTC_OK;
}

/**
 * Read a block of data from the device capture memory.
 *
 * @param[in] devc   The device context. Must not be NULL.
 * @param[in] addr   Address to read from.
 * @param[in] data   Buffer to read data into. Must be 4 byte aligned.
 * @param[in] length Length in bytes of buffer to read.
 *
 * @retval OTC_OK Success.
 * @retval OTC_ERR Error.
 */
static int openlb_read_block(struct dev_context *devc, uint32_t addr, uint8_t *data, int length)
{
	uint8_t  wr_buf[(8 * MAX_RD_CHUNKS)];
	uint8_t  rd_buf[(MAX_RD_CHUNKS * MAX_CHUNK_SIZE) + (MAX_RD_CHUNKS * 4)];
	int      actual_wr = 0;
	int      actual_rd = 0;

	while (actual_wr < length) {
		int chunks   = 0;

		struct ob_command_s *cmd = (struct ob_command_s *)wr_buf;
		for (int i=0;i<MAX_RD_CHUNKS;i++) {
			cmd->command = CMD_ID_READ;
			cmd->length  = MAX_CHUNK_SIZE/4;
			cmd->seq_num = devc->seq_num++;
			cmd->addr    = addr;
			cmd++;

			addr        += MAX_CHUNK_SIZE;
			actual_wr   += MAX_CHUNK_SIZE;
			chunks      += 1;
			if (actual_wr >= length)
				break;
		}

		otc_dbg("write: %d [chunks=%d]\n", (int)sizeof(struct ob_command_s) * chunks, chunks);
		if (openlb_write(devc, wr_buf, sizeof(struct ob_command_s) * chunks) < 0)
			return OTC_ERR;

		int read_total = 0;
		int expected   = (MAX_CHUNK_SIZE + 4) * chunks;
		do {
			int rd_len = expected;
			if (openlb_read(devc, rd_buf, expected) < 0)
				return OTC_ERR;

			read_total += rd_len;
			otc_dbg("read: %d out of %d\n", read_total, expected);
		}
		while (read_total < expected);

		otc_dbg("read_total: %d out of %d\n", actual_rd, length);

		uint8_t *p = rd_buf;
		for (int i=0;i<chunks;i++) {
			int remain = length - actual_rd;
			if (remain > MAX_CHUNK_SIZE)
				remain = MAX_CHUNK_SIZE;

			memcpy(data, p, remain);

			data      += MAX_CHUNK_SIZE;
			p         += MAX_CHUNK_SIZE + 4;
			actual_rd += remain;
		}
	}

	return OTC_OK;
}

/**
 * Pass sample buffer to libsigrok core.
 *
 * @param[in] devc   The device context. Must not be NULL.
 * @param[in] samples_to_send Number of samples in the sample buffer (devc->data_buf).
 */
static void openlb_send_samples(struct otc_dev_inst *sdi, uint32_t samples_to_send)
{
	struct otc_datafeed_packet packet;
	struct otc_datafeed_logic logic;
	struct dev_context *devc;

	otc_spew("Sending %d samples.", samples_to_send);

	devc = sdi->priv;

	packet.type    = OTC_DF_LOGIC;
	packet.payload = &logic;
	logic.unitsize = 2;
	logic.length   = samples_to_send * logic.unitsize;
	logic.data     = devc->data_buf;

	otc_session_send(sdi, &packet);
}

/**
 * Copy a single sample into the sample buffer.
 *
 * This may or may not be passed to libsigrok based on the flush argument
 * or the buffer becoming full.
 *
 * @param[in] sdi Device instance data. Must not be NULL.
 * @param[in] sample Sample value.
 * @param[in] flush Flush sample buffer - pass to libsigrok.
 */
static void openlb_push_sample(struct otc_dev_inst *sdi, uint16_t sample, int flush)
{
	struct dev_context *devc = sdi->priv;

	if (devc->data_pos == (DATA_BUF_SIZE-1))
		flush = 1;

	if (flush && devc->data_pos > 0) {
		otc_dbg("flushing %d\n", devc->data_pos);
		openlb_send_samples(sdi, devc->data_pos);
		devc->data_pos = 0;
	}

	if (devc->num_samples < devc->limit_samples) {
		devc->data_buf[devc->data_pos++] = sample;
		devc->num_samples++;
	}
}

/**
 * Map configured triggers to device specific register values.
 *
 * @param[in] sdi Device instance data. Must not be NULL.
 */
OTC_PRIV int openlb_convert_triggers(const struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	struct otc_trigger *trigger;
	struct otc_trigger_stage *stage;
	struct otc_trigger_match *match;
	const GSList *l, *m;
	uint16_t channel_bit;

	devc = sdi->priv;
	devc->trigger_enable  = 0x0000;
	devc->trigger_sense   = 0x0000;
	devc->trigger_level   = 0x0000;

	if (!(trigger = otc_session_trigger_get(sdi->session)))
		return OTC_OK;

	if (g_slist_length(trigger->stages) > 1) {
		otc_err("This device only supports 1 trigger stage.");
		return OTC_ERR;
	}

	for (l = trigger->stages; l; l = l->next) {
		stage = l->data;
		for (m = stage->matches; m; m = m->next) {
			match = m->data;
			if (!match->channel->enabled)
				/* Ignore disabled channels with a trigger. */
				continue;

			channel_bit = (1 << (match->channel->index));

			/* Sense: 1 == high, edge: 1 == rising edge. */
			if (match->match == OTC_TRIGGER_ONE || match->match == OTC_TRIGGER_RISING)
				devc->trigger_sense |= channel_bit;

			/* Level vs edge */
			if (match->match == OTC_TRIGGER_ONE || match->match == OTC_TRIGGER_ZERO)
				devc->trigger_level |= channel_bit;

			/* Trigger enabled */
			if (match->match == OTC_TRIGGER_ONE || match->match == OTC_TRIGGER_ZERO ||
				match->match == OTC_TRIGGER_RISING || match->match == OTC_TRIGGER_FALLING)
				devc->trigger_enable |= channel_bit;
		}
	}

	otc_dbg("Trigger sense/level/enable = 0x%04x / 0x%04x / 0x%04x.",
			devc->trigger_sense,
			devc->trigger_level,
			devc->trigger_enable);

	return OTC_OK;
}


OTC_PRIV int openlb_close(struct dev_context *devc)
{
	int ret;

	/* Note: Caller checks devc and devc->ftdic. */
	if ((ret = ftdi_usb_close(devc->ftdic)) < 0) {
		otc_err("Failed to close FTDI device (%d): %s.",
			   ret, ftdi_get_error_string(devc->ftdic));
		return OTC_ERR;
	}

	return OTC_OK;
}

static int openlb_stop_acquisition(struct dev_context *devc)
{
	/* Make sure capturing is not enabled. */
	if (openlb_write32(devc, CFG_BASE_ADDR + LA_BUFFER_CFG, 0) < 0)
		return OTC_ERR;

	return OTC_OK;
}

OTC_PRIV int openlb_start_acquisition(struct dev_context *devc)
{
	uint32_t cfg_reg = 0;

	/* Stop previous capture */
	openlb_stop_acquisition(devc);

	/* Write clock config first (allows resync between domains) */
	int clk_div      = (OTC_MHZ(100) / devc->sample_rate) - 1;
	cfg_reg |= (clk_div << LA_BUFFER_CFG_CLK_DIV_SHIFT);
	cfg_reg |= (0       << LA_BUFFER_CFG_WIDTH_SHIFT);
	if (openlb_write32(devc, CFG_BASE_ADDR + LA_BUFFER_CFG, cfg_reg) < 0)
		return OTC_ERR;

	/* Configure triggers */
	if (openlb_write32(devc, CFG_BASE_ADDR + LA_BUFFER_TRIG_ENABLE, devc->trigger_enable) < 0)
		return OTC_ERR;
	if (openlb_write32(devc, CFG_BASE_ADDR + LA_BUFFER_TRIG_SENSE,  devc->trigger_sense) < 0)
		return OTC_ERR;
	if (openlb_write32(devc, CFG_BASE_ADDR + LA_BUFFER_TRIG_LEVEL,  devc->trigger_level) < 0)
		return OTC_ERR;

	/* Start capture */
	cfg_reg |= (LA_BUFFER_CFG_ENABLED_MASK << LA_BUFFER_CFG_ENABLED_SHIFT);
	if (openlb_write32(devc, CFG_BASE_ADDR + LA_BUFFER_CFG, cfg_reg) < 0)
		return OTC_ERR;

	return OTC_OK;
}

OTC_PRIV int openlb_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;

	(void)fd;
	(void)revents;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	if (!devc->ftdic)
		return TRUE;

	/* Determine when enough samples have been captured */
	uint32_t level = 0;
	if (openlb_read32(devc, CFG_BASE_ADDR + LA_BUFFER_SAMPLES, &level) < 0)
		return OTC_ERR;

	/* Normal if not triggered yet. */
	if (level == 0) {
		otc_spew("Captured 0 samples, nothing to do.");
		return TRUE;
	}

	/* Capture limit not yet hit */
	if (devc->limit_samples && (level < devc->limit_samples)) {
		otc_spew("Samples ready - but not complete.");
		return TRUE;
	}

	/* Check for data loss (this should never happen) */
	uint32_t value = 0;
	if (openlb_read32(devc, CFG_BASE_ADDR + LA_BUFFER_STS, &value) < 0)
		return OTC_ERR;

	if (value & (1 << LA_BUFFER_STS_DATA_LOSS_SHIFT)) {
		otc_err("Data loss detected.");
		otc_dev_acquisition_stop(sdi);
		return FALSE;
	}

	/* Read the actual total capture length */
	if (openlb_read32(devc, CFG_BASE_ADDR + LA_BUFFER_CURRENT, &level) < 0)
		return FALSE;

	if (level == 0) {
		otc_spew("Captured 0 samples, nothing to do.");
		return TRUE;
	}

	otc_info("Samples ready for extraction.");
	uint32_t base = MEM_BASE_ADDR;
	uint32_t size = level;
	uint8_t  *data = g_malloc0(size);
	if (!data) {
		otc_err("Could not allocate buffer.");
		openlb_stop_acquisition(devc);
		otc_dev_acquisition_stop(sdi);
		return FALSE;
	}

	/* Read all the captured data from memory - this may take a while. */
	if (openlb_read_block(devc, base, data, size) == OTC_ERR) {
		otc_err("Error - cannot read captured data.");
		openlb_stop_acquisition(devc);
		otc_dev_acquisition_stop(sdi);
		return FALSE;
	}

	/* Convert RLE data to individual samples */
	uint32_t *sample_buf = (uint32_t *)data;
	for (uint32_t i=0; i<size/4; i++) {
		uint16_t repeats = sample_buf[i] >> 16;
		uint16_t value   = sample_buf[i] >> 0;

		for (uint16_t j=0;j<repeats;j++)
			openlb_push_sample(sdi, value, (j == (repeats -1)) && (i == ((size/4)-1)));
	}

	openlb_stop_acquisition(devc);
	otc_dev_acquisition_stop(sdi);
	g_free(data);

	return TRUE;
}
