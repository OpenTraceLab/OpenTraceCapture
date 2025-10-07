# PR #145 Full Port Roadmap

## Executive Summary
This document outlines the complete porting strategy for libsigrok PR #145 to OpenTraceCapture, including FT232R support preservation.

## Current Status
✅ **Phase 0 Complete**: Critical timing bug fixed (FT232H clock divider)

## Phased Approach

### Phase 1: Sample Rate Validation (2-3 hours)
**Goal**: Prevent invalid sample rate configurations

**Changes Required**:
1. Add sample rate validation in `config_set()`
2. Calculate valid rates based on chip type
3. Return error for unsupported rates

**Files**:
- `src/hardware/ftdi-la/api.c`
- `src/hardware/ftdi-la/protocol.c`

**Code Additions** (~50 lines):
```c
static int validate_samplerate(const struct ftdi_chip_desc *desc, uint64_t rate)
{
    uint64_t base_clock = 60000000; // 60MHz for MPSSE
    uint64_t min_rate = base_clock / 65536; // Max divisor
    uint64_t max_rate = base_clock / desc->samplerate_div;
    
    if (rate < min_rate || rate > max_rate)
        return OTC_ERR_ARG;
    
    // Check if rate is achievable with integer divisor
    uint64_t divisor = base_clock / rate;
    if (base_clock / divisor != rate)
        return OTC_ERR_ARG;
    
    return OTC_OK;
}
```

**Testing**:
- Try invalid sample rates
- Verify error messages
- Test boundary conditions

### Phase 2: libusb-Based Scanning (4-6 hours)
**Goal**: Remove libftdi dependency from device enumeration

**Changes Required**:
1. Rewrite `scan()` to use libusb directly
2. Remove `ftdi_context` from scanning
3. Keep libftdi for FT232R acquisition

**Files**:
- `src/hardware/ftdi-la/api.c`

**Code Changes** (~100 lines):
```c
static GSList *scan(struct otc_dev_driver *di, GSList *options)
{
    struct libusb_device **devlist;
    struct libusb_context *usb_ctx;
    GSList *devices = NULL;
    
    libusb_init(&usb_ctx);
    libusb_get_device_list(usb_ctx, &devlist);
    
    for (size_t i = 0; devlist[i]; i++) {
        scan_device_usb(devlist[i], &devices);
    }
    
    libusb_free_device_list(devlist, 1);
    libusb_exit(usb_ctx);
    
    return devices;
}
```

**Testing**:
- Verify all chip types detected
- Check USB descriptor parsing
- Test with multiple devices

### Phase 3: Dual-Mode Architecture (8-12 hours)
**Goal**: Support both MPSSE (FT232H/FT2232H/FT4232H) and bitbang (FT232R) modes

**Changes Required**:
1. Add mode detection in device context
2. Create separate acquisition paths
3. Implement mode-specific functions

**Files**:
- `src/hardware/ftdi-la/protocol.h` (add mode enum)
- `src/hardware/ftdi-la/protocol.c` (dual paths)
- `src/hardware/ftdi-la/api.c` (mode selection)

**New Structures**:
```c
enum ftdi_chip_mode {
    FTDI_MODE_BITBANG,  // FT232R
    FTDI_MODE_MPSSE,    // FT232H, FT2232H, FT4232H
};

struct ftdi_chip_desc {
    uint16_t vendor;
    uint16_t product;
    enum ftdi_chip_mode mode;
    int samplerate_div;
    int max_channels;
    char *channel_names[];
};
```

**Acquisition Paths**:
```c
static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    
    if (devc->desc->mode == FTDI_MODE_BITBANG)
        return start_acquisition_bitbang(sdi);
    else
        return start_acquisition_mpsse(sdi);
}
```

**Testing**:
- Test FT232R (bitbang mode)
- Test FT232H (MPSSE mode)
- Verify mode detection
- Test mode switching

### Phase 4: libusb-Based MPSSE Acquisition (12-16 hours)
**Goal**: Rewrite MPSSE acquisition using libusb directly

**Changes Required**:
1. Remove libftdi from MPSSE path
2. Implement direct USB bulk transfers
3. Add USB event handling
4. Improve buffer management

**Files**:
- `src/hardware/ftdi-la/protocol.c` (major rewrite)
- `src/hardware/ftdi-la/protocol.h` (new structures)

**New Code** (~400 lines):
```c
struct usb_transfer_context {
    struct libusb_transfer *transfer;
    struct dev_context *devc;
    uint8_t *buffer;
    size_t buffer_size;
};

static void LIBUSB_CALL transfer_callback(struct libusb_transfer *transfer)
{
    struct usb_transfer_context *ctx = transfer->user_data;
    
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        process_samples(ctx->devc, transfer->buffer, 
                       transfer->actual_length);
        
        // Resubmit transfer
        libusb_submit_transfer(transfer);
    }
}

static int start_acquisition_mpsse(const struct otc_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    
    // Configure MPSSE mode
    configure_mpsse(devc);
    
    // Allocate transfer buffers
    allocate_transfers(devc);
    
    // Submit initial transfers
    submit_transfers(devc);
    
    // Add USB event source
    otc_session_source_add_usb(sdi->session, devc->usb_ctx,
                               handle_usb_events, sdi);
    
    return OTC_OK;
}
```

**Testing**:
- Long captures (buffer handling)
- High sample rates
- USB disconnect during capture
- Multiple simultaneous captures

### Phase 5: Multi-Channel Support (4-6 hours)
**Goal**: Support all channels on FT4232H (32 channels total)

**Changes Required**:
1. Expand channel arrays
2. Add BDBUS, CDBUS, DDBUS support
3. Update channel configuration

**Files**:
- `src/hardware/ftdi-la/api.c`
- `src/hardware/ftdi-la/protocol.h`

**Code Changes**:
```c
static const struct ftdi_chip_desc ft4232h_desc = {
    .vendor = 0x0403,
    .product = 0x6011,
    .mode = FTDI_MODE_MPSSE,
    .samplerate_div = 20,
    .max_channels = 32,
    .channel_names = {
        // ADBUS (Interface A)
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
        // BDBUS (Interface B)
        "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
        // CDBUS (Interface C)
        "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7",
        // DDBUS (Interface D)
        "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
        NULL
    }
};
```

**Testing**:
- Test all 32 channels
- Verify channel naming
- Test partial channel selection

### Phase 6: Dynamic Sample Rate Configuration (2-3 hours)
**Goal**: Configure sample rate at acquisition time, not at set time

**Changes Required**:
1. Store requested rate in device context
2. Apply rate in `dev_acquisition_start()`
3. Validate at both set and start time

**Files**:
- `src/hardware/ftdi-la/api.c`
- `src/hardware/ftdi-la/protocol.c`

**Code Changes**:
```c
static int config_set(uint32_t key, GVariant *data, ...)
{
    case OTC_CONF_SAMPLERATE:
        devc->requested_samplerate = g_variant_get_uint64(data);
        // Don't configure hardware yet
        return OTC_OK;
}

static int dev_acquisition_start(const struct otc_dev_inst *sdi)
{
    // Now configure the hardware
    return configure_samplerate(sdi, devc->requested_samplerate);
}
```

**Testing**:
- Change rate between captures
- Verify rate applied correctly
- Test error handling

## Complete Implementation Timeline

### Conservative Estimate
- **Phase 1**: 3 hours
- **Phase 2**: 6 hours
- **Phase 3**: 12 hours
- **Phase 4**: 16 hours
- **Phase 5**: 6 hours
- **Phase 6**: 3 hours
- **Testing**: 8 hours
- **Documentation**: 2 hours
- **Buffer**: 4 hours

**Total**: ~60 hours (1.5 weeks full-time)

### Aggressive Estimate
- **Phases 1-6**: 30 hours
- **Testing**: 4 hours
- **Documentation**: 1 hour
- **Buffer**: 2 hours

**Total**: ~37 hours (1 week full-time)

## Risk Assessment

### High Risk Items
1. **USB event handling** - Complex, easy to get wrong
2. **Buffer management** - Can cause dropped samples
3. **FT232R compatibility** - Dual-mode complexity
4. **Hardware availability** - Need all chip types for testing

### Medium Risk Items
1. **Sample rate calculation** - Math errors possible
2. **Channel configuration** - Off-by-one errors
3. **USB disconnect handling** - Edge case testing

### Low Risk Items
1. **Sample rate validation** - Straightforward logic
2. **Channel naming** - Simple string arrays
3. **Configuration storage** - Standard patterns

## Testing Strategy

### Unit Testing
- Sample rate validation logic
- Divisor calculations
- Channel mapping
- Mode detection

### Integration Testing
- Device scanning
- Configuration changes
- Acquisition start/stop
- Data transfer

### Hardware Testing
Required hardware:
- ✅ FT232R (bitbang mode)
- ✅ FT232H (MPSSE mode, 8 channels)
- ✅ FT2232H (MPSSE mode, 8 channels)
- ✅ FT4232H (MPSSE mode, 32 channels)
- ✅ TUMPA (FT2232H variant)

Test scenarios:
1. Basic capture (all devices)
2. High sample rate (max for each device)
3. Long capture (buffer stress test)
4. USB disconnect during capture
5. Multiple devices simultaneously
6. All channels (FT4232H)
7. Sample rate changes between captures

### Regression Testing
- Verify existing functionality still works
- Test with existing capture files
- Validate timing accuracy
- Check resource cleanup

## Rollout Strategy

### Option A: Big Bang
- Implement all phases
- Test thoroughly
- Release as major update
- **Risk**: High
- **Timeline**: 6-8 weeks

### Option B: Incremental
- Release Phase 1 immediately (validation)
- Release Phase 2-3 together (dual-mode)
- Release Phase 4-6 together (full rewrite)
- **Risk**: Medium
- **Timeline**: 8-12 weeks

### Option C: Parallel Driver (Recommended)
- Create `ftdi-la-ng` (next generation)
- Keep `ftdi-la` as fallback
- Gradual migration
- **Risk**: Low
- **Timeline**: 8-12 weeks

## Success Criteria

### Must Have
- ✅ FT232H timing accurate (already done)
- ✅ FT232R support preserved
- ✅ No regressions in existing functionality
- ✅ All chip types work

### Should Have
- ✅ Sample rate validation
- ✅ Multi-channel support (FT4232H)
- ✅ Improved buffer management
- ✅ Better error messages

### Nice to Have
- ✅ 15MHz sample rate on FT232H
- ✅ USB disconnect handling
- ✅ Performance improvements
- ✅ Reduced CPU usage

## Conclusion

The full port of PR #145 is feasible but requires significant effort. The phased approach allows for incremental progress with testing at each stage. The parallel driver strategy (Option C) provides the safest rollout path.

**Current Status**: Phase 0 complete (critical fix applied)

**Recommended Next Step**: Implement Phase 1 (sample rate validation) as low-risk improvement

**Long-term Goal**: Complete Phases 2-6 as dedicated project with proper testing
