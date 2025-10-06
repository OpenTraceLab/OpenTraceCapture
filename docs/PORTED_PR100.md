# Ported libsigrok PR #100: ETommens ETM-xxxxP Power Supply Support

## Overview
Ported support for ETommens ETM-xxxxP series power supplies to the rdtech-dps driver.

## Source
- **Upstream PR**: https://github.com/sigrokproject/libsigrok/pull/100
- **Author**: Original libsigrok contributors
- **Port Date**: 2025-10-07

## Changes Applied

### protocol.h
- Added `MODEL_ETOMMENS` to `rdtech_dps_model_type` enum
- Added `power_digits` field to `rdtech_dps_range` struct
- Added `etommens_etm_xxxxp_model` struct with classid, modelid, name fields
- Added `union model` with rdtech_model and etm_model pointers
- Modified `dev_context` to use `model_type` enum and `union model`
- Added `etommens_etm_xxxxp_device_info_get()` function declaration

### api.c
- Added `devopts_etm` array for ETommens-specific device options
- Updated all `rdtech_dps_range` definitions to include `power_digits = 0`
- Added `etommens_models` array with ETM-3010P and ETM-305P models
- Updated all `devc->model` accesses to `devc->model.rdtech_model` for union compatibility

## Supported Models
- eTM-3010P / RS310P / HM310P (classid 0x4B50, modelid 3010)
- etM-305P / RS305P / HM305P (classid 0x4B50, modelid 305)

## Technical Details
- ETommens devices share the rdtech-dps driver but use different protocol
- Union model approach allows single driver to support multiple device families
- Power digits field enables proper decimal precision for power measurements

## Status
Partial port - data structures and model definitions complete. Protocol implementation pending.
