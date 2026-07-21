# Native Luau SDK for Android (ARM64)

[![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen.svg)]()
[![Target ABI](https://img.shields.io/badge/ABI-arm64--v8a-blue.svg)]()
[![Min SDK](https://img.shields.io/badge/Min%20SDK-24+-orange.svg)]()

High-performance, modular Native SDK for Android dedicated to Luau-based extensions, HTML scraping, JavaScript execution, JSON processing, regular expressions, cryptography, and data transformation.

---

## ⚡ Key Highlights

- **Embedded Luau VM:** Fast, sandboxed Luau scripting engine compiled for `arm64-v8a`.
- **100% Native Heavy Processing:** Heavy DOM parsing, regex scanning, cryptography, and JSON serialization run entirely in native C/C++ via compiled static libraries.
- **LexSoup JSoup Replacement (`com.luau.android.lexsoup`):** High-speed, drop-in Java/Kotlin replacement for JSoup powered by native Lexbor C++ engine (`LexSoup.parse`, `Document`, `Element`, `Elements`).
- **novelx-sources Global Compatibility Shim:** Pre-injected global helper functions (`html_select`, `html_attr`, `url_resolve`, `regex_replace`, `string_clean`) to run extension scripts without modification.
- **11 Built-in Native Modules:**
  - `html` (Powered by **Lexbor** HTML parser & CSS engine)
  - `js` (Powered by **QuickJS-NG** ECMAScript engine)
  - `json` (Powered by **yyjson** zero-copy JSON parser)
  - `regex` (Powered by **PCRE2** regular expression engine)
  - `url` (Powered by **Lexbor URL**)
  - `encoding` (Powered by **Lexbor Encoding**)
  - `crypto` (Pure C Cryptography Engine for MD5, SHA1, SHA256, SHA512, HMAC, and secure Random/UUID)
  - `buffer` (Zero-copy binary & typed data buffers)
  - `select` (Batch extraction scraping DSL)
  - `util` (String & general utilities)
  - `time` (Timestamp formatting & parsing)
- **Zero-Copy Data Transfer:** `buffer` module provides direct native memory sharing between Kotlin and Luau.
- **Clean Submodule Design:** All third-party libraries tracked as clean Git submodules in `third_party/`.

---

## 📁 Repository Layout

```
luau-android/
├── android/              # Android AAR library & Kotlin JNI integration
│   ├── kotlin/           # Kotlin source files (LuauRuntime, LuauException)
│   └── src/test/         # 17 JUnit Unit & Benchmark Test Suites
├── native/               # Native C/C++ engine & modules
│   ├── core/             # VM runtime, allocator, Luau module loader
│   ├── modules/          # html, js, json, regex, url, crypto, select, etc.
│   └── bindings/jni/     # JNI C++ bridge
├── third_party/          # Clean Git Submodules (luau, lexbor, quickjs, yyjson, pcre2)
├── patches/              # NDK / Android build patch management
├── docs/                 # Architecture & SDK documentation
├── benchmarks/           # ARM64 Benchmark results & scripts
├── CMakeLists.txt        # Native build system configuration
├── API_REFERENCE.md      # Full API Reference for Kotlin & Luau
└── README.md             # Project overview & quickstart guide
```

---

## 🚀 Quick Start Guide

### 1. Requirements
- Android NDK `28.2.13676358`
- CMake `3.22.1` or higher
- Gradle `8.x` / JDK `17`
- Target Architecture: `arm64-v8a` (Android API 24+)

### 2. Building the Project

```bash
# Clone submodules
git submodule update --init --recursive

# Build Android AAR & Native Library
./gradlew assembleDebug

# Run Unit & Benchmark Test Suite (17 Tests)
./gradlew :android:testDebugUnitTest
```

---

## 💻 Usage Example in Kotlin

```kotlin
import com.luau.android.LuauRuntime

// Initialize Luau Runtime (optional memory limit: 16MB)
LuauRuntime(16 * 1024 * 1024).use { runtime ->
    val script = """
        local html   = require("html")
        local select = require("select")
        local crypto = require("crypto")

        local doc = html.parse([[
            <html><body><h1 class="title">Welcome</h1></body></html>
        ]])

        local data = select
            .from(doc)
            :text("title", ".title")
            :build()

        data.hash = crypto.sha256(data.title)
        return data.title .. " (SHA256: " .. data.hash:sub(1, 8) .. ")"
    """.trimIndent()

    val result = runtime.execute(script)
    println(result)
    // Output: Welcome (SHA256: 9e06482e)
}
```

### ☕ LexSoup Drop-In JSoup Replacement Example

```kotlin
import com.luau.android.lexsoup.LexSoup

val html = """<html><body><a href="/book/123" class="book-link">Book Title</a></body></html>"""

LexSoup.parse(html).use { doc ->
    val link = doc.selectFirst("a.book-link")
    println(link?.attr("href")) // "/book/123"
    println(link?.text())        // "Book Title"
}
```

---

## ⚡ Performance Benchmarks (ARM64-v8a)

Tested on native ARM64 architecture:

| Component | Workload | Time (ms) | Speed |
|-----------|----------|-----------|-------|
| **Luau Core VM** | 1,000,000 Math & Modulo loop | **166.15 ms** | ~6.0M ops/sec |
| **Lexbor HTML** | Parse 100KB HTML + 100 CSS Queries | **22.12 ms** | ~4.5K queries/sec |
| **yyjson JSON** | 100x Parse + Stringify (500 items) | **659.81 ms** | ~75.7K items/sec |
| **PCRE2 Regex** | 1,000 regex scans | **15.38 ms** | ~65K matches/sec |
| **OpenSSL Crypto** | 5,000 SHA256 + MD5 + HMAC operations | **67.12 ms** | ~74.5K hashes/sec |

*For complete benchmark logs, see [benchmarks/RESULTS.md](file:///data/data/com.termux/files/home/luau-android/benchmarks/RESULTS.md).*

---

## 📚 Documentation & API

- **[API_REFERENCE.md](file:///data/data/com.termux/files/home/luau-android/API_REFERENCE.md)** — Complete Kotlin & Luau module API reference.
- **[docs/ARCHITECTURE.md](file:///data/data/com.termux/files/home/luau-android/docs/ARCHITECTURE.md)** — Architecture diagram, memory management rules, and thread safety.
- **[patches/README.md](file:///data/data/com.termux/files/home/luau-android/patches/README.md)** — Patch management workflow.
