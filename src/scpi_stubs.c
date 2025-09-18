/*
 * SCPI transport stubs for missing implementations
 */

#include <config.h>
#include <opentracecapture/libopentracecapture.h>
#include "libopentracecapture-internal.h"

/* TCP transport stubs */
static int scpi_tcp_open(struct otc_scpi_dev_inst *scpi) { (void)scpi; return OTC_ERR; }
static int scpi_tcp_source_add(struct otc_session *session, void *priv, int events, int timeout, otc_receive_data_callback cb, void *cb_data) { (void)session; (void)priv; (void)events; (void)timeout; (void)cb; (void)cb_data; return OTC_ERR; }
static int scpi_tcp_source_remove(struct otc_session *session, void *priv) { (void)session; (void)priv; return OTC_ERR; }
static int scpi_tcp_send(void *priv, const char *command) { (void)priv; (void)command; return OTC_ERR; }
static int scpi_tcp_read_begin(void *priv) { (void)priv; return OTC_ERR; }
static int scpi_tcp_read_data(void *priv, char *buf, int maxlen) { (void)priv; (void)buf; (void)maxlen; return OTC_ERR; }
static int scpi_tcp_read_complete(void *priv) { (void)priv; return OTC_ERR; }
static int scpi_tcp_close(struct otc_scpi_dev_inst *scpi) { (void)scpi; return OTC_ERR; }
static void scpi_tcp_free(void *priv) { (void)priv; }

OTC_PRIV const struct otc_scpi_dev_inst scpi_tcp_raw_dev = {
	.name = "tcp-raw", .prefix = "", .transport = SCPI_TRANSPORT_RAW_TCP,
	.priv_size = 0, .open = scpi_tcp_open, .source_add = scpi_tcp_source_add,
	.source_remove = scpi_tcp_source_remove, .send = scpi_tcp_send,
	.read_begin = scpi_tcp_read_begin, .read_data = scpi_tcp_read_data,
	.read_complete = scpi_tcp_read_complete, .close = scpi_tcp_close, .free = scpi_tcp_free,
};

OTC_PRIV const struct otc_scpi_dev_inst scpi_tcp_rigol_dev = {
	.name = "tcp-rigol", .prefix = "", .transport = SCPI_TRANSPORT_RIGOL,
	.priv_size = 0, .open = scpi_tcp_open, .source_add = scpi_tcp_source_add,
	.source_remove = scpi_tcp_source_remove, .send = scpi_tcp_send,
	.read_begin = scpi_tcp_read_begin, .read_data = scpi_tcp_read_data,
	.read_complete = scpi_tcp_read_complete, .close = scpi_tcp_close, .free = scpi_tcp_free,
};

/* USB TMC transport stub */
OTC_PRIV const struct otc_scpi_dev_inst scpi_usbtmc_libusb_dev = {
	.name = "usbtmc-libusb", .prefix = "", .transport = SCPI_TRANSPORT_USBTMC,
	.priv_size = 0, .open = scpi_tcp_open, .source_add = scpi_tcp_source_add,
	.source_remove = scpi_tcp_source_remove, .send = scpi_tcp_send,
	.read_begin = scpi_tcp_read_begin, .read_data = scpi_tcp_read_data,
	.read_complete = scpi_tcp_read_complete, .close = scpi_tcp_close, .free = scpi_tcp_free,
};
