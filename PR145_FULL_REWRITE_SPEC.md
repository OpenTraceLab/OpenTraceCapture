# PR #145 Full Rewrite Specification

## Executive Summary

This document provides a complete specification for porting patches 4-7 and 9 from libsigrok PR #145 to OpenTraceCapture, implementing a dual-mode FTDI logic analyzer driver that preserves FT232R support.

## Project Scope

### Objectives
1. Implement libusb-based acquisition for MPSSE-capable chips (FT232H, FT2232H, FT4232H)
2. Preserve libftdi-based acquisition for FT232R (bitbang mode)
3. Add multi-channel support (up to 32 channels on FT4232H)
4. Improve buffer management to prevent dropped samples
5. Add sample rate validation
6. Defer hardware configuration to acquisition time

### Non-Objectives
- Rewriting FT232R support (keep existing implementation)
- Supporting MPSSE mode on FT232R (hardware limitation)
- Backward compatibility with old capture files (format unchanged)

## Technical Architecture

### Dual-Mode Design

```
┌──────────────────────────────────────────────────┐
│              FTDI-LA Driver Core                 │
│  (Device Detection, Configuration, API)          │
└────────────────┬─────────────────────────────────┘
                 │
        ┌────────┴────────┐
        │                 │
┌───────▼──────┐  ┌──────▼────────┐
│ MPSSE Path   │  │ Bitbang Path  │
│ (libusb)     │  │ (libftdi)     │
├──────────────┤  ├───────────────┤
│ • FT232H     │  │ • FT232R      │
│ • FT2232H    │  │               │
│ • FT4232H    │  │               │
│ • TUMPA      │  │               │
└──────────────┘  └───────────────┘
```

### Mode Detection Logic

```c
enum ftdi_chip_mode {
    FTDI_MODE_BITBANG,  /* FT232R: 48MHz clock, bitbang protocol */
    FTDI_MODE_MPSSE,    /* Others: 120MHz clock, MPSSE protocol */
};

static enum ftdi_chip_mode get_chip_mode(uint16_t product_id)
{
    return (product_id == 0x6001) ? FTDI_MODE_BITBANG : FTDI_MODE_MPSSE;
}
```

## File Structure

### New Files

**src/hardware/ftdi-la/mpsse.c** (~800 lines)
- MPSSE mode implementation
- libusb-based acquisition
- USB transfer management
- Multi-channel support

**src/hardware/ftdi-la/mpsse.h** (~100 lines)
- MPSSE mode structures
- Function declarations

**src/hardware/ftdi-la/bitbang.c** (~200 lines)
- Bitbang mode implementation
- libftdi-based acquisition (existing code refactored)
- FT232R support

**src/hardware/ftdi-la/bitbang.h** (~50 lines)
- Bitbang mode structures
- Function declarations

### Modified Files

**src/hardware/ftdi-la/api.c**
- Add mode detection
- Route to appropriate implementation
- Update device scanning
- Add multi-channel support

**src/hardware/ftdi-la/protocol.h**
- Add mode enum
- Add MPSSE structures
- Update device context

**src/hardware/ftdi-la/protocol.c**
- Add sample rate validation
- Add clock configuration
- Common utility functions

## Data Structures

### Updated Chip Descriptor

```c
struct ftdi_chip_desc {
    uint16_t vendor;
    uint16_t product;
    enum ftdi_chip_mode mode;
    
    /* Clock configuration */
    uint32_t base_clock;        /* Base clock frequency in Hz */
    uint32_t bitbang_divisor;   /* Divisor for bitbang mode */
    
    /* Interface configuration */
    gboolean multi_iface;       /* Multiple interfaces available */
    uint8_t num_ifaces;         /* Number of interfaces */
    
    /* Sample rate limits per interface */
    uint64_t max_sample_rates[4];  /* Max rate for each interface */
    
    /* Channel names (8 per interface) */
    char *channel_names[];
};
```

### Updated Device Context

```c
struct dev_context {
    const struct ftdi_chip_desc *desc;
    enum ftdi_chip_mode mode;
    
    /* USB configuration (MPSSE mode) */
    uint8_t usb_iface_idx;      /* USB interface index */
    uint8_t ftdi_iface_idx;     /* FTDI interface index (1-4) */
    uint16_t in_ep_pkt_size;    /* Endpoint packet size */
    uint8_t in_ep_addr;         /* Endpoint address */
    
    /* libftdi context (bitbang mode) */
    struct ftdi_context *ftdic;
    
    /* Sample rate configuration */
    struct clock_config cur_clk;
    uint64_t requested_samplerate;
    
    /* Acquisition state */
    uint64_t limit_samples;
    uint64_t samples_sent;
    gboolean acq_aborted;
    
    /* MPSSE mode: USB transfers */
    struct libusb_transfer **transfers;
    size_t num_transfers;
    unsigned char **transfer_buffers;
    
    /* Bitbang mode: data buffer */
    unsigned char *data_buf;
    uint64_t bytes_received;
};
```

### Clock Configuration

```c
struct clock_config {
    uint64_t rate_millihz;      /* Actual rate in millihertz */
    uint32_t encoded_divisor;   /* Hardware divisor value */
};
```

## Implementation Details

### Phase 1: Core Infrastructure (8-10 hours)

#### 1.1 Update Chip Descriptors

```c
static const struct ftdi_chip_desc ft232h_desc = {
    .vendor = 0x0403,
    .product = 0x6014,
    .mode = FTDI_MODE_MPSSE,
    .base_clock = 120000000u,
    .bitbang_divisor = 2u,
    .multi_iface = TRUE,
    .num_ifaces = 1,
    .max_sample_rates = {6000000, 0, 0, 0},  /* 6MHz max */
    .channel_names = {
        "ADBUS0", "ADBUS1", "ADBUS2", "ADBUS3",
        "ADBUS4", "ADBUS5", "ADBUS6", "ADBUS7",
        NULL
    }
};

static const struct ftdi_chip_desc ft232r_desc = {
    .vendor = 0x0403,
    .product = 0x6001,
    .mode = FTDI_MODE_BITBANG,
    .base_clock = 48000000u,
    .bitbang_divisor = 1u,
    .multi_iface = FALSE,
    .num_ifaces = 1,
    .max_sample_rates = {1600000, 0, 0, 0},  /* 1.6MHz max */
    .channel_names = {
        "TXD", "RXD", "RTS#", "CTS#",
        "DTR#", "DSR#", "DCD#", "RI#",
        NULL
    }
};

static const struct ftdi_chip_desc ft4232h_desc = {
    .vendor = 0x0403,
    .product = 0x6011,
    .mode = FTDI_MODE_MPSSE,
    .base_clock = 120000000u,
    .bitbang_divisor = 2u,
    .multi_iface = TRUE,
    .num_ifaces = 4,
    .max_sample_rates = {6000000, 6000000, 6000000, 6000000},
    .channel_names = {
        /* Interface A */
        "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
        /* Interface B */
        "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
        /* Interface C */
        "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7",
        /* Interface D */
        "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
        NULL
    }
};
```

#### 1.2 Sample Rate Calculation

```c
static struct clock_config calculate_clock_config(
    uint32_t requested_rate,
    const struct ftdi_chip_desc *chip,
    uint8_t iface_idx)
{
    const uint8_t fraction_codes[8] = {0, 3, 2, 4, 1, 5, 6, 7};
    struct clock_config res;
    uint32_t bb_clock, divisor_eighths;
    
    bb_clock = chip->base_clock / chip->bitbang_divisor;
    
    /* Special cases for divisors 0 and 1 */
    if (requested_rate > (bb_clock * 5) / 6) {
        res.rate_millihz = bb_clock * 1000ull;
        res.encoded_divisor = 0;
        return res;
    }
    
    if (requested_rate > bb_clock / 2) {
        res.rate_millihz = (bb_clock * 2 / 3) * 1000ull;
        res.encoded_divisor = 1;
        return res;
    }
    
    /* Calculate divisor with fractional part */
    divisor_eighths = (bb_clock * 8) / requested_rate;
    
    /* Round to nearest 1/8 */
    if (divisor_eighths % 8)
        divisor_eighths = ((divisor_eighths / 8) * 8) + 8;
    
    uint32_t int_part = divisor_eighths / 8;
    uint32_t frac_part = (divisor_eighths % 8) / 2;
    
    res.rate_millihz = ((bb_clock * 8000ull) / divisor_eighths);
    res.encoded_divisor = (int_part << 3) | fraction_codes[frac_part];
    
    return res;
}
```

#### 1.3 Sample Rate Validation

```c
OTC_PRIV int ftdi_la_validate_samplerate(
    const struct otc_dev_inst *sdi,
    uint64_t requested_rate,
    uint8_t iface_idx)
{
    struct dev_context *devc = sdi->priv;
    uint64_t max_rate = devc->desc->max_sample_rates[iface_idx];
    uint64_t min_rate = devc->desc->base_clock / 65536;
    
    if (requested_rate > max_rate) {
        otc_err("Sample rate %" PRIu64 " exceeds maximum %" PRIu64 ".",
                requested_rate, max_rate);
        return OTC_ERR_ARG;
    }
    
    if (requested_rate < min_rate) {
        otc_err("Sample rate %" PRIu64 " below minimum %" PRIu64 ".",
                requested_rate, min_rate);
        return OTC_ERR_ARG;
    }
    
    return OTC_OK;
}
```

### Phase 2: MPSSE Implementation (20-25 hours)

See separate document: PR145_MPSSE_IMPLEMENTATION.md

### Phase 3: Bitbang Preservation (5-8 hours)

See separate document: PR145_BITBANG_IMPLEMENTATION.md

### Phase 4: Integration (8-10 hours)

See separate document: PR145_INTEGRATION.md

## Testing Strategy

### Unit Tests
- Clock configuration calculation
- Sample rate validation
- Divisor encoding
- Channel mapping

### Integration Tests
- Device scanning
- Mode detection
- Configuration changes
- Acquisition start/stop

### Hardware Tests

**Required Hardware**:
- FT232R (bitbang mode)
- FT232H (MPSSE mode, 8 channels)
- FT2232H (MPSSE mode, 16 channels)
- FT4232H (MPSSE mode, 32 channels)

**Test Scenarios**:
1. Basic capture on each device
2. Maximum sample rate
3. Minimum sample rate
4. All channels (FT4232H)
5. Long capture (buffer stress)
6. USB disconnect during capture
7. Multiple rate changes
8. Default rate (no explicit config)

## Risk Mitigation

### High-Risk Items
1. **USB transfer management** - Use proven patterns from fx2lafw
2. **Buffer overflow** - Implement flow control and backpressure
3. **FT232R regression** - Extensive testing before release

### Mitigation Strategies
1. **Phased rollout** - Release as ftdi-la-ng initially
2. **Fallback option** - Keep old driver available
3. **Extensive testing** - Test matrix for all devices
4. **Code review** - Peer review of critical sections

## Timeline

### Conservative Estimate
- Phase 1 (Infrastructure): 10 hours
- Phase 2 (MPSSE): 25 hours
- Phase 3 (Bitbang): 8 hours
- Phase 4 (Integration): 10 hours
- Testing: 12 hours
- Documentation: 3 hours
- Buffer: 6 hours
**Total: 74 hours (~2 weeks)**

### Aggressive Estimate
- Phase 1: 8 hours
- Phase 2: 20 hours
- Phase 3: 5 hours
- Phase 4: 8 hours
- Testing: 6 hours
- Documentation: 2 hours
- Buffer: 3 hours
**Total: 52 hours (~1.5 weeks)**

## Success Criteria

### Must Have
- ✅ All existing functionality preserved
- ✅ FT232R works identically to current driver
- ✅ FT232H timing accurate (already fixed)
- ✅ No sample drops at supported rates
- ✅ Sample rate validation working

### Should Have
- ✅ Multi-channel support on FT4232H
- ✅ Improved buffer management
- ✅ Better error messages
- ✅ USB disconnect handling

### Nice to Have
- ✅ Performance improvements
- ✅ Lower CPU usage
- ✅ Better debugging output

## Deliverables

1. Updated driver code (6 files)
2. Unit tests
3. Integration tests
4. Hardware test results
5. User documentation
6. Migration guide
7. Performance comparison

## Next Steps

1. Review and approve specification
2. Set up development environment
3. Acquire test hardware
4. Implement Phase 1
5. Test Phase 1
6. Proceed to Phase 2

## Appendices

- Appendix A: USB Protocol Details
- Appendix B: MPSSE Command Reference
- Appendix C: Bitbang Protocol
- Appendix D: Test Procedures
- Appendix E: Performance Benchmarks
