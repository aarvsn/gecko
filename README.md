# Gecko

Gecko is a cross-platform libretro frontend for Windows, macOS, Linux, and Android. It provides a graphical interface for managing games, loading emulator cores, configuring controls, and playing games.

The desktop application uses Qt 6, while the Android version uses Kotlin and Jetpack Compose. Both share a common C++17 emulation backend.

Gecko includes the GA330 core, based on the GA330-libretro project, and supports loading additional compatible libretro cores from the cores directory.

### Features

* Libretro support: Load compatible emulator cores from the cores directory and associate them with supported game formats.

* Game library: Scan folders recursively, display game artwork, and generate cover images when artwork is unavailable.

* Quick menu: Access save states, screenshots, reset, video filters, HUD settings, fullscreen, and exit while playing.

* Video options: Choose between scanlines, CRT, and LCD filters, with nearest-neighbor or bilinear scaling.

* Aspect ratios: Support core-defined, 4:3, 16:9, and stretched display modes.

* Performance overlay: Display FPS, emulation speed, pause status, and fast-forward status.

* Fast-forward: Hold Tab on desktop to increase emulation speed, with configurable multipliers up to 10x.

* Save states: Three quick-save slots per game, with support for loading and saving states.

* Save files: Automatically load and save battery-backed SRAM files.

* Input configuration: Remap keyboard controls, configure physical gamepads, and use the Android virtual gamepad.

* Settings: Configure directories, appearance, video, audio, and emulation options.

* Core manager: Discover installed cores and view their names, versions, supported extensions, and game associations.

### Build Verification

The Linux build has been tested in CI-like conditions.

The `gecko-host-tests` suite passes all 44 checks, covering:

* Configuration serialization and deserialization.

* Game library scanning.

* Libretro core loading.

* RGB565, 1555, and XRGB8888 frame conversion.

* Animation and input handling.

* Core options and live updates.

* Audio processing.

* Save-state and rewind operations.

* Reset and core unloading.

Gecko also includes a GUI self-test:

```bash
gecko --selftest <core> <outdir>
```

It launches the Qt interface in headless mode, renders the library and game views, and captures screenshots of the available video filters.

Screenshots are available in `docs/screenshots/`.

### Documentation

* [BUILDING.md](BUILDING.md) — Build instructions for desktop and Android.

* [ARCHITECTURE.md](ARCHITECTURE.md) — Backend architecture, frontend integration, threading, and libretro support.

* [android/README.md](android/README.md) — Android-specific instructions.

* vendor/ga330-libretro/ — Vendored GA330 core and local modifications.

### Quick Start

### Desktop

```bash
cmake -B build -DCMAKE_PREFIX_PATH=<path-to-qt6>
cmake --build build -j
./build/bin/gecko
```

* Open Tools → Settings and select your cores directory.

* Add your ROM folders through Library → Add folder.

* Double-click a game to launch it.

* If no core is assigned, select one. Gecko remembers the association.

### Android

```bash
cd android
./gradlew :app:assembleDebug
adb install app/build/outputs/apk/debug/app-debug.apk
```

Copy compatible cores into:

```text
Android/data/com.gecko.emulator/files/cores/
```

ROMs can be placed in:

```text
Android/data/com.gecko.emulator/files/roms/
```

Alternatively, use the in-app folder picker.

### License

Gecko is licensed under the MIT License. See LICENSE.

The bundled GA330-libretro core and `libretro.h` retain their respective upstream licenses and copyright notices. See [NOTICE.md](https://NOTICE.md?utm_source=chatgpt.com) and the vendor directory for details.
