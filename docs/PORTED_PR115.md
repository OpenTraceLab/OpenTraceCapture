# Ported libsigrok PR #115: MHINSTEK MHS-5200A Function Generator Support

## Overview
Added complete driver for MHINSTEK MHS-5200A dual-channel function generator with all PR review comments addressed.

## Source
- **Upstream PR**: https://github.com/sigrokproject/libsigrok/pull/115
- **Author**: Peter Skarpetis
- **Port Date**: 2025-10-07
- **Patches Applied**: 2 patches (skeleton + implementation)

## PR Comments Addressed

### 1. SERIALCOMM Definition
- Added `#define SERIALCOMM "57600/8n1"` at top of api.c
- Replaced hardcoded string with SERIALCOMM constant
- Improves maintainability and consistency

### 2. Extra Blank Lines Removed
- Cleaned up unnecessary blank lines throughout
- Fixed whitespace formatting issues
- Removed leading spaces from lines

### 3. C++ Style Comments Converted
- Changed all `//` comments to `/* */` style
- Maintains C89 compatibility
- Consistent with project coding style

### 4. Editor Directives Removed
- Removed vim modelines (`/* vim: ... */`)
- Removed Emacs local variables blocks
- Keeps code clean and editor-agnostic

### 5. Whitespace Issues Fixed
- Removed extra spaces and tabs
- Consistent indentation throughout
- Clean formatting

## Changes Applied

### MHINSTEK MHS-5200A Driver (src/hardware/mhinstek-mhs-5200a/)
- `api.c` - Driver API implementation (418 lines)
- `protocol.c` - Protocol handling and communication (629 lines)
- `protocol.h` - Protocol definitions and structures (117 lines)

## Supported Features

### Hardware Specifications
- **Model**: MHINSTEK MHS-5200A
- **Channels**: 2 independent channels
- **Communication**: Serial RS-232, 57600 baud, 8N1
- **Max Frequency**: 21 MHz (sine), 6 MHz (other waveforms)

### Waveform Types
- Sine wave (1 µHz - 21 MHz)
- Square wave (1 µHz - 6 MHz) with duty cycle control
- Triangle wave (1 µHz - 6 MHz)
- Rising sawtooth (1 µHz - 6 MHz)
- Falling sawtooth (1 µHz - 6 MHz)

### Channel Controls
- **Frequency**: 1 µHz to 21 MHz (waveform dependent)
- **Amplitude**: 0-20V peak-to-peak
- **Offset**: ±10V DC offset
- **Phase**: 0-360° (0.1° resolution)
- **Duty Cycle**: 0-100% (square wave only)
- **Attenuation**: 0dB or -20dB

### Device Options
- Per-channel enable/disable
- Pattern mode selection
- Continuous operation
- Sample/time limits
- Independent channel configuration

## Technical Details

### Protocol
- ASCII command-based protocol
- Commands terminated with newline
- Responses end with "OK"
- 50ms read/write timeouts
- Maximum command length: 32 bytes

### Command Examples
- Get model: `:r00=0.`
- Set frequency: `:s00=<freq>.`
- Set waveform: `:s01=<type>.`
- Set amplitude: `:s02=<amp>.`
- Set offset: `:s03=<offset>.`
- Set duty cycle: `:s04=<duty>.`
- Set phase: `:s05=<phase>.`
- Enable output: `:s06=1.`

### Waveform Specifications
Each waveform has:
- Minimum frequency (1 µHz)
- Maximum frequency (6-21 MHz)
- Frequency step (1 µHz)
- Supported options (frequency, amplitude, offset, phase, duty cycle)

### Attenuation Types
- `ATTENUATION_0DB`: Full amplitude (0-20V)
- `ATTENUATION_MINUS_20DB`: Reduced amplitude (0-2V)

## Build Integration
- Added mhinstek-mhs-5200a to drivers list
- Proper meson.build configuration
- All files compile without errors or warnings

## Code Quality
- Proper copyright headers added
- C-style comments throughout
- No editor directives
- Clean whitespace
- SERIALCOMM constant defined
- Consistent formatting

## Status
Complete port with all PR review comments addressed and code quality improvements applied.

## Notes
- All SR_/sr_ identifiers converted to OTC_/otc_ conventions
- Serial communication at 57600 baud
- Dual-channel independent operation
- Full waveform control per channel
- Compatible with signal generator infrastructure
