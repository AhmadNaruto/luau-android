#include <jni.h>
#include "runtime.h"
#include <stdlib.h>
#include <stdint.h>

extern "C" {

JNIEXPORT jlong JNICALL Java_com_luau_android_LuauRuntime_nativeCreateRuntime(JNIEnv *env, jobject thiz, jlong memory_limit) {
    luau_runtime_t *runtime = luau_create_runtime((size_t)memory_limit);
    return (jlong)(uintptr_t)runtime;
}

JNIEXPORT void JNICALL Java_com_luau_android_LuauRuntime_nativeDestroyRuntime(JNIEnv *env, jobject thiz, jlong handle) {
    luau_runtime_t *runtime = (luau_runtime_t*)(uintptr_t)handle;
    if (runtime) {
        luau_destroy_runtime(runtime);
    }
}

JNIEXPORT jstring JNICALL Java_com_luau_android_LuauRuntime_nativeExecuteScript(
    JNIEnv *env, jobject thiz, jlong handle, jstring jsource, jstring jchunkname, jdouble timeout_seconds) {
    
    luau_runtime_t *runtime = (luau_runtime_t*)(uintptr_t)handle;
    if (!runtime) {
        return env->NewStringUTF("Invalid runtime handle");
    }

    if (!jsource) {
        return env->NewStringUTF("Source script cannot be null");
    }

    const char *source = env->GetStringUTFChars(jsource, NULL);
    const char *chunkname = NULL;
    if (jchunkname) {
        chunkname = env->GetStringUTFChars(jchunkname, NULL);
    }

    char *err_msg = NULL;
    int status = luau_execute_script(runtime, source, chunkname, (double)timeout_seconds, &err_msg);

    env->ReleaseStringUTFChars(jsource, source);
    if (jchunkname && chunkname) {
        env->ReleaseStringUTFChars(jchunkname, chunkname);
    }

    if (status != 0) {
        jstring jerr = env->NewStringUTF(err_msg ? err_msg : "Unknown error");
        if (err_msg) free(err_msg);
        return jerr;
    }

    return NULL; // Success
}

}
