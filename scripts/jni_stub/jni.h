// Minimal JNI API stub for host-side syntax checking of gecko_jni.cpp.
// NOT a real JNI implementation — used only by the Gecko build check when no
// JDK/NDK headers are available (see scripts/check-android-jni.sh).
#pragma once
#include <cstddef>
#include <cstdint>

typedef int32_t jint;
typedef int64_t jlong;
typedef int8_t jbyte;
typedef uint8_t jboolean;
typedef uint16_t jchar;
typedef int16_t jshort;
typedef float jfloat;
typedef double jdouble;
typedef int jsize;

#define JNI_TRUE true
#define JNI_FALSE false
#define JNIEXPORT __attribute__((visibility("default")))
#define JNICALL

class _jobject {};
class _jclass : public _jobject {};
class _jstring : public _jobject {};
class _jarray : public _jobject {};
class _jintArray : public _jarray {};
class _jshortArray : public _jarray {};

typedef _jobject* jobject;
typedef _jclass* jclass;
typedef _jstring* jstring;
typedef _jarray* jarray;
typedef _jintArray* jintArray;
typedef _jshortArray* jshortArray;

struct JNIEnv {
    const char* GetStringUTFChars(jstring, jboolean*) { return ""; }
    void ReleaseStringUTFChars(jstring, const char*) {}
    jstring NewStringUTF(const char*) { return nullptr; }
    jsize GetArrayLength(jarray) { return 0; }
    void SetIntArrayRegion(jintArray, jsize, jsize, const jint*) {}
    void SetShortArrayRegion(jshortArray, jsize, jsize, const jshort*) {}
};
