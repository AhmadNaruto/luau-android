# Luau Native SDK Android (ARM64-v8a) Benchmark Results

**Date:** July 21, 2026  
**Environment:** Android ARM64-v8a (Linux NDK 28)

---

## 📊 Post-Optimization Performance Summary

| Category | Workload / Operations | Before (ms) | After (ms) | Speedup | Operations / sec |
|----------|----------------------|-------------|------------|---------|------------------|
| **Luau Core VM** | 1,000,000 Math & Modulo loop | 166.15 ms | **163.28 ms** | +2% | ~6,124,000 ops/sec |
| **Lexbor HTML** | Parse 100KB HTML + 100 CSS Queries | 22.12 ms | **13.38 ms** | **+39.5%** | ~7,470 queries/sec |
| **yyjson JSON** | 100 iterations of Parse + Stringify (500 items each) | 659.81 ms | **510.89 ms** | **+22.6%** | ~97,800 items/sec |
| **PCRE2 Regex** | `find_all` across 3 URL captures (1,000 iterations) | 15.38 ms | **12.12 ms** | **+21.2%** | ~82,500 matches/sec |
| **QuickJS-NG JS** | 100 executions of recursive `fib(15)` | 156.23 ms | **116.67 ms** | **+25.3%** | ~857 evals/sec |
| **OpenSSL Crypto** | 5,000 SHA256 + MD5 + HMAC operations | 67.12 ms | **53.10 ms** | **+20.9%** | ~94,150 hashes/sec |

---

## 🛡️ Applied Safety & Stability Enhancements

1. **Iterative Stack-based DFS (`select_module.cpp`)**: Replaced recursive tree traversal with fixed-buffer stack allocation. Prevents stack overflow on deep HTML trees and accelerated DOM querying by ~40%.
2. **256MB Upper Allocation Guard (`buffer_module.cpp`)**: Capped maximum single buffer size at 256MB to prevent Out-Of-Memory (OOM) memory exhaustion attacks.
3. **JNI Local Reference Cleanup (`proxy.cpp`)**: Properly deleted local `jstring` and exception references before longjmp unwinding in `luaL_error`. Prevents local reference table leaks.
4. **Compiler Vectorization & Inlining (`CMakeLists.txt`)**: Enabled `-O3 -flto -fvectorize -fomit-frame-pointer` for production builds.
