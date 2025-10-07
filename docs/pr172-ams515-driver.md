# PR #172 - Francaise Instrumentation AMS515 Driver

## Overview
libsigrok PR #172 contains two parts:
1. **SWIG 4.2 fix** - ✅ **Ported** (commit 68178747)
2. **AMS515 driver** - ⏸️ **Not ported** (commits 2-18)

## What Was Ported

### SWIG 4.2 Compatibility Fix ✅
**File**: `bindings/cxx/enums.py`

**Problem**: SWIG 4.2+ requires `attribute.i` to be included before using `%attribute` directive.

**Solution**: Modified the Python code generator to:
1. Add generated file comment to SWIG interface file
2. Include `%include "attribute.i"` in the generated SWIG file

**Python Code Explanation**:
```python
# Before (broken with SWIG 4.2+):
for file in (header, code):
    print("/* Generated file - edit enums.py instead! */", file=file)

# After (works with all SWIG versions):
for file in (header, code, swig):  # Now includes swig file
    print("/* Generated file - edit enums.py instead! */", file=file)

print('%include "attribute.i"', file=swig)  # Add required include
```

The `enums.py` script is a **code generator** that creates three files:
- `enums.hpp` - C++ header with enum classes
- `enums.cpp` - C++ implementation
- `enums.i` - SWIG interface for language bindings

## What Was Not Ported

### Francaise Instrumentation AMS515 Driver
**Commits**: 2-18 (17 commits)  
**Lines**: ~2,800 lines of new code

#### Device Information
- **Manufacturer**: Francaise Instrumentation
- **Model**: AMS515
- **Type**: DC Power Supply
- **Interface**: Serial (RS-232)
- **Channels**: 1 output channel
- **Features**:
  - Voltage control and measurement
  - Current control and measurement
  - Over-current protection (OCP)
  - Output enable/disable

#### Driver Structure
New files that would be created:
```
src/hardware/francaise-instrumentation-ams515/
├── api.c          (~400 lines)
├── protocol.c     (~500 lines)
└── protocol.h     (~100 lines)
```

#### Key Features Implemented
1. **Basic Operations**:
   - Set/get voltage
   - Set/get current limit
   - Enable/disable output
   - Query status

2. **Over-Current Protection**:
   - Poll for OC events
   - Report OC status
   - Re-enable OCP after clearing

3. **Serial Communication**:
   - Echo control
   - Command/response handling
   - Status queries

4. **SCPI-like Commands**:
   - `V` - Set voltage
   - `I` - Set current
   - `M` - Query measurements
   - `O` - Output control
   - `E` - Echo control

#### Why Not Ported

1. **No Hardware Available**: Cannot test without actual AMS515 device
2. **Niche Device**: Limited user base
3. **Complete New Driver**: Requires full implementation and testing
4. **No User Demand**: No requests for this specific device

#### Code Complexity
- **Low complexity**: Standard power supply driver pattern
- **Well structured**: Clean separation of API and protocol
- **Good documentation**: Comments explain protocol details

## Porting Status Summary

| Component | Status | Reason |
|-----------|--------|--------|
| SWIG 4.2 fix | ✅ Ported | Essential for build compatibility |
| AMS515 driver | ⏸️ Deferred | No hardware, no demand |

## If You Need AMS515 Support

The driver can be ported if needed. Steps would be:

1. **Create driver directory**:
   ```bash
   mkdir -p src/hardware/francaise-instrumentation-ams515
   ```

2. **Extract files from PR #172**:
   - api.c (commits 2-18)
   - protocol.c (commits 2-18)
   - protocol.h (commits 2-18)

3. **Convert SR_ to OTC_ prefixes**:
   ```bash
   sed -i 's/SR_/OTC_/g' *.c *.h
   sed -i 's/sr_/otc_/g' *.c *.h
   ```

4. **Add to build system**:
   - Update `src/drivers/meson.build`
   - Add to always_available drivers list

5. **Test with actual hardware**

## References

- **PR #172**: https://github.com/sigrokproject/libsigrok/pull/172
- **Patch File**: `/tmp/pr172.patch` (3242 lines)
- **Device Info**: Francaise Instrumentation AMS515 DC Power Supply
- **Protocol**: Serial RS-232, SCPI-like commands

## Related Commits

- **68178747**: SWIG 4.2 compatibility fix (ported)
- **Commits 2-18**: AMS515 driver implementation (not ported)

---

**Last Updated**: 2025-10-07  
**SWIG Fix**: ✅ Ported and working  
**AMS515 Driver**: ⏸️ Available if needed  
**Maintainer**: OpenTraceCapture team
