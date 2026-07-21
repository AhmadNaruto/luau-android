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
}
