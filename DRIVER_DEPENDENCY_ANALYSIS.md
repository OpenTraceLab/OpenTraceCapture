# Driver Dependency Analysis: libsigrok vs OpenTraceCapture

## Dependency Categories from Original libsigrok

### 1. No dependencies (always available)
- demo
- asix-omega-rtm-cli
- fluke-45
- gwinstek-psp
- hameg-hmo
- hp-3457a
- hp-59306a
- ipdbg-la
- lecroy-xstream
- maynuo-m97
- rigol-dg
- rigol-ds
- scpi-dmm
- scpi-pps
- siglent-sdl10x0
- siglent-sds
- yokogawa-dlm

### 2. libusb only
- dreamsourcelab-dslogic
- fx2lafw
- greatfet
- hantek-4032l
- hantek-6xxx
- hantek-dso
- ikalogic-scanalogic2
- kecheng-kc-330b
- kingst-la2016
- lascar-el-usb
- lecroy-logicstudio
- microchip-pickit2
- saleae-logic-pro
- saleae-logic16
- sysclk-lwla
- sysclk-sla5032
- testo
- uni-t-dmm
- zeroplus-logic-cube

### 3. libusb + libftdi
- chronovu-la
- ftdi-la

### 4. libftdi only
- asix-sigma
- ikalogic-scanaplus
- pipistrello-ols

### 5. serial_comm (generic serial)
- agilent-dmm
- appa-55ii
- arachnid-labs-re-load-pro
- atorch
- atten-pps3xxx
- bkprecision-1856d
- cem-dt-885x
- center-3xx
- colead-slm
- conrad-digi-35-cpu
- devantech-eth008
- fluke-dmm
- gmc-mh-1x-2x
- gwinstek-gds-800
- gwinstek-gpd
- icstation-usbrelay
- itech-it8500
- juntek-jds6600
- kern-scale
- korad-kaxxxxp
- manson-hcs-3xxx
- mastech-ms6514
- mic-985xx
- motech-lps-30x
- norma-dmm
- openbench-logic-sniffer
- pce-322a
- raspberrypi-pico
- rdtech-dps
- rdtech-um
- rohde-schwarz-sme-0x
- serial-dmm
- serial-lcr
- teleinfo
- tondaj-sl-814
- uni-t-ut181a
- uni-t-ut32x
- zketech-ebd-usb

### 6. libserialport (specific serial port library)
- link-mso19

### 7. serial_comm + libnettle
- rdtech-tc

### 8. libhidapi
- dcttech-usbrelay

### 9. libieee1284 (parallel port)
- hung-chang-dso-2100

### 10. libgpib
- hp-3478a

### 11. sys_timerfd_h (Linux-specific)
- baylibre-acme

### 12. sys_mman_h + sys_ioctl_h (platform-specific)
- beaglelogic

### 13. bluetooth_comm + libgio
- mooshimeter-dmm

### 14. Special/Unknown
- labjack-u12 (no deps listed, likely needs vendor SDK)

## Current OpenTraceCapture Implementation Status

### ✅ Correctly Implemented
1. **libftdi1-dependent** (FIXED):
   - chronovu-la ✅
   - asix-sigma ✅
   - ikalogic-scanaplus ✅
   - pipistrello-ols ✅

2. **libserialport-dependent**:
   - link-mso19 ✅
   - uni-t-ut181a ✅
   - appa-55ii ✅
   - gwinstek-psp ✅
   - itech-it8500 ✅
   - saleae-logic-pro ✅

3. **Linux-specific**:
   - baylibre-acme ✅

4. **nettle-dependent**:
   - rdtech-tc ✅

5. **ieee1284-dependent**:
   - hung-chang-dso-2100 ✅

6. **gpib-dependent**:
   - lecroy-xstream ✅
   - rohde-schwarz-hmo ✅ (NOTE: Original has no deps, but meson has gpib)
   - tektronix-tds ✅ (not in original list)
   - yokogawa-dlm ✅ (NOTE: Original has no deps, but meson has gpib)

7. **hidapi-dependent**:
   - saleae-logic-pro ✅

8. **Platform-specific (disabled)**:
   - beaglelogic ✅
   - ipdbg-la ✅

9. **Bluetooth (disabled)**:
   - mooshimeter-dmm ✅

### ⚠️ Potential Issues to Investigate

1. **ftdi-la driver**: Present in original libsigrok but NOT in OpenTraceCapture
   - Requires: libusb + libftdi
   - Status: MISSING from OpenTraceCapture

2. **Drivers in original but not in OpenTraceCapture**:
   - ftdi-la
   - greatfet
   - labjack-u12
   - lecroy-logicstudio
   - sysclk-lwla
   - sysclk-sla5032
   - testo
   - uni-t-dmm
   - zeroplus-logic-cube
   - fluke-45
   - fluke-dmm
   - gwinstek-gds-800
   - maynuo-m97
   - rohde-schwarz-sme-0x
   - scpi-dmm
   - scpi-pps
   - siglent-sdl10x0
   - dcttech-usbrelay

3. **Drivers in OpenTraceCapture but not in original**:
   - rdtech-dps (in original as rdtech-dps)
   - rdtech-um (in original as rdtech-um)
   - rdtech-tc (in original as rdtech-tc)
   - All appear to be present in original

4. **Dependency mismatches**:
   - **rohde-schwarz-hmo**: Original has NO deps, OpenTraceCapture requires gpib
   - **yokogawa-dlm**: Original has NO deps, OpenTraceCapture requires gpib
   - **hameg-hmo**: Original has NO deps, OpenTraceCapture in always_available ✅

### 🔍 Drivers Needing Verification

Check if these are correctly in "always_available" or need conditions:

1. **hameg-hmo** - Original: no deps, Current: always_available ✅
2. **rigol-ds** - Original: no deps, Current: always_available ✅
3. **siglent-sds** - Original: no deps, Current: always_available ✅
4. **rigol-dg** - Original: no deps, Current: always_available ✅
5. **hp-59306a** - Original: no deps, Current: always_available ✅

## Recommendations

1. ✅ **DONE**: Move libftdi1-dependent drivers to conditional section
2. **TODO**: Investigate missing drivers (ftdi-la, greatfet, etc.) - were they intentionally excluded?
3. **TODO**: Verify rohde-schwarz-hmo and yokogawa-dlm gpib dependency - is this correct or should they be always available?
4. **TODO**: Consider if dcttech-usbrelay should be added (needs libhidapi)
5. **TODO**: Document which drivers were intentionally excluded and why
