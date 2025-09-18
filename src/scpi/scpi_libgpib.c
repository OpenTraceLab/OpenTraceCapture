/*
 * This file is part of the libopentracecapture project.
 *
 * Copyright (C) 2014 Martin Ling <martin-opentracelab@earth.li>
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
#include <gpib/ib.h>
#include <string.h>
#include <opentracecapture/libopentracecapture.h>
#include "../libopentracecapture-internal.h"
#include "../scpi.h"

#define LOG_PREFIX "scpi_gpib"

struct scpi_gpib {
	char *name;
	int descriptor;
	int read_started;
};

static int scpi_gpib_dev_inst_new(void *priv, struct drv_context *drvc,
		const char *resource, char **params, const char *serialcomm)
{
	struct scpi_gpib *gscpi = priv;

	(void)drvc;
	(void)resource;
	(void)serialcomm;

	if (!params || !params[1])
			return OTC_ERR;

	gscpi->name = g_strdup(params[1]);

	return OTC_OK;
}

static int scpi_gpib_open(struct otc_scpi_dev_inst *scpi)
{
	struct scpi_gpib *gscpi = scpi->priv;

	if ((gscpi->descriptor = ibfind(gscpi->name)) < 0)
		return OTC_ERR;

	return OTC_OK;
}

static int scpi_gpib_connection_id(struct otc_scpi_dev_inst *scpi,
		char **connection_id)
{
	struct scpi_gpib *gscpi = scpi->priv;

	*connection_id = g_strdup_printf("%s/%s", scpi->prefix, gscpi->name);

	return OTC_OK;
}

static int scpi_gpib_source_add(struct otc_session *session, void *priv,
		int events, int timeout, otc_receive_data_callback cb, void *cb_data)
{
	(void) priv;

	/* Hook up a dummy handler to receive data from the device. */
	return otc_session_source_add(session, -1, events, timeout, cb, cb_data);
}

static int scpi_gpib_source_remove(struct otc_session *session, void *priv)
{
	(void) priv;

	return otc_session_source_remove(session, -1);
}

static int scpi_gpib_send(void *priv, const char *command)
{
	struct scpi_gpib *gscpi = priv;
	int len = strlen(command);

	ibwrt(gscpi->descriptor, command, len);

	if (ibsta & ERR)
	{
		otc_err("Error while sending SCPI command: '%s': iberr = %s.",
			command, gpib_error_string(iberr));
		return OTC_ERR;
	}

	if (ibcnt < len)
	{
		otc_err("Failed to send all of SCPI command: '%s': "
				"len = %d, ibcnt = %d.", command, len, ibcnt);
		return OTC_ERR;
	}

	otc_spew("Successfully sent SCPI command: '%s'.", command);

	return OTC_OK;
}

static int scpi_gpib_read_begin(void *priv)
{
	struct scpi_gpib *gscpi = priv;

	gscpi->read_started = 0;

	return OTC_OK;
}

static int scpi_gpib_read_data(void *priv, char *buf, int maxlen)
{
	struct scpi_gpib *gscpi = priv;

	ibrd(gscpi->descriptor, buf, maxlen);

	if (ibsta & ERR)
	{
		otc_err("Error while reading SCPI response: "
			"iberr = %s, ibsta = %d.",
			gpib_error_string(iberr), ibsta);
		return OTC_ERR;
	}

	gscpi->read_started = 1;

	return ibcnt;
}

static int scpi_gpib_read_complete(void *priv)
{
	struct scpi_gpib *gscpi = priv;

	return gscpi->read_started && (ibsta & END);
}

static int scpi_gpib_close(struct otc_scpi_dev_inst *scpi)
{
	struct scpi_gpib *gscpi = scpi->priv;

	/* Put device in back local mode to prevent lock-out of front panel. */
	ibloc(gscpi->descriptor);
	/* Now it's safe to close the handle. */
	ibonl(gscpi->descriptor, 0);

	return OTC_OK;
}

static void scpi_gpib_free(void *priv)
{
	struct scpi_gpib *gscpi = priv;

	g_free(gscpi->name);
}

OTC_PRIV int otc_scpi_gpib_spoll(struct otc_scpi_dev_inst *scpi, char *buf)
{
	struct scpi_gpib *gscpi = scpi->priv;

	g_mutex_lock(&scpi->scpi_mutex);
	ibrsp(gscpi->descriptor, buf);

	if (ibsta & ERR) {
		otc_err("Error while serial polling: iberr = %s.",
			gpib_error_string(iberr));
		g_mutex_unlock(&scpi->scpi_mutex);
		return OTC_ERR;
	}
	g_mutex_unlock(&scpi->scpi_mutex);
	otc_spew("Successful serial poll: 0x%x", (uint8_t)buf[0]);

	return OTC_OK;
}

OTC_PRIV const struct otc_scpi_dev_inst scpi_libgpib_dev = {
	.name          = "GPIB",
	.prefix        = "libgpib",
	.transport     = SCPI_TRANSPORT_LIBGPIB,
	.priv_size     = sizeof(struct scpi_gpib),
	.dev_inst_new  = scpi_gpib_dev_inst_new,
	.open          = scpi_gpib_open,
	.connection_id = scpi_gpib_connection_id,
	.source_add    = scpi_gpib_source_add,
	.source_remove = scpi_gpib_source_remove,
	.send          = scpi_gpib_send,
	.read_begin    = scpi_gpib_read_begin,
	.read_data     = scpi_gpib_read_data,
	.read_complete = scpi_gpib_read_complete,
	.close         = scpi_gpib_close,
	.free          = scpi_gpib_free,
};
