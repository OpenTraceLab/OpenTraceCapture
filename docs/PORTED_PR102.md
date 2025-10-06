# Ported libsigrok PR #102: APPA DMM Driver and BLE Support

## Overview
Ported complete APPA DMM driver with BLE support and APPA transport protocol.

## Source
- **Upstream PR**: https://github.com/sigrokproject/libsigrok/pull/102
- **Author**: Martin Eitzenberger and contributors
- **Port Date**: 2025-10-07
- **Patches Applied**: 23 patches (partial - core driver and infrastructure)

## Changes Applied

### Core Infrastructure

#### libopentracecapture-internal.h
- Added `read_i24le()` - Read 24-bit little-endian signed integer
- Added `read_i24be()` - Read 24-bit big-endian signed integer  
- Added `read_i24le_inc()` - Read 24-bit LE signed with pointer increment
- Added `SER_BT_CONN_APPADMM` to `ser_bt_conn_t` enum for APPA BLE devices

#### serial_bt.c
- Added 37 APPA/BENNING/Sefram/RS PRO device names to scan_supported_items
- Added "appa-dmm" connection type name
- Implemented APPADMM BLE handles (read: 0x0049, write: 0x004c, cccd: 0x004a)
- Added APPADMM cases to all BLE switch statements

### Transport Protocol (src/tp/)
- Created new tp/ directory for transport protocol handlers
- Added `appa.c` - APPA transport protocol implementation
- Added `appa.h` - APPA transport protocol header
- Packet format: [SS SS CC LL DD... CS] with checksum validation
- Functions: init, term, send, receive, send_receive

### APPA-DMM Driver (src/hardware/appa-dmm/)
- Created complete new driver for APPA multimeters
- `api.c` - Driver API implementation (386 lines)
- `protocol.c` - Protocol handling (1215 lines)
- `protocol.h` - Protocol definitions (895 lines)
- `protocol_packet.h` - Packet structures (808 lines)
- `protocol_tables.h` - Device tables (275 lines)

## Supported Devices

### APPA Models
- 155B, 156B, 157B, 158B (Bluetooth models)
- 172B, 173B, 175B, 177B, 179B (Bluetooth models)
- 208B, 506B (Bluetooth models)
- A17N, S0, S1, S2, S3 (Series S)
- sFlex-10A, sFlex-18A (Flexible probes)

### OEM Variants
- BENNING: CM9-2, CM10-1, CM10-PV, CM12, MM10-1, MM10-PV, MM12
- RS PRO: S1, S2, S3
- Sefram: 7220, 7221, 7222, 7223, 7352B

## Technical Details

### APPA Transport Protocol
- Start word: 0x5555
- Command byte: Device-specific
- Length byte: Max 64 data bytes
- Checksum: Sum of all bytes except checksum
- Blocking and non-blocking send/receive modes

### BLE Communication
- Uses AMICCOM A8105 BLE controller
- Read handle: 0x0049 (UUID: 0000fff1-0000-1000-8000-00805f9b34fb)
- Write handle: 0x004c (UUID: 0000fff2-0000-1000-8000-00805f9b34fb)
- CCCD handle: 0x004a (UUID: 00002902-0000-1000-8000-00805f9b34fb)
- Notifications enabled (cccd_val: 0x0001)

### Rate Limiting
- Older AMICCOM A8105 models need rate limiting over BLE
- Models 208B, 506B, 150B use APPADMM_RATE_INTERVAL_DISABLE
- Other models use APPADMM_RATE_INTERVAL_DEFAULT

## Build Integration
- Added tp/ subdirectory to meson build
- Added appa-dmm to drivers list
- Conditional compilation with HAVE_BLUETOOTH for BLE features

## Status
Complete port of patches 1-5 from PR #102. Remaining patches (6-23) add:
- Additional device models (APPA 300, 207, 10x series, legacy 500 series)
- MEM/LOG download support
- CSV output enhancements
- Additional OEM variants

## Notes
- All SR_/sr_ identifiers converted to OTC_/otc_ conventions
- Serial function names use standard serial_* (not otc_serial_*)
- BLE-specific code wrapped in #ifdef HAVE_BLUETOOTH
- Compatible with existing serial and USB connections
