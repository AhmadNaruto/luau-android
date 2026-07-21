# Native Luau Android SDK — Complete API Reference

This document provides a comprehensive API reference for the **Native Luau Android SDK** (`arm64-v8a`).

---

## Table of Contents

- [Kotlin API](#kotlin-api)
  - [LuauRuntime](#luauruntime)
  - [LuauException](#luauexception)
  - [LexSoup (Drop-in JSoup Replacement)](#lexsoup-drop-in-jsoup-replacement)
- [Native Luau Modules](#native-luau-modules)
  - [html (Lexbor HTML Parser)](#1-html-lexbor)
  - [js (QuickJS-NG Engine)](#2-js-quickjs-ng)
  - [json (yyjson Parser)](#3-json-yyjson)
  - [regex (PCRE2 Engine)](#4-regex-pcre2)
  - [url (Lexbor URL)](#5-url-lexbor)
  - [encoding (Lexbor Encoding)](#6-encoding-lexbor)
  - [crypto (Pure C Cryptography)](#7-crypto-pure-c-cryptography)
  - [buffer (Zero-Copy Data Buffer)](#8-buffer-zero-copy)
  - [select (Fluent Extraction DSL)](#9-select-scraping-dsl)
  - [util (Helper Utilities)](#10-util-utilities)
  - [time (Time & Timing)](#11-time-date--timing)
  - [jni (Java Native Interface Proxy)](#12-jni-android-java-proxy)
  - [novelx-sources Global Compatibility Shim](#13-novelx-sources-global-compatibility-shim)

---

## Kotlin API

### `LuauRuntime`
`com.luau.android.LuauRuntime`

Class wrapping an embedded Luau VM instance. Implements `AutoCloseable`.

#### Constructor
```kotlin
LuauRuntime(memoryLimitBytes: Long = 0)
```
- `memoryLimitBytes`: Max memory quota for the Luau VM in bytes (0 = unlimited).

#### Methods

```kotlin
fun execute(script: String, timeoutSeconds: Double = 0.0): Any?
```
Compiles and executes a Luau script string.
- `script`: Code string to execute.
- `timeoutSeconds`: Max allowed execution time in seconds (0.0 = unlimited).
- **Returns:** Result returned by the script (or `null`).
- **Throws:** `LuauException` on syntax error, runtime error, memory limit, or timeout.

```kotlin
override fun close()
```
Destroys the Luau VM and frees all associated native memory resources.

---

### `LuauException`
`com.luau.android.LuauException : Exception`

Exception thrown when script compilation, execution, memory allocation, or timeout fails.

---

### `LexSoup` (Drop-in JSoup Replacement)
`com.luau.android.lexsoup.LexSoup`

High-speed native C++ replacement for JSoup powered by the **Lexbor** HTML engine.

#### Classes & Methods
```kotlin
// Entry point
LexSoup.parse(html: String): Document

// Document
doc.select(cssQuery: String): Elements
doc.selectFirst(cssQuery: String): Element?
doc.title(): String
doc.body(): Element
doc.text(): String
doc.close()

// Element
element.select(cssQuery: String): Elements
element.selectFirst(cssQuery: String): Element?
element.attr(attributeKey: String): String
element.hasAttr(attributeKey: String): Boolean
element.text(): String
element.tagName(): String
element.id(): String
element.className(): String

// Elements (List<Element>)
elements.first(): Element?
elements.last(): Element?
elements.attr(attributeKey: String): String
elements.text(): String
elements.size(): Int
```

---

## Native Luau Modules

Modules are loaded in Luau via `require("module_name")`.

---

### 1. `html` (Lexbor)
High-performance HTML parser & DOM engine.

#### Module Methods
```lua
local html = require("html")
local doc = html.parse(html_string)
```
- `html.parse(html_str: string) -> Document`: Parses an HTML string and returns a refcounted `Document` userdata.

#### Document Methods
```lua
doc:querySelector(selector: string) -> Element | nil
```
Finds the first matching element using CSS selector (`.class`, `#id`, `tag`).

```lua
doc:querySelectorAll(selector: string) -> { Element }
```
Finds all matching elements as an array table.

#### Element Methods
```lua
el:text() -> string
```
Extracts inner text of the element recursively.

```lua
el:tag() -> string
```
Returns lowercased tag name (e.g., `"div"`, `"a"`).

```lua
el:attr(name: string) -> string | nil
```
Returns attribute value by name.

```lua
el:querySelector(selector: string) -> Element | nil
el:querySelectorAll(selector: string) -> { Element }
```
Performs query scoped to child nodes of `el`.

---

### 2. `js` (QuickJS-NG)
Embedded JavaScript execution engine.

#### Module Methods
```lua
local js = require("js")
local ctx = js.new([memory_limit_bytes])
```
Creates a new isolated JSContext.

#### JSContext Methods
```lua
ctx:eval(code: string) -> any
```
Evaluates JavaScript code and converts result to Luau primitive (`number`, `string`, `boolean`, `nil`).

```lua
ctx:set(name: string, value: any)
```
Sets a global variable inside the JS context.

```lua
ctx:get(name: string) -> any
```
Gets a global variable from the JS context.

```lua
ctx:gc()
```
Triggers QuickJS garbage collection.

---

### 3. `json` (yyjson)
High-speed C JSON parser.

#### Module Methods
```lua
local json = require("json")
```

```lua
json.parse(json_str: string) -> table | any
json.decode(json_str: string) -> table | any
```
Parses JSON string into Luau tables / primitives.

```lua
json.stringify(value: table | any) -> string
json.encode(value: table | any) -> string
```
Serializes Luau table / primitive into JSON string.

---

### 4. `regex` (PCRE2)
Native Perl-compatible Regular Expressions.

#### Module Methods
```lua
local regex = require("regex")
local re = regex.compile(pattern: string [, flags: string])
```
Compiles pattern. `flags`: `"i"` (caseless), `"m"` (multiline), `"s"` (dotall), `"x"` (extended).

#### Pattern Methods
```lua
re:match(subject: string) -> table | nil
```
Matches subject. Returns table where `[0]` is full match, `[1..N]` are capture groups, and named captures are table fields.

```lua
re:find(subject: string [, offset: number]) -> table | nil
```
Finds match starting from 0-based byte offset.

```lua
re:find_all(subject: string) -> { table }
```
Returns array of match tables for all non-overlapping occurrences.

```lua
re:replace(subject: string, replacement: string [, replace_all: boolean]) -> string
```
Replaces matches with replacement. Supports `$0`..`$9` and `${name}` substitutions.

```lua
re:split(subject: string [, limit: number]) -> { string }
```
Splits subject string by regex delimiter.

---

### 5. `url` (Lexbor)
URL parsing & resolution.

#### Module Methods
```lua
local url = require("url")
```

```lua
url.parse(url_str: string) -> table | nil
```
Returns `{ scheme = "...", host = "...", port = 8080, path = "...", query = "...", fragment = "...", href = "..." }`.

```lua
url.join(base_url: string, relative_path: string) -> string
url.resolve(base_url: string, relative_path: string) -> string
```
Resolves relative path against base URL.

```lua
url.encode(str: string) -> string
```
URL component percent-encoding.

```lua
url.decode(str: string) -> string
```
URL component percent-decoding (`+` decoded as space).

---

### 6. `encoding` (Lexbor Encoding)
Binary & text encodings.

#### Module Methods
```lua
local enc = require("encoding")
```

```lua
enc.base64_encode(str: string) -> string
enc.base64_decode(b64_str: string) -> string
```
Base64 encoding/decoding.

```lua
enc.utf8_valid(str: string) -> boolean
```
Returns `true` if string is valid UTF-8.

```lua
enc.utf8_len(str: string) -> number
```
Returns UTF-8 codepoint length (character count).

```lua
enc.utf8_sub(str: string, start: number, length: number) -> string
```
Substrings by UTF-8 codepoint boundaries (1-based index).

```lua
enc.utf8_to_utf16le(str: string) -> string
enc.utf16le_to_utf8(raw_bytes: string) -> string
```
UTF-8 ↔ UTF-16LE conversion.

```lua
enc.detect(str: string) -> string
```
Detects text encoding (e.g. `"ascii"`, `"utf-8"`).

---

### 7. `crypto` (Pure C Cryptography)
Lightweight, standalone C implementations for hash digests and cryptographic functions (MD5, SHA1, SHA256, SHA512, HMAC, secure random, UUID v4).

#### Module Methods
```lua
local crypto = require("crypto")
```

```lua
crypto.md5(data: string [, as_hex: boolean = true]) -> string
crypto.sha1(data: string [, as_hex: boolean = true]) -> string
crypto.sha256(data: string [, as_hex: boolean = true]) -> string
crypto.sha512(data: string [, as_hex: boolean = true]) -> string
```
Computes message digest (hex string or raw bytes).

```lua
crypto.hmac(key: string, data: string, algo: string [, as_hex: boolean = true]) -> string
```
Computes HMAC (algo: `"sha256"`, `"md5"`, `"sha1"`, `"sha512"`).

```lua
crypto.random(bytes_count: number) -> string
```
Generates cryptographically secure random bytes.

```lua
crypto.uuid() -> string
```
Generates a UUID v4 string (`"xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"`).

---

### 8. `buffer` (Zero-Copy)
Native typed memory buffers.

#### Module Methods
```lua
local buffer = require("buffer")
local buf = buffer.create(size_in_bytes: number)
```

#### Buffer Methods
```lua
buf:size() -> number
buf:writeString(offset: number, str: string)
buf:readString(offset: number, length: number) -> string
buf:writeFloat64(offset: number, val: number)
buf:readFloat64(offset: number) -> number
buf:writeInt32(offset: number, val: number)
buf:readInt32(offset: number) -> number
buf:slice(offset: number, length: number) -> Buffer
```

---

### 9. `select` (Scraping DSL)
Batched extraction DSL engine.

#### Module Usage
```lua
local select = require("select")

local result = select
    .from(doc)
    :text(key: string, selector: string)
    :attr(key: string, selector: string, attr_name: string)
    :html(key: string, selector: string)
    :build() -> table
```
Compiles operations into a native execution plan and extracts fields in a single batch pass.

---

### 10. `util` (Utilities)
String & general helpers.

#### Module Methods
```lua
local util = require("util")

util.trim(str) -> string
util.split(str, sep) -> { string }
util.join(tbl, sep) -> string
util.startswith(str, prefix) -> boolean
util.endswith(str, suffix) -> boolean
util.contains(str, substr) -> boolean
util.lower(str) -> string
util.upper(str) -> string
util.capitalize(str) -> string
util.sleep(ms: number)
util.uuid() -> string
```

---

### 11. `time` (Date & Timing)
Time & timestamp functions.

#### Module Methods
```lua
local time = require("time")

time.now() -> number -- epoch float with microsecond precision
time.unix() -> number -- integer epoch seconds
time.format([ts: number, fmt: string]) -> string -- strftime format
time.parse(date_str: string [, fmt: string]) -> number | nil
time.sleep(ms: number)
```

---

### 12. `jni` (Android Java Proxy)
Java class reflection & invocation from Luau.

```lua
local Jni = require("jni")
local System = Jni.import("java/lang/System")
local currentTime = System.currentTimeMillis()
System.out:println("Printed via JNI!")
```

---

### 13. `novelx-sources` Global Compatibility Shim
Pre-injected global helper functions available automatically without requiring `require()` calls:

```lua
-- HTML Helpers
html_select(body_html, selector) -> { { text: string, html: string, href: string } }
html_select_first(body_html, selector) -> { text: string, html: string, href: string } | nil
html_attr(html_str, selector, attr_name) -> string
html_text(html_str, selector) -> string

-- URL & String Helpers
url_resolve(base_url, href) -> string
url_encode(str) -> string
url_decode(str) -> string

string_trim(str) -> string
string_starts_with(str, prefix) -> boolean
string_ends_with(str, suffix) -> boolean
string_contains(str, substr) -> boolean
string_lower(str) -> string
string_upper(str) -> string
string_capitalize(str) -> string
string_normalize(str) -> string
string_clean(str) -> string

-- Regex Helpers
regex_replace(text, pattern, replacement) -> string
regex_match(text, pattern) -> table | nil
```
