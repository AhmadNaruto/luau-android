package com.luau.android.lexsoup

class Elements(private val list: List<Element> = emptyList()) : List<Element> by list {

    fun select(cssQuery: String): Elements {
        val result = mutableListOf<Element>()
        for (el in list) {
            result.addAll(el.select(cssQuery))
        }
        return Elements(result)
    }

    fun first(): Element? = list.firstOrNull()

    fun last(): Element? = list.lastOrNull()

    fun attr(attributeKey: String): String {
        return first()?.attr(attributeKey) ?: ""
    }

    fun text(): String {
        return list.joinToString(" ") { it.text() }.trim()
    }

    fun hasText(): Boolean = text().isNotEmpty()

    override fun toString(): String = text()
}
