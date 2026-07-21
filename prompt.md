# Native Luau SDK for Android (ARM64)

## Objective

Develop a modular, high-performance Native SDK for Android dedicated to Luau-based extensions.

The SDK provides a native-first scripting environment for Android applications.

Primary use cases:

- HTML scraping
- DOM parsing
- JavaScript execution
- JSON processing
- URL manipulation
- Regular expressions
- Character encoding
- Cryptography
- Data transformation

The runtime is designed as a processing engine.

Network communication is NOT handled by native code.

---

# Architecture Principle

The architecture must follow:

Android Application

        |
        |
      Kotlin

        |
        |
       JNI

        |
        |
 Native Luau Runtime

        |
        |
 Native Modules

        |
        +----------------+
        | Lexbor         |
        | QuickJS-NG     |
        | yyjson         |
        | PCRE2          |
        | OpenSSL        |
        +----------------+


---

# Kotlin Responsibility

Kotlin is ONLY responsible for:

- Android lifecycle integration
- Runtime creation and destruction
- JNI bridge
- Android API provider
- Network communication
- Storage access
- Database access
- Preferences
- Image operations
- Download management
- Logging

Kotlin must NOT perform:

- HTML parsing
- DOM processing
- JSON processing
- Regex processing
- JavaScript execution
- Cryptographic processing


---

# Network Architecture

HTTP is implemented entirely by Kotlin.

Native runtime MUST NOT perform network operations.

The Kotlin HTTP layer provides:

- response body
- headers
- cookies
- status code
- request metadata

Flow:

Kotlin HTTP

    |
    |
 response data

    |
    |
 Luau Runtime

    |
    |
 parsing
 extraction
 transformation


The SDK does not implement:

- HTTP client
- TLS stack
- Cookie manager
- Proxy handling
- Connection pooling


---

# Third-party Dependencies

Never copy third-party source code into this project.

All external libraries must be included as Git submodules.

Always track official upstream repositories.

The project must remain easily upgradeable by updating submodules.


---

# Required Submodules


## Luau

Repository:

https://github.com/luau-lang/luau


Purpose:

- Luau VM
- Compiler
- Bytecode
- Standard libraries


Requirements:

- Do not fork
- Do not modify upstream
- Use official Luau embedding APIs
- Do not assume Lua 5.x C API compatibility


---

## Lexbor

Repository:

https://github.com/lexbor/lexbor


Purpose:

- HTML parser
- DOM
- CSS selectors
- URL parser
- Encoding


Compile only required modules.

Do not expose Lexbor internal structures.


---

## QuickJS-NG

Repository:

https://github.com/quickjs-ng/quickjs


Purpose:

- ECMAScript execution engine


QuickJS provides JavaScript execution only.

It is NOT a browser engine.

Do not implement:

- window
- document
- navigator
- fetch
- XMLHttpRequest

unless explicitly added later.


---

## yyjson

Repository:

https://github.com/ibireme/yyjson


Purpose:

High-performance JSON parser.


---

## PCRE2

Repository:

https://github.com/PCRE2Project/pcre2


Purpose:

Native regular expression engine.


---

## OpenSSL

Repository:

https://github.com/openssl/openssl


Purpose:

Cryptographic operations.


Required features:

- MD5
- SHA1
- SHA256
- SHA512
- HMAC
- Random generator


Compile only required components.

Disable:

- tests
- applications
- documentation
- unused algorithms


---

# Third-party Modification Policy

Third-party repositories must remain clean and synchronized with upstream.

Do NOT directly modify third-party source code.

If a modification is absolutely required for:

- Android compatibility
- ARM64 build support
- NDK compatibility
- CMake integration
- disabling unnecessary features
- build system fixes

then the modification MUST be implemented as a patch.


Requirements:

- Keep upstream repository unchanged
- Store all modifications outside third-party directories
- Apply patches automatically during build configuration
- Document every patch
- Explain the reason for each patch
- Make patches easy to remove when upstream provides a solution


Repository structure:

project/

    patches/

        luau/
            0001-android-build-fix.patch

        lexbor/
            0001-disable-unused-modules.patch

        quickjs/
            0001-ndk-compatibility.patch


    third_party/

        luau/
        lexbor/
        quickjs/
        yyjson/
        pcre2/
        openssl/


Patch workflow:

git submodule update

        |

apply patches

        |

configure CMake

        |

build Android library


Never:

- commit modified third-party files
- create hidden forks
- replace upstream source
- copy third-party implementation into wrapper code


Every patch must document:

- upstream commit/version
- patch purpose
- affected files
- expected impact
- removal condition


The project must remain upgradeable by:

git submodule update --remote

apply patches

build


---

# Repository Layout


project/


third_party/

    luau/
    lexbor/
    quickjs/
    yyjson/
    pcre2/
    openssl/


patches/


native/

    core/

        runtime/
        luau/
        memory/


    modules/

        html/
        js/
        json/
        regex/
        url/
        encoding/
        crypto/
        buffer/
        util/
        time/
        select/


    bindings/

        jni/


    common/


android/

    kotlin/


Never place wrapper code inside third-party repositories.


---

# Build System

Use:

- CMake
- Android NDK


Target ABI:

arm64-v8a


Minimum Android:

API 24+


Every dependency must build from its own submodule directory.

Do not:

- copy source files
- vendor snapshots
- replace upstream repositories


Use CMake configuration to link dependencies.


---

# Luau Runtime

Implement:

- VM creation
- VM destruction
- Script execution
- Module registration
- Memory management
- Error handling


Each runtime owns:

- one Luau VM
- one module registry
- one allocator context


Thread model:

- one runtime per Luau VM
- no shared mutable global state
- native modules should be reentrant whenever possible


---

# Luau Module System

Implement a native module loader.

Example:

local html = require("html")
local json = require("json")
local regex = require("regex")


Modules are registered during runtime initialization.

Module loading must not require JNI calls.

---

# Native Modules


## html

Powered by:

Lexbor


Expose jsoup-inspired API:

- Document
- Element
- Elements
- Node


Features:

- HTML parsing
- CSS selectors
- DOM traversal
- DOM modification
- text extraction
- attribute access


Rules:

- Document owns DOM tree
- Elements reference Document memory
- Never expose Lexbor pointers


---

## js

Powered by:

QuickJS-NG


API:

local js=require("js")

local ctx=js.new()

ctx:eval(code)

ctx:set(name,value)

ctx:get(name)


Support:

- multiple contexts
- memory limits
- execution timeout
- exception reporting


Each context owns JavaScript values.


---

## json

Powered by:

yyjson


Expose:

json.encode()

json.decode()

json.parse()

json.stringify()


---

## regex

Powered by:

PCRE2


Expose:

regex.compile()

match()

find()

replace()

split()

find_all()


Compiled regex objects must be reusable.


---

## url

Powered by:

Lexbor URL module.


Expose:

url.parse()

url.join()

url.resolve()

url.encode()

url.decode()


---

## encoding

Powered by:

Lexbor Encoding.


Support:

- UTF conversion
- Encoding detection
- Unicode normalization


---

## buffer

Native zero-copy buffer.


Purpose:

Efficient transfer between Kotlin and native runtime.


Support:

- UTF-8 data
- binary data
- ByteArray mapping


Avoid unnecessary copying.


---

## crypto

Powered by:

OpenSSL.


Expose:

crypto.md5()

crypto.sha1()

crypto.sha256()

crypto.sha512()

crypto.hmac()

crypto.random()

crypto.uuid()


---

## util

Native helpers:


trim()

split()

join()

startswith()

endswith()

contains()

lower()

upper()

capitalize()

sleep()

uuid()


---

## time

Expose:


time.now()

time.unix()

time.format()

time.parse()

time.sleep()


---

## select

High-level extraction DSL.


Example:


local result =
select
.from(doc)

:text("title","h1")

:attr(
"cover",
".cover img",
"src"
)

:build()


Implementation requirements:

The DSL must compile into a native execution plan.


Avoid:

- repeated Luau/native transitions
- temporary objects
- executing every chain step separately


Operations should execute in batches.


---

# Android Modules

Only Android-specific modules communicate with Kotlin.


Expose:


android.http

android.storage

android.database

android.preferences

android.image

android.download

android.log


Everything else remains entirely native.


---

# JNI Design

JNI exposes ONLY:

- Runtime creation
- Runtime destruction
- Script execution
- Module registration
- Android bridge


Do NOT expose:

- DOM APIs
- Parser APIs
- Regex APIs
- JSON APIs
- Native object internals
- Raw pointers


---

# Memory Management

Requirements:


Every userdata implements:

__gc


Rules:

- No memory leaks
- No double free
- No dangling references
- No raw pointer exposure


Ownership model:


Document

    |
    +-- owns DOM tree


Element

    |
    +-- references Document


QuickJS Context

    |
    +-- owns JavaScript values


Regex Object

    |
    +-- owns compiled pattern


Use:

- arena allocator
- object pooling
- reference counting where required


---

# Performance Requirements

Prioritize:

- zero-copy
- UTF-8 internally
- cached userdata metatables
- cached strings
- arena allocation
- object pooling
- batched native operations


Avoid:

- small JNI calls
- frequent VM/native transitions
- temporary allocations
- unnecessary copying


---

# Security Requirements

The runtime must support:

- QuickJS memory limit
- QuickJS execution timeout
- recursion limit
- UTF-8 validation
- input validation
- no native pointer exposure


---

# Documentation

Provide:

- Architecture documentation
- Memory ownership documentation
- Thread safety documentation
- Module reference
- Examples
- Performance notes
- Contribution guide
- Submodule update guide


---

# Testing

Provide:


Unit tests:

- module loading
- Luau execution
- DOM parsing
- JSON processing
- Regex matching
- Crypto functions


Integration tests:

- Kotlin JNI communication
- Android runtime lifecycle


Stress tests:

- Large HTML documents
- Large JSON payloads
- Long-running scripts


Memory tests:

- Leak detection
- Address Sanitizer
- Undefined Behavior Sanitizer


Benchmarks:

- HTML parsing
- JSON parsing
- Regex
- JavaScript execution
- Luau execution


---

# Deliverables

Provide:

- Complete Android project
- CMake build system
- Git submodule configuration
- Patch management system
- Native wrappers
- Luau bindings
- JNI bridge
- Kotlin integration
- Unit tests
- Integration tests
- Stress tests
- Benchmarks
- Documentation


The final project must:

- avoid third-party forks
- remain upgradeable through submodule updates
- keep third-party repositories clean
- isolate wrapper code
- minimize JNI overhead
- keep heavy processing entirely native
- be suitable for production Android applications
