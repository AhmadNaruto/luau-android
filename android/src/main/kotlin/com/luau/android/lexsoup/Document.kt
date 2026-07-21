package com.luau.android.lexsoup

import java.io.Closeable

class Document internal constructor(
    internal val docHandlePtr: Long
) : Closeable {

    private val rootNodePtr: Long = if (docHandlePtr != 0L) {
        LexSoupBridge.nativeGetRootNode(docHandlePtr)
    } else 0L

    fun select(cssQuery: String): Elements {
        if (docHandlePtr == 0L || rootNodePtr == 0L) return Elements(emptyList())
        val ptrs = LexSoupBridge.nativeSelect(docHandlePtr, rootNodePtr, cssQuery)
        return Elements(ptrs.map { Element(it, docHandlePtr) })
    }

    fun selectFirst(cssQuery: String): Element? {
        if (docHandlePtr == 0L || rootNodePtr == 0L) return null
        val ptr = LexSoupBridge.nativeSelectFirst(docHandlePtr, rootNodePtr, cssQuery)
        return if (ptr != 0L) Element(ptr, docHandlePtr) else null
    }

    fun body(): Element {
        return selectFirst("body") ?: Element(rootNodePtr, docHandlePtr)
    }

    fun title(): String {
        return selectFirst("title")?.text() ?: ""
    }

    fun text(): String {
        if (rootNodePtr == 0L) return ""
        return LexSoupBridge.nativeText(rootNodePtr).trim()
    }

    override fun close() {
        if (docHandlePtr != 0L) {
            LexSoupBridge.nativeFreeDoc(docHandlePtr)
        }
    }

    protected fun finalize() {
        close()
    }
}
