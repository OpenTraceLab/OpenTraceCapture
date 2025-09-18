# Missing Dependencies for Complete Driver Support

## Summary
- **Total drivers available:** 87
- **Successfully built:** 16 (18%)
- **Missing:** 71 drivers (82%)

## Required Components to Build All Drivers

### 1. SCPI Framework (25+ drivers)
**Missing files:**
- `src/scpi/scpi_tcp.c` - TCP transport
- `src/scpi/scpi_usbtmc_libusb.c` - USB TMC transport  
- `src/scpi/scpi_visa.c` - VISA transport
- `src/scpi/scpi_vxi.c` - VXI-11 transport

**Affected drivers:**
- fluke-45, gwinstek-gds-800, hameg-hmo, hp-3457a, hp-3478a
- lecroy-xstream, rigol-dg, rigol-ds, rohde-schwarz-sme-0x
- scpi-dmm, scpi-pps, siglent-sds, yokogawa-dlm

### 2. Enhanced Serial Framework (35+ drivers)
**Missing functions:**
- `otc_serial_dev_inst_new()` - Serial device creation
- `serial_open()`, `serial_close()` - Connection management
- `serial_read_blocking()`, `serial_write_blocking()` - I/O
- `serial_source_add()` - Event loop integration
- `std_serial_dev_open()` - Standard serial operations

**Affected drivers:**
- agilent-dmm, appa-55ii, atten-pps3xxx, center-3xx, fluke-dmm
- gwinstek-gpd, korad-kaxxxxp, manson-hcs-3xxx, norma-dmm
- openbench-logic-sniffer, serial-dmm, teleinfo, uni-t-dmm

### 3. DMM Parser Library (15+ drivers)
**Missing parsers:**
- `src/dmm/fs9721.c` - Fortune Semiconductor FS9721
- `src/dmm/es519xx.c` - Cyrustek ES519xx
- `src/dmm/metex14.c` - Metex 14-byte protocol
- `src/dmm/rs9lcd.c` - RadioShack 22-168

**Affected drivers:**
- serial-dmm, uni-t-dmm, center-3xx, voltcraft-vc870

### 4. Proprietary Libraries (5+ drivers)
**Missing vendor SDKs:**
- Zeroplus analyzer library (`analyzer_set_freq()`, etc.)
- Sysclk LWLA library
- Lecroy LogicStudio library

**Affected drivers:**
- zeroplus-logic-cube, sysclk-lwla, lecroy-logicstudio

### 5. Network Support (5+ drivers)
**Missing components:**
- TCP client/server framework
- Ethernet device discovery
- Network configuration

**Affected drivers:**
- beaglelogic, ipdbg-la, devantech-eth008

### 6. Bluetooth Support (1 driver)
**Missing:**
- Bluetooth LE framework
- Device pairing/discovery

**Affected drivers:**
- mooshimeter-dmm

### 7. Additional Frameworks
**Scale/LCR support:**
- kern-scale, serial-lcr

**Modbus support:**
- baylibre-acme, maynuo-m97

**HID enhancements:**
- Multiple HID device types

## Implementation Priority

### Phase 1: SCPI Framework (25 drivers)
1. Add TCP transport (`scpi_tcp.c`)
2. Add USB TMC transport (`scpi_usbtmc_libusb.c`)
3. Enable SCPI in core build

### Phase 2: Enhanced Serial (35 drivers)  
1. Implement missing serial functions
2. Add event loop integration
3. Add standard serial device operations

### Phase 3: DMM Parsers (15 drivers)
1. Add common DMM protocol parsers
2. Integrate with serial framework

### Phase 4: Network Support (5 drivers)
1. Add TCP client framework
2. Add device discovery protocols

### Phase 5: Proprietary Libraries (5 drivers)
1. Create vendor library stubs
2. Add dynamic loading support

**Total potential:** All 87 drivers with complete framework implementation.
