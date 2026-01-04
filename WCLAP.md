# WCLAP Upstream Proposals

This document outlines proposals for the [WCLAP](https://github.com/WebCLAP) ecosystem based on CLASP's implementation experience.

## Context

CLASP and WCLAP Bridge are parallel implementations of the same concept: hosting WebAssembly audio plugins as native CLAP plugins. Rather than maintaining separate ecosystems, we propose contributing CLASP's cross-platform work upstream and standardizing on compatible interfaces.

**CLASP contributes:**
- Cross-platform support (Windows, Linux - WCLAP is currently macOS-only)
- AOT compilation caching infrastructure
- WebView GUI implementation (on WCLAP's wishlist)
- Developer tooling (`clasp-tool`)

**WCLAP provides:**
- Established ABI specification
- Existing ecosystem and documentation
- Browser-side hosting (`wclap-js`)

---

## Proposal 1: Fast-Scan Manifest Sidecar

### Problem
DAWs scan hundreds of plugins at startup. Currently, hosts must instantiate each WASM module to read its `clap_plugin_descriptor`. This is slow compared to native plugins where metadata can be read from headers or manifest files.

### Proposal
Add an optional `manifest.json` sidecar file to `.wclap` bundles:

```
MyPlugin.wclap/
├── module.wasm      # Authoritative (required)
├── manifest.json    # Optional, for fast scanning
└── resources/       # Presets, UI assets, etc.
```

### Manifest Schema
```json
{
  "wclap_manifest_version": "1.0",
  "id": "com.vendor.my-plugin",
  "name": "My Plugin",
  "vendor": "Vendor Name",
  "version": "1.0.0",
  "url": "https://vendor.com/my-plugin",
  "features": ["instrument", "synthesizer", "stereo"],
  "clap_version": "1.2.0",
  "wasm_hash": "sha256:abc123..."
}
```

### Behavior
1. If `manifest.json` exists and `wasm_hash` matches `module.wasm`, use cached metadata
2. Otherwise, instantiate WASM and query descriptor (existing behavior)
3. WASM remains authoritative - manifest is a performance optimization only

### Precedent
- VST3: `moduleinfo.json`
- Audio Units: `Info.plist`
- LV2: `.ttl` manifest files

---

## Proposal 2: AOT Cache Location Standardization

### Context
Both WCLAP and CLASP use Wasmtime's built-in AOT caching (via config flags). This works well.

### Proposal
Standardize the cache location so hosts can share cached modules:

```
~/.wclap/cache/
├── <plugin_id>_<wasm_hash>.cwasm     # Compiled module
└── <plugin_id>_<wasm_hash>.meta.json # Cached metadata (for fast scanning)
```

### Cache Key
```
hash = SHA256(module.wasm + wasmtime_version + target_triple + config_flags)
```

### Benefits
- Shared cache across WCLAP-compatible hosts
- Plugin developers can ship pre-compiled `.cwasm` for common platforms
- Metadata cache enables fast scanning without WASM instantiation

---

## Proposal 3: License Validation Extension

### Problem
Commercial plugin developers need copy protection mechanisms. WASM's sandboxing actually helps here (plugins can't directly access filesystems), but there's no standard API for license validation.

### Proposal
Add an optional host extension for license management:

```c
#define WCLAP_EXT_LICENSING "com.wclap.licensing/1"

typedef struct wclap_host_licensing {
    // Check if plugin is licensed for this machine
    // Returns: 0 = not licensed, 1 = licensed, -1 = error
    int32_t (*check_license)(
        const wclap_host_t* host,
        const char* product_id,
        const char* license_key
    );

    // Get machine fingerprint for online activation
    const char* (*get_machine_id)(const wclap_host_t* host);

    // Request online validation (async, callback invoked with result)
    void (*validate_online)(
        const wclap_host_t* host,
        const char* validation_url,
        const char* license_key,
        const char* machine_id,
        void (*callback)(int32_t result, const char* message)
    );

    // Store/retrieve persistent license data
    int32_t (*store_license_data)(
        const wclap_host_t* host,
        const uint8_t* data,
        uint32_t size
    );

    uint32_t (*get_license_data)(
        const wclap_host_t* host,
        uint8_t* buffer,
        uint32_t max_size
    );
} wclap_host_licensing_t;
```

### Usage Pattern
```cpp
bool MyPlugin::activate() {
    auto* licensing = (wclap_host_licensing_t*)
        host->get_extension(host, WCLAP_EXT_LICENSING);

    if (!licensing) {
        // Host doesn't support licensing - demo mode
        return activateDemoMode();
    }

    // Validate license
    int result = licensing->check_license(host, PRODUCT_ID, stored_key);
    if (result == 1) {
        licensed_ = true;
        return true;
    }

    return activateDemoMode();
}
```

### Security Considerations
- Client-side validation can always be bypassed with enough effort
- Server-side validation (`validate_online`) is more secure but requires network
- Watermarking (unique builds per customer) tracks leakers
- Demo mode restrictions (noise injection, time limits) encourage purchase

---

## Proposal 4: Cross-Platform CMake Module

### Problem
WCLAP Bridge's CMake currently only supports macOS. Cross-platform audio plugins need Windows and Linux support.

### Proposal
Contribute CLASP's cross-platform CMake configuration:

```cmake
# Platform-specific configurations
if(APPLE)
    # Existing macOS config
elseif(WIN32)
    # Windows: WebView2, Windows threading
elseif(UNIX)
    # Linux: WebKitGTK, pthreads
endif()

# Wasmtime fetching with platform detection
FetchContent_Declare(wasmtime
    URL ${WASMTIME_URL_${CMAKE_SYSTEM_NAME}_${CMAKE_SYSTEM_PROCESSOR}}
)
```

### Includes
- Windows WebView2 integration
- Linux WebKitGTK integration
- Platform-specific threading primitives
- Cross-platform Wasmtime binary fetching

---

## Proposal 5: WebView GUI Extension Support

### Problem
The CLAP draft webview extension is on WCLAP's wishlist but not implemented.

### Proposal
Contribute CLASP's WebView implementation:

**Features:**
- Host-managed mode (use host's browser engine when available)
- Self-contained fallback (CHOC-based: WKWebView/WebView2/WebKitGTK)
- Bidirectional messaging (host ↔ UI ↔ DSP)
- Resource serving from bundle

**JavaScript API:**
```javascript
// Provided by host
wclap.setParam(id, value);
wclap.getParam(id);
wclap.getPluginInfo();
wclap.sendMessage(buffer);  // Binary messaging to DSP

// Callbacks from host
wclap.onParamChange = (id, value) => { ... };
wclap.onMessage = (buffer) => { ... };
```

---

## Implementation Status

| Proposal | CLASP Status | Upstream Status |
|----------|--------------|-----------------|
| Fast-scan manifest | Implemented (metadata cache) | Not yet proposed |
| AOT cache location | Custom path | Uses Wasmtime default |
| License extension | Designed | Not yet proposed |
| Cross-platform CMake | Implemented | macOS only |
| WebView GUI | Implemented | Wishlist |

---

## Next Steps

1. Open discussion issues on WebCLAP/wclap-bridge for each proposal
2. Submit cross-platform CMake as PR
3. Coordinate on ABI compatibility testing
4. Consider project merge if goals align

## Contact

CLASP: https://github.com/user/clasp
