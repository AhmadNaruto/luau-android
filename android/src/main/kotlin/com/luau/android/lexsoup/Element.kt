package com.luau.android.lexsoup

class Element internal constructor(
    internal val nodePtr: Long,
    internal val docHandlePtr: Long
) {
    fun select(cssQuery: String): Elements {
        if (nodePtr == 0L || docHandlePtr == 0L) return Elements(emptyList())
        val ptrs = LexSoupBridge.nativeSelect(docHandlePtr, nodePtr, cssQuery)
        return Elements(ptrs.map { Element(it, docHandlePtr) })
    }

    fun selectFirst(cssQuery: String): Element? {
        if (nodePtr == 0L || docHandlePtr == 0L) return null
        val ptr = LexSoupBridge.nativeSelectFirst(docHandlePtr, nodePtr, cssQuery)
        return if (ptr != 0L) Element(ptr, docHandlePtr) else null
    }

    fun attr(attributeKey: String): String {
        if (nodePtr == 0L) return ""
        return LexSoupBridge.nativeAttr(nodePtr, attributeKey)
    }

    fun hasAttr(attributeKey: String): Boolean {
        return attr(attributeKey).isNotEmpty()
    }

    fun text(): String {
        if (nodePtr == 0L) return ""
        return LexSoupBridge.nativeText(nodePtr).trim()
    }

    fun tagName(): String {
        if (nodePtr == 0L) return ""
        return LexSoupBridge.nativeTagName(nodePtr)
    }

    fun id(): String = attr("id")

    fun className(): String = attr("class")

    override fun toString(): String = text()
}
