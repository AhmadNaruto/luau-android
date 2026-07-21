# NoveLA Integration Plan: Replace LuaJ, JSoup, and Readability4J with luau-android

This document outlines the current state and step-by-step architectural plan for integrating `luau-android` into `/data/data/com.termux/files/home/NoveLA`.

---

## 📊 Current Status: COMPLETED (NOVELX EXTENSIONS & LUAU ENGINE VERIFIED)

- **`:luau-android` AAR module:** Compiled 100% successfully (`bundleDebugAar` & `assembleDebug` OK). Native Luau C++ engine compiled with CMake & NDK 28 ARM64.
- **`LuaJ` Removal:** 100% complete across `NoveLA/scraper` (`LuaSourceAdapter`, `LuaSourceLoader`, `LuaFilterSupport`, `LuaSettingsSupport`).
- **`JSoup` Removal & LexSoup Adaptation:** 100% complete across `networking`, `data`, `scraper`, `features/reader`, `tooling/epub_parser`.
  - Refactored `NovelUpdates.kt` & `BakaUpdates.kt` for LexSoup DOM structure.
  - Refactored `ContentHelpers.kt`, `SelectorTransforms.kt`, `LuaSettingsSupport.kt`.
  - Refactored `NetworkBridge.kt` for non-blocking/suspend client invocation.
  - Refactored `LuaSourceAdapter.kt` and `LuaSourceLoader.kt` to align with `LuauRuntime` and `SourceInterface`.
  - Refactored `TextToItemsConverter.kt` to use `LexSoup`.
  - Fixed `DownloaderRepository.kt` imports and structure.
- **NovelX Extension Compatibility & Fixes:**
  - Added Map lookup support in `JniProxyHelper.kt` for accessing HTTP response fields (`res.success`, `res.body`, `res.statusCode`).
  - Added HTML helper shim functions (`html_select`, `html_select_first`, `html_attr`, `html_remove`, `html_text`, `log_error`) in native Luau `COMPATIBILITY_SHIM_SCRIPT`.
  - Added `evalJson` method to `LuauRuntime` for evaluating Luau expressions and parsing returned tables/arrays into `BookResult`, `ChapterResult`, `LuaFilter`, and `SourceMetadata`.
- **`Readability4J` Removal:** 100% complete (`DownloaderRepository.kt` uses `LexSoup`).
- **Build Status:** **BUILD SUCCESSFUL** (`NoveLA_v1.4.0-debug.apk` built at `app/build/outputs/apk/debug/NoveLA_v1.4.0-debug.apk` - 50 MB).

---

## 📋 Verification Artifact

The APK artifact is located at:
`app/build/outputs/apk/debug/NoveLA_v1.4.0-debug.apk`
