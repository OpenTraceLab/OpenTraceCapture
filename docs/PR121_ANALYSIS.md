# PR #121 Analysis: GWInstek GDS-2xxx Support

## Overview
PR #121 adds support for GWInstek GDS-2xxx series oscilloscopes to the existing gwinstek-gds-800 driver.

## Source
- **Upstream PR**: https://github.com/sigrokproject/libsigrok/pull/121
- **Author**: Richard Allen
- **Status**: NOT PORTED - Requires significant refactoring
- **Reason**: Multiple unresolved review comments and design issues

## Patches in PR
1. **Patch 1/5**: Add GDS-2xxx support - Needs formatting fixes
2. **Patch 2/5**: Support current libserialport - **SKIP** (workaround for 9-year-old bug)
3. **Patch 3/5**: Split channels into channel-groups - Needs review
4. **Patch 4/5**: Handle dirty GDS RX buffer - Needs review
5. **Patch 5/5**: Support probe attenuation factor - Needs review

## Critical Review Comments

### 1. Indentation and Formatting Issues
**Multiple locations** - Inconsistent indentation, extra spaces, missing spaces
- Fix bracket placement
- Remove extra spaces after `*`
- Add spaces after keywords (if, for, while)
- Wrap expressions in brackets for operator precedence

### 2. NUM_VDIV Confusion (MAJOR ISSUE)
**protocol.h** - Fundamental misunderstanding of what NUM_VDIV represents

**Problem**: 
- Original code had `VERTICAL_DIVISIONS = 10`
- Author changed it based on display divisions (8 on screen)
- Reviewer clarified: NUM_VDIV is the **number of vertical scaling settings**, not display divisions
- It's the size of the `vdivs` array (number of settings on the vertical scaling knob)

**Current State**:
- GDS-800: Unknown (needs verification)
- GDS-2xxx: 11 different vertical division settings according to author
- **Unresolved**: Correct value needs to be determined for each model

**Impact**: Affects voltage conversion calculations:
```c
float vbit = volts_per_division * VERTICAL_DIVISIONS / 256.0;
for (i = 0; i < num_samples; i++)
    samples[i] = ((float) ((int16_t) (RB16(&devc->rcv_buffer[i*2])))) * vbit;
```

### 3. mV Check Removal (REGRESSION)
**protocol.c** - Removed "mV" check needed for GDS-800 compatibility

**Problem**:
- Author removed check that GDS-800 series needs
- Would break existing GDS-800 support
- **Resolution**: Author agreed to restore it

### 4. Probe Attenuation Handling
**protocol.h/protocol.c** - Unclear if scope handles probe scaling internally

**Problem**:
- Author initially thought manual probe scaling was needed
- Testing showed scope already corrects for probe scaling
- **Unresolved**: Needs proper implementation or removal

### 5. Voltage Conversion Logic
**protocol.c** - Confusing vbit and vbitlog calculations

**Problem**:
- Reviewer: "I seriously don't understand the vbit and vbitlog calculations either"
- Needs refactoring for clarity
- May have been "accidentally correct" due to offsetting errors
- **Unresolved**: Requires complete review and testing

## Specific Code Issues

### Issue 1: Model Detection Logic
```c
// Original problematic code:
if (strcmp(hw_info->manufacturer, "GW") != 0 ||
     !(strncmp(hw_info->model, "GDS-8", 5) == 0 ||
       strncmp(hw_info->model, "GDS-2", 5) == 0 )) {
```

**Problems**:
- Extra spaces
- Poor bracket placement
- Confusing double-negative logic

**Should be**:
```c
if ((strcmp(hw_info->manufacturer, "GW") != 0) ||
    ((strncmp(hw_info->model, "GDS-8", 5) != 0) &&
     (strncmp(hw_info->model, "GDS-2", 5) != 0))) {
```

### Issue 2: Driver Info Structure
```c
// Added second driver but unclear if needed
static struct sr_dev_driver gwinstek_gds_800_driver_info;
static struct sr_dev_driver gwinstek_gds_2000_driver_info;
```

**Problem**: May not need separate driver structures

## Why Not Ported

### 1. Unresolved Technical Issues
- NUM_VDIV value incorrect/unclear
- Voltage conversion logic needs refactoring
- Probe attenuation handling unclear

### 2. Potential Regressions
- mV check removal would break GDS-800
- Changes may affect voltage accuracy
- Untested on original GDS-800 hardware

### 3. Code Quality Issues
- Multiple formatting problems
- Confusing logic that needs clarification
- Calculations that "accidentally work"

### 4. Incomplete Review Process
- Author acknowledged issues but didn't provide final fixes
- Reviewer requested refactoring that wasn't done
- No confirmation of correct NUM_VDIV values

## Recommendations

### For Future Port Attempt:

1. **Verify NUM_VDIV Values**
   - Test on actual GDS-800 hardware
   - Test on actual GDS-2xxx hardware
   - Document the correct values for each model series

2. **Refactor Voltage Conversion**
   - Clarify vbit and vbitlog calculations
   - Add detailed comments explaining the math
   - Verify voltages match scope display

3. **Create Model Table**
   - Separate GDS-800 and GDS-2xxx parameters
   - Include NUM_VDIV per model
   - Include mV check requirement per model

4. **Fix All Formatting**
   - Consistent indentation
   - Proper bracket placement
   - Correct spacing throughout

5. **Test Thoroughly**
   - Verify no regression on GDS-800
   - Verify GDS-2xxx works correctly
   - Verify voltage readings are accurate

## Alternative Approach

Instead of modifying the existing driver, consider:
- Creating a separate gwinstek-gds-2000 driver
- Sharing common code through helper functions
- Avoiding risk of breaking GDS-800 support

## Conclusion

**Status**: NOT RECOMMENDED FOR PORTING

This PR has too many unresolved issues and potential regressions. The author and reviewer identified fundamental problems with the voltage conversion logic and NUM_VDIV handling that were never properly resolved.

Porting this would risk:
1. Breaking existing GDS-800 support
2. Introducing incorrect voltage measurements
3. Adding confusing/incorrect code to the codebase

**Recommendation**: Wait for a revised PR that addresses all review comments, or implement GDS-2xxx support from scratch with proper testing.
