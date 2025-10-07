# PR #145 Patches 4-7, 9 Implementation Guide

## Overview
This document provides the complete implementation for porting patches 4, 5, 6, 7, and 9 from libsigrok PR #145 to OpenTraceCapture while preserving FT232R support.

## Status
⚠️ **COMPLEX IMPLEMENTATION REQUIRED**

These patches constitute a complete driver rewrite (~1,500 lines of changes) that:
- Removes libftdi dependency
- Implements direct libusb communication
- Adds multi-channel support (8→32 channels)
- Improves buffer management
- Adds sample rate validation
- Defers hardware configuration to acquisition time

## Critical Challenge: FT232R Support

### The Problem
- Patches 4-7 rewrite the driver for **MPSSE mode** (FT232H, FT2232H, FT4232H)
- FT232R uses **bitbang mode** (completely different protocol)
- Original patch 8 removes FT232R support
- User wants FT232R support preserved

### Solution Required
Implement **dual-mode driver** with:
1. MPSSE mode path (new libusb code)
2. Bitbang mode path (keep existing libftdi code for FT232R)
3. Mode detection and routing

## Implementation Approach

### Option 1: Minimal Hybrid (Recommended)
Keep current driver mostly intact, add only critical improvements:

**Changes**:
1. ✅ Fix FT232H clock divider (DONE - patch 2)
2. Add sample rate validation (patch 7)
3. Add deferred sample rate config (patch 9)
4. Keep libftdi for all chips

**Effort**: 4-6 hours
**Risk**: Low
**Benefit**: Fixes bugs, adds validation, keeps everything working

### Option 2: Full Rewrite with Dual Mode
Complete port of patches 4-7, 9 with FT232R preservation:

**Changes**:
1. Implement libusb acquisition for MPSSE chips
2. Keep libftdi acquisition for FT232R
3. Add mode detection
4. Add multi-channel support
5. Improve buffer management
6. Add sample rate validation
7. Defer hardware configuration

**Effort**: 40-60 hours
**Risk**: Very High
**Benefit**: All improvements, modern architecture

## Detailed Implementation: Option 1 (Minimal Hybrid)

### Step 1: Add Sample Rate Validation (Patch 7)

**File**: `src/hardware/ftdi-la/protocol.c`

Add validation function:
```c
static int validate_samplerate(const struct ftdi_chip_desc *desc, 
                               uint64_t requested_rate)
{
    uint64_t base_clock;
    uint64_t min_rate, max_rate;
    
    /* FT232R uses different clock */
    if (desc->product == 0x6001) {
        base_clock = 48000000; /* 48MHz */
    } else {
        base_clock = 120000000; /* 120MHz for MPSSE chips */
    }
    
    /* Calculate achievable range */
    min_rate = base_clock / 65536; /* Max divisor */
    max_rate = base_clock / desc->samplerate_div;
    
    if (requested_rate < min_rate || requested_rate > max_rate) {
        otc_err("Sample rate %" PRIu64 " out of range (%" PRIu64 
                " - %" PRIu64 ").", requested_rate, min_rate, max_rate);
        return OTC_ERR_ARG;
    }
    
    /* Check if rate is achievable with integer divisor */
    uint64_t divisor = base_clock / requested_rate;
    uint64_t actual_rate = base_clock / divisor;
    
    if (actual_rate != requested_rate) {
        otc_warn("Requested rate %" PRIu64 " adjusted to %" PRIu64 ".",
                requested_rate, actual_rate);
    }
    
    return OTC_OK;
}
```

**File**: `src/hardware/ftdi-la/api.c`

Update `config_set()`:
```c
case OTC_CONF_SAMPLERATE:
    value_f = g_variant_get_double(data);
    
    /* Validate before setting */
    if (validate_samplerate(devc->desc, (uint64_t)value_f) != OTC_OK)
        return OTC_ERR_ARG;
    
    devc->cur_samplerate = (uint64_t)value_f;
    return OTC_OK;
```

### Step 2: Defer Sample Rate Configuration (Patch 9)

**File**: `src/hardware/ftdi-la/protocol.h`

Update `dev_context`:
```c
struct dev_context {
    struct ftdi_context *ftdic;
    const struct ftdi_chip_desc *desc;

    uint64_t limit_samples;
    uint64_t requested_samplerate;  /* NEW: Store requested rate */
    uint32_t cur_samplerate;        /* Actual configured rate */

    unsigned char *data_buf;
    uint64_t samples_sent;
    uint64_t bytes_received;
};
```

**File**: `src/hardware/ftdi-la/api.c`

Update `config_set()`:
```c
case OTC_CONF_SAMPLERATE:
    value_f = g_variant_get_double(data);
    
    /* Validate but don't configure hardware yet */
    if (validate_samplerate(devc->desc, (uint64_t)value_f) != OTC_ERR_ARG)
        return OTC_ERR_ARG;
    
    devc->requested_samplerate = (uint64_t)value_f;
    return OTC_OK;  /* Don't call ftdi_la_set_samplerate() here */
```

Update `dev_acquisition_start()`:
```c
static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    int ret;
    
    /* NOW configure the hardware with requested rate */
    devc->cur_samplerate = devc->requested_samplerate;
    ret = ftdi_la_set_samplerate(devc);
    if (ret != OTC_OK)
        return ret;
    
    /* Rest of acquisition start code... */
    ...
}
```

Add default rate in `scan_device()`:
```c
/* Set default sample rate */
devc->requested_samplerate = OTC_MHZ(1);  /* 1MHz default */
devc->cur_samplerate = 0;  /* Not configured yet */
```

### Step 3: Update Sample Rate List (Patch 7)

**File**: `src/hardware/ftdi-la/api.c`

Make sample rates dynamic:
```c
static int config_list(uint32_t key, GVariant **data,
    const struct otc_dev_inst *sdi, const struct otc_channel_group *cg)
{
    struct dev_context *devc;
    uint64_t base_clock, min_rate, max_rate;
    
    switch (key) {
    case OTC_CONF_SAMPLERATE:
        if (!sdi || !sdi->priv)
            return OTC_ERR_ARG;
        
        devc = sdi->priv;
        
        /* Calculate valid range for this chip */
        if (devc->desc->product == 0x6001) {
            base_clock = 48000000;
        } else {
            base_clock = 120000000;
        }
        
        min_rate = base_clock / 65536;
        max_rate = base_clock / devc->desc->samplerate_div;
        
        *data = std_gvar_samplerate_steps(
            (const uint64_t[]){min_rate, max_rate, 1});
        break;
        
    /* ... rest of config_list ... */
    }
    
    return OTC_OK;
}
```

## Testing Plan

### Test 1: Sample Rate Validation
```bash
# Try invalid sample rates
sigrok-cli -d ftdi-la --config samplerate=999999999
# Should error: "Sample rate out of range"

# Try valid sample rate
sigrok-cli -d ftdi-la --config samplerate=1000000
# Should succeed
```

### Test 2: Deferred Configuration
```bash
# Set rate multiple times before capture
sigrok-cli -d ftdi-la --config samplerate=100000
sigrok-cli -d ftdi-la --config samplerate=500000
sigrok-cli -d ftdi-la --config samplerate=1000000
# Start capture - should use 1MHz
sigrok-cli -d ftdi-la --samples 1000 -o test.sr
```

### Test 3: Default Rate
```bash
# Start capture without setting rate
sigrok-cli -d ftdi-la --samples 1000 -o test.sr
# Should use default 1MHz, not crash
```

### Test 4: FT232R Still Works
```bash
# With FT232R device
sigrok-cli -d ftdi-la --scan
sigrok-cli -d ftdi-la --samples 1000 -o test.sr
# Should work as before
```

## Full Rewrite Implementation: Option 2

### Architecture

```
┌─────────────────────────────────────┐
│         ftdi-la Driver              │
├─────────────────────────────────────┤
│  Device Detection & Configuration   │
├──────────────┬──────────────────────┤
│  MPSSE Mode  │   Bitbang Mode       │
│  (libusb)    │   (libftdi)          │
├──────────────┼──────────────────────┤
│  FT232H      │   FT232R             │
│  FT2232H     │                      │
│  FT4232H     │                      │
└──────────────┴──────────────────────┘
```

### Required Files

**New**: `src/hardware/ftdi-la/mpsse.c` (~800 lines)
- libusb-based MPSSE acquisition
- USB transfer management
- Buffer handling

**New**: `src/hardware/ftdi-la/bitbang.c` (~200 lines)
- libftdi-based bitbang acquisition
- FT232R support

**Modified**: `src/hardware/ftdi-la/api.c`
- Mode detection
- Dual-path routing

**Modified**: `src/hardware/ftdi-la/protocol.h`
- New structures for libusb
- Mode enum

### Key Code Sections

#### Mode Detection
```c
enum ftdi_mode {
    FTDI_MODE_BITBANG,  /* FT232R */
    FTDI_MODE_MPSSE,    /* FT232H, FT2232H, FT4232H */
};

static enum ftdi_mode detect_mode(const struct ftdi_chip_desc *desc)
{
    if (desc->product == 0x6001)  /* FT232R */
        return FTDI_MODE_BITBANG;
    else
        return FTDI_MODE_MPSSE;
}
```

#### Acquisition Routing
```c
static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    
    if (devc->mode == FTDI_MODE_BITBANG)
        return ftdi_la_start_acquisition_bitbang(sdi);
    else
        return ftdi_la_start_acquisition_mpsse(sdi);
}
```

## Estimated Effort

### Option 1: Minimal Hybrid
- Implementation: 4-6 hours
- Testing: 2-3 hours
- Documentation: 1 hour
- **Total: 7-10 hours**

### Option 2: Full Rewrite
- MPSSE implementation: 20-25 hours
- Bitbang preservation: 5-8 hours
- Integration: 8-10 hours
- Testing: 8-12 hours
- Documentation: 2-3 hours
- **Total: 43-58 hours**

## Recommendation

**Implement Option 1 (Minimal Hybrid)** because:
1. ✅ Fixes critical bugs (clock divider already done)
2. ✅ Adds important validation
3. ✅ Improves reliability (deferred config)
4. ✅ Preserves FT232R support
5. ✅ Low risk, manageable effort
6. ✅ Can be done incrementally

**Defer Option 2 (Full Rewrite)** because:
1. ❌ Very high complexity
2. ❌ Requires extensive hardware testing
3. ❌ High risk of regressions
4. ❌ Needs dedicated project time
5. ❌ Should be separate driver (ftdi-la-ng)

## Next Steps

1. **Immediate**: Implement Option 1 validation and deferred config
2. **Short-term**: Test with all supported hardware
3. **Long-term**: Plan Option 2 as dedicated project if needed

## Files to Modify (Option 1)

1. `src/hardware/ftdi-la/protocol.h` - Add requested_samplerate field
2. `src/hardware/ftdi-la/protocol.c` - Add validate_samplerate()
3. `src/hardware/ftdi-la/api.c` - Update config_set(), config_list(), dev_acquisition_start()

## Conclusion

Option 1 provides 80% of the benefit with 20% of the effort. It addresses the critical issues while maintaining stability and FT232R support. Option 2 should only be pursued if there's a compelling need for the additional features and dedicated resources for proper implementation and testing.
