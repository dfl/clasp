# CLASP Build Makefile
# Cross-platform build for macOS, Windows, Linux
#
# Usage:
#   make                    # Build for current platform
#   make release            # Build optimized release
#   make debug              # Build with debug symbols
#   make clean              # Clean build artifacts
#   make install            # Install to standard CLAP location
#
# For cross-platform builds on CI/cloud VMs:
#   On macOS:   make release ARCH=universal  (builds x86_64 + arm64)
#   On Windows: make release (use MSVC or MinGW)
#   On Linux:   make release

# Detect platform
UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
    PLATFORM := macos
    CLAP_EXT := clap
    INSTALL_DIR := $(HOME)/Library/Audio/Plug-Ins/CLAP
else ifeq ($(UNAME),Linux)
    PLATFORM := linux
    CLAP_EXT := clap
    INSTALL_DIR := $(HOME)/.clap
else ifeq ($(OS),Windows_NT)
    PLATFORM := windows
    CLAP_EXT := clap
    INSTALL_DIR := $(LOCALAPPDATA)/Programs/Common/CLAP
endif

# Build configuration
BUILD_TYPE ?= Release
BUILD_DIR := build
CMAKE_FLAGS := -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

# macOS universal binary support
ifeq ($(PLATFORM),macos)
    ifeq ($(ARCH),universal)
        CMAKE_FLAGS += -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
    else ifeq ($(ARCH),arm64)
        CMAKE_FLAGS += -DCMAKE_OSX_ARCHITECTURES=arm64
    else ifeq ($(ARCH),x86_64)
        CMAKE_FLAGS += -DCMAKE_OSX_ARCHITECTURES=x86_64
    endif
    # Minimum macOS version for WebKit support
    CMAKE_FLAGS += -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
endif

# Windows specific
ifeq ($(PLATFORM),windows)
    # Use Ninja if available (faster), otherwise default
    CMAKE_GEN := -G "Ninja"
endif

# Linux specific
ifeq ($(PLATFORM),linux)
    CMAKE_FLAGS += -DCMAKE_POSITION_INDEPENDENT_CODE=ON
endif

.PHONY: all release debug clean install submodules configure build

all: release

# Initialize git submodules
submodules:
	@echo "==> Initializing submodules..."
	git submodule update --init --recursive

# Configure CMake
configure: submodules
	@echo "==> Configuring for $(PLATFORM) ($(BUILD_TYPE))..."
	cmake -S . -B $(BUILD_DIR) $(CMAKE_FLAGS) $(CMAKE_GEN)

# Build
build: configure
	@echo "==> Building..."
	cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) --parallel

release:
	$(MAKE) build BUILD_TYPE=Release

debug:
	$(MAKE) build BUILD_TYPE=Debug

# Install to standard CLAP location
install: release
	@echo "==> Installing to $(INSTALL_DIR)..."
	@mkdir -p "$(INSTALL_DIR)"
ifeq ($(PLATFORM),macos)
	@echo "Removing quarantine attribute..."
	@xattr -rd com.apple.quarantine $(BUILD_DIR)/clasp.clap 2>/dev/null || true
	@echo "Signing plugin (ad-hoc)..."
	@codesign -s - -f --deep $(BUILD_DIR)/clasp.clap
	@cp -R $(BUILD_DIR)/clasp.clap "$(INSTALL_DIR)/"
else
	@cp $(BUILD_DIR)/clasp.$(CLAP_EXT) "$(INSTALL_DIR)/"
endif
	@echo "Installed to $(INSTALL_DIR)/clasp.clap"

# Clean build artifacts
clean:
	@echo "==> Cleaning..."
	rm -rf build

# Package for distribution
package: release
	@echo "==> Packaging for distribution..."
	@mkdir -p dist
ifeq ($(PLATFORM),macos)
	@cd $(BUILD_DIR) && zip -r ../../dist/clasp-$(PLATFORM)-$(shell uname -m).zip clasp.clap
else ifeq ($(PLATFORM),linux)
	@cd $(BUILD_DIR) && tar czf ../../dist/clasp-$(PLATFORM)-$(shell uname -m).tar.gz clasp.clap
else ifeq ($(PLATFORM),windows)
	@cd $(BUILD_DIR) && 7z a ../../dist/clasp-windows-x64.zip clasp.clap
endif
	@echo "Package created in dist/"

# Help
help:
	@echo "CLASP Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make              Build release for current platform"
	@echo "  make release      Build optimized release"
	@echo "  make debug        Build with debug symbols"
	@echo "  make install      Install to standard CLAP folder"
	@echo "  make package      Create distributable package"
	@echo "  make clean        Remove build artifacts"
	@echo ""
	@echo "Options:"
	@echo "  ARCH=universal    macOS: build x86_64 + arm64 fat binary"
	@echo "  ARCH=arm64        macOS: build for Apple Silicon only"
	@echo "  ARCH=x86_64       macOS: build for Intel only"
	@echo ""
	@echo "Platform: $(PLATFORM)"
	@echo "Install dir: $(INSTALL_DIR)"
