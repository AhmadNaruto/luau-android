# Native Luau SDK Android ARM64
# Development Plan


==================================================
PHASE 0 — PROJECT FOUNDATION
==================================================

Goal:

Setup repository, dependencies, build system, and project structure.


Tasks:

[ ] Create Git repository

[ ] Configure Git submodules

[ ] Create third_party directory

[ ] Create patches directory

[ ] Create native directory

[ ] Create android directory

[ ] Create tests directory

[ ] Create benchmarks directory

[ ] Create docs directory


Add submodules:

[ ] Luau
    https://github.com/luau-lang/luau

[ ] Lexbor
    https://github.com/lexbor/lexbor

[ ] QuickJS-NG
    https://github.com/quickjs-ng/quickjs

[ ] yyjson
    https://github.com/ibireme/yyjson

[ ] PCRE2
    https://github.com/PCRE2Project/pcre2

[ ] OpenSSL
    https://github.com/openssl/openssl


Build setup:

[ ] Configure Gradle

[ ] Configure CMake

[ ] Configure Android NDK

[ ] Target ABI:
    arm64-v8a only

[ ] Minimum Android:
    API 24+

Expected output:

libnative_luau.so



==================================================
PHASE 1 — NATIVE CORE RUNTIME
==================================================

Goal:

Create embedded Luau VM runtime.


Implement:

native/core/runtime/


Components:

[ ] Runtime lifecycle

[ ] VM creation

[ ] VM destruction

[ ] Script execution

[ ] Error handling

[ ] Allocator integration


API:

createRuntime()

destroyRuntime()

executeScript()


Test:

[ ] Execute simple Luau script

Example:

print("hello")

Expected:

hello



==================================================
PHASE 2 — JNI BRIDGE
==================================================

Goal:

Create minimal Kotlin ↔ Native communication.


JNI exposes only:

[ ] Runtime creation

[ ] Runtime destruction

[ ] Script execution

[ ] Module registration

[ ] Android bridge


Do NOT expose:

[ ] HTML parser API

[ ] JSON API

[ ] Regex API

[ ] DOM objects

[ ] Native pointers


Test:

[ ] Create runtime from Kotlin

[ ] Execute Luau script

[ ] Destroy runtime



==================================================
PHASE 3 — LUAU MODULE SYSTEM
==================================================

Goal:

Implement native module loader.


Support:

require("module")


Implement:

native/core/luau/


Components:

[ ] Module registry

[ ] Native module loader

[ ] Module lifecycle


Register modules:

[ ] html

[ ] json

[ ] regex

[ ] js

[ ] crypto

[ ] buffer

[ ] util

[ ] time


Test:

local json=require("json")



==================================================
PHASE 4 — MEMORY MANAGEMENT SYSTEM
==================================================

Goal:

Create safe native object lifecycle.


Implement:

native/core/memory/


Components:

[ ] Arena allocator

[ ] Object pool

[ ] Reference counter

[ ] Native userdata manager


Rules:

[ ] Every userdata implements __gc

[ ] No memory leak

[ ] No double free

[ ] No dangling reference

[ ] No raw pointer exposure


Test:

[ ] Create object

[ ] Release object

[ ] Force GC

[ ] Check memory



==================================================
PHASE 5 — BUFFER MODULE
==================================================

Goal:

Efficient Kotlin ↔ Native data transfer.


Implement:

native/modules/buffer/


Features:

[ ] UTF-8 buffer

[ ] Binary buffer

[ ] ByteArray mapping

[ ] Zero-copy transfer


Used by:

[ ] html

[ ] json

[ ] crypto


Test:

[ ] Transfer large HTML document

[ ] Verify no unnecessary copy



==================================================
PHASE 6 — HTML MODULE (LEXBOR)
==================================================

Goal:

Create jsoup-inspired HTML API.


Implement:

native/modules/html/


Objects:

[ ] Document

[ ] Element

[ ] Elements

[ ] Node


Features:

[ ] HTML parsing

[ ] CSS selector

[ ] DOM traversal

[ ] Text extraction

[ ] Attribute extraction

[ ] DOM modification


Memory model:

Document owns DOM tree.

Element references Document memory.


Example:

local doc=html.parse(data)

local title=
doc:select("title")
:text()


Tests:

[ ] Parse HTML

[ ] CSS selector

[ ] Extract text

[ ] Extract attributes

[ ] Modify DOM



==================================================
PHASE 7 — JSON MODULE (YYJSON)
==================================================

Goal:

High-performance JSON processing.


Implement:

native/modules/json/


API:

json.encode()

json.decode()

json.parse()

json.stringify()


Optimization:

[ ] Zero-copy parsing

[ ] Native allocation


Tests:

[ ] Small JSON

[ ] Large JSON

[ ] Invalid JSON



==================================================
PHASE 8 — REGEX MODULE (PCRE2)
==================================================

Goal:

Native regular expression engine.


Implement:

native/modules/regex/


Objects:

[ ] Pattern userdata

[ ] Match userdata


API:

regex.compile()

match()

find()

replace()

split()

find_all()


Tests:

[ ] Pattern compile

[ ] Match

[ ] Replace

[ ] Stress test



==================================================
PHASE 9 — URL AND ENCODING MODULE
==================================================

Goal:

Provide URL and text processing.


URL powered by:

Lexbor URL


API:

url.parse()

url.join()

url.resolve()

url.encode()

url.decode()


Encoding:

[ ] UTF conversion

[ ] Encoding detection

[ ] Unicode normalization


Tests:

[ ] URL parsing

[ ] Unicode handling



==================================================
PHASE 10 — CRYPTO MODULE
==================================================

Goal:

Provide native cryptography.


Powered by:

OpenSSL


API:

crypto.md5()

crypto.sha1()

crypto.sha256()

crypto.sha512()

crypto.hmac()

crypto.random()

crypto.uuid()


Tests:

[ ] Compare known hash vectors

[ ] Random generation test



==================================================
PHASE 11 — QUICKJS-NG MODULE
==================================================

Goal:

Embed JavaScript runtime.


Implement:

native/modules/js/


Object:

JSContext userdata


API:

local ctx=js.new()

ctx:eval(code)

ctx:set(name,value)

ctx:get(name)


Support:

[ ] Multiple contexts

[ ] Memory limit

[ ] Execution timeout

[ ] Exception reporting


Do NOT implement:

[ ] Browser DOM

[ ] window

[ ] document

[ ] fetch



==================================================
PHASE 12 — SELECT DSL ENGINE
==================================================

Goal:

Create high-level scraping API.


Example:


select
.from(doc)
:text("title","h1")
:attr("cover",".cover img","src")
:build()


Implementation:

Lua DSL

        |

Native execution plan

        |

Batch execution


Requirements:

[ ] Avoid repeated native transitions

[ ] Avoid temporary objects

[ ] Execute operations in batch


Tests:

[ ] Simple extraction

[ ] Multiple selector extraction

[ ] Large document extraction



==================================================
PHASE 13 — ANDROID MODULES
==================================================

Goal:

Expose Android functionality through Kotlin.


Modules:


android.http

android.storage

android.database

android.preferences

android.image

android.download

android.log


Rules:

Only Android modules communicate with Kotlin.


All processing remains native.



==================================================
PHASE 14 — SECURITY HARDENING
==================================================

Implement:


Luau:

[ ] Instruction limit

[ ] Memory limit


QuickJS:

[ ] Execution timeout

[ ] Memory quota


Native:

[ ] UTF-8 validation

[ ] Input validation

[ ] Pointer protection



==================================================
PHASE 15 — PERFORMANCE OPTIMIZATION
==================================================

Profile:

[ ] Luau execution

[ ] HTML parsing

[ ] JSON parsing

[ ] Regex

[ ] JavaScript execution


Optimize:


[ ] Zero-copy

[ ] Object pooling

[ ] Arena reuse

[ ] Cached metatables

[ ] Cached strings

[ ] Reduce allocations

[ ] Reduce JNI calls



==================================================
PHASE 16 — TESTING
==================================================

Unit tests:


[ ] Luau runtime

[ ] Module loading

[ ] HTML parsing

[ ] JSON

[ ] Regex

[ ] Crypto



Integration tests:


[ ] Kotlin JNI

[ ] Android lifecycle



Stress tests:


[ ] Large HTML

[ ] Large JSON

[ ] Long-running scripts



Memory tests:


[ ] Leak detection

[ ] Address Sanitizer

[ ] Undefined Behavior Sanitizer



Benchmarks:


[ ] HTML benchmark

[ ] JSON benchmark

[ ] Regex benchmark

[ ] Luau benchmark

[ ] QuickJS benchmark



==================================================
PHASE 17 — DOCUMENTATION
==================================================

Create:


docs/


[ ] architecture.md

[ ] memory.md

[ ] modules.md

[ ] jni.md

[ ] performance.md

[ ] submodules.md

[ ] patches.md

[ ] contribution.md



==================================================
MVP DEVELOPMENT ORDER
==================================================


MVP 1:

[ ] Luau VM

[ ] JNI

[ ] Module loader

[ ] Memory system

[ ] Buffer

[ ] HTML module

[ ] JSON module



MVP 2:

[ ] Regex

[ ] URL

[ ] Encoding

[ ] Crypto



MVP 3:

[ ] QuickJS

[ ] Select DSL

[ ] Android modules



FINAL GOAL:

Production-ready Native Luau SDK for Android.

Requirements:

- Native-first architecture
- Minimal JNI overhead
- No third-party forks
- Upgradeable dependencies
- Safe memory management
- High performance
- Modular design
