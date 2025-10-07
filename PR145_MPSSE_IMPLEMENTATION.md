# MPSSE Mode Implementation Details

## Overview
This document details the MPSSE (Multi-Protocol Synchronous Serial Engine) mode implementation for FT232H, FT2232H, and FT4232H chips.

## File: src/hardware/ftdi-la/mpsse.c

### USB Constants

```c
#define USB_TIMEOUT 100  /* Non-data transfer timeout (ms) */
#define MS_PER_TRANSFER 100  /* Target duration per transfer */
#define BUFFER_SIZE_MS 1000  /* Total buffer size in ms */
#define MIN_TRANSFER_BUFFERS 2
#define MAX_TRANSFER_BUFFERS 16

/* FTDI vendor requests */
#define VENDOR_OUT (LIBUSB_REQUEST_TYPE_VENDOR | \
                    LIBUSB_RECIPIENT_DEVICE | \
                    LIBUSB_ENDPOINT_OUT)
#define REQ_RESET 0x00
#define REQ_SET_BAUD_RATE 0x03
#define REQ_SET_LATENCY_TIMER 0x09
#define REQ_SET_BITMODE 0x0b
#define SET_BITMODE_BITBANG 1

#define NUM_STATUS_BYTES 2  /* FTDI adds 2 status bytes per packet */
```

### USB Transfer Management

```c
struct usb_transfer_ctx {
    struct libusb_transfer *transfer;
    struct otc_dev_inst *sdi;
    unsigned char *buffer;
    size_t buffer_size;
    gboolean submitted;
};

static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer)
{
    struct usb_transfer_ctx *ctx = transfer->user_data;
    struct otc_dev_inst *sdi = ctx->sdi;
    struct dev_context *devc = sdi->priv;
    
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        /* Process received data */
        process_samples(sdi, transfer->buffer, transfer->actual_length);
        
        /* Resubmit if not aborted */
        if (!devc->acq_aborted) {
            int ret = libusb_submit_transfer(transfer);
            if (ret < 0) {
                otc_err("Failed to resubmit transfer: %s",
                       libusb_error_name(ret));
                devc->acq_aborted = TRUE;
            }
        }
    } else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        /* Expected during stop */
        otc_dbg("Transfer cancelled.");
    } else {
        otc_err("Transfer failed: %s", 
               libusb_strerror(transfer->status));
        devc->acq_aborted = TRUE;
    }
    
    /* Check if acquisition should stop */
    if (devc->acq_aborted || 
        (devc->limit_samples && devc->samples_sent >= devc->limit_samples)) {
        stop_acquisition(sdi);
    }
}
```

### Sample Processing

```c
static void process_samples(struct otc_dev_inst *sdi,
                           unsigned char *data, size_t len)
{
    struct dev_context *devc = sdi->priv;
    struct otc_datafeed_packet packet;
    struct otc_datafeed_logic logic;
    size_t i, samples_to_send;
    unsigned char *clean_data;
    
    /* Remove FTDI status bytes (2 per packet) */
    size_t num_packets = len / devc->in_ep_pkt_size;
    size_t samples_available = num_packets * 
                               (devc->in_ep_pkt_size - NUM_STATUS_BYTES);
    
    /* Allocate buffer for clean data */
    clean_data = g_malloc(samples_available);
    
    /* Copy data, skipping status bytes */
    size_t out_idx = 0;
    for (i = 0; i < num_packets; i++) {
        size_t pkt_offset = i * devc->in_ep_pkt_size;
        size_t data_size = devc->in_ep_pkt_size - NUM_STATUS_BYTES;
        
        memcpy(clean_data + out_idx, 
               data + pkt_offset + NUM_STATUS_BYTES,
               data_size);
        out_idx += data_size;
    }
    
    /* Determine how many samples to send */
    if (devc->limit_samples) {
        uint64_t remaining = devc->limit_samples - devc->samples_sent;
        samples_to_send = MIN(samples_available, remaining);
    } else {
        samples_to_send = samples_available;
    }
    
    /* Send samples */
    packet.type = OTC_DF_LOGIC;
    packet.payload = &logic;
    logic.length = samples_to_send;
    logic.unitsize = 1;
    logic.data = clean_data;
    
    otc_session_send(sdi, &packet);
    
    devc->samples_sent += samples_to_send;
    
    g_free(clean_data);
}
```

### Transfer Allocation

```c
static int allocate_transfers(struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    struct otc_usb_dev_inst *usb = sdi->conn;
    size_t num_transfers, packets_per_transfer;
    size_t samples_per_transfer, bytes_per_transfer;
    unsigned int timeout, cur_samplerate;
    
    cur_samplerate = DIV_ROUND_CLOSEST(devc->cur_clk.rate_millihz, 1000);
    
    /* Calculate packets per transfer */
    packets_per_transfer = DIV_ROUND_UP(
        (cur_samplerate * MS_PER_TRANSFER) / 1000,
        devc->in_ep_pkt_size - NUM_STATUS_BYTES);
    
    samples_per_transfer = packets_per_transfer * 
                          (devc->in_ep_pkt_size - NUM_STATUS_BYTES);
    bytes_per_transfer = packets_per_transfer * devc->in_ep_pkt_size;
    
    /* Calculate number of transfers for buffer */
    num_transfers = (cur_samplerate * BUFFER_SIZE_MS) / 
                   (samples_per_transfer * 1000);
    num_transfers = CLAMP(num_transfers, 
                         MIN_TRANSFER_BUFFERS, 
                         MAX_TRANSFER_BUFFERS);
    
    /* Calculate timeout */
    timeout = (num_transfers * samples_per_transfer * 1000ull) / 
              cur_samplerate;
    timeout += timeout / 4;  /* 25% safety margin */
    
    otc_dbg("Using %zu USB transfers of %zu bytes, timeout %u ms.",
           num_transfers, bytes_per_transfer, timeout);
    
    /* Allocate transfer structures */
    devc->transfers = g_malloc0_n(num_transfers, sizeof(struct libusb_transfer *));
    devc->transfer_buffers = g_malloc0_n(num_transfers, sizeof(unsigned char *));
    devc->num_transfers = num_transfers;
    
    for (size_t i = 0; i < num_transfers; i++) {
        devc->transfer_buffers[i] = g_malloc(bytes_per_transfer);
        devc->transfers[i] = libusb_alloc_transfer(0);
        
        if (!devc->transfers[i]) {
            otc_err("Failed to allocate USB transfer %zu.", i);
            return OTC_ERR_MALLOC;
        }
        
        struct usb_transfer_ctx *ctx = g_malloc(sizeof(*ctx));
        ctx->sdi = sdi;
        ctx->buffer = devc->transfer_buffers[i];
        ctx->buffer_size = bytes_per_transfer;
        ctx->transfer = devc->transfers[i];
        ctx->submitted = FALSE;
        
        libusb_fill_bulk_transfer(devc->transfers[i],
                                 usb->devhdl,
                                 devc->in_ep_addr,
                                 devc->transfer_buffers[i],
                                 bytes_per_transfer,
                                 transfer_callback,
                                 ctx,
                                 timeout);
    }
    
    return OTC_OK;
}
```

### Hardware Configuration

```c
static int configure_hardware(struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    struct otc_usb_dev_inst *usb = sdi->conn;
    uint16_t index_val;
    int ret;
    
    /* Reset device */
    ret = libusb_control_transfer(usb->devhdl, VENDOR_OUT, REQ_RESET,
                                  0, devc->ftdi_iface_idx, NULL, 0, USB_TIMEOUT);
    if (ret < 0) {
        otc_err("Failed to reset device: %s", libusb_error_name(ret));
        return OTC_ERR;
    }
    
    /* Set latency timer to 1ms (minimum) */
    ret = libusb_control_transfer(usb->devhdl, VENDOR_OUT, REQ_SET_LATENCY_TIMER,
                                  1, devc->ftdi_iface_idx, NULL, 0, USB_TIMEOUT);
    if (ret < 0) {
        otc_err("Failed to set latency timer: %s", libusb_error_name(ret));
        return OTC_ERR;
    }
    
    /* Set sample rate */
    if (devc->desc->multi_iface)
        index_val = ((devc->cur_clk.encoded_divisor >> 16) << 8) | 
                    devc->ftdi_iface_idx;
    else
        index_val = devc->cur_clk.encoded_divisor >> 16;
    
    ret = libusb_control_transfer(usb->devhdl, VENDOR_OUT, REQ_SET_BAUD_RATE,
                                  devc->cur_clk.encoded_divisor & 0xffff,
                                  index_val, NULL, 0, USB_TIMEOUT);
    if (ret < 0) {
        otc_err("Failed to set sample rate: %s", libusb_error_name(ret));
        return OTC_ERR;
    }
    
    /* Set bitbang mode, all pins input */
    uint16_t mode = (SET_BITMODE_BITBANG << 8) | 0x00;
    ret = libusb_control_transfer(usb->devhdl, VENDOR_OUT, REQ_SET_BITMODE,
                                  mode, devc->ftdi_iface_idx, NULL, 0, USB_TIMEOUT);
    if (ret < 0) {
        otc_err("Failed to set bitbang mode: %s", libusb_error_name(ret));
        return OTC_ERR;
    }
    
    return OTC_OK;
}
```

### Acquisition Start

```c
OTC_PRIV int ftdi_la_start_acquisition_mpsse(struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    int ret;
    
    /* Configure hardware */
    ret = configure_hardware(sdi);
    if (ret != OTC_OK)
        return ret;
    
    /* Allocate USB transfers */
    ret = allocate_transfers(sdi);
    if (ret != OTC_OK)
        return ret;
    
    /* Submit all transfers */
    for (size_t i = 0; i < devc->num_transfers; i++) {
        ret = libusb_submit_transfer(devc->transfers[i]);
        if (ret < 0) {
            otc_err("Failed to submit transfer %zu: %s",
                   i, libusb_error_name(ret));
            return OTC_ERR;
        }
    }
    
    /* Add USB event source */
    struct otc_dev_driver *di = sdi->driver;
    struct drv_context *drvc = di->context;
    
    otc_session_source_add_usb(sdi->session, drvc->otc_ctx->libusb_ctx,
                              100, handle_usb_events, sdi);
    
    /* Send acquisition start packet */
    std_session_send_df_header(sdi);
    
    return OTC_OK;
}
```

### USB Event Handling

```c
static int handle_usb_events(int fd, int revents, void *cb_data)
{
    struct otc_dev_inst *sdi = cb_data;
    struct dev_context *devc = sdi->priv;
    struct otc_dev_driver *di = sdi->driver;
    struct drv_context *drvc = di->context;
    struct timeval tv = {0, 0};
    
    (void)fd;
    (void)revents;
    
    libusb_handle_events_timeout(drvc->otc_ctx->libusb_ctx, &tv);
    
    /* Check if acquisition should stop */
    if (devc->acq_aborted) {
        stop_acquisition(sdi);
        return FALSE;
    }
    
    return TRUE;
}
```

### Acquisition Stop

```c
static void stop_acquisition(struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    
    /* Cancel all transfers */
    for (size_t i = 0; i < devc->num_transfers; i++) {
        if (devc->transfers[i])
            libusb_cancel_transfer(devc->transfers[i]);
    }
    
    /* Remove USB event source */
    otc_session_source_remove_usb(sdi->session);
    
    /* Send end packet */
    std_session_send_df_end(sdi);
    
    /* Free transfers */
    for (size_t i = 0; i < devc->num_transfers; i++) {
        if (devc->transfers[i]) {
            g_free(devc->transfers[i]->user_data);
            libusb_free_transfer(devc->transfers[i]);
        }
        g_free(devc->transfer_buffers[i]);
    }
    
    g_free(devc->transfers);
    g_free(devc->transfer_buffers);
    devc->transfers = NULL;
    devc->transfer_buffers = NULL;
    devc->num_transfers = 0;
}

OTC_PRIV int ftdi_la_stop_acquisition_mpsse(struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    
    devc->acq_aborted = TRUE;
    
    return OTC_OK;
}
```

## Testing

### Unit Tests
- Clock configuration calculation
- Transfer size calculation
- Status byte removal
- Sample counting

### Integration Tests
- Start/stop acquisition
- USB disconnect handling
- Buffer overflow prevention
- Sample rate accuracy

### Hardware Tests
- Capture at various sample rates
- Long captures (hours)
- Maximum sample rate
- All channels simultaneously

## Performance Considerations

1. **Transfer Size**: Optimized for 100ms of data per transfer
2. **Buffer Count**: 2-16 transfers based on sample rate
3. **Timeout**: Dynamically calculated with 25% margin
4. **Latency**: 1ms minimum for responsive captures
5. **CPU Usage**: Asynchronous transfers minimize CPU load

## Error Handling

1. **USB Errors**: Log and abort acquisition
2. **Transfer Failures**: Cancel remaining transfers
3. **Buffer Overflow**: Backpressure via transfer resubmission
4. **Disconnect**: Graceful cleanup via callback

## Debugging

Enable debug output:
```bash
export OTC_LOG_LEVEL=5  # SPEW level
```

Key debug messages:
- Transfer allocation details
- Sample rate configuration
- USB transfer status
- Sample counts
