# Contributing to CLASP

Thank you for your interest in contributing to CLASP! We welcome all contributions, from bug reports and feature suggestions to code changes and documentation improvements.

## 🚀 How to Contribute

### Reporting Bugs
- Use the [GitHub Issue Tracker](https://github.com/dflowenfels/clasp/issues).
- Provide a clear and descriptive title.
- Include steps to reproduce the issue, your environment (OS, DAW, CLASP version), and any relevant logs or screenshots.

### Suggesting Features
- Open a new issue with the "feature request" label.
- Describe the feature you'd like to see and why it would be useful.

### Pull Requests
1. **Fork the repo** and create your branch from `main`.
2. **Setup your environment** following the [README.md](README.md).
3. **If you've added code** that should be tested, add tests!
4. **Update the documentation** if you've changed logic or added new features.
5. **Ensure the build passes** for your changes.
6. **Submit the PR** with a clear description of the changes and link to any related issues.

## 🛠 Development Setup

CLASP is built using CMake and requires a C++17 compiler.

```bash
# Clone with submodules
git clone --recursive https://github.com/dflowenfels/clasp.git
cd clasp

# Build in debug mode
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

See the [README.md](README.md) for detailed build instructions and platform-specific requirements.

## ⚖️ Code of Conduct

Please be respectful and professional in all interactions within this project. We follow standard open-source community norms.

## 📜 License

By contributing to CLASP, you agree that your contributions will be licensed under the project's [MIT License](LICENSE).
