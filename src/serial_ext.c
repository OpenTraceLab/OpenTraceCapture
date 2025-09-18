/*
 * Extended serial functions for OpenTraceCapture
 */

#include <config.h>
#include <glib.h>
#include <libserialport.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

OTC_PRIV struct otc_serial_dev_inst *otc_serial_dev_inst_new(const char *port, const char *serialcomm)
{
	struct otc_serial_dev_inst *serial;
	
	serial = g_malloc0(sizeof(struct otc_serial_dev_inst));
	serial->port = g_strdup(port);
	serial->serialcomm = g_strdup(serialcomm);
	
	return serial;
}

OTC_PRIV int serial_open(struct otc_serial_dev_inst *serial, int flags)
{
	int ret;
	
	(void)flags;
	
	if (!serial || !serial->port)
		return OTC_ERR_ARG;
		
	ret = sp_get_port_by_name(serial->port, &serial->sp_data);
	if (ret != SP_OK)
		return OTC_ERR;
		
	ret = sp_open(serial->sp_data, SP_MODE_READ_WRITE);
	if (ret != SP_OK)
		return OTC_ERR;
		
	return OTC_OK;
}

OTC_PRIV int serial_close(struct otc_serial_dev_inst *serial)
{
	if (!serial || !serial->sp_data)
		return OTC_ERR_ARG;
		
	sp_close(serial->sp_data);
	sp_free_port(serial->sp_data);
	serial->sp_data = NULL;
	
	return OTC_OK;
}

OTC_PRIV int serial_read_blocking(struct otc_serial_dev_inst *serial, void *buf, size_t count, unsigned int timeout_ms)
{
	if (!serial || !serial->sp_data || !buf)
		return OTC_ERR_ARG;
		
	return sp_blocking_read(serial->sp_data, buf, count, timeout_ms);
}

OTC_PRIV int serial_write_blocking(struct otc_serial_dev_inst *serial, const void *buf, size_t count, unsigned int timeout_ms)
{
	if (!serial || !serial->sp_data || !buf)
		return OTC_ERR_ARG;
		
	return sp_blocking_write(serial->sp_data, buf, count, timeout_ms);
}

OTC_PRIV int serial_source_add(struct otc_session *session, struct otc_serial_dev_inst *serial, int events, int timeout, otc_receive_data_callback cb, void *cb_data)
{
	/* Stub implementation - would need proper event loop integration */
	(void)session; (void)serial; (void)events; (void)timeout; (void)cb; (void)cb_data;
	return OTC_ERR;
}

OTC_PRIV int std_serial_dev_open(struct otc_dev_inst *sdi)
{
	struct otc_serial_dev_inst *serial = sdi->conn;
	return serial_open(serial, 0);
}

OTC_PRIV int std_serial_dev_close(struct otc_dev_inst *sdi)
{
	struct otc_serial_dev_inst *serial = sdi->conn;
	return serial_close(serial);
}
