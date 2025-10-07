# PR #145 Analysis: FTDI-LA Driver Rewrite

## Overview
PR #145 is a major rewrite of the ftdi-la driver consisting of 9 patches totaling 2,338 lines of changes. The PR fundamentally changes the driver architecture from using libftdi to using libusb directly.

## Source Information
- **Original PR**: libsigrok PR #145
- **Author**: Thomas Hebb <tommyhebb@gmail.com>
- **Scope**: Complete driver rewrite
- **Patch Size**: 2,338 lines, 863 additions
- **Patches**: 9 separate commits

## Patch Breakdown

### Patch 1/9: Add missing libusb_close() calls, remove unneeded code
- Fixes resource leaks in chronovu-la, dreamsourcelab-dslogic, fx2lafw
- **Status**: Not directly applicable to ftdi-la

### Patch 2/9: Fix clock divider for FT232H
- Changes samplerate_div from 30 to 20 for FT232H
- **Issue**: Current driver has incorrect clock divider causing 66% timing error
- **Fix**: Simple one-line change
- **Status**: SHOULD PORT

### Patch 3/9: Don't depend on libftdi in scan()
- Removes libftdi dependency from device scanning
- Uses libusb directly for device enumeration
- **Status**: SHOULD PORT

### Patch 4/9: Implement acquisition using libusb
- **MAJOR CHANGE**: Rewrites entire acquisition path
- Removes libftdi dependency completely
- Implements direct libusb bulk transfers
- Adds proper USB event handling
- **Size**: ~400 lines of new code
- **Status**: COMPLEX - Requires careful porting

### Patch 5/9: Prevent dropped samples
- Improves sample buffering to prevent data loss
- Adds better flow control
- **Status**: SHOULD PORT (depends on patch 4)

### Patch 6/9: Support all channels on multi-channel chips
- Adds support for BDBUS, CDBUS, DDBUS channels on FT4232H
- Expands channel support from 8 to 32 channels
- **Status**: SHOULD PORT

### Patch 7/9: Don't allow sample rates the hardware can't achieve
- Validates sample rates against hardware capabilities
- Prevents invalid configurations
- **Status**: SHOULD PORT

### Patch 8/9: Drop support for FT232R
- **REMOVES FT232R SUPPORT**
- Reason: FT232R uses different protocol (bitbang mode vs MPSSE)
- **Status**: SKIP - User wants to keep FT232R support

### Patch 9/9: Write sample rate to hardware at capture time
- Moves sample rate configuration to acquisition start
- Improves reliability
- **Status**: SHOULD PORT

## Key Technical Changes

### Architecture Shift
**Before**: libftdi → libusb
**After**: Direct libusb communication

### Acquisition Method
**Before**: 
- Uses `ftdi_read_data()` synchronous reads
- Simple polling loop
- Limited buffer management

**After**:
- Direct `libusb_bulk_transfer()` calls
- Asynchronous USB event handling
- Sophisticated buffer management
- Better error handling

### Channel Support
**Before**: 8 channels (ADBUS only)
**After**: Up to 32 channels (ADBUS, BDBUS, CDBUS, DDBUS)

### Sample Rate Handling
**Before**: 
- Hardcoded divisors
- No validation
- FT232H has wrong divisor (30 instead of 20)

**After**:
- Correct divisors
- Validation against hardware limits
- Dynamic calculation

## Critical Issue: FT232H Clock Divider

### Problem
Current driver uses `samplerate_div = 30` for FT232H, but correct value is 20.

### Impact
- Actual sample rate is 66% of configured rate
- 1 second pulse appears as 666ms
- Affects all FT232H measurements

### Fix
```c
static const struct ftdi_chip_desc ft232h_desc = {
    .vendor = 0x0403,
    .product = 0x6014,
    .samplerate_div = 20,  // Changed from 30
    ...
};
```

## FT232R Support Challenge

### Why Patch 8 Removes It
- FT232R uses **bitbang mode** (different protocol)
- Other chips use **MPSSE mode**
- Incompatible at protocol level
- Would require separate code paths

### Options to Keep FT232R
1. **Dual-mode driver**: Detect chip type and use appropriate protocol
2. **Separate FT232R path**: Keep old libftdi code for FT232R only
3. **Bitbang implementation**: Implement bitbang mode in new architecture

## PR Comments to Address

### Comment 1 (tchebb): Consistency
```c
// Change:
if (conn_parts[0][0] != '\0') {
// To:
if (conn_parts[0][0]) {
```

### Comment 2 (tchebb): Check acq_aborted again
```c
/* Check this again, since send_samples() may have set it. */
if (!devc->acq_aborted) {
    // ...
}
```

### Comment 3 (tchebb): Test unplugging during capture
- TODO: Verify USB disconnect handling works correctly

### Comment 4 (tchebb): Make const
- Some variables should be marked const

### Comment 5 (tchebb): Rename function
- Rename to `handle_usb_events()` or similar

### Comment 6 (codeottomayer): Timing validation
- FT232H timing issue confirmed (66% error)
- Fixed by patch 2 (clock divider correction)
- Needs testing at 15MHz sample rate

## Porting Strategy

### Phase 1: Critical Fixes (Immediate)
1. ✅ Fix FT232H clock divider (patch 2)
2. ✅ Add sample rate validation (patch 7)

### Phase 2: Architecture Update (Complex)
1. Port libusb-based scanning (patch 3)
2. Implement libusb acquisition (patch 4)
3. Add buffer improvements (patch 5)
4. Move sample rate config to acquisition start (patch 9)

### Phase 3: Feature Additions
1. Add multi-channel support (patch 6)
2. Expand to 32 channels for FT4232H

### Phase 4: FT232R Preservation
1. Implement dual-mode support
2. Keep bitbang mode for FT232R
3. Use MPSSE mode for other chips

## Estimated Effort

### Minimal Port (Patches 2, 7 only)
- **Effort**: 1-2 hours
- **Impact**: Fixes critical timing bug, adds validation
- **Risk**: Low

### Full Port (Patches 1-7, 9, skip 8)
- **Effort**: 8-16 hours
- **Impact**: Complete driver rewrite, major improvements
- **Risk**: High - requires extensive testing

### Full Port + FT232R Support
- **Effort**: 16-24 hours
- **Impact**: All improvements + FT232R compatibility
- **Risk**: Very High - requires dual-mode implementation

## Recommendation

### Option A: Quick Fix (Recommended for now)
Port only patches 2 and 7:
- Fix FT232H clock divider
- Add sample rate validation
- Minimal risk, immediate benefit
- Addresses critical timing bug

### Option B: Full Rewrite (Future work)
Complete port with FT232R support:
- Requires dedicated development time
- Needs extensive hardware testing
- Should be separate project
- Consider creating ftdi-la-ng (new generation) driver

## Files to Modify

### For Quick Fix (Option A)
1. `src/hardware/ftdi-la/protocol.h` - Update ft232h_desc
2. `src/hardware/ftdi-la/api.c` - Add sample rate validation

### For Full Port (Option B)
1. `src/hardware/ftdi-la/protocol.h` - Complete restructure
2. `src/hardware/ftdi-la/protocol.c` - Rewrite acquisition
3. `src/hardware/ftdi-la/api.c` - Update all functions
4. Add new USB handling code
5. Implement dual-mode support for FT232R

## Testing Requirements

### Hardware Needed
- FT232H device (critical timing bug)
- FT2232H device
- FT4232H device (for multi-channel)
- FT232R device (for compatibility)

### Test Cases
1. Sample rate accuracy (oscilloscope verification)
2. 15MHz sample rate on FT232H
3. Multi-channel capture on FT4232H
4. FT232R bitbang mode
5. USB disconnect during capture
6. Long captures (buffer handling)

## Conclusion

PR #145 is a substantial improvement but requires significant porting effort. The critical FT232H timing bug should be fixed immediately (patch 2). The full rewrite should be considered as a separate, well-tested project with proper FT232R dual-mode support.

**Immediate Action**: Port patch 2 (clock divider fix) to resolve the 66% timing error affecting all FT232H users.

**Future Work**: Plan full driver rewrite as dedicated project with comprehensive testing.
