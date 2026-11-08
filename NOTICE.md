# Notices

## Gecko

The `core/`, `desktop/`, `android/` and `tests/` trees are original work
written for the Gecko project and are MIT-licensed (see `LICENSE`).

## libretro API header

`core/include/gecko/libretro.h` (also vendored as
`vendor/ga330-libretro/include/libretro.h`) is
Copyright (C) 2010-2020 The RetroArch team, MIT-licensed. The license
statement is embedded at the top of the file.

## GA330-libretro (vendored)

`vendor/ga330-libretro/` contains a snapshot of the upstream GA330-libretro
project (https://github.com/libretro-mirrors or the author's repository):
a libretro core that emulates the Gemei GA330 handheld (ARM926 CPU via
Unicorn), a minimal replacement ARM OS (`minisys`), and libCCOS.

The upstream project did not include a license file at snapshot time; it is
vendored here unmodified except for the small build-system patch documented
in `vendor/ga330-libretro/PATCHES.md`. If you redistribute binaries of the
GA330 core, please check the upstream repository for license updates.

Third-party code expected by the vendored core (not included in this
snapshot):

- **Unicorn Engine** — BSD-2-Clause; expected under
  `vendor/ga330-libretro/dependencies/unicorn/` (empty submodule upstream)
- **GSL** — MIT; headers vendored under `dependencies/GSL/`

## Trademarks

"Gecko" as used here is the name of this open-source frontend. All product
names and trademarks are property of their respective owners.
