# Luau Native SDK Android (ARM64-v8a) Benchmark Results

**Date:** July 21, 2026  
**Environment:** Android ARM64-v8a (Linux NDK 28)

---

## 📊 Performance Summary Table

| Category | Workload / Operations | Total Time (ms) | Operations / sec |
|----------|----------------------|-----------------|------------------|
| **Luau Core VM** | 1,000,000 Math & Modulo loop | **166.15 ms** | ~6,018,600 ops/sec |
| **Lexbor HTML** | Parse 100KB HTML + 100 CSS Queries | **22.12 ms** | ~4,520 queries/sec |
| **yyjson JSON** | 100 iterations of Parse + Stringify (500 items each) | **659.81 ms** | ~75,700 items/sec |
| **PCRE2 Regex** | `find_all` across 3 URL captures (1,000 iterations) | **15.38 ms** | ~65,000 matches/sec |
| **QuickJS-NG JS** | 100 executions of recursive `fib(15)` | **156.23 ms** | ~640 eval/sec |
| **OpenSSL Crypto** | 5,000 SHA256 + MD5 + HMAC operations | **67.12 ms** | ~74,500 hashes/sec |

---

## 💡 Performance Highlights

- **HTML Parsing (Lexbor):** 100KB HTML file was parsed and queried via 100 CSS selectors in just **22.12 ms** natively.
- **Crypto Operations (OpenSSL):** Executed 5,000 cryptographic hash operations in **67.12 ms** (~13 µs per hash).
- **Regex Engine (PCRE2):** 1,000 regex scans completed in **15.38 ms** (~15 µs per execution).
