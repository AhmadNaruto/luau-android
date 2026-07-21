package com.luau.android

import org.junit.Assert.*
import org.junit.Assume
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import java.io.File

class LuauRuntimeTest {

    companion object {
        private var isNativeLoaded = false

        @BeforeClass
        @JvmStatic
        fun setUp() {
            val roots = listOf(File("."), File(".."), File("../android"), File("android"))
            for (r in roots) {
                val buildDir = File(r, "build")
                if (buildDir.exists() && buildDir.isDirectory) {
                    val found = buildDir.walkTopDown().find { it.name == "libnative_luau.so" }
                    if (found != null) {
                        try {
                            val stlFile = File(found.parentFile, "libc++_shared.so")
                            if (stlFile.exists()) {
                                System.load(stlFile.absolutePath)
                            }
                            System.load(found.absolutePath)
                            isNativeLoaded = true
                            break
                        } catch (e: UnsatisfiedLinkError) {
                            System.err.println("Native library architecture mismatch or load error: ${e.message}")
                        }
                    }
                }
            }
            if (!isNativeLoaded) {
                try {
                    System.loadLibrary("native_luau")
                    isNativeLoaded = true
                } catch (e: UnsatisfiedLinkError) {
                    System.err.println("Could not load library: ${e.message}")
                }
            }
        }
    }

    @Before
    fun checkNativeLibrary() {
        Assume.assumeTrue("Skipping native tests: libnative_luau.so is compiled for ARM64-v8a target ABI and cannot be loaded on host JVM architecture", isNativeLoaded)
    }

    @Test
    fun testExecuteSimple() {
        LuauRuntime().use { runtime ->
            runtime.execute("print('Hello from Luau!')")
        }
    }

    @Test
    fun testSyntaxError() {
        LuauRuntime().use { runtime ->
            try {
                runtime.execute("if true then") // Missing end
                fail("Should have syntax error")
            } catch (e: LuauException) {
                val msg = e.message?.lowercase() ?: ""
                assertTrue("Expected syntax error message but was '$msg'", msg.contains("expected"))
            }
        }
    }

    @Test
    fun testMemoryLimit() {
        // Create runtime with a limit of 512KB (enough to initialize, but will fail when allocating a large table)
        try {
            LuauRuntime(512 * 1024).use { runtime ->
                try {
                    // Try allocating a huge table
                    runtime.execute("local t = {}; for i = 1, 100000 do t[i] = i end")
                    fail("Should have run out of memory")
                } catch (e: LuauException) {
                    val msg = e.message?.lowercase() ?: ""
                    assertTrue("Expected memory error but was '$msg'", msg.contains("memory") || msg.contains("alloc") || msg.contains("limit") || msg.contains("out of"))
                }
            }
        } catch (e: LuauException) {
            // Memory limit exceeded during runtime initialization or load, which is also a valid memory limit enforcement
            val msg = e.message?.lowercase() ?: ""
            assertTrue("Expected memory error during init but was '$msg'", msg.contains("memory") || msg.contains("alloc") || msg.contains("limit") || msg.contains("failed to initialize"))
        }
    }

    @Test
    fun testTimeoutLimit() {
        LuauRuntime().use { runtime ->
            try {
                // Infinite loop with 0.1s timeout
                runtime.execute("while true do end", timeoutSeconds = 0.1)
                fail("Should have timed out")
            } catch (e: LuauException) {
                assertTrue(e.message?.contains("timeout") == true)
            }
        }
    }

    @Test
    fun testModuleLoading() {
        LuauRuntime().use { runtime ->
            val script = """
                local html = require("html")
                local json = require("json")
                local regex = require("regex")
                local select_mod = require("select")
                local js = require("js")
                local crypto = require("crypto")
                local buffer = require("buffer")
                local util = require("util")
                local time = require("time")
                
                assert(type(html) == "table")
                assert(type(json) == "table")
                assert(type(regex) == "table")
                assert(type(select_mod) == "table")
                assert(type(js) == "table")
                assert(type(crypto) == "table")
                assert(type(buffer) == "table")
                assert(type(util) == "table")
                assert(type(time) == "table")
                
                print("All modules loaded successfully!")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testJniProxy() {
        LuauRuntime().use { runtime ->
            val script = """
                local Jni = require("jni")
                local System = Jni.import("java/lang/System")
                
                -- Check static method call
                local time = System:currentTimeMillis()
                assert(type(time) == "number")
                assert(time > 0)
                
                -- Check static field retrieval and instance method call
                local out = System.out
                assert(type(out) == "userdata")
                out:println("Hello JNI Proxy from Luau script!")
                
                -- Check field access exception (non-existent field should trigger method call failure on invocation, or field get failure)
                local success, err = pcall(function()
                    local x = System.nonExistentField
                    -- If nonExistentField resolves to a method proxy closure, invoking it will fail
                    x()
                end)
                assert(not success)
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testBufferModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local buffer = require("buffer")
                local buf = buffer.create(1024)
                
                -- Check size
                assert(buf:size() == 1024)
                
                -- Check string read/write
                buf:writeString(0, "Hello")
                assert(buf:readString(0, 5) == "Hello")
                
                -- Check float64 read/write
                buf:writeFloat64(10, 123.456)
                assert(buf:readFloat64(10) == 123.456)
                
                -- Check int32 read/write
                buf:writeInt32(20, -987654)
                assert(buf:readInt32(20) == -987654)
                
                -- Check slice
                local sliced = buf:slice(0, 5)
                assert(sliced:size() == 5)
                assert(sliced:readString(0, 5) == "Hello")
                
                -- Check out of bounds error
                local success, err = pcall(function()
                    buf:readString(1020, 10)
                end)
                assert(not success)
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testHtmlModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local html = require("html")
                local doc = html.parse("<div><p class='test' id='p-id'>Hello</p></div>")
                
                -- Check root query
                local p = doc:querySelector(".test")
                assert(p ~= nil)
                assert(p:text() == "Hello")
                assert(p:tag() == "p")
                assert(p:attr("class") == "test")
                assert(p:attr("id") == "p-id")
                
                -- Check ID query
                local p_by_id = doc:querySelector("#p-id")
                assert(p_by_id ~= nil)
                assert(p_by_id:text() == "Hello")
                
                -- Check tag query
                local p_by_tag = doc:querySelector("p")
                assert(p_by_tag ~= nil)
                assert(p_by_tag:text() == "Hello")
                
                -- Check child querySelector
                local div = doc:querySelector("div")
                assert(div ~= nil)
                local p_child = div:querySelector(".test")
                assert(p_child ~= nil)
                assert(p_child:text() == "Hello")
                
                -- Check non-existent query
                local missing = doc:querySelector(".missing")
                assert(missing == nil)

                -- Verify memory model (refcounted document handle)
                local doc2 = html.parse("<div><span class='test2'>World</span></div>")
                local span = doc2:querySelector(".test2")
                assert(span ~= nil)
                doc2 = nil
                collectgarbage()

                -- The tree must NOT be freed yet because span still references the handle
                assert(span:text() == "World")
                span = nil
                collectgarbage()
                -- The tree is now freed safely
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testJsonModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local json = require("json")
                local obj = json.parse('{"a": 1, "b": [true, null, "ok"]}')
                
                -- Verify parsed values
                assert(obj.a == 1)
                assert(obj.b[1] == true)
                assert(obj.b[2] == nil)
                assert(obj.b[3] == "ok")
                
                -- Verify stringification
                local str = json.stringify(obj)
                assert(str ~= nil)
                
                -- Parse back and check again
                local obj2 = json.parse(str)
                assert(obj2.a == 1)
                assert(obj2.b[1] == true)
                assert(obj2.b[2] == nil)
                assert(obj2.b[3] == "ok")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testRegexModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local regex = require("regex")
                local re = regex.compile("(?<tag><[^>]+>)")
                
                -- Check matching
                local match = re:match("hello <div> world")
                assert(match ~= nil)
                assert(match[0] == "<div>") -- full match
                assert(match[1] == "<div>") -- capture group 1
                assert(match.tag == "<div>") -- named capture group
                
                -- Check non-matching
                local no_match = re:match("hello world")
                assert(no_match == nil)
                
                -- Check find with offset
                local re_num = regex.compile("\\d+")
                local f1 = re_num:find("abc 123 def 456", 0)
                assert(f1 ~= nil and f1[0] == "123")
                local f2 = re_num:find("abc 123 def 456", 7)
                assert(f2 ~= nil and f2[0] == "456")

                -- Check find_all
                local all = re_num:find_all("abc 123 def 456 ghi 789")
                assert(#all == 3, "expected 3 matches, got " .. #all)
                assert(all[1][0] == "123")
                assert(all[2][0] == "456")
                assert(all[3][0] == "789")

                -- Check replace (single & replace_all)
                local r1 = re_num:replace("apple 100 banana 200", "NUM")
                assert(r1 == "apple NUM banana 200", "replace single failed: " .. r1)
                local r2 = re_num:replace("apple 100 banana 200", "NUM", true)
                assert(r2 == "apple NUM banana NUM", "replace_all failed: " .. r2)

                -- Check replace with capture substitution $0 / $1
                local re_pair = regex.compile("(\\w+)\\s*=\\s*(\\d+)")
                local r3 = re_pair:replace("score = 100", "$1 points", false)
                assert(r3 == "score points", "replace substitution failed: " .. r3)

                -- Check split
                local parts = regex.compile(",\\s*"):split("apple, banana, cherry")
                assert(#parts == 3, "split length failed: " .. #parts)
                assert(parts[1] == "apple")
                assert(parts[2] == "banana")
                assert(parts[3] == "cherry")

                -- Check invalid regex
                local success, err = pcall(function()
                    regex.compile("([invalid")
                end)
                assert(not success)

                print("Regex module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testUrlModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local url = require("url")

                -- url.parse: full URL
                local parsed = url.parse("https://user:pass@example.com:8080/path/to/page?q=hello#section")
                assert(parsed ~= nil, "parse returned nil")
                assert(parsed.scheme == "https", "expected scheme=https, got " .. tostring(parsed.scheme))
                assert(parsed.host   == "example.com", "expected host=example.com, got " .. tostring(parsed.host))
                assert(parsed.port   == 8080, "expected port=8080, got " .. tostring(parsed.port))
                assert(parsed.path   ~= nil and #parsed.path > 0, "expected non-empty path")
                assert(parsed.href   ~= nil and #parsed.href > 0, "expected non-empty href")

                -- url.parse: simple URL
                local simple = url.parse("http://example.com/")
                assert(simple.scheme == "http", "expected http scheme")
                assert(simple.host   == "example.com", "expected example.com host")
                assert(simple.port   == nil, "expected nil port for default port")

                -- url.encode / url.decode round-trip
                local raw     = "hello world & foo=bar"
                local encoded = url.encode(raw)
                assert(encoded ~= nil, "encode returned nil")
                assert(not encoded:find(" "), "encoded string should have no spaces")
                local decoded = url.decode(encoded)
                assert(decoded == raw, "decode(encode(raw)) ~= raw: " .. tostring(decoded))

                -- url.decode: + as space
                local plus_decoded = url.decode("hello+world")
                assert(plus_decoded == "hello world", "expected + decoded to space")

                -- url.join: resolve relative URL against base
                local joined = url.join("https://example.com/base/page", "/other/path")
                assert(joined ~= nil, "join returned nil")
                assert(joined:find("example.com") ~= nil, "joined URL should contain host")
                assert(joined:find("/other/path") ~= nil, "joined URL should contain /other/path")

                print("URL module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testEncodingModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local enc = require("encoding")

                -- base64 encode/decode round-trip
                local raw = "Hello, World! \0\1\2\255"
                local b64 = enc.base64_encode(raw)
                assert(b64 ~= nil and #b64 > 0, "base64_encode returned empty")
                local decoded = enc.base64_decode(b64)
                assert(decoded == raw, "base64 round-trip failed")

                -- known vector
                assert(enc.base64_encode("Man") == "TWFu", "base64 encode 'Man' failed")
                assert(enc.base64_decode("TWFu") == "Man", "base64 decode 'TWFu' failed")
                assert(enc.base64_encode("Ma") == "TWE=", "base64 encode 'Ma' failed: " .. enc.base64_encode("Ma"))
                assert(enc.base64_encode("M") == "TQ==", "base64 encode 'M' failed")

                -- UTF-8 validation
                assert(enc.utf8_valid("hello") == true,  "ASCII should be valid UTF-8")
                assert(enc.utf8_valid("こんにちは") == true, "Japanese should be valid UTF-8")
                assert(enc.utf8_valid("\xFF\xFE") == false, "Invalid bytes should fail")

                -- UTF-8 codepoint length
                assert(enc.utf8_len("hello") == 5,   "ASCII len should be 5")
                assert(enc.utf8_len("こんにちは") == 5, "Japanese len should be 5")

                -- UTF-8 sub
                assert(enc.utf8_sub("hello world", 1, 5) == "hello", "utf8_sub 1..5 failed")
                assert(enc.utf8_sub("こんにちは", 2, 3) == "んに", "utf8_sub Japanese 2..3 failed")

                -- UTF-8 ↔ UTF-16LE round-trip
                local utf16 = enc.utf8_to_utf16le("hello")
                assert(utf16 ~= nil and #utf16 == 10, "UTF-16LE should be 10 bytes for 'hello'")
                local back = enc.utf16le_to_utf8(utf16)
                assert(back == "hello", "UTF-16LE→UTF-8 round-trip failed")

                -- Encoding detection
                assert(enc.detect("hello") == "ascii", "pure ASCII should detect as ascii")
                assert(enc.detect("こんにちは") == "utf-8", "Japanese should detect as utf-8")

                print("Encoding module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testCryptoModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local crypto = require("crypto")

                -- MD5 known vector: md5("") = "d41d8cd98f00b204e9800998ecf8427e"
                assert(crypto.md5("") == "d41d8cd98f00b204e9800998ecf8427e",
                    "MD5 empty failed: " .. crypto.md5(""))
                assert(crypto.md5("hello") == "5d41402abc4b2a76b9719d911017c592",
                    "MD5 hello failed: " .. crypto.md5("hello"))

                -- SHA256 known vector: sha256("") = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
                assert(crypto.sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                    "SHA256 empty failed")
                assert(crypto.sha256("hello") == "2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824",
                    "SHA256 hello failed: " .. crypto.sha256("hello"))

                -- SHA512 known vector
                local sha512_empty = crypto.sha512("")
                local sha512_expected = "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"
                assert(sha512_empty == sha512_expected,
                    "SHA512 empty failed, got: " .. sha512_empty)

                -- Binary output (as_hex = false)
                local raw_sha256 = crypto.sha256("hello", false)
                assert(type(raw_sha256) == "string" and #raw_sha256 == 32,
                    "SHA256 raw should be 32 bytes, got " .. #raw_sha256)

                -- HMAC-SHA256 known vector
                local hmac_result = crypto.hmac("key", "The quick brown fox jumps over the lazy dog", "sha256")
                assert(hmac_result == "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8",
                    "HMAC-SHA256 failed: " .. hmac_result)

                -- HMAC-MD5
                local hmac_md5 = crypto.hmac("key", "message", "md5")
                assert(type(hmac_md5) == "string" and #hmac_md5 == 32,
                    "HMAC-MD5 should return 32 hex chars")

                -- Random bytes
                local r1 = crypto.random(16)
                assert(type(r1) == "string" and #r1 == 16, "random(16) should be 16 bytes")
                local r2 = crypto.random(16)
                assert(r1 ~= r2, "two random(16) calls should differ")

                -- UUID v4 format check
                local uuid = crypto.uuid()
                assert(type(uuid) == "string" and #uuid == 36, "UUID should be 36 chars: " .. uuid)
                assert(uuid:sub(9,9) == "-" and uuid:sub(14,14) == "-" and
                       uuid:sub(19,19) == "-" and uuid:sub(24,24) == "-",
                    "UUID format invalid: " .. uuid)
                -- Version nibble should be '4'
                assert(uuid:sub(15,15) == "4", "UUID version nibble should be 4: " .. uuid)
                -- Variant nibble should be 8, 9, a, or b
                local variant = uuid:sub(20,20)
                assert(variant == "8" or variant == "9" or variant == "a" or variant == "b",
                    "UUID variant nibble invalid: " .. variant)

                print("Crypto module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testJsModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local js = require("js")

                -- Create JS context
                local ctx = js.new()
                assert(ctx ~= nil, "js.new() returned nil")

                -- eval: arithmetic
                local result = ctx:eval("1 + 2")
                assert(result == 3, "expected 3, got " .. tostring(result))

                -- eval: string
                local str = ctx:eval("'hello' + ' world'")
                assert(str == "hello world", "expected 'hello world', got " .. tostring(str))

                -- eval: boolean
                local b = ctx:eval("true && false")
                assert(b == false, "expected false")

                -- eval: null/undefined → nil
                local n = ctx:eval("null")
                assert(n == nil, "null should be nil")

                -- set + get round-trip
                ctx:set("luau_val", 42)
                local got = ctx:get("luau_val")
                assert(got == 42, "expected 42, got " .. tostring(got))

                ctx:set("luau_str", "world")
                local got_str = ctx:get("luau_str")
                assert(got_str == "world", "expected 'world', got " .. tostring(got_str))

                -- eval uses set variable
                local computed = ctx:eval("luau_val * 2")
                assert(computed == 84, "expected 84, got " .. tostring(computed))

                -- eval: JS function definition and call
                ctx:eval("function add(a, b) { return a + b; }")
                local sum = ctx:eval("add(10, 32)")
                assert(sum == 42, "expected 42, got " .. tostring(sum))

                -- Error handling
                local ok, err = pcall(function()
                    ctx:eval("throw new Error('test error')")
                end)
                assert(not ok, "should have thrown")
                assert(err ~= nil and tostring(err):find("test error") ~= nil,
                    "error message should contain 'test error', got: " .. tostring(err))

                -- Syntax error
                local ok2, err2 = pcall(function()
                    ctx:eval("if (")
                end)
                assert(not ok2, "syntax error should throw")

                -- GC does not crash
                ctx:gc()

                print("JS module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }
    @Test
    fun testSelectModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local html   = require("html")
                local select = require("select")

                local doc = html.parse([[
                    <html>
                    <head><title>Test Page</title></head>
                    <body>
                        <h1 class="title">Hello World</h1>
                        <img class="cover" src="https://example.com/cover.jpg" alt="cover">
                        <p id="desc">A description paragraph.</p>
                        <a href="https://example.com" class="link">Click here</a>
                    </body>
                    </html>
                ]])

                -- Fluent DSL: chain operations and batch-execute
                local result = select
                    .from(doc)
                    :text("title",       "title")
                    :text("heading",     "h1")
                    :attr("cover_url",   ".cover", "src")
                    :attr("cover_alt",   ".cover", "alt")
                    :text("desc",        "#desc")
                    :attr("link_href",   ".link",  "href")
                    :text("missing",     ".nonexistent")
                    :build()

                assert(result ~= nil, "build() returned nil")
                assert(result.title   == "Test Page",        "title: " .. tostring(result.title))
                assert(result.heading == "Hello World",      "heading: " .. tostring(result.heading))
                assert(result.cover_url == "https://example.com/cover.jpg",
                    "cover_url: " .. tostring(result.cover_url))
                assert(result.cover_alt == "cover",          "cover_alt: " .. tostring(result.cover_alt))
                assert(result.desc == "A description paragraph.",
                    "desc: " .. tostring(result.desc))
                assert(result.link_href == "https://example.com",
                    "link_href: " .. tostring(result.link_href))
                assert(result.missing == nil, "missing selector should return nil")

                -- Second independent build from same doc
                local result2 = select
                    .from(doc)
                    :attr("img_src", ".cover", "src")
                    :build()
                assert(result2.img_src == "https://example.com/cover.jpg",
                    "img_src: " .. tostring(result2.img_src))

                print("Select module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testUtilModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local util = require("util")

                -- trim
                assert(util.trim("   hello world   ") == "hello world")

                -- split & join
                local parts = util.split("a,b,c", ",")
                assert(#parts == 3 and parts[1] == "a" and parts[2] == "b" and parts[3] == "c")
                assert(util.join(parts, "-") == "a-b-c")

                -- startswith, endswith, contains
                assert(util.startswith("hello world", "hello") == true)
                assert(util.endswith("hello world", "world") == true)
                assert(util.contains("hello world", "lo wo") == true)

                -- lower, upper, capitalize
                assert(util.lower("Hello World") == "hello world")
                assert(util.upper("Hello World") == "HELLO WORLD")
                assert(util.capitalize("hello WORLD") == "Hello world")

                -- uuid
                local u = util.uuid()
                assert(type(u) == "string" and #u == 36)

                print("Util module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testTimeModule() {
        LuauRuntime().use { runtime ->
            val script = """
                local time = require("time")

                -- now & unix
                local now_val = time.now()
                assert(type(now_val) == "number" and now_val > 0)
                local unix_val = time.unix()
                assert(type(unix_val) == "number" and unix_val > 0)

                -- format & parse
                local formatted = time.format(1700000000, "%Y-%m-%d %H:%M:%S")
                assert(type(formatted) == "string" and #formatted > 0)

                print("Time module OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }

    @Test
    fun testNovelXSourcesCompatibilityShim() {
        LuauRuntime().use { runtime ->
            val script = """
                -- Test string & regex global helpers
                assert(string_trim("  hello  ") == "hello")
                assert(string_starts_with("https://site.com", "https"))
                assert(string_ends_with("image.png", ".png"))
                assert(string_contains("hello world", "world"))
                assert(string_clean("  hello   world  ") == "hello world")
                assert(url_resolve("https://example.com/base/", "page.html") == "https://example.com/base/page.html")
                assert(regex_replace("apple 123", "[0-9]+", "NUM") == "apple NUM")

                -- Test HTML global helpers
                local html_data = [[<div class="list"><a href="/book/1" title="Book Title">Link</a></div>]]
                local items = html_select(html_data, ".list a")
                assert(#items == 1)
                assert(items[1].href == "/book/1")
                assert(items[1].text == "Link")

                local first = html_select_first(html_data, ".list a")
                assert(first ~= nil and first.text == "Link")

                local title = html_attr(html_data, ".list a", "title")
                assert(title == "Book Title")

                print("novelx-sources compatibility shim OK")
            """.trimIndent()
            runtime.execute(script)
        }
    }
}
