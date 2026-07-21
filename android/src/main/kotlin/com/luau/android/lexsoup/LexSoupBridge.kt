package com.luau.android.lexsoup

internal object LexSoupBridge {
    init {
        try {
            System.loadLibrary("native_luau")
        } catch (_: UnsatisfiedLinkError) {
        }
    }

    @JvmStatic
    external fun nativeParse(html: String): Long

    @JvmStatic
    external fun nativeFreeDoc(handlePtr: Long)

    @JvmStatic
    external fun nativeGetRootNode(handlePtr: Long): Long

    @JvmStatic
    external fun nativeSelect(handlePtr: Long, nodePtr: Long, selector: String): LongArray

    @JvmStatic
    external fun nativeSelectFirst(handlePtr: Long, nodePtr: Long, selector: String): Long

    @JvmStatic
    external fun nativeAttr(nodePtr: Long, attrName: String): String

    @JvmStatic
    external fun nativeText(nodePtr: Long): String

    @JvmStatic
    external fun nativeTagName(nodePtr: Long): String
}
