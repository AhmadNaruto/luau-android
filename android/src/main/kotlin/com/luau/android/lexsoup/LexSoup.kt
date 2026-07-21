package com.luau.android.lexsoup

class LexSoup private constructor() {
    companion object {
        @JvmStatic
        fun parse(html: String): Document {
            val handlePtr = LexSoupBridge.nativeParse(html)
            return Document(handlePtr)
        }

        @JvmStatic
        fun parse(html: String, baseUri: String): Document {
            return parse(html)
        }
    }
}
