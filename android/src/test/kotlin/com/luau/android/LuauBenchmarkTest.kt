package com.luau.android

import org.junit.Assume
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import java.io.File
import kotlin.system.measureNanoTime

class LuauBenchmarkTest {

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
        Assume.assumeTrue("Skipping benchmark: libnative_luau.so is compiled for ARM64-v8a target ABI and cannot be loaded on host JVM architecture", isNativeLoaded)
    }

    private inline fun measureMs(block: () -> Unit): Double {
        return measureNanoTime(block) / 1_000_000.0
    }

    @Test
    fun runAllBenchmarks() {
        println("==================================================")
        println("        LUAU NATIVE SDK BENCHMARK SUITE          ")
        println("==================================================")

        LuauRuntime().use { runtime ->
            // 1. Luau VM Core Performance
            val luauScript = """
                local sum = 0
                for i = 1, 1000000 do
                    sum = sum + (i % 7)
                end
                return sum
            """.trimIndent()
            // Warmup
            runtime.execute(luauScript)
            val luauTime = measureMs {
                for (i in 1..10) {
                    runtime.execute(luauScript)
                }
            } / 10.0
            println("[1/6] Luau Core Math (1,000,000 iterations): %.2f ms / run".format(luauTime))

            // 2. Lexbor HTML Parsing Benchmark
            val htmlData = StringBuilder().apply {
                append("<html><body>")
                for (i in 1..1000) {
                    append("<div class='item' id='item-$i'><a href='https://example.com/$i'>Item $i</a><p>Description text $i</p></div>")
                }
                append("</body></html>")
            }.toString()
            val htmlScript = """
                local html = require("html")
                local doc = html.parse([[$htmlData]])
                local count = 0
                for i = 1, 100 do
                    local el = doc:querySelector("#item-" .. i)
                    if el then count = count + 1 end
                end
                return count
            """.trimIndent()
            runtime.execute(htmlScript) // Warmup
            val htmlTime = measureMs {
                runtime.execute(htmlScript)
            }
            println("[2/6] Lexbor HTML Parsing & 100 CSS Queries (100KB HTML): %.2f ms".format(htmlTime))

            // 3. yyjson JSON Processing Benchmark
            val jsonData = StringBuilder().apply {
                append("{\"status\":\"ok\",\"items\":[")
                for (i in 1..500) {
                    if (i > 1) append(",")
                    append("{\"id\":$i,\"name\":\"Product $i\",\"price\":${i * 1.5},\"active\":true}")
                }
                append("]}")
            }.toString()
            val jsonScript = """
                local json = require("json")
                local payload = [[$jsonData]]
                for i = 1, 100 do
                    local obj = json.parse(payload)
                    local str = json.stringify(obj)
                end
            """.trimIndent()
            runtime.execute(jsonScript) // Warmup
            val jsonTime = measureMs {
                runtime.execute(jsonScript)
            }
            println("[3/6] yyjson JSON Parse + Stringify (100 iterations of 500 items): %.2f ms".format(jsonTime))

            // 4. PCRE2 Regex Benchmark
            val regexScript = """
                local regex = require("regex")
                local pattern = regex.compile([[https?://[a-zA-Z0-9.-]+/[a-zA-Z0-9./_?=-]+]])
                local text = "Find links: https://example.com/item/1 and http://test.org/api?v=2 and https://sub.domain.com/path"
                local matches = 0
                for i = 1, 1000 do
                    local list = pattern:find_all(text)
                    matches = matches + #list
                end
                return matches
            """.trimIndent()
            runtime.execute(regexScript) // Warmup
            val regexTime = measureMs {
                runtime.execute(regexScript)
            }
            println("[4/6] PCRE2 Regex find_all (1,000 iterations): %.2f ms".format(regexTime))

            // 5. QuickJS-NG JS Engine Benchmark
            val jsScript = """
                local js = require("js")
                local ctx = js.new()
                ctx:eval("function fib(n) { return n <= 1 ? n : fib(n-1) + fib(n-2); }")
                local sum = 0
                for i = 1, 100 do
                    sum = sum + ctx:eval("fib(15)")
                end
                return sum
            """.trimIndent()
            runtime.execute(jsScript) // Warmup
            val jsTime = measureMs {
                runtime.execute(jsScript)
            }
            println("[5/6] QuickJS-NG JS Engine (100 runs of fib(15)): %.2f ms".format(jsTime))

            // 6. OpenSSL Crypto Benchmark
            val cryptoScript = """
                local crypto = require("crypto")
                local data = "The quick brown fox jumps over the lazy dog"
                for i = 1, 5000 do
                    local hash1 = crypto.sha256(data)
                    local hash2 = crypto.md5(data)
                    local hmac = crypto.hmac("key", data, "sha256")
                end
            """.trimIndent()
            runtime.execute(cryptoScript) // Warmup
            val cryptoTime = measureMs {
                runtime.execute(cryptoScript)
            }
            println("[6/6] OpenSSL Crypto (5,000 SHA256 + MD5 + HMAC operations): %.2f ms".format(cryptoTime))

            println("==================================================")
            println("        BENCHMARK EXECUTION COMPLETED             ")
            println("==================================================")
        }
    }
}
