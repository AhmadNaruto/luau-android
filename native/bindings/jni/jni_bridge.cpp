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
    runtime->env = (void*)env;
    int status = luau_execute_script(runtime, source, chunkname, (double)timeout_seconds, &err_msg);
    runtime->env = NULL;

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

JNIEXPORT jstring JNICALL Java_com_luau_android_LuauRuntime_nativeEvalJson(
    JNIEnv *env, jobject thiz, jlong handle, jstring jsource, jdouble timeout_seconds) {

    luau_runtime_t *runtime = (luau_runtime_t*)(uintptr_t)handle;
    if (!runtime || !jsource) return NULL;

    const char *source = env->GetStringUTFChars(jsource, NULL);
    char *out_json = NULL;
    char *err_msg = NULL;

    runtime->env = (void*)env;
    int status = luau_eval_json(runtime, source, (double)timeout_seconds, &out_json, &err_msg);
    runtime->env = NULL;

    env->ReleaseStringUTFChars(jsource, source);

    if (status != 0) {
        if (err_msg) free(err_msg);
        return NULL;
    }

    jstring result = NULL;
    if (out_json) {
        result = env->NewStringUTF(out_json);
        free(out_json);
    }
    return result;
}

}
