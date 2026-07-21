#include "proxy.h"
#include "runtime.h"
#include "lualib.h"
#include <jni.h>
#include <stdlib.h>
#include <string.h>

// Userdata layout for representing JNI references in Luau
typedef struct {
    jobject obj;
    bool is_class;
} jni_ref_t;

// Global JNI Cache structure
static jclass g_helper_class = NULL;
static jclass g_boolean_class = NULL;
static jclass g_number_class = NULL;
static jclass g_string_class = NULL;
static jclass g_class_class = NULL;

static jmethodID g_importClass = NULL;
static jmethodID g_getField = NULL;
static jmethodID g_setField = NULL;
static jmethodID g_invokeMethod = NULL;

static jmethodID g_booleanValue = NULL;
static jmethodID g_doubleValue = NULL;
static jmethodID g_booleanValueOf = NULL;
static jmethodID g_doubleValueOf = NULL;

static void init_jni_cache(JNIEnv *env) {
    if (g_helper_class != NULL) return; // Already initialized

    jclass local_helper = env->FindClass("com/luau/android/JniProxyHelper");
    if (local_helper) {
        g_helper_class = (jclass)env->NewGlobalRef(local_helper);
        env->DeleteLocalRef(local_helper);
    }

    jclass local_boolean = env->FindClass("java/lang/Boolean");
    g_boolean_class = (jclass)env->NewGlobalRef(local_boolean);
    env->DeleteLocalRef(local_boolean);

    jclass local_number = env->FindClass("java/lang/Number");
    g_number_class = (jclass)env->NewGlobalRef(local_number);
    env->DeleteLocalRef(local_number);

    jclass local_string = env->FindClass("java/lang/String");
    g_string_class = (jclass)env->NewGlobalRef(local_string);
    env->DeleteLocalRef(local_string);

    jclass local_class = env->FindClass("java/lang/Class");
    g_class_class = (jclass)env->NewGlobalRef(local_class);
    env->DeleteLocalRef(local_class);

    g_importClass = env->GetStaticMethodID(g_helper_class, "importClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    g_getField = env->GetStaticMethodID(g_helper_class, "getField", "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
    g_setField = env->GetStaticMethodID(g_helper_class, "setField", "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/Object;)V");
    g_invokeMethod = env->GetStaticMethodID(g_helper_class, "invokeMethod", "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");

    g_booleanValue = env->GetMethodID(g_boolean_class, "booleanValue", "()Z");
    g_doubleValue = env->GetMethodID(g_number_class, "doubleValue", "()D");

    g_booleanValueOf = env->GetStaticMethodID(g_boolean_class, "valueOf", "(Z)Ljava/lang/Boolean;");
    g_doubleValueOf = env->GetStaticMethodID(env->FindClass("java/lang/Double"), "valueOf", "(D)Ljava/lang/Double;");
}

static int push_jni_ref(lua_State *L, jobject obj, bool is_class, JNIEnv *env) {
    if (!obj) {
        lua_pushnil(L);
        return 1;
    }

    jni_ref_t *ref = (jni_ref_t*)lua_newuserdata(L, sizeof(jni_ref_t));
    ref->obj = env->NewGlobalRef(obj);
    ref->is_class = is_class;

    luaL_getmetatable(L, "JniRef");
    lua_setmetatable(L, -2);
    return 1;
}

static jobject lua_to_jobject(lua_State *L, int index, JNIEnv *env) {
    init_jni_cache(env);

    if (lua_isnil(L, index)) {
        return NULL;
    }
    if (lua_isboolean(L, index)) {
        jboolean val = lua_toboolean(L, index) ? JNI_TRUE : JNI_FALSE;
        return env->CallStaticObjectMethod(g_boolean_class, g_booleanValueOf, val);
    }
    if (lua_isnumber(L, index)) {
        jdouble val = lua_tonumber(L, index);
        return env->CallStaticObjectMethod(env->FindClass("java/lang/Double"), g_doubleValueOf, val);
    }
    if (lua_isstring(L, index)) {
        const char *str = lua_tostring(L, index);
        return env->NewStringUTF(str);
    }
    if (lua_isuserdata(L, index)) {
        jni_ref_t *ref = (jni_ref_t*)lua_touserdata(L, index);
        if (ref && ref->obj) {
            return env->NewLocalRef(ref->obj);
        }
    }
    return NULL;
}

static void jobject_to_lua(lua_State *L, jobject obj, JNIEnv *env) {
    init_jni_cache(env);

    if (!obj) {
        lua_pushnil(L);
        return;
    }

    if (env->IsInstanceOf(obj, g_boolean_class)) {
        jboolean val = env->CallBooleanMethod(obj, g_booleanValue);
        lua_pushboolean(L, val == JNI_TRUE ? 1 : 0);
        return;
    }
    if (env->IsInstanceOf(obj, g_number_class)) {
        jdouble val = env->CallDoubleMethod(obj, g_doubleValue);
        lua_pushnumber(L, val);
        return;
    }
    if (env->IsInstanceOf(obj, g_string_class)) {
        jstring jstr = (jstring)obj;
        const char *str = env->GetStringUTFChars(jstr, NULL);
        lua_pushstring(L, str);
        env->ReleaseStringUTFChars(jstr, str);
        return;
    }

    bool is_class = env->IsInstanceOf(obj, g_class_class) == JNI_TRUE;
    push_jni_ref(L, obj, is_class, env);
}

// Upvalues: [target_ref_userdata, method_name_string]
static int jni_method_call(lua_State *L) {
    luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
    if (!runtime || !runtime->env) {
        luaL_error(L, "JNI environment is not available");
        return 0;
    }
    JNIEnv *env = (JNIEnv*)runtime->env;
    init_jni_cache(env);

    jni_ref_t *target_ref = (jni_ref_t*)lua_touserdata(L, lua_upvalueindex(1));
    const char *method_name = lua_tostring(L, lua_upvalueindex(2));

    int start_arg = 1;
    // Skip target if called via colon syntax
    if (lua_gettop(L) >= 1 && lua_isuserdata(L, 1)) {
        jni_ref_t *arg1_ref = (jni_ref_t*)lua_touserdata(L, 1);
        if (arg1_ref && arg1_ref->obj == target_ref->obj) {
            start_arg = 2;
        }
    }

    int num_params = lua_gettop(L) - start_arg + 1;
    if (num_params < 0) num_params = 0;

    jclass obj_class = env->FindClass("java/lang/Object");
    jobjectArray jargs = env->NewObjectArray(num_params, obj_class, NULL);
    for (int i = 0; i < num_params; ++i) {
        jobject jarg = lua_to_jobject(L, start_arg + i, env);
        env->SetObjectArrayElement(jargs, i, jarg);
        if (jarg) env->DeleteLocalRef(jarg);
    }

    jstring jmethod_name = env->NewStringUTF(method_name);
    jobject result = env->CallStaticObjectMethod(g_helper_class, g_invokeMethod, target_ref->obj, jmethod_name, jargs);
    env->DeleteLocalRef(jmethod_name);
    env->DeleteLocalRef(jargs);

    if (env->ExceptionCheck()) {
        jthrowable ex = env->ExceptionOccurred();
        env->ExceptionClear();

        char err_buf[512];
        err_buf[0] = '\0';

        if (ex) {
            jclass ex_class = env->GetObjectClass(ex);
            jmethodID get_msg = env->GetMethodID(ex_class, "toString", "()Ljava/lang/String;");
            jstring jmsg = (jstring)env->CallObjectMethod(ex, get_msg);
            if (jmsg) {
                const char *msg_str = env->GetStringUTFChars(jmsg, NULL);
                if (msg_str) {
                    snprintf(err_buf, sizeof(err_buf), "JNI method call '%s' failed: %s", method_name, msg_str);
                    env->ReleaseStringUTFChars(jmsg, msg_str);
                }
                env->DeleteLocalRef(jmsg);
            }
            env->DeleteLocalRef(ex_class);
            env->DeleteLocalRef(ex);
        }

        if (err_buf[0] == '\0') {
            snprintf(err_buf, sizeof(err_buf), "JNI method call '%s' failed", method_name);
        }

        luaL_error(L, "%s", err_buf);
        return 0;
    }

    jobject_to_lua(L, result, env);
    if (result) env->DeleteLocalRef(result);
    return 1;
}

static int jni_ref_index(lua_State *L) {
    luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
    if (!runtime || !runtime->env) {
        luaL_error(L, "JNI environment is not available");
        return 0;
    }
    JNIEnv *env = (JNIEnv*)runtime->env;
    init_jni_cache(env);

    jni_ref_t *ref = (jni_ref_t*)luaL_checkudata(L, 1, "JniRef");
    const char *key = luaL_checkstring(L, 2);

    // 1. Try accessing as a field first
    jstring jkey = env->NewStringUTF(key);
    jobject val = env->CallStaticObjectMethod(g_helper_class, g_getField, ref->obj, jkey);
    env->DeleteLocalRef(jkey);

    if (env->ExceptionCheck()) {
        env->ExceptionClear(); // Fail silently, fallback to method closure

        // 2. Return a method proxy closure
        lua_pushvalue(L, 1); // target userdata
        lua_pushvalue(L, 2); // method name string
        lua_pushcclosure(L, jni_method_call, "jni_method_call", 2);
        return 1;
    }

    jobject_to_lua(L, val, env);
    if (val) env->DeleteLocalRef(val);
    return 1;
}

static int jni_ref_newindex(lua_State *L) {
    luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
    if (!runtime || !runtime->env) {
        luaL_error(L, "JNI environment is not available");
        return 0;
    }
    JNIEnv *env = (JNIEnv*)runtime->env;
    init_jni_cache(env);

    jni_ref_t *ref = (jni_ref_t*)luaL_checkudata(L, 1, "JniRef");
    const char *key = luaL_checkstring(L, 2);
    jobject val = lua_to_jobject(L, 3, env);

    env->CallStaticVoidMethod(g_helper_class, g_setField, ref->obj, env->NewStringUTF(key), val);
    if (val) env->DeleteLocalRef(val);

    if (env->ExceptionCheck()) {
        jthrowable ex = env->ExceptionOccurred();
        env->ExceptionClear();

        jclass ex_class = env->GetObjectClass(ex);
        jmethodID get_msg = env->GetMethodID(ex_class, "toString", "()Ljava/lang/String;");
        jstring jmsg = (jstring)env->CallObjectMethod(ex, get_msg);
        const char *msg_str = env->GetStringUTFChars(jmsg, NULL);

        luaL_error(L, "JNI field set '%s' failed: %s", key, msg_str);

        env->ReleaseStringUTFChars(jmsg, msg_str);
        env->DeleteLocalRef(jmsg);
        env->DeleteLocalRef(ex);
    }
    return 0;
}

static int jni_ref_gc(lua_State *L) {
    jni_ref_t *ref = (jni_ref_t*)lua_touserdata(L, 1);
    if (ref && ref->obj) {
        luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
        if (runtime && runtime->env) {
            JNIEnv *env = (JNIEnv*)runtime->env;
            env->DeleteGlobalRef(ref->obj);
        }
        ref->obj = NULL;
    }
    return 0;
}

static int jni_import(lua_State *L) {
    luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
    if (!runtime || !runtime->env) {
        luaL_error(L, "JNI environment is not available");
        return 0;
    }
    JNIEnv *env = (JNIEnv*)runtime->env;
    init_jni_cache(env);

    const char *class_name = luaL_checkstring(L, 1);
    jstring jclass_name = env->NewStringUTF(class_name);
    jobject clazz = env->CallStaticObjectMethod(g_helper_class, g_importClass, jclass_name);
    env->DeleteLocalRef(jclass_name);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        luaL_error(L, "JNI Class not found: %s", class_name);
        return 0;
    }

    push_jni_ref(L, clazz, true, env);
    env->DeleteLocalRef(clazz);
    return 1;
}

int luaopen_jni(lua_State *L) {
    luau_runtime_t *runtime = (luau_runtime_t*)lua_getthreaddata(L);
    if (runtime && runtime->env) {
        init_jni_cache((JNIEnv*)runtime->env);
    }

    // 1. Create metatable for JniRef
    luaL_newmetatable(L, "JniRef");
    lua_pushcfunction(L, jni_ref_index, "__index");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, jni_ref_newindex, "__newindex");
    lua_setfield(L, -2, "__newindex");
    lua_pushcfunction(L, jni_ref_gc, "__gc");
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // 2. Create the jni module table
    lua_newtable(L);
    lua_pushcfunction(L, jni_import, "import");
    lua_setfield(L, -2, "import");

    return 1;
}
