# Patch Management Policy

This directory contains patches for third-party submodules when modifications are required for Android ARM64/NDK compatibility or CMake integration without modifying upstream repositories directly.

## Guidelines

- All third-party submodules (`third_party/*`) must remain clean.
- Modifications should be generated as unified `.patch` files using `git diff`.
- Patches must be named according to the convention: `<module>/0001-<description>.patch`.
- Every patch file must document:
  1. Target upstream library and commit/tag.
  2. Purpose of the patch.
  3. Affected source files.
  4. Removal condition (e.g., when upstream resolves the issue).

## Applying Patches

Patches can be applied automatically via CMake or manually:

```bash
git submodule update --init --recursive
# To apply a patch:
# git apply patches/<module>/0001-<description>.patch
```
