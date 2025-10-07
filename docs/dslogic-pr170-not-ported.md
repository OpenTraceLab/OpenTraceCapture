# DSLogic PR #170 - Not Ported

## Overview
libsigrok PR #170 contains significant performance optimizations and new device support for the DreamSourceLab DSLogic series, but has **not been ported** to OpenTraceCapture due to complexity and structural differences.

## PR Details
- **Source**: https://github.com/sigrokproject/libsigrok/pull/170
- **Author**: Christian Eisendle
- **Commits**: 15 commits, ~2500 lines of changes
- **Status**: Not ported

## Changes in PR #170

### 1. Performance Optimizations
- **Optimized deinterleave_buffer**: Forward-copy optimization assuming minimal signal changes
- **Worker thread**: Dedicated thread for sample processing to enable double-buffering
- **Queue-based processing**: Asynchronous USB transfer handling

### 2. New Device Support
- **DSLogic U3Pro16**: USB 3.0 device with Cypress CYUSB3014-BZX
  - USB ID: `2a0e:002a`
  - 2GB memory depth
  - Up to 1 GHz sample rate
  - New API version (DS_API_V2)

### 3. Stability Improvements
- Better USB transfer handling
- Continuous mode enabled by default
- Proper handling of max sample count
- GPIF wordwide mode for USB 2.0 devices

### 4. API Changes
- New `dslogic_profile` fields:
  - `max_samplerate_200` / `_400` / `_800`
  - `api_version` (DS_API_V1 vs DS_API_V2)
  - `max_sample_depth_200` / `_400` / `_800`
  - `usb_speed`
  - `usb_packet_size`
- New `dev_context` fields for threading:
  - `data_proc_thread`
  - `data_proc_mutex`
  - `data_proc_state_cond`
  - `data_proc_state`
  - `completed_transfer`

## Why Not Ported

### 1. Structural Incompatibility
The PR requires significant changes to core data structures that would affect all DSLogic devices:

```c
// Current OpenTraceCapture structure
struct dslogic_profile {
	uint16_t vid;
	uint16_t pid;
	const char *vendor;
	const char *model;
	const char *model_version;
	const char *firmware;
	uint32_t dev_caps;
	const char *usb_manufacturer;
	const char *usb_product;
	uint64_t mem_depth;
};

// PR #170 requires adding:
+ uint64_t max_samplerate_200;
+ uint64_t max_samplerate_400;
+ uint64_t max_samplerate_800;
+ uint64_t max_sample_depth_200;
+ uint64_t max_sample_depth_400;
+ uint64_t max_sample_depth_800;
+ uint8_t usb_speed;
+ uint16_t usb_packet_size;
+ enum dslogic_api_version api_version;
```

### 2. Threading Complexity
The PR introduces a worker thread with mutex/condition variable synchronization:
- Requires careful testing to avoid race conditions
- Changes USB transfer callback behavior significantly
- Adds ~200 lines of threading code

### 3. Algorithm Changes
The deinterleave_buffer optimization changes the core sample processing algorithm:
- Assumes signal stability (forward-copy optimization)
- May not work correctly for all signal types
- Requires extensive testing with various capture scenarios

### 4. Testing Requirements
- Needs multiple DSLogic devices (USB 2.0 and USB 3.0)
- Requires high-speed capture testing
- Performance regression testing needed
- Thread safety validation required

## Impact Assessment

### Current Functionality
✅ All existing DSLogic devices work correctly:
- DSLogic
- DSCope
- DSLogic Pro
- DSLogic Plus
- DSLogic Basic

### Missing Functionality
❌ DSLogic U3Pro16 not supported
❌ Performance optimizations not available
❌ Worker thread double-buffering not implemented

### Risk of Porting
⚠️ **High Risk**:
- Could break existing DSLogic devices
- Threading bugs are hard to debug
- Performance optimization may cause data corruption if not tested thoroughly
- Requires significant refactoring

## Recommendations

### For Users
- Current DSLogic support is **stable and functional**
- If you need U3Pro16 support, consider using upstream libsigrok
- Performance is adequate for most use cases

### For Developers
- **Defer this PR** until there's specific user demand
- Consider porting in phases:
  1. First: Add U3Pro16 device profile (simpler)
  2. Then: Add performance optimizations (complex)
  3. Finally: Add worker thread (most complex)
- Requires dedicated testing with actual hardware
- Should be a separate major feature branch

## Alternative Approach

If U3Pro16 support is needed, a minimal implementation could:
1. Add device profile with basic parameters
2. Use existing deinterleave_buffer (slower but stable)
3. Skip worker thread optimization
4. Test thoroughly before merging

This would provide basic U3Pro16 support without the risk of breaking existing devices.

## Related Files

- `src/hardware/dreamsourcelab-dslogic/api.c`
- `src/hardware/dreamsourcelab-dslogic/protocol.c`
- `src/hardware/dreamsourcelab-dslogic/protocol.h`

## References

- **PR #170**: https://github.com/sigrokproject/libsigrok/pull/170
- **DSLogic U3Pro16 Wiki**: https://sigrok.org/wiki/DreamSourceLab_DSLogic_U3Pro16
- **Patch File**: Available in `/tmp/pr170.patch` (2563 lines)

---

**Last Updated**: 2025-10-07  
**Status**: Not ported - too complex, high risk  
**Recommendation**: Defer until specific user demand  
**Maintainer**: OpenTraceCapture team
