/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2020 Andreas Sandberg <andreas@sandberg.pp.se>
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
#include <opentracecapture/libopentracecapture.h>
#include <math.h>
#include <nettle/aes.h>
#include <stdlib.h>
#include <string.h>

#include "../../libopentracecapture-internal.h"
#include "protocol.h"

#define PROBE_TO_MS	1000
#define WRITE_TO_MS	1
#define POLL_PERIOD_MS	100

/*
 * Response data (raw sample data) consists of three adjacent chunks
 * of 64 bytes each. These chunks start with their magic string, and
 * end in a 32bit checksum field. Measurement values are scattered
 * across these 192 bytes total size. All multi-byte integer values
 * are represented in little endian format. Typical size is 32 bits.
 */

#define MAGIC_PAC1	0x70616331	/* 'pac1' */
#define MAGIC_PAC2	0x70616332	/* 'pac2' */
#define MAGIC_PAC3	0x70616333	/* 'pac3' */

#define PAC_LEN 64
#define PAC_CRC_POS (PAC_LEN - sizeof(uint32_t))

/* Offset to PAC block from start of poll data */
#define OFF_PAC1 (0 * PAC_LEN)
#define OFF_PAC2 (1 * PAC_LEN)
#define OFF_PAC3 (2 * PAC_LEN)
#define TC_POLL_LEN (3 * PAC_LEN)
#if TC_POLL_LEN > RDTECH_TC_RSPBUFSIZE
#  error "response length exceeds receive buffer space"
#endif

#define OFF_MODEL 4
#define LEN_MODEL 4

#define OFF_FW_VER 8
#define LEN_FW_VER 4

#define OFF_SERIAL 12

static const uint8_t aes_key[] = {
	0x58, 0x21, 0xfa, 0x56, 0x01, 0xb2, 0xf0, 0x26,
	0x87, 0xff, 0x12, 0x04, 0x62, 0x2a, 0x4f, 0xb0,
	0x86, 0xf4, 0x02, 0x60, 0x81, 0x6f, 0x9a, 0x0b,
	0xa7, 0xf1, 0x06, 0x61, 0x9a, 0xb8, 0x72, 0x88,
};

static const struct rdtech_tc_channel_desc rdtech_tc_channels[] = {
	{ "V",  {   0 + 48, BVT_LE_UINT32, }, { 100, 1e6, }, 4, OTC_MQ_VOLTAGE, OTC_UNIT_VOLT },
	{ "I",  {   0 + 52, BVT_LE_UINT32, }, {  10, 1e6, }, 5, OTC_MQ_CURRENT, OTC_UNIT_AMPERE },
	{ "D+", {  64 + 32, BVT_LE_UINT32, }, {  10, 1e3, }, 2, OTC_MQ_VOLTAGE, OTC_UNIT_VOLT },
	{ "D-", {  64 + 36, BVT_LE_UINT32, }, {  10, 1e3, }, 2, OTC_MQ_VOLTAGE, OTC_UNIT_VOLT },
	{ "E0", {  64 + 12, BVT_LE_UINT32, }, {   1, 1e3, }, 3, OTC_MQ_ENERGY, OTC_UNIT_WATT_HOUR },
	{ "E1", {  64 + 20, BVT_LE_UINT32, }, {   1, 1e3, }, 3, OTC_MQ_ENERGY, OTC_UNIT_WATT_HOUR },
};

static gboolean check_pac_crc(uint8_t *data)
{
	uint16_t crc_calc;
	uint32_t crc_recv;

	crc_calc = otc_crc16(OTC_CRC16_DEFAULT_INIT, data, PAC_CRC_POS);
	crc_recv = read_u32le(&data[PAC_CRC_POS]);
	if (crc_calc != crc_recv) {
		otc_spew("CRC error. Calculated: %0x" PRIx16 ", expected: %0x" PRIx32,
			crc_calc, crc_recv);
		return FALSE;
	}

	return TRUE;
}

static int process_poll_pkt(struct dev_context *devc, uint8_t *dst)
{
	struct aes256_ctx ctx;
	gboolean ok;

	aes256_set_decrypt_key(&ctx, aes_key);
	aes256_decrypt(&ctx, TC_POLL_LEN, dst, devc->buf);

	ok = TRUE;
	ok &= read_u32be(&dst[OFF_PAC1]) == MAGIC_PAC1;
	ok &= read_u32be(&dst[OFF_PAC2]) == MAGIC_PAC2;
	ok &= read_u32be(&dst[OFF_PAC3]) == MAGIC_PAC3;
	if (!ok) {
		otc_err("Invalid poll response packet (magic values).");
		return OTC_ERR_DATA;
	}

	ok &= check_pac_crc(&dst[OFF_PAC1]);
	ok &= check_pac_crc(&dst[OFF_PAC2]);
	ok &= check_pac_crc(&dst[OFF_PAC3]);
	if (!ok) {
		otc_err("Invalid poll response packet (checksum).");
		return OTC_ERR_DATA;
	}

	if (otc_log_loglevel_get() >= OTC_LOG_SPEW) {
		static const size_t chunk_max = 32;

		const uint8_t *rdptr;
		size_t rdlen, chunk_addr, chunk_len;
		GString *txt;

		otc_spew("check passed on decrypted receive data");
		rdptr = dst;
		rdlen = TC_POLL_LEN;
		chunk_addr = 0;
		while (rdlen) {
			chunk_len = rdlen;
			if (chunk_len > chunk_max)
				chunk_len = chunk_max;
			txt = otc_hexdump_new(rdptr, chunk_len);
			otc_spew("%04zx  %s", chunk_addr, txt->str);
			otc_hexdump_free(txt);
			chunk_addr += chunk_len;
			rdptr += chunk_len;
			rdlen -= chunk_len;
		}
	}

	return OTC_OK;
}

OTC_PRIV int rdtech_tc_probe(struct otc_serial_dev_inst *serial, struct dev_context *devc)
{
	static const char *poll_cmd_cdc = "getva";
	static const char *poll_cmd_ble = "bgetva\r\n";

	int len;
	uint8_t poll_pkt[TC_POLL_LEN];

	/* Construct the request text. Which differs across transports. */
	devc->is_bluetooth = ser_name_is_bt(serial);
	snprintf(devc->req_text, sizeof(devc->req_text), "%s",
		devc->is_bluetooth ? poll_cmd_ble : poll_cmd_cdc);
	otc_dbg("is bluetooth %d -> poll request '%s'.",
		devc->is_bluetooth, devc->req_text);

	/* Transmit the request. */
	len = serial_write_blocking(serial,
		devc->req_text, strlen(devc->req_text), WRITE_TO_MS);
	if (len < 0) {
		otc_err("Failed to send probe request.");
		return OTC_ERR;
	}

	/* Receive a response. */
	len = serial_read_blocking(serial, devc->buf, TC_POLL_LEN, PROBE_TO_MS);
	if (len != TC_POLL_LEN) {
		otc_err("Failed to read probe response.");
		return OTC_ERR;
	}

	if (process_poll_pkt(devc, poll_pkt) != OTC_OK) {
		otc_err("Unrecognized TC device!");
		return OTC_ERR;
	}

	devc->channels = rdtech_tc_channels;
	devc->channel_count = ARRAY_SIZE(rdtech_tc_channels);
	devc->dev_info.model_name = g_strndup((const char *)&poll_pkt[OFF_MODEL], LEN_MODEL);
	devc->dev_info.fw_ver = g_strndup((const char *)&poll_pkt[OFF_FW_VER], LEN_FW_VER);
	devc->dev_info.serial_num = read_u32le(&poll_pkt[OFF_SERIAL]);

	return OTC_OK;
}

OTC_PRIV int rdtech_tc_poll(const struct otc_dev_inst *sdi, gboolean force)
{
	struct dev_context *devc;
	int64_t now, elapsed;
	struct otc_serial_dev_inst *serial;
	int len;

	/*
	 * Don't send the request while receive data is being accumulated.
	 * Defer request transmission when a previous request has not yet
	 * seen any response data at all (more probable to happen shortly
	 * after connecting to the peripheral).
	 */
	devc = sdi->priv;
	if (!force) {
		if (devc->rdlen)
			return OTC_OK;
		if (!devc->rx_after_tx)
			return OTC_OK;
	}

	/*
	 * Send the request when the transmit interval was reached. Or
	 * when the caller forced the transmission.
	 */
	now = g_get_monotonic_time() / 1000;
	elapsed = now - devc->cmd_sent_at;
	if (!force && elapsed < POLL_PERIOD_MS)
		return OTC_OK;

	/*
	 * Transmit another measurement request. Only advance the
	 * interval after successful transmission.
	 */
	serial = sdi->conn;
	len = serial_write_blocking(serial,
		devc->req_text, strlen(devc->req_text), WRITE_TO_MS);
	if (len < 0) {
		otc_err("Unable to send poll request.");
		return OTC_ERR;
	}
	devc->cmd_sent_at = now;
	devc->rx_after_tx = 0;

	return OTC_OK;
}

static int handle_poll_data(struct otc_dev_inst *sdi)
{
	struct dev_context *devc;
	uint8_t poll_pkt[TC_POLL_LEN];
	size_t ch_idx;
	const struct rdtech_tc_channel_desc *pch;
	int ret;
	float v;

	devc = sdi->priv;
	otc_spew("Received poll packet (len: %zu).", devc->rdlen);
	if (devc->rdlen < TC_POLL_LEN) {
		otc_err("Insufficient poll packet length: %zu", devc->rdlen);
		return OTC_ERR_DATA;
	}

	if (process_poll_pkt(devc, poll_pkt) != OTC_OK) {
		otc_err("Failed to process poll packet.");
		return OTC_ERR_DATA;
	}

	ret = OTC_OK;
	std_session_send_df_frame_begin(sdi);
	for (ch_idx = 0; ch_idx < devc->channel_count; ch_idx++) {
		pch = &devc->channels[ch_idx];
		ret = bv_get_value_len(&v, &pch->spec, poll_pkt, TC_POLL_LEN);
		if (ret != OTC_OK)
			break;
		ret = feed_queue_analog_submit_one(devc->feeds[ch_idx], v, 1);
		if (ret != OTC_OK)
			break;
	}
	std_session_send_df_frame_end(sdi);

	otc_sw_limits_update_frames_read(&devc->limits, 1);
	if (otc_sw_limits_check(&devc->limits))
		otc_dev_acquisition_stop(sdi);

	return ret;
}

static int recv_poll_data(struct otc_dev_inst *sdi, struct otc_serial_dev_inst *serial)
{
	struct dev_context *devc;
	size_t space;
	int len;
	int ret;

	/* Receive data became available. Drain the transport layer. */
	devc = sdi->priv;
	while (devc->rdlen < TC_POLL_LEN) {
		space = sizeof(devc->buf) - devc->rdlen;
		len = serial_read_nonblocking(serial,
			&devc->buf[devc->rdlen], space);
		if (len < 0)
			return OTC_ERR_IO;
		if (len == 0)
			return OTC_OK;
		devc->rdlen += len;
		devc->rx_after_tx += len;
	}

	/*
	 * TODO Want to (re-)synchronize to the packet stream? The
	 * 'pac1' string literal would be a perfect match for that.
	 */

	/* Process packets when their reception has completed. */
	while (devc->rdlen >= TC_POLL_LEN) {
		ret = handle_poll_data(sdi);
		if (ret != OTC_OK)
			return ret;
		devc->rdlen -= TC_POLL_LEN;
		if (devc->rdlen)
			memmove(devc->buf, &devc->buf[TC_POLL_LEN], devc->rdlen);
	}

	return OTC_OK;
}

OTC_PRIV int rdtech_tc_receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct dev_context *devc;
	struct otc_serial_dev_inst *serial;
	int ret;

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;
	if (!(devc = sdi->priv))
		return TRUE;

	/* Handle availability of receive data. */
	serial = sdi->conn;
	if (revents == G_IO_IN) {
		ret = recv_poll_data(sdi, serial);
		if (ret != OTC_OK)
			otc_dev_acquisition_stop(sdi);
	}

	/* Check configured acquisition limits. */
	if (otc_sw_limits_check(&devc->limits)) {
		otc_dev_acquisition_stop(sdi);
		return TRUE;
	}

	/* Periodically retransmit measurement requests. */
	(void)rdtech_tc_poll(sdi, FALSE);

	return TRUE;
}
