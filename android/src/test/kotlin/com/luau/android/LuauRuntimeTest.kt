package com.luau.android

import org.junit.Assert.*
import org.junit.BeforeClass
import org.junit.Test
import java.io.File

class LuauRuntimeTest {

    companion object {
        @BeforeClass
        @JvmStatic
        fun setUp() {
            // Search for libnative_luau.so recursively in nearby build folders
            val roots = listOf(File("."), File(".."), File("../android"), File("android"))
            var loaded = false
            for (r in roots) {
                val buildDir = File(r, "build")
                if (buildDir.exists() && buildDir.isDirectory) {
                    val found = buildDir.walkTopDown().find { it.name == "libnative_luau.so" }
                    if (found != null) {
                        val stlFile = File(found.parentFile, "libc++_shared.so")
                        if (stlFile.exists()) {
                            System.load(stlFile.absolutePath)
                        }
                        System.load(found.absolutePath)
                        loaded = true
                        break
                    }
                }
            }
            if (!loaded) {
                try {
                    System.loadLibrary("native_luau")
                } catch (e: UnsatisfiedLinkError) {
                    System.err.println("Could not load library: ${e.message}")
                }
            }
        }
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
                
                -- Check invalid regex
                local success, err = pcall(function()
                    regex.compile("([invalid")
                end)
                assert(not success)
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
}
