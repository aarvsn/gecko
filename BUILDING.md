# Building Gecko

## 1. Prerequisites (all desktop platforms)

- CMake ≥ 3.21
- A C++17 compiler: MSVC 2022 (Windows), Clang 13+ (macOS), GCC 10+ (Linux)
- **Qt 6.4+** with the **Widgets** module; the **Multimedia** module enables
  desktop audio (the app builds and runs without it, just silently)
- Optional: **SDL2** for gamepad support (auto-detected)
- Optional (GA330 core only): `arm-none-eabi-gcc` + Unicorn 2 — see
  *Building the GA330 core* below

### Getting Qt

| Platform | Suggestion |
| --- | --- |
| Windows | Qt online installer or `aqt install-qt windows desktop 6.7.3 win64_msvc2019_64 -m qtmultimedia` |
| macOS | `brew install qt` |
| Linux | Distro packages (`qt6-base-dev qt6-multimedia-dev`) or aqt |
| Any | `pip install aqtinstall && aqt install-qt <host> desktop 6.7.3 gcc_64 -m qtmultimedia` |

## 2. Configure & build (desktop)

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.7.3/gcc_64
cmake --build build -j
```

Binaries land in `build/bin/`:

- `gecko` — the Qt 6 desktop frontend
- `gecko-host-tests` — headless engine test suite
- `gecko_testcore` — synthetic test core (next to the tests)

Run the tests:

```bash
./build/bin/gecko-host-tests          # 44 checks
# Headless GUI self-test + screenshots:
QT_QPA_PLATFORM=offscreen ./build/bin/gecko --selftest ./build/bin/gecko_testcore.so ./out
```

### Useful CMake options

| Option | Default | Meaning |
| --- | --- | --- |
| `GECKO_BUILD_DESKTOP` | ON | Qt 6 frontend (skipped with a warning if Qt 6 is missing) |
| `GECKO_BUILD_TESTS` | ON | Test core + host tests |
| `GECKO_WITH_SDL2` | ON | Gamepad support when SDL2 is found |
| `GECKO_BUILD_GA330_CORE` | OFF | Also build the vendored GA330 core |

## 3. Platform notes

### Windows
- Use the Visual Studio generator or Ninja:
  `cmake -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=C:/Qt/6.7.3/msvc2019_64`
- `desktop/resources/gecko.rc` (icon resource) is picked up automatically
  when present
- To deploy: `windeployqt build/bin/gecko.exe`

### macOS
- Builds as an app bundle (`gecko.app`) automatically
- `macdeployqt build/bin/gecko.app` for a distributable bundle

### Linux
- Needs GL dev headers (libglvnd-dev) — Qt 6's CMake config requires them
- Output is a plain binary; ship Qt libs alongside or rely on distro Qt

## 4. Android

See `android/README.md`. Short version:

- Android Studio with SDK 35, NDK r27, CMake 3.22.1, JDK 17
- `cd android && ./gradlew :app:assembleDebug`
- The Gradle build compiles `app/src/main/jni/` (JNI bridge) together with
  the `core/` sources into `libgecko_native.so` for `arm64-v8a` and
  `x86_64`

## 5. Building the GA330 core (optional)

The vendored core emulates the Gemei GA330 handheld (ARM926 via Unicorn).
Upstream expects the Unicorn submodule:

```bash
git clone https://github.com/unicorn-engine/unicorn.git \
    vendor/ga330-libretro/dependencies/unicorn
cmake -B build -DGECKO_BUILD_GA330_CORE=ON ...
cmake --build build -j          # builds GA330_libretro.so
cmake --install build           # or copy the .so into your cores folder
```

You will also need `arm-none-eabi-gcc` to build `minisys` (see the
`vendor/ga330-libretro/README.md`). Copy the resulting core into your cores
directory and Gecko will list it as **GA330**; any CCDL application runs as
"content".

## 6. Troubleshooting

- **"Qt6 not found"** — pass `-DCMAKE_PREFIX_PATH` pointing at your Qt
  install (the directory that contains `lib/cmake/Qt6`)
- **"Could NOT find OpenGL"** — install GL dev headers
  (`libglvnd-dev` on Debian/Ubuntu, `mesa-libGL-devel` on Fedora)
- **No audio** — build without Qt Multimedia (checked at configure time);
  install `qt6-multimedia-dev` and reconfigure
- **Android: `ninja: command not found`** — install CMake + Ninja via the
  SDK Manager
