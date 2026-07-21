package com.luau.android

import com.luau.android.lexsoup.LexSoup
import org.junit.Assert.*
import org.junit.Assume
import org.junit.Before
import org.junit.BeforeClass
import org.junit.Test
import java.io.File

class LexSoupTest {

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
                            System.err.println("Native library load error: ${e.message}")
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
        Assume.assumeTrue("Skipping LexSoup test on host architecture mismatch", isNativeLoaded)
    }

    @Test
    fun testLexSoupParsingAndSelection() {
        val html = """
            <html>
                <head><title>Test Page Title</title></head>
                <body>
                    <div class="container">
                        <h1 id="main-header">Hello LexSoup!</h1>
                        <ul class="book-list">
                            <li><a href="/book/1" class="item-link" data-id="101">Book One</a></li>
                            <li><a href="/book/2" class="item-link" data-id="102">Book Two</a></li>
                        </ul>
                    </div>
                </body>
            </html>
        """.trimIndent()

        LexSoup.parse(html).use { doc ->
            assertEquals("Test Page Title", doc.title())

            val header = doc.selectFirst("#main-header")
            assertNotNull(header)
            assertEquals("Hello LexSoup!", header?.text())

            val links = doc.select("ul.book-list a.item-link")
            assertEquals(2, links.size)

            val firstLink = links.first()
            assertNotNull(firstLink)
            assertEquals("/book/1", firstLink?.attr("href"))
            assertEquals("Book One", firstLink?.text())
            assertEquals("101", firstLink?.attr("data-id"))

            val secondLink = links[1]
            assertEquals("/book/2", secondLink.attr("href"))
            assertEquals("Book Two", secondLink.text())
        }
    }
}
