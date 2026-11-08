# Gecko for Android

Qt6 powers the desktop builds; on Android the same `gecko-core` engine is
wrapped in a native Android app:

- **UI**: Kotlin + Jetpack Compose (Material 3), dynamic color
- **Rendering**: `SurfaceView` + `Canvas.drawBitmap` driven by a paced frame
  loop thread; frames are converted to `ARGB_8888` inside the JNI bridge
- **Audio**: `AudioTrack` (PCM 16-bit stereo) pulling from a native ring buffer
- **Input**: translucent virtual gamepad (Compose) + physical gamepads/keyboards
  via `KeyEvent` mapping

## Layout

```
android/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── jni/                 # CMake + JNI bridge (links gecko-core sources)
│       │   ├── CMakeLists.txt
│       │   ├── gecko_jni.cpp
│       │   └── gecko_jni.h
│       ├── java/com/gecko/emulator/
│       │   ├── MainActivity.kt            # library + navigation
│       │   ├── library/                   # library scan + Compose grid
│       │   ├── game/                      # GameActivity, session, gamepad
│       │   ├── settings/                  # settings + cores screens
│       │   ├── native_/GeckoNative.kt     # JNI bindings + RetroPad ids
│       │   └── ui/Theme.kt
│       └── res/
├── build.gradle.kts
├── settings.gradle.kts
└── gradle.properties
```

## Requirements

- Android Studio Ladybug (or newer) with SDK 35
- NDK r27 and CMake 3.22.1+ (SDK Manager → SDK Tools)
- JDK 17

## Build & run

```bash
cd android
./gradlew :app:assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

Or open the `android/` folder in Android Studio and press Run.

## Installing content

The app uses its **app-specific external directory** — no storage permissions
needed:

```
/storage/emulated/0/Android/data/com.gecko.emulator/files/
├── roms/      ROMs (scanned recursively)
├── cores/     libretro core .so libraries
├── system/    BIOS files for cores that need them
├── saves/     SRAM + save states (slots 1-3)
└── screenshots/
```

You can also use the in-app actions:
- **Folder icon** — pick a ROM folder via SAF; contents are copied into `roms/`
- **Import icon** — pick individual ROM files
- **Cores screen** — verify which cores were found

Any libretro core works. Copy the matching `.so` (e.g. `snes9x_libretro_android.so`)
into `cores/`; when launching a ROM whose extension has no core yet, Gecko asks
once and remembers the mapping in `core_map.txt`.
