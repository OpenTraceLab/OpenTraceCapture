# Contributing to OpenTraceCapture

Thank you for your interest in contributing to OpenTraceCapture!

## Development Setup

### Building from Source

```bash
git clone <repository-url>
cd OpenTraceCapture
meson setup builddir
meson compile -C builddir
```

### Testing Changes

Run the test suite to ensure your changes don't break existing functionality:

```bash
meson test -C builddir
```

## Contribution Guidelines

### Code Style

- Follow the existing code style in the project
- Use consistent indentation and formatting
- Add appropriate comments for complex logic
- Ensure API compatibility where applicable

### Submitting Changes

1. Fork the repository
2. Create a feature branch from main
3. Make your changes with clear, descriptive commit messages
4. Test your changes thoroughly
5. Submit a pull request with a detailed description

### Reporting Issues

Use GitHub issues to report bugs or request features. Please include:

- Clear description of the issue or feature request
- Steps to reproduce (for bugs)
- Expected vs actual behavior
- System information (OS, compiler version, etc.)

## Code of Conduct

This project follows our [Code of Conduct](CODE_OF_CONDUCT.md). Please read and follow it in all interactions.

## License

By contributing to OpenTraceCapture, you agree that your contributions will be licensed under the GNU General Public License (GPL), version 3 or later.
