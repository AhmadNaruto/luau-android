# Architecture & Native SDK Guide

## Architecture Overview

```
Android Application (Kotlin)
        │
       JNI
        │
 Native Luau Engine (arm64-v8a)
        │
 ┌──────┴──────┬──────────────┬──────────────┬──────────────┬──────────────┐
 Lexbor      QuickJS-NG     yyjson         PCRE2          Pure-C         Native Modules
 (HTML/URL)   (JS Exec)     (JSON)        (Regex)        (Crypto)       (Buffer/DSL)
```

- **Kotlin Responsibility:** Lifecycle, JNI creation, network communication (OkHttp/Cronet), storage/preferences, logging.
- **Native Core Responsibility:** Embedded Luau VM runtime, memory management, zero-copy buffer operations, HTML/DOM parsing, JS execution, JSON processing, Crypto, PCRE2 Regex, and Select DSL execution.

## Memory Ownership & Safety Model

1. **Luau VM:** Each runtime instance owns one isolated Luau VM instance with thread-safe garbage collection.
2. **Userdata Handles:** Native handles (e.g. `native_html_doc_handle_t`) use atomic reference counting.
3. **HTML DOM Tree:** Owned by `Document`. `Node` and `Element` instances retain a reference to the `Document` handle, preventing use-after-free even if the `Document` variable is collected in Luau.
4. **Userdata Metatables:** Registered once per VM with standard `__gc` meta-methods to guarantee native resources are released on GC.

## Thread Safety

- Luau VMs do not share mutable global state.
- Each `LuauRuntime` instance is thread-confined; reentrant native C/C++ modules can be called safely across worker threads as long as separate runtime instances or synchronized locks are used.

## Native Module System Reference

- **`html`** (Lexbor): `html.parse(str)` → Document with `querySelector()`, `querySelectorAll()`, `text()`, `tag()`, `attr()`.
- **`js`** (QuickJS-NG): `js.new()` → JSContext with `eval()`, `set()`, `get()`, `gc()`.
- **`json`** (yyjson): `json.parse()`, `json.stringify()`, `json.encode()`, `json.decode()`.
- **`regex`** (PCRE2): `regex.compile(pattern, flags)` → Pattern with `match()`, `find()`, `find_all()`, `replace()`, `split()`.
- **`url`** (Lexbor URL): `url.parse()`, `url.join()`, `url.encode()`, `url.decode()`.
- **`encoding`** (Lexbor Encoding): `base64_encode()`, `base64_decode()`, `utf8_valid()`, `utf8_len()`, `utf8_sub()`, `utf8_to_utf16le()`, `utf16le_to_utf8()`, `detect()`.
- **`crypto`** (Pure-C): `crypto.md5()`, `crypto.sha1()`, `crypto.sha256()`, `crypto.sha512()`, `crypto.hmac()`, `crypto.random()`, `crypto.uuid()`.
- **`buffer`**: `buffer.create(size)` → Zero-copy binary/UTF-8 buffer with typed getters/setters (`writeString`, `readString`, `writeFloat64`, `readFloat64`, `writeInt32`, `readInt32`, `slice`).
- **`select`**: Fluent scraping DSL (`select.from(doc):text(...):attr(...):build()`).
- **`util`**: `trim()`, `split()`, `join()`, `startswith()`, `endswith()`, `contains()`, `lower()`, `upper()`, `capitalize()`, `sleep()`, `uuid()`.
- **`time`**: `now()`, `unix()`, `format()`, `parse()`, `sleep()`.
