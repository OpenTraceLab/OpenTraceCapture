/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _MSC_VER
#include <unistd.h>
#endif
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <zip.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

#define LOG_PREFIX "virtual-session"

/* size of payloads sent across the session bus */
/** @cond PRIVATE */
#define CHUNKSIZE (4 * 1024 * 1024)
/** @endcond */

OTC_PRIV struct otc_dev_driver session_driver_info;

struct session_vdev {
	char *sessionfile;
	char *capturefile;
	struct zip *archive;
	struct zip_file *capfile;
	int bytes_read;
	uint64_t samplerate;
	int unitsize;
	int num_logic_channels;
	int num_analog_channels;
	int cur_analog_channel;
	GArray *analog_channels;
	int cur_chunk;
	gboolean finished;
};

static const uint32_t devopts[] = {
	OTC_CONF_CAPTUREFILE | OTC_CONF_SET,
	OTC_CONF_CAPTURE_UNITSIZE | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_NUM_LOGIC_CHANNELS | OTC_CONF_SET,
	OTC_CONF_NUM_ANALOG_CHANNELS | OTC_CONF_SET,
	OTC_CONF_SAMPLERATE | OTC_CONF_GET | OTC_CONF_SET,
	OTC_CONF_SESSIONFILE | OTC_CONF_SET,
};

static gboolean stream_session_data(struct otc_dev_inst *sdi)
{
	struct session_vdev *vdev;
	struct otc_datafeed_packet packet;
	struct otc_datafeed_logic logic;
	struct otc_datafeed_analog analog;
	struct otc_analog_encoding encoding;
	struct otc_analog_meaning meaning;
	struct otc_analog_spec spec;
	struct zip_stat zs;
	int ret, got_data;
	char capturefile[128];
	void *buf;

	got_data = FALSE;
	vdev = sdi->priv;

	if (!vdev->capfile) {
		/* No capture file opened yet, or finished with the last
		 * chunked one. */
		if (vdev->capturefile && (vdev->cur_chunk == 0)) {
			/* capturefile is always the unchunked base name. */
			if (zip_stat(vdev->archive, vdev->capturefile, 0, &zs) != -1) {
				/* No chunks, just a single capture file. */
				vdev->cur_chunk = 0;
				if (!(vdev->capfile = zip_fopen(vdev->archive,
						vdev->capturefile, 0)))
					return FALSE;
				otc_dbg("Opened %s.", vdev->capturefile);
			} else {
				/* Try as first chunk filename. */
				snprintf(capturefile, sizeof(capturefile) - 1, "%s-1", vdev->capturefile);
				if (zip_stat(vdev->archive, capturefile, 0, &zs) != -1) {
					vdev->cur_chunk = 1;
					if (!(vdev->capfile = zip_fopen(vdev->archive,
							capturefile, 0)))
						return FALSE;
					otc_dbg("Opened %s.", capturefile);
				} else {
					otc_err("No capture file '%s' in " "session file '%s'.",
							vdev->capturefile, vdev->sessionfile);
					return FALSE;
				}
			}
		} else {
			/* Capture data is chunked, advance to the next chunk. */
			vdev->cur_chunk++;
			snprintf(capturefile, sizeof(capturefile) - 1, "%s-%d", vdev->capturefile,
					vdev->cur_chunk);
			if (zip_stat(vdev->archive, capturefile, 0, &zs) != -1) {
				if (!(vdev->capfile = zip_fopen(vdev->archive,
						capturefile, 0)))
					return FALSE;
				otc_dbg("Opened %s.", capturefile);
			} else if (vdev->cur_analog_channel < vdev->num_analog_channels) {
				vdev->capturefile = g_strdup_printf("analog-1-%d",
						vdev->num_logic_channels + vdev->cur_analog_channel + 1);
				vdev->cur_analog_channel++;
				vdev->cur_chunk = 0;
				return TRUE;
			} else {
				/* We got all the chunks, finish up. */
				g_free(vdev->capturefile);

				/* If the file has logic channels, the initial value for
				 * capturefile is set by stream_session_data() - however only
				 * once. In order to not mess this mechanism up, we simulate
				 * this here if needed. For purely analog files, capturefile
				 * is not set.
				 */
				if (vdev->num_logic_channels)
					vdev->capturefile = g_strdup("logic-1");
				else
					vdev->capturefile = NULL;
				return FALSE;
			}
		}
	}

	buf = g_malloc(CHUNKSIZE);

	/* unitsize is not defined for purely analog session files. */
	if (vdev->unitsize)
		ret = zip_fread(vdev->capfile, buf,
				CHUNKSIZE / vdev->unitsize * vdev->unitsize);
	else
		ret = zip_fread(vdev->capfile, buf, CHUNKSIZE);

	if (ret > 0) {
		if (vdev->cur_analog_channel != 0) {
			got_data = TRUE;
			packet.type = OTC_DF_ANALOG;
			packet.payload = &analog;
			/* TODO: Use proper 'digits' value for this device (and its modes). */
			otc_analog_init(&analog, &encoding, &meaning, &spec, 2);
			analog.meaning->channels = g_slist_prepend(NULL,
					g_array_index(vdev->analog_channels,
						struct otc_channel *, vdev->cur_analog_channel - 1));
			analog.num_samples = ret / sizeof(float);
			analog.meaning->mq = OTC_MQ_VOLTAGE;
			analog.meaning->unit = OTC_UNIT_VOLT;
			analog.meaning->mqflags = OTC_MQFLAG_DC;
			analog.data = (float *) buf;
		} else if (vdev->unitsize) {
			got_data = TRUE;
			if (ret % vdev->unitsize != 0)
				otc_warn("Read size %d not a multiple of the"
					" unit size %d.", ret, vdev->unitsize);
			packet.type = OTC_DF_LOGIC;
			packet.payload = &logic;
			logic.length = ret;
			logic.unitsize = vdev->unitsize;
			logic.data = buf;
		} else {
			/*
			 * Neither analog data, nor logic which has
			 * unitsize, must be an unexpected API use.
			 */
			otc_warn("Neither analog nor logic data. Ignoring.");
		}
		if (got_data) {
			vdev->bytes_read += ret;
			otc_session_send(sdi, &packet);
		}
	} else {
		/* done with this capture file */
		zip_fclose(vdev->capfile);
		vdev->capfile = NULL;
		if (vdev->cur_chunk != 0) {
			/* There might be more chunks, so don't fall through
			 * to the OTC_DF_END here. */
			got_data = TRUE;
		}
	}
	g_free(buf);

	return got_data;
}

static int receive_data(int fd, int revents, void *cb_data)
{
	struct otc_dev_inst *sdi;
	struct session_vdev *vdev;

	(void)fd;
	(void)revents;

	sdi = cb_data;
	vdev = sdi->priv;

	if (!vdev->finished && !stream_session_data(sdi))
		vdev->finished = TRUE;
	if (!vdev->finished)
		return G_SOURCE_CONTINUE;

	if (vdev->capfile) {
		zip_fclose(vdev->capfile);
		vdev->capfile = NULL;
	}
	if (vdev->archive) {
		zip_discard(vdev->archive);
		vdev->archive = NULL;
	}

	std_session_send_df_end(sdi);

	return G_SOURCE_REMOVE;
}

/* driver callbacks */

static int dev_open(struct otc_dev_inst *sdi)
{
	struct otc_dev_driver *di;
	struct drv_context *drvc;
	struct session_vdev *vdev;

	di = sdi->driver;
	drvc = di->context;
	vdev = g_malloc0(sizeof(struct session_vdev));
	sdi->priv = vdev;
	drvc->instances = g_slist_append(drvc->instances, sdi);

	return OTC_OK;
}

static int dev_close(struct otc_dev_inst *sdi)
{
	const struct session_vdev *const vdev = sdi->priv;
	g_free(vdev->sessionfile);
	g_free(vdev->capturefile);

	g_free(sdi->priv);
	sdi->priv = NULL;

	return OTC_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct session_vdev *vdev;

	(void)cg;

	if (!sdi)
		return OTC_ERR;

	vdev = sdi->priv;

	switch (key) {
	case OTC_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(vdev->samplerate);
		break;
	case OTC_CONF_CAPTURE_UNITSIZE:
		*data = g_variant_new_uint64(vdev->unitsize);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_set(uint32_t key, GVariant *data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	struct session_vdev *vdev;

	(void)cg;

	vdev = sdi->priv;

	switch (key) {
	case OTC_CONF_SAMPLERATE:
		vdev->samplerate = g_variant_get_uint64(data);
		otc_info("Setting samplerate to %" PRIu64 ".", vdev->samplerate);
		break;
	case OTC_CONF_SESSIONFILE:
		g_free(vdev->sessionfile);
		vdev->sessionfile = g_strdup(g_variant_get_string(data, NULL));
		otc_info("Setting sessionfile to '%s'.", vdev->sessionfile);
		break;
	case OTC_CONF_CAPTUREFILE:
		g_free(vdev->capturefile);
		vdev->capturefile = g_strdup(g_variant_get_string(data, NULL));
		otc_info("Setting capturefile to '%s'.", vdev->capturefile);
		break;
	case OTC_CONF_CAPTURE_UNITSIZE:
		vdev->unitsize = g_variant_get_uint64(data);
		break;
	case OTC_CONF_NUM_LOGIC_CHANNELS:
		vdev->num_logic_channels = g_variant_get_int32(data);
		break;
	case OTC_CONF_NUM_ANALOG_CHANNELS:
		vdev->num_analog_channels = g_variant_get_int32(data);
		break;
	default:
		return OTC_ERR_NA;
	}

	return OTC_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
	return STD_CONFIG_LIST(key, data, sdi, cg, NO_OPTS, NO_OPTS, devopts);
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
	struct session_vdev *vdev;
	int ret;
	GSList *l;
	struct otc_channel *ch;

	vdev = sdi->priv;
	vdev->bytes_read = 0;
	vdev->cur_analog_channel = 0;
	vdev->analog_channels = g_array_sized_new(FALSE, FALSE,
			sizeof(struct otc_channel *), vdev->num_analog_channels);
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->type == OTC_CHANNEL_ANALOG)
			g_array_append_val(vdev->analog_channels, ch);
	}
	vdev->cur_chunk = 0;
	vdev->finished = FALSE;

	otc_info("Opening archive %s file %s", vdev->sessionfile,
		vdev->capturefile);

	if (!(vdev->archive = zip_open(vdev->sessionfile, 0, &ret))) {
		otc_err("Failed to open session file '%s': "
		       "zip error %d.", vdev->sessionfile, ret);
		return OTC_ERR;
	}

	std_session_send_df_header(sdi);

	/* freewheeling source */
	otc_session_source_add(sdi->session, -1, 0, 0, receive_data, (void *)sdi);

	return OTC_OK;
}

static int dev_acquisition_stop(struct otc_dev_inst *sdi)
{
	struct session_vdev *vdev;

	vdev = sdi->priv;

	vdev->finished = TRUE;

	return OTC_OK;
}

/** @private */
OTC_PRIV struct otc_dev_driver session_driver = {
	.name = "virtual-session",
	.longname = "Session-emulating driver",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = NULL,
	.dev_list = NULL,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
