# PR #145 Patches 4-7,9 Implementation Tasks

## Status
Patches cannot be applied directly due to code differences. Manual implementation required.

## Key Changes Summary

### Patch 4: Replace libftdi with libusb
- Remove `#include <ftdi.h>`
- Add direct libusb calls
- Remove `ftdi_context` from dev_context
- Add USB device info fields
- Implement direct USB transfers

### Patch 5: Prevent dropped samples  
- Improve buffer management
- Add transfer resubmission

### Patch 6: Multi-channel support
- Add support for all interfaces on FT4232H
- Expand from 8 to 32 channels

### Patch 7: Sample rate validation
- Add validation before setting rate
- Calculate achievable rates

### Patch 9: Defer sample rate config
- Store requested rate
- Apply at acquisition time

## Converted Patch Available
File: `/tmp/pr145_otc.patch` (2253 lines)
- SR_* → OTC_* conversions done
- sr_* → otc_* conversions done
- Ready for manual application

## Next Steps
1. Extract final file states from patch
2. Manually update OpenTraceCapture files
3. Test compilation
4. Test with hardware

## Estimated Effort
- File extraction: 1 hour
- Manual porting: 4-6 hours
- Testing: 2-3 hours
- Total: 7-10 hours
