package com.luau.android

import java.io.Closeable

class LuauRuntime(memoryLimit: Long = 0) : Closeable {
    private var nativeHandle: Long = 0

    init {
        nativeHandle = nativeCreateRuntime(memoryLimit)
        if (nativeHandle == 0L) {
            throw LuauException("Failed to initialize native Luau runtime")
        }
    }

    /**
     * Executes the Luau source script.
     * @param source The Luau source code to run.
     * @param chunkName An optional name for the chunk (used in error stacktraces/backtraces).
     * @param timeoutSeconds Max execution time in seconds (0.0 for unlimited).
     * @throws LuauException if compilation or execution fails.
     */
    fun execute(source: String, chunkName: String? = null, timeoutSeconds: Double = 0.0) {
        check(nativeHandle != 0L) { "Runtime has been destroyed" }
        val error = nativeExecuteScript(nativeHandle, source, chunkName, timeoutSeconds)
        if (error != null) {
            throw LuauException(error)
        }
    }

    fun evalJson(source: String, timeoutSeconds: Double = 0.0): String? {
        check(nativeHandle != 0L) { "Runtime has been destroyed" }
        return nativeEvalJson(nativeHandle, source, timeoutSeconds)
    }

    override fun close() {
        if (nativeHandle != 0L) {
            nativeDestroyRuntime(nativeHandle)
            nativeHandle = 0L
        }
    }

    protected fun finalize() {
        close()
    }

    // Native functions
    private external fun nativeCreateRuntime(memoryLimit: Long): Long
    private external fun nativeDestroyRuntime(handle: Long)
    private external fun nativeExecuteScript(
        handle: Long,
        source: String,
        chunkName: String?,
        timeoutSeconds: Double
    ): String?
    private external fun nativeEvalJson(
        handle: Long,
        source: String,
        timeoutSeconds: Double
    ): String?

    companion object {
        init {
            try {
                System.loadLibrary("native_luau")
            } catch (e: UnsatisfiedLinkError) {
                // Ignore: might be loaded via absolute path in test environments
            }
        }
    }
}
