#!/usr/bin/env bash
# Syntax-checks the Android JNI bridge against gecko-core without the NDK.
# Uses a minimal JNI stub (scripts/jni_stub) — this validates C++ syntax
# and symbol wiring only, not the Android build itself (use Android Studio
# or ./gradlew :app:assembleDebug for that).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

g++ -fPIC -std=c++17 -Wall -Wextra \
    -I"$ROOT/android/app/src/main/jni" \
    -I"$ROOT/core/include" \
    -I"$ROOT/scripts/jni_stub" \
    -c "$ROOT/android/app/src/main/jni/gecko_jni.cpp" \
    -o "$OUT/gecko_jni.o"

echo "JNI bridge syntax check: OK"
