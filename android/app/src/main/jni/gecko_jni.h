// SPDX-License-Identifier: MIT
// GeckoNative — JNI bridge declarations between the Kotlin app and gecko-core.
// One native "session" per handle; the Kotlin side drives the frame loop
// (frame thread) and the audio loop (AudioTrack writer thread).
//
// Generated symbol names follow `javac -h` conventions for
// com.gecko.emulator.native_.GeckoNative (note the trailing underscore in the
// package name, which becomes native_1 in the symbol).

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle ---------------------------------------------------------------
JNIEXPORT jlong JNICALL
Java_com_gecko_emulator_native_1GeckoNative_create(JNIEnv*, jclass);
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_destroy(JNIEnv*, jclass, jlong handle);

// Core / content ----------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_gecko_emulator_native_1GeckoNative_loadCore(JNIEnv*, jclass, jlong, jstring path);
JNIEXPORT jboolean JNICALL
Java_com_gecko_emulator_native_1GeckoNative_loadGame(JNIEnv*, jclass, jlong, jstring path);
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_unloadGame(JNIEnv*, jclass, jlong);
JNIEXPORT jstring JNICALL
Java_com_gecko_emulator_native_1GeckoNative_coreName(JNIEnv*, jclass, jlong);

// Frame loop --------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_runFrame(JNIEnv*, jclass, jlong);
JNIEXPORT jint JNICALL
Java_com_gecko_emulator_native_1GeckoNative_frameWidth(JNIEnv*, jclass, jlong);
JNIEXPORT jint JNICALL
Java_com_gecko_emulator_native_1GeckoNative_frameHeight(JNIEnv*, jclass, jlong);
// Copies the latest frame (0xAARRGGBB ints) into `pixels`; returns count copied.
JNIEXPORT jint JNICALL
Java_com_gecko_emulator_native_1GeckoNative_readFramePixels(JNIEnv*, jclass, jlong, jintArray pixels);
JNIEXPORT jdouble JNICALL
Java_com_gecko_emulator_native_1GeckoNative_measuredFps(JNIEnv*, jclass, jlong);

// Audio -------------------------------------------------------------------
JNIEXPORT jdouble JNICALL
Java_com_gecko_emulator_native_1GeckoNative_audioSampleRate(JNIEnv*, jclass, jlong);
// Reads up to `maxFrames` stereo frames into `out`; returns frames read.
JNIEXPORT jint JNICALL
Java_com_gecko_emulator_native_1GeckoNative_readAudio(JNIEnv*, jclass, jlong, jshortArray out, jint maxFrames);

// Input -------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_setButton(JNIEnv*, jclass, jlong, jint port, jint retroId, jboolean pressed);

// States ------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_gecko_emulator_native_1GeckoNative_saveState(JNIEnv*, jclass, jlong, jstring path);
JNIEXPORT jboolean JNICALL
Java_com_gecko_emulator_native_1GeckoNative_loadState(JNIEnv*, jclass, jlong, jstring path);
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_reset(JNIEnv*, jclass, jlong);
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_setPaused(JNIEnv*, jclass, jlong, jboolean paused);
JNIEXPORT void JNICALL
Java_com_gecko_emulator_native_1GeckoNative_setSpeed(JNIEnv*, jclass, jlong, jdouble multiplier);

#ifdef __cplusplus
}
#endif
