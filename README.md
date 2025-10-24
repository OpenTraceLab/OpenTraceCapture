# OpenTraceCapture

OpenTraceCapture is a portable, cross-platform signal analysis library for various hardware devices including logic analyzers, oscilloscopes, and multimeters.

## About

This project provides enhanced trace capture capabilities and improved hardware support for modern signal analysis workflows.

## Build Instructions

```bash
git clone <repository-url>
cd OpenTraceCapture
meson setup builddir
meson compile -C builddir
meson install -C builddir
```

## Requirements

- Meson >= 0.60.0
- gcc (>= 4.0) or clang
- pkg-config >= 0.22
- libglib >= 2.32.0
- libusb >= 1.0.16
- libzip >= 0.10
- Additional optional dependencies (see original README for full list)

## License

OpenTraceCapture is licensed under the GNU General Public License (GPL), version 3 or later.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

## Code of Conduct

This project follows our [Code of Conduct](CODE_OF_CONDUCT.md).

## Original Project

This project is based on libsigrok. See the original README file for detailed information about supported hardware and additional features.
