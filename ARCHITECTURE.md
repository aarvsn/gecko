# Gecko architecture

## Components

```
gecko/
├── core/                     gecko-core — platform-agnostic libretro host
│   ├── include/gecko/        public API (no UI deps)
│   └── src/                  implementation
├── desktop/                  Qt 6 frontend (Windows / macOS / Linux)
├── android/                  Kotlin + Compose frontend (JNI → gecko-core)
├── tests/
│   ├── testcore/             synthetic libretro core ("GeckoTest")
│   └── host_tests/           headless engine test suite (44 checks)
└── vendor/ga330-libretro/    vendored upstream GA330 core (Unicorn-based)
```

## gecko-core

| Module | Responsibility |
| --- | --- |
| `emulator.{hpp,cpp}` | The libretro host: resolves core exports, drives `retro_*`, implements the environment callback (system/save dirs, pixel format, variables/core options, log, AV info, input bitmasks, fast-forward, …), load/run/reset, save states, SRAM, core messages |
| `runner.{hpp,cpp}` | Owns the emulation thread; paces `runFrame()` to the core's fps; speed multiplier (fast-forward) and pause; rolling FPS measurement |
| `video_sink.hpp` | Sinks receive **XRGB8888** frames only — the emulator converts 0RGB1555 / RGB565 / XRGB8888 in `convertToXrgb8888` (video_sink.cpp) before dispatching |
| `audio_sink.hpp` | `AudioSink` (stereo interleaved s16), `NullAudioSink`, `AudioRingBuffer` (mutex FIFO) + `RingAudioSink` used by Android |
| `input_source.hpp` | `InputSource` mirrors `retro_input_state_t`; `JoypadStateSource` is a thread-safe implementation with 4 ports, buttons + analog, incl. `RETRO_DEVICE_ID_JOYPAD_MASK` |
| `config.{hpp,cpp}` | Dotted-key settings persisted as pretty JSON (own parser/serializer in `json.{hpp,cpp}` — no external deps), atomic saves |
| `core_discovery.{hpp,cpp}` | Scans a directory for `retro_api_version` exports; caches identity per path; never dlcloses (RetroArch-style, cores are not dlclose-safe) |
| `library.{hpp,cpp}` | Recursive ROM scan (bounded depth), extension filter, artwork lookup, title prettifier |
| `dynamic_library.{hpp,cpp}` | dlopen / LoadLibrary wrapper |

**Threading contract.** One `Emulator` per process is "active" (libretro
callbacks have no user-data; a file-static active pointer forwards the C
trampolines). All public `Emulator` calls happen on the emu thread — the
`EmulatorRunner` — while UI layers push input in and marshal frames/samples
out through thread-safe sinks:

```
UI thread                          emulation thread
─────────                          ────────────────
JoypadStateSource.setButton()  →   retro_run() → input_state_cb
FrameBuffer.takeLatest()       ←   video_cb → convert → FrameBuffer.frame()
AudioRingBuffer (Android)      ←   audio_batch_cb → AudioSink.write()
EmulatorRunner.measuredFps()
```

### libretro coverage (environment)

Supported: `GET_OVERSCAN`, `GET_CAN_DUPE`, `SET_MESSAGE`, `SHUTDOWN`,
`SET_PERFORMANCE_LEVEL`, `GET_SYSTEM_DIRECTORY`, `GET_SAVE_DIRECTORY`,
`SET_PIXEL_FORMAT` (all three formats), `SET_INPUT_DESCRIPTORS`,
`SET_KEYBOARD_CALLBACK`, `GET_VARIABLE`, `SET_VARIABLES` (legacy),
`GET_VARIABLE_UPDATE`, `SET_SUPPORT_NO_GAME`, `GET_LIBRETRO_PATH`,
`GET_LOG_INTERFACE`, `GET_CORE_ASSETS_DIRECTORY`, `SET_SYSTEM_AV_INFO`,
`SET_GEOMETRY`, `SET_CONTROLLER_INFO`, `GET_USERNAME`, `GET_LANGUAGE`,
`SET_SUPPORT_ACHIEVEMENTS`, `GET_AUDIO_VIDEO_ENABLE`,
`SET_MINIMUM_AUDIO_LATENCY`, `GET_FASTFORWARDING`, `GET_INPUT_BITMASKS`,
`GET_CORE_OPTIONS_VERSION`, `SET_CORE_OPTIONS` (+`_INTL`, `_DISPLAY`),
`GET_TARGET_REFRESH_RATE`.

Rejected explicitly (no HW rendering in v1): `SET_HW_RENDER`,
`GET_CURRENT_SOFTWARE_FRAMEBUFFER`, `GET_HW_RENDER_INTERFACE`,
`SET_MEMORY_MAPS`, rumble/location/MIDI/perf/disk-control interfaces.
Unknown commands are logged at debug level and answered `false`.

## Qt 6 desktop frontend

| Class | Role |
| --- | --- |
| `MainWindow` | Menu bar, toolbar, library grid (`QListView` IconMode), status bar; launch flow (resolve core → `EmulatorWindow`) |
| `LibraryModel` | Grid model; loads artwork or paints generated cover tiles (stable per-title color, initials, extension badge) |
| `EmulatorWindow` | Hosts the game view; Esc overlay menu, F11 fullscreen, Tab fast-forward; persists geometry |
| `EmulatorView` | The display widget: owns `Emulator` + `EmulatorRunner` + `FrameBuffer` + `AudioBridge` + `InputManager`; repaint timer (~16 ms) blits the latest frame, then applies the filter overlay and HUD; routes key events to the joypad |
| `FrameBuffer` | `VideoSink` on the emu thread producing `QImage`s (implicit sharing keeps copies cheap) |
| `AudioBridge` | Qt Multimedia `QAudioSink` in pull mode over the core's `AudioRingBuffer`; silently disabled when Multimedia is absent |
| `InputManager` | Keyboard → joypad map (config-persisted, editable in `InputMapperDialog`); optional SDL2 gamepad polling |
| `SettingsDialog` / `CoreManagerDialog` | Directories, theme, video/audio/emulation pages; core listing + extension mapping |
| `OverlayMenu` | Translucent quick menu (keyboard + mouse) |

Theme: **system-native by default** (no style forced). Light/Dark options
apply Fusion with a matched palette.

Screenshots are the current frame saved as PNG under the screenshots
directory.

## Android frontend

```
Kotlin/Compose ─── JNI (gecko_jni.cpp) ──── gecko-core
   │                                            │
   │ EmulatorSession:                           │
   │  · frame loop thread → runFrame()          │
   │    readFramePixels() → Canvas.drawBitmap   │
   │  · AudioTrack thread → readAudio()         │
   │  · VirtualGamepad → setButton()            │
```

- `GeckoNative.kt` — the JNI surface (create/destroy, load core/game, frame
  blit, audio pull, input, states, speed)
- `EmulatorSession` — owns the native handle; paces the frame thread to the
  core fps; blits via `Surface.lockCanvas`
- `GameActivity` — fullscreen game screen with HUD, virtual gamepad, in-game
  bottom-sheet menu (save/load slots, screenshot, reset, filter, exit) and
  physical gamepad `KeyEvent` mapping
- `LibraryRepository` — scans `files/roms`, `files/cores`; SAF import copies
  documents into real files (native code needs paths); extension→core map in
  `core_map.txt`

## Test strategy

- `gecko_testcore` — a real libretro core that renders an animated pattern,
  consumes input, emits audio, publishes core options and supports save
  states
- `gecko-host-tests` — drives it through the **actual** `Emulator` pipeline
  (dlopen, env callbacks, all conversions) and asserts outcomes; the
  save-state check relies on the core's deterministic simulation (reload →
  re-run N frames → identical output)
- `gecko --selftest <core> <outdir>` — the real Qt widget stack under
  `QT_QPA_PLATFORM=offscreen`: screenshots the library window and the game
  view with each filter, and verifies frames animate (pixel memcmp over
  300 ms)
