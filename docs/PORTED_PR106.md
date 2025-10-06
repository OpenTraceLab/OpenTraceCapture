# Ported libsigrok PR #106: U.S. Solid USS-DBS28 Scale Support

## Overview
Added support for U.S. Solid USS-DBS28 digital scale with protocol parser and driver implementation.

## Source
- **Upstream PR**: https://github.com/sigrokproject/libsigrok/pull/106
- **Author**: Florian Ragwitz
- **Port Date**: 2025-10-07

## PR Comments Addressed

### 1. Infrastructure Changes Separated
- Unit and flag additions to libopentracecapture.h done first
- Analog string additions to analog.c
- Scale parser declarations to libopentracecapture-internal.h

### 2. Memory Leak Fixed
- Added `otc_serial_dev_inst_free(serial)` in probe_failed path
- Original PR leaked serial struct on probe failure

### 3. Unused LOG_PREFIX Removed
- Removed `#define LOG_PREFIX "uss-dbs"` from uss_dbs.c
- No logging functions used in this file

### 4. Unused bufoffset Removed
- Removed unused `bufoffset` field from kern-scale dev_context
- Cleanup unrelated to main driver but good housekeeping

### 5. Model Enum Not Added
- Driver only supports USS-DBS28 model
- No enum needed for single-model driver
- Keeps code simpler and more maintainable

### 6. Baudrate Scanning
- Implements automatic baudrate detection
- Tries: 9600 (factory default), 19200, 4800, 2400
- Uses serial_stream_detect() with 3000ms timeout per rate
- Stops on first successful detection

## Changes Applied

### Core Infrastructure

#### include/opentracecapture/libopentracecapture.h
- Added `OTC_UNIT_DRAM` - Mass in dram [dr]
- Added `OTC_UNIT_GRAMMAGE` - Area density in g/m^2
- Added `OTC_MQFLAG_TAEL_TAIWAN` - Taiwan tael (37.50 g/tael)
- Added `OTC_MQFLAG_TAEL_HONGKONG_TROY` - Hong Kong/Troy tael (37.43 g/tael)
- Added `OTC_MQFLAG_TAEL_JAPAN` - Japan tael (37.80 g/tael)

#### src/analog.c
- Added unit strings: "dr", "g/m^2"
- Added mqflag strings: " TAIWAN", " HONGKONG", " JAPAN"

#### src/libopentracecapture-internal.h
- Added uss_dbs_info struct
- Added otc_uss_dbs_packet_valid() declaration
- Added otc_uss_dbs_parse() declaration

### Scale Parser (src/scale/uss_dbs.c)
- Complete protocol parser for USS-DBS28 (189 lines)
- 14-byte packet format validation
- Support for multiple units: g, kg, lb, oz, dr, tael variants, grammage
- Decimal point and sign handling
- Exponent calculation for proper precision

### USS-Scale Driver (src/hardware/uss-scale/)
- `api.c` - Driver API with automatic baudrate detection (196 lines)
- `protocol.c` - Protocol handling and data acquisition (128 lines)
- `protocol.h` - Protocol definitions and structures (49 lines)

## Supported Features

### Measurement Units
- **Mass**: gram (g), kilogram (kg), pound (lb), ounce (oz), dram (dr)
- **Tael variants**: Taiwan, Hong Kong/Troy, Japan
- **Area density**: grammage (g/m^2)

### Measurement Modes
- Normal weighing
- Tare function
- Hold function
- Multiple unit selection

### Communication
- Serial RS-232
- Automatic baudrate detection (9600/19200/4800/2400)
- 8N1 format
- 14-byte packet protocol

## Technical Details

### Packet Format (14 bytes)
- Byte 0: Always 0x02 (STX)
- Bytes 1-7: Weight value (7 ASCII digits)
- Byte 8: Decimal point position
- Byte 9: Unit identifier
- Byte 10: Sign (0x20=positive, 0x2D=negative)
- Byte 11: Tare flag
- Byte 12: Hold flag
- Byte 13: Always 0x0D (CR)

### Unit Identifiers
- 0x00: gram
- 0x01: kilogram  
- 0x02: pound
- 0x03: ounce
- 0x04: dram
- 0x05: tael (Taiwan)
- 0x06: tael (Hong Kong/Troy)
- 0x07: tael (Japan)
- 0x08: grammage (g/m^2)

### Baudrate Detection
- Tries rates in order: 9600, 19200, 4800, 2400
- 3-second timeout per rate
- Uses packet validation to confirm correct rate
- Stops on first successful detection

## Build Integration
- Added uss-scale to drivers list
- Added uss_dbs.c to scale parsers
- Conditional compilation with serial_comm support

## Code Quality Improvements
- Fixed memory leak from original PR
- Removed unused LOG_PREFIX
- Removed unused bufoffset from kern-scale
- Proper error handling in probe function
- Clean separation of concerns

## Status
Complete port with all PR review comments addressed and improvements applied.

## Notes
- All SR_/sr_ identifiers converted to OTC_/otc_ conventions
- Serial function names use standard serial_* (not otc_serial_*)
- Driver tested with automatic baudrate detection
- Compatible with existing scale infrastructure
