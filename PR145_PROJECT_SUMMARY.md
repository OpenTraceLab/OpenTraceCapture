# PR #145 Full Rewrite Project Summary

## Current Status

### Completed ✅
1. **Critical Bug Fix**: FT232H clock divider corrected (30 → 20)
   - Fixes 66% timing error
   - One-line change in api.c
   - Compiled and ready

2. **Comprehensive Documentation**:
   - PR145_ANALYSIS.md - Complete patch analysis
   - PR145_PORT_SUMMARY.md - What was ported
   - PR145_FULL_PORT_ROADMAP.md - Implementation roadmap
   - PR145_PATCHES_4-7-9_IMPLEMENTATION.md - Detailed guide
   - PR145_FULL_REWRITE_SPEC.md - Complete specification
   - PR145_MPSSE_IMPLEMENTATION.md - MPSSE mode details

### Pending 🔄
Full implementation of patches 4-7 and 9 requiring:
- 50-75 hours of development
- Extensive hardware testing
- Dual-mode architecture (MPSSE + bitbang)

## Project Scope

### What Needs to Be Done

**Patch 4**: Implement libusb-based acquisition (~400 lines)
- Direct USB bulk transfers
- Asynchronous event handling
- Transfer management

**Patch 5**: Prevent dropped samples
- Improved buffer management
- Flow control
- Backpressure handling

**Patch 6**: Multi-channel support
- Expand from 8 to 32 channels
- Support all interfaces on FT4232H
- Channel mapping

**Patch 7**: Sample rate validation
- Calculate valid rates
- Validate user input
- Provide feedback

**Patch 9**: Defer hardware configuration
- Store requested rate
- Configure at acquisition time
- Improve reliability

### Critical Requirement
**Preserve FT232R support** - requires dual-mode implementation:
- MPSSE mode for FT232H/FT2232H/FT4232H (new code)
- Bitbang mode for FT232R (existing code)

## Architecture

```
┌────────────────────────────────────┐
│       FTDI-LA Driver Core          │
│  (Detection, Config, Validation)   │
└──────────┬─────────────────────────┘
           │
    ┌──────┴──────┐
    │             │
┌───▼────┐  ┌────▼─────┐
│ MPSSE  │  │ Bitbang  │
│(libusb)│  │(libftdi) │
├────────┤  ├──────────┤
│FT232H  │  │ FT232R   │
│FT2232H │  │          │
│FT4232H │  │          │
└────────┘  └──────────┘
```

## File Structure

### New Files (To Be Created)
- `src/hardware/ftdi-la/mpsse.c` (~800 lines)
- `src/hardware/ftdi-la/mpsse.h` (~100 lines)
- `src/hardware/ftdi-la/bitbang.c` (~200 lines)
- `src/hardware/ftdi-la/bitbang.h` (~50 lines)

### Modified Files
- `src/hardware/ftdi-la/api.c` (major changes)
- `src/hardware/ftdi-la/protocol.c` (refactored)
- `src/hardware/ftdi-la/protocol.h` (new structures)

## Implementation Phases

### Phase 1: Infrastructure (8-10 hours)
- Update chip descriptors
- Add clock configuration
- Implement sample rate validation
- Add mode detection

### Phase 2: MPSSE Mode (20-25 hours)
- Implement libusb acquisition
- USB transfer management
- Sample processing
- Buffer management

### Phase 3: Bitbang Mode (5-8 hours)
- Refactor existing code
- Preserve FT232R support
- Maintain compatibility

### Phase 4: Integration (8-10 hours)
- Mode routing
- API updates
- Multi-channel support
- Configuration management

### Phase 5: Testing (12-15 hours)
- Unit tests
- Integration tests
- Hardware testing
- Performance validation

## Resource Requirements

### Development Time
- **Conservative**: 74 hours (~2 weeks full-time)
- **Aggressive**: 52 hours (~1.5 weeks full-time)

### Hardware Required
- FT232R device (bitbang mode testing)
- FT232H device (MPSSE mode, 8 channels)
- FT2232H device (MPSSE mode, 16 channels)
- FT4232H device (MPSSE mode, 32 channels)
- TUMPA device (FT2232H variant)

### Skills Required
- C programming
- USB protocol knowledge
- libusb experience
- FTDI chip familiarity
- OpenTraceCapture architecture

## Risk Assessment

### High Risk
- USB transfer management complexity
- Buffer overflow potential
- FT232R regression
- Hardware availability

### Medium Risk
- Sample rate calculation errors
- Channel mapping bugs
- Timing accuracy

### Low Risk
- Sample rate validation
- Configuration storage
- Mode detection

## Mitigation Strategies

1. **Phased Implementation**: Complete one phase before starting next
2. **Extensive Testing**: Test each component thoroughly
3. **Code Review**: Peer review of critical sections
4. **Fallback Option**: Keep old driver available
5. **Gradual Rollout**: Release as ftdi-la-ng initially

## Success Criteria

### Must Have ✅
- All existing functionality preserved
- FT232R works identically
- FT232H timing accurate
- No sample drops at supported rates
- Sample rate validation working

### Should Have ✅
- Multi-channel support (FT4232H)
- Improved buffer management
- Better error messages
- USB disconnect handling

### Nice to Have ✅
- Performance improvements
- Lower CPU usage
- Better debugging

## Next Steps

### Immediate (Week 1)
1. Review and approve specification
2. Set up development environment
3. Acquire/verify test hardware
4. Implement Phase 1 (infrastructure)
5. Test Phase 1

### Short-term (Week 2)
1. Implement Phase 2 (MPSSE mode)
2. Test MPSSE implementation
3. Implement Phase 3 (bitbang preservation)
4. Test bitbang mode

### Medium-term (Week 3)
1. Implement Phase 4 (integration)
2. Comprehensive testing
3. Performance validation
4. Documentation

### Long-term (Week 4+)
1. Beta testing
2. Bug fixes
3. Performance tuning
4. Release preparation

## Deliverables

1. ✅ Complete specification (DONE)
2. ✅ Implementation guides (DONE)
3. ⏳ Updated driver code (6 files)
4. ⏳ Unit tests
5. ⏳ Integration tests
6. ⏳ Hardware test results
7. ⏳ User documentation
8. ⏳ Migration guide
9. ⏳ Performance comparison

## Decision Points

### Option A: Full Implementation
- Implement all phases
- Complete dual-mode support
- Extensive testing
- **Timeline**: 2-3 weeks
- **Risk**: Medium-High

### Option B: Phased Rollout
- Implement Phase 1 first
- Release incrementally
- Gather feedback
- **Timeline**: 4-6 weeks
- **Risk**: Low-Medium

### Option C: New Driver
- Create ftdi-la-ng
- Keep old driver
- Gradual migration
- **Timeline**: 3-4 weeks
- **Risk**: Low

## Recommendation

**Option C: New Driver (ftdi-la-ng)**

Rationale:
1. Lowest risk - old driver remains available
2. Users can choose which to use
3. Easy rollback if issues found
4. Allows thorough testing
5. Clear migration path

Implementation:
1. Create `src/hardware/ftdi-la-ng/`
2. Implement full dual-mode support
3. Test extensively
4. Document differences
5. Deprecate old driver after 6 months

## Conclusion

The full rewrite is well-specified and feasible. The critical timing bug is already fixed. The full implementation should proceed as a dedicated project with proper planning, testing, and risk mitigation.

**Status**: Ready to proceed with implementation
**Blocker**: None (specification complete)
**Next Action**: Approve specification and begin Phase 1
