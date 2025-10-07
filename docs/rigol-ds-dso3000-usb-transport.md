# DSO3000 USB Transport Implementation Notes

## Overview
This document describes the USB transport layer for Agilent DSO3000 series oscilloscopes (e.g., DSO3062A), which was part of libsigrok PR #163 but not yet implemented in OpenTraceCapture.

## Current Status

### ✅ Implemented (as of commit 2c0b6e38)
- PROTOCOL_V1 support for DSO3000 series
- FORMAT_HEX data format parsing
- Hex data reader (`rigol_ds_read_hex_data()`)
- DSO3000-specific voltage conversion
- DSO3062A device model
- Works with TCP/IP and Serial connections

### ⏸️ Not Implemented
- USB transport layer (`scpi_ds5000usb_libusb.c`)
- USB-specific infrastructure changes

## USB Transport Details

### New File Required
**src/scpi/scpi_ds5000usb_libusb.c** (~423 lines)
- Complete USB transport implementation for DSO3000/DS5000 protocol

### Infrastructure Changes Required

#### 1. Build System
**File**: `src/scpi/meson.build` or equivalent
```meson
if libusb.found()
  scpi_sources += files('scpi_ds5000usb_libusb.c')
endif
```

#### 2. Internal Header
**File**: `src/libopentracecapture-internal.h`
```c
struct otc_ds5000usb_dev_inst {
	char *device;
	int fd;
};

OTC_PRIV struct otc_ds5000usb_dev_inst *otc_ds5000usb_dev_inst_new(const char *device);
OTC_PRIV void otc_ds5000usb_dev_inst_free(struct otc_ds5000usb_dev_inst *ds5000usb);
```

#### 3. Device Management
**File**: `src/device.c`
```c
OTC_PRIV struct otc_ds5000usb_dev_inst *otc_ds5000usb_dev_inst_new(const char *device)
{
	struct otc_ds5000usb_dev_inst *ds5000usb;

	ds5000usb = g_malloc0(sizeof(*ds5000usb));
	ds5000usb->device = g_strdup(device);
	ds5000usb->fd = -1;

	return ds5000usb;
}

OTC_PRIV void otc_ds5000usb_dev_inst_free(struct otc_ds5000usb_dev_inst *ds5000usb)
{
	if (!ds5000usb)
		return;

	g_free(ds5000usb->device);
	g_free(ds5000usb);
}
```

#### 4. SCPI Header
**File**: `src/scpi.h`
```c
enum scpi_transport_layer {
	SCPI_TRANSPORT_DS5000USB,  // Add this
	SCPI_TRANSPORT_LIBGPIB,
	SCPI_TRANSPORT_SERIAL,
	// ... rest
};
```

#### 5. SCPI Registration
**File**: `src/scpi/scpi.c`
```c
OTC_PRIV extern const struct otc_scpi_dev_inst scpi_ds5000usb_libusb_dev;

static const struct otc_scpi_dev_inst *scpi_devs[] = {
	&scpi_serial_dev,
	&scpi_tcp_raw_dev,
	&scpi_tcp_rigol_dev,
#ifdef HAVE_LIBUSB_1_0
	&scpi_ds5000usb_libusb_dev,  // Add this
	&scpi_usbtmc_libusb_dev,
#endif
	// ... rest
};
```

#### 6. udev Rules
**File**: `contrib/60-libopentracecapture.rules` or equivalent
```
# Agilent DSO3000 series
ATTRS{idVendor}=="0400", ATTRS{idProduct}=="c55d", ENV{ID_SIGROK}="1"
```

## USB Protocol Characteristics

### Hardware Details
- **Vendor ID**: 0x0400
- **Product ID**: 0xc55d
- **Interface Class**: LIBUSB_CLASS_VENDOR_SPEC
- **Interface Protocol**: 0xff (vendor-specific)

### Communication Method
Uses **libusb_control_transfer()** for all communication:

```c
// Read response length
libusb_control_transfer(devhdl, 
    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
    READ_RESPONSE,      // bRequest = 0
    RESPONSE_LENGTH,    // wValue = 0
    0,                  // wIndex
    &length, 1,         // data buffer
    TRANSFER_TIMEOUT);

// Read response data
libusb_control_transfer(devhdl,
    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
    READ_RESPONSE,      // bRequest = 0
    RESPONSE_DATA,      // wValue = 1
    0,                  // wIndex
    buffer, length,     // data buffer
    TRANSFER_TIMEOUT);

// Write character
libusb_control_transfer(devhdl,
    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
    WRITE_CHAR,         // bRequest = 1
    character,          // wValue = char (convert \n to \r)
    0,                  // wIndex
    NULL, 0,            // no data
    TRANSFER_TIMEOUT);
```

### Reliability Issues

The DSO3000 USB firmware is **notoriously unreliable**. The vendor documentation actually recommends against using USB except with their official software.

**Required Workarounds:**
1. **Retry logic**: Wrap all control transfers with 10 retry attempts
2. **Timeout handling**: Gracefully handle LIBUSB_ERROR_TIMEOUT
3. **Buffer flushing**: Flush all buffered data on connection open
4. **Character conversion**: Convert '\n' to '\r' in output

**Example Retry Wrapper:**
```c
static int scpi_ds5000usb_libusb_control_transfer(
    libusb_device_handle *dev_handle,
    uint8_t request_type, uint8_t bRequest, uint16_t wValue,
    uint16_t wIndex, unsigned char *data, uint16_t wLength,
    unsigned int timeout)
{
	int retries = 10;
	int ret;

	while (retries--) {
		ret = libusb_control_transfer(dev_handle, request_type,
			bRequest, wValue, wIndex, data, wLength, timeout);
		if (ret != LIBUSB_ERROR_TIMEOUT)
			break;
		otc_dbg("Timed out. %d more tries...", retries);
	}

	return ret;
}
```

## Implementation Complexity

### Effort Estimate
- **New code**: ~423 lines (scpi_ds5000usb_libusb.c)
- **Infrastructure**: ~50 lines across 6 files
- **Testing**: Requires actual DSO3000 hardware
- **Time**: 4-8 hours for experienced developer

### Risk Assessment
- **Low risk**: Self-contained transport layer
- **No impact**: Existing functionality unaffected
- **Testing challenge**: Needs physical hardware
- **Maintenance**: Additional code path to maintain

## Alternative Connection Methods

DSO3000 devices can connect via:

1. **TCP/IP** (if network-enabled)
   - Use existing `scpi_tcp_raw_dev` transport
   - More reliable than USB
   - Already supported

2. **Serial (RS-232)**
   - Use existing `scpi_serial_dev` transport
   - Most reliable connection method
   - Already supported

3. **USB** (requires implementation)
   - Vendor-specific protocol
   - Unreliable firmware
   - Needs workarounds

## Recommendations

### For Users
- **Prefer TCP/IP or Serial** connections when available
- USB support can be added if there's demand
- Current implementation is production-ready for non-USB connections

### For Developers
- USB transport is a **future enhancement**, not a blocker
- Can be implemented independently without affecting existing code
- Consider implementing only if users specifically request it
- Test thoroughly with actual hardware before release

## References

- **Original PR**: https://github.com/sigrokproject/libsigrok/pull/163
- **Protocol Documentation**: https://github.com/cktben/dso3000 (dso3000 project by Ben Johnson)
- **OpenTraceCapture Commit**: 2c0b6e38 (protocol support without USB)

## Related Files in OpenTraceCapture

- `src/hardware/rigol-ds/protocol.h` - Protocol definitions including PROTOCOL_V1
- `src/hardware/rigol-ds/protocol.c` - Hex data reader and data conversion
- `src/hardware/rigol-ds/api.c` - DSO3000 series and DSO3062A model
- `src/scpi/scpi_tcp.c` - TCP transport (works with DSO3000)

---

**Last Updated**: 2025-10-07  
**Status**: USB transport not implemented, protocol support complete  
**Maintainer**: OpenTraceCapture team
