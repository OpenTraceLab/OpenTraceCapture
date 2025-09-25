# OpenTraceCapture

OpenTraceCapture is a fork of libsigrok, providing a portable, cross-platform signal analysis library for various hardware devices including logic analyzers, oscilloscopes, and multimeters.

## About

This project extends the original libsigrok functionality with enhanced trace capture capabilities and improved hardware support. OpenTraceCapture maintains compatibility with the libsigrok API while adding new features for modern signal analysis workflows.

## Build Instructions

### Using Meson (Recommended)

```bash
git clone <repository-url>
cd OpenTraceCapture
meson setup builddir
meson compile -C builddir
meson install -C builddir
```


## Requirements

- Meson >= 0.60.0 (for Meson builds)
- gcc (>= 4.0) or clang
- pkg-config >= 0.22
- libglib >= 2.32.0
- libzip >= 0.10
- Additional optional dependencies (see original README for full list)

## License

OpenTraceCapture is licensed under the GNU General Public License (GPL), version 3 or later.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

## Code of Conduct

This project follows our [Code of Conduct](CODE_OF_CONDUCT.md).

## Original Project

This is a fork of [libsigrok](http://sigrok.org/wiki/Libsigrok). See the original README file for detailed information about supported hardware and additional features.
