# PR #146 Port Plan: Aim-TTi DC Power Supply Driver

## Overview
- **PR**: https://github.com/sigrokproject/libsigrok/pull/146
- **Status**: Open (not merged)
- **Author**: Daniel Anselmi
- **Description**: Add support for Aim-TTi DC power supplies (CPX, QPX, MX, QL series)

## Supported Models
- CPX200DP, CPX400SP, CPX400DP (2-channel, 180-420W)
- QPX1200, QPX600DP (1-2 channel, 600-1200W)
- MX100TP, MX180TP, MX100QP (3-channel, 105-125W)
- QL355P (1-channel, 105W)

## Review Comments to Address

### Code Style Issues
1. **Unreadable code** - Use regular code instead of complex macros
2. **Variable declarations** - Declare at top of function
3. **Comment style** - Use `/* */` style, not `//`
4. **Remove debug code** - Take out development debug statements
5. **Simplify conditionals** - Use if-else instead of ternary where it doesn't improve readability
6. **Indentation** - Fix indentation issues
7. **Spelling** - Fix "acquisition" misspelling

### Functional Issues
1. **Track state in memory** - For settings that can be set but not read, track in dev_context
2. **Proper else blocks** - Use `} else {` format

## Port Strategy
1. Create driver skeleton with proper OpenTraceCapture API
2. Implement SCPI communication
3. Add model detection and channel configuration
4. Implement voltage/current control and monitoring
5. Add OVP/OCP support
6. Implement tracking mode for multi-channel models
7. Address all review comments during implementation

## Files to Create
- src/hardware/aim-tti-dps/api.c
- src/hardware/aim-tti-dps/protocol.c
- src/hardware/aim-tti-dps/protocol.h
- src/hardware/aim-tti-dps/meson.build
