# Ported PR #77 from libsigrok - COMPLETE

## Source
https://github.com/sigrokproject/libsigrok/pull/77

## Description
Added support for "quirks" in SCPI framework and Siglent SPD3303 series power supplies.

## Status
✅ **FULLY PORTED** - All 3 patches have been successfully ported and compiled.

## Changes Made

### 1. SCPI Quirks Framework (Patch 1/3) ✅

#### src/scpi.h
- Added `enum scpi_quirks` with the following quirks:
  - `SCPI_QUIRK_CMD_OMIT_LF` - Device doesn't expect NL/LF after commands
  - `SCPI_QUIRK_OPC_UNSUPPORTED` - Device doesn't support OPC? command
  - `SCPI_QUIRK_SLOW_CHANNEL_SELECT` - Device needs delay after channel change
  - `SCPI_QUIRK_DELAY_AFTER_CMD` - Device requires delay after sending command
- Added `uint32_t quirks` field to `struct otc_scpi_dev_inst`

#### src/scpi/scpi.c
- Modified `scpi_send_variadic()` to check `SCPI_QUIRK_CMD_OMIT_LF` before adding newline
- Modified `scpi_get_data()` to add delay if `SCPI_QUIRK_DELAY_AFTER_CMD` is set
- Modified `otc_scpi_get_opc()` to handle `SCPI_QUIRK_OPC_UNSUPPORTED`
- Modified `otc_scpi_cmd()` to add delay if `SCPI_QUIRK_SLOW_CHANNEL_SELECT` is set
- Modified `otc_scpi_cmd_resp()` to add delay if `SCPI_QUIRK_SLOW_CHANNEL_SELECT` is set

#### src/scpi/scpi_usbtmc_libusb.c
- Added `blacklist_slow[]` array with Siglent SPD3303 VID/PID (0x0483:0x7540)
- Modified `scpi_usbtmc_libusb_open()` to check blacklist and set `SCPI_QUIRK_DELAY_AFTER_CMD`

### 2. udev Rules (Patch 2/3) ✅

#### contrib/60-opentracecapture.rules
- Updated Siglent section to include USBTMC devices
- Added SPD3303X-E PSU (0483:7540)
- Added SDS1052DL+ scope (f4ec:ee3a)
- Added SDS1104X-E scope (f4ec:ee38)
- Kept existing SDS1202X-E scope / SDG1010 waveform generator (f4ed:ee3a)

### 3. Siglent SPD3303 Driver Support (Patch 3/3) ✅

#### src/hardware/scpi-pps/protocol.h
- Added `SCPI_CMD_GET_CHANNEL_CONFIG` and `SCPI_CMD_SET_CHANNEL_CONFIG` commands
- Added `SCPI_DIALECT_SIGLENT` to `enum pps_scpi_dialect`
- Added `uint32_t priv_status` field to `struct dev_context` for device-specific status

#### src/hardware/scpi-pps/api.c
- Modified `probe_device()` to set Siglent quirks when dialect is `SCPI_DIALECT_SIGLENT`
- Modified `config_get()`:
  - Updated `OTC_CONF_ENABLED` case to use STRING type for Siglent
  - Added `OTC_CONF_CHANNEL_CONFIG` case
  - Updated `otc_scpi_cmd_resp()` calls to pass `channel_group_name` as extra parameter
  - Added Siglent-specific handling for `SCPI_CMD_GET_OUTPUT_ENABLED` (parse hex status register)
  - Added Siglent-specific handling for `SCPI_CMD_GET_OUTPUT_REGULATION` (parse hex status register)
  - Added Siglent-specific handling for `SCPI_CMD_GET_CHANNEL_CONFIG` (parse channel mode)
- Modified `config_set()`:
  - Updated `OTC_CONF_ENABLED` to pass `channel_group_name` for Siglent
  - Updated `OTC_CONF_VOLTAGE_TARGET` to pass `channel_group_name` for Siglent
  - Updated `OTC_CONF_CURRENT_LIMIT` to pass `channel_group_name` for Siglent
  - Added `OTC_CONF_CHANNEL_CONFIG` case with Siglent mode translation (Parallel/Series/Independent → 2/1/0)

#### src/hardware/scpi-pps/protocol.c
- Updated `scpi_pps_receive_data()` to pass `channel_group_name` to `otc_scpi_cmd_resp()`

#### src/hardware/scpi-pps/profiles.c
- Added `siglent_spd3303_devopts[]` - device options (continuous, limits, channel config)
- Added `siglent_spd3303_devopts_cg[]` - channel group options (regulation, voltage, current, enabled)
- Added `siglent_spd3303x_ch[]` - channel specs for SPD3303X (0-32V, 0-3.2A, 0.001 resolution)
- Added `siglent_spd3303xe_ch[]` - channel specs for SPD3303X-E (0-32V, 0-3.2A, 0.01 resolution)
- Added `siglent_spd3303_cg[]` - channel group specs (2 channels with OVP/OCP)
- Added `siglent_spd3303_cmd[]` - SCPI command mappings for Siglent devices
- Added `siglent_spd3303_update_status()` - status update function that:
  - Reads and parses hex status register
  - Detects regulation changes (CC/CV)
  - Detects enable/disable changes
  - Detects channel mode changes (Independent/Series/Parallel)
  - Sends meta frames for channel config changes
- Added three device profiles:
  - **SPD3303C** - using SPD3303X-E channel specs
  - **SPD3303X** - using SPD3303X channel specs (higher resolution)
  - **SPD3303X-E** - using SPD3303X-E channel specs

## Identifier Mapping

All identifiers were renamed from libsigrok to OpenTraceCapture conventions:
- `SR_` → `OTC_`
- `sr_` → `otc_`
- `struct sr_scpi_dev_inst` → `struct otc_scpi_dev_inst`
- `struct sr_dev_inst` → `struct otc_dev_inst`
- `SR_OK` → `OTC_OK`
- `SR_ERR` → `OTC_ERR`
- `SR_CONF_*` → `OTC_CONF_*`
- `SR_MQ_*` → `OTC_MQ_*`
- `SR_MQFLAG_*` → `OTC_MQFLAG_*`
- `sr_session_send_meta()` → `otc_session_send_meta()`
- `sr_info()` → `otc_info()`
- `sr_atol_base()` → `otc_atol_base()`
- `sr_atoi()` → `otc_atoi()`
- etc.

## Build System

No build system changes were required as OpenTraceCapture already uses Meson (the upstream PR was for autotools).

## Testing

✅ The code compiles successfully with no errors or warnings.

## Features Added

The Siglent SPD3303 series power supplies now have full support including:

1. **Basic Measurements**
   - Voltage measurement
   - Current measurement
   - Power measurement

2. **Control**
   - Voltage target setting
   - Current limit setting
   - Output enable/disable

3. **Status Monitoring**
   - Regulation mode (CC/CV)
   - Output enabled state
   - Channel configuration (Independent/Series/Parallel)

4. **Multi-Channel Support**
   - 2 independent channels
   - Series mode (up to 64V)
   - Parallel mode (up to 6.4A)

5. **Device-Specific Workarounds**
   - Commands sent without LF termination
   - OPC command not used
   - Delays after channel selection
   - Delays after commands (for USBTMC interface)

## Supported Models

- **Siglent SPD3303C** - Basic model
- **Siglent SPD3303X** - Higher resolution (0.001V/0.001A)
- **Siglent SPD3303X-E** - Standard resolution (0.01V/0.01A)

All models support 0-32V, 0-3.2A per channel, with up to 102.4W power output.

