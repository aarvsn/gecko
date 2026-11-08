# Gecko Android — keep the JNI surface intact.
-keepclasseswithmembernames class com.gecko.emulator.native_.GeckoNative {
    native <methods>;
}
-keepclassmembers class com.gecko.emulator.native_.GeckoNative {
    native <methods>;
}
