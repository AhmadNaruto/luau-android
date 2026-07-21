cmake_minimum_required(VERSION 3.10)
project(native_luau C CXX)

# C++ Standard configuration
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Configure submodules

# 1. Luau
set(LUAU_EXTERN_C ON CACHE BOOL "Use extern C for all APIs" FORCE)
set(LUAU_BUILD_CLI OFF CACHE BOOL "Build CLI" FORCE)
set(LUAU_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
add_subdirectory(third_party/luau EXCLUDE_FROM_ALL)

# 2. Lexbor
set(LEXBOR_BUILD_SHARED OFF CACHE BOOL "Build shared" FORCE)
set(LEXBOR_BUILD_STATIC ON CACHE BOOL "Build static" FORCE)
set(LEXBOR_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(LEXBOR_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(LEXBOR_BUILD_UTILS OFF CACHE BOOL "Build utils" FORCE)
set(LEXBOR_INSTALL_HEADERS OFF CACHE BOOL "Install headers" FORCE)
add_subdirectory(third_party/lexbor EXCLUDE_FROM_ALL)

# 3. QuickJS (Core JavaScript runtime compiled statically)
add_library(quickjs STATIC
    third_party/quickjs/dtoa.c
    third_party/quickjs/libregexp.c
    third_party/quickjs/libunicode.c
    third_party/quickjs/quickjs.c
)
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE QUICKJS_NG_BUILD)
target_include_directories(quickjs PUBLIC third_party/quickjs)
target_link_libraries(quickjs PRIVATE m)

# 4. yyjson
set(YYJSON_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
add_subdirectory(third_party/yyjson EXCLUDE_FROM_ALL)

# 5. PCRE2 (8-bit static library)
set(PCRE2_BUILD_PCRE2_8 ON CACHE BOOL "Build 8-bit library" FORCE)
set(PCRE2_BUILD_PCRE2_16 OFF CACHE BOOL "Build 16-bit library" FORCE)
set(PCRE2_BUILD_PCRE2_32 OFF CACHE BOOL "Build 32-bit library" FORCE)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(PCRE2_SUPPORT_JIT OFF CACHE BOOL "Support JIT" FORCE)
add_subdirectory(third_party/pcre2 EXCLUDE_FROM_ALL)

# Optimization compiler flags
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
    add_compile_options(-O3 -flto -fvectorize -fomit-frame-pointer)
endif()

# Shared SDK Target
add_library(native_luau SHARED
    native/core/runtime/runtime.cpp
    native/core/runtime/allocator.cpp
    native/bindings/jni/jni_bridge.cpp
    native/core/luau/loader.cpp
    native/modules/html/html_module.cpp
    native/modules/js/js_module.cpp
    native/modules/json/json_module.cpp
    native/modules/regex/regex_module.cpp
    native/modules/url/url_module.cpp
    native/modules/encoding/encoding_module.cpp
    native/modules/crypto/crypto_module.cpp
    native/modules/buffer/buffer_module.cpp
    native/modules/util/util_module.cpp
    native/modules/time/time_module.cpp
    native/modules/select/select_module.cpp
    native/bindings/jni/proxy.cpp
    native/bindings/jni/lexsoup_jni.cpp
)

# Header search paths for our library
target_include_directories(native_luau PRIVATE
    native
    native/common
    native/core/runtime
    native/core/luau
    native/core/memory
    native/bindings/jni
    native/modules/html
    native/modules/js
    native/modules/json
    native/modules/regex
    native/modules/url
    native/modules/encoding
    native/modules/crypto
    native/modules/buffer
    native/modules/util
    native/modules/time
    native/modules/select
    third_party/luau/VM/include
    third_party/luau/Compiler/include
    third_party/lexbor/source
    third_party/quickjs
    third_party/yyjson/src
    third_party/pcre2/src
)

# Link SDK dependencies
target_link_libraries(native_luau PRIVATE
    Luau.VM
    Luau.Compiler
    lexbor_static
    quickjs
    yyjson
    pcre2-8
    log # Android log library
)
