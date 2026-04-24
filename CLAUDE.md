# Linphone Desktop - Project Notes

## Overview
Linphone Desktop v6.2.0 — Qt6/C++17 VoIP softphone using MVVM architecture.
- C++ backend (LibLinphone SDK) + QML UI (~185 .qml files, ~179 .cpp files)
- Dual-threaded: SDK thread (CoreModel) + UI thread (QApplication)
- Linphone SDK v5.5.0 vendored at `external/linphone-sdk/`

## Repository Setup
- **Fork:** https://github.com/rammohan-y/linphone-desktop
- **Upstream:** https://gitlab.linphone.org/BC/public/linphone-desktop.git
- **Submodules removed:** All dependencies vendored directly into the repo (commit `12f3ded7`)
- **SDK source:** Originally from https://github.com/BelledonneCommunications/linphone-sdk.git
- External libraries downloaded as ZIPs from gitlab.linphone.org at pinned commits

## Build Environment
- **OS:** Ubuntu 24.04.3 LTS
- **Compiler:** GCC 13.3.0
- **CMake:** 3.28.3
- **Qt6:** 6.10.0 installed via aqtinstall at `~/Qt/6.10.0/gcc_64/`
- **Build type:** RelWithDebInfo

### Build Commands
```bash
export Qt6_DIR="$HOME/Qt/6.10.0/gcc_64/lib/cmake/Qt6"
export PATH="$HOME/Qt/6.10.0/gcc_64/bin:$PATH"
cd build
cmake .. -DCMAKE_BUILD_PARALLEL_LEVEL=10 -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build . --parallel 10 --config RelWithDebInfo
cmake --install .
./OUTPUT/bin/linphone --verbose
```

### System Dependencies Installed
```bash
sudo apt install -y ninja-build nasm yasm doxygen meson autoconf libtool-bin \
  python3-pystache libgl1-mesa-dev libglu1-mesa-dev libegl1-mesa-dev \
  libx11-dev libxkbcommon-dev libxkbcommon-x11-dev libudev-dev \
  libv4l-dev libasound2-dev libpulse-dev libglew-dev libxinerama-dev
```

## Vendoring Patches Applied
Three files modified to work without git submodule history:

1. **`external/linphone-sdk/bctoolbox/cmake/BCToolboxCMakeUtils.cmake:144`**
   - Changed `FATAL_ERROR` to `WARNING` with fallback version `5.5.0` when `git describe` fails

2. **`Linphone/CMakeLists.txt:54-55`**
   - Hardcoded `LINPHONEAPP_VERSION` to `6.2.0` instead of calling `bc_compute_full_version`

3. **`cmake/install/install.cmake:29-43`**
   - Removed `git describe --always` and `bc_compute_full_version` calls, hardcoded `6.2.0`

## Key Directories
- `Linphone/core/` — C++ business logic (SDK thread)
- `Linphone/model/` — Data models wrapping LibLinphone
- `Linphone/view/` — QML UI (pages, controls, styles)
- `Linphone/tool/` — Utilities, platform-native code
- `Linphone/data/` — Resources (icons, fonts, translations, config)
- `external/linphone-sdk/` — Vendored SDK (liblinphone, mediastreamer2, oRTP, belle-sip, etc.)

## External Dependencies (vendored in external/linphone-sdk/)
All pinned to specific commits from gitlab.linphone.org/BC/public/external/:
- Audio codecs: opus, speex, codec2, gsm, bv16, opencore-amr, vo-amrwbenc, bcg729
- Video codecs: openh264, libvpx, aom, dav1d
- Video utils: libyuv, libjpeg-turbo
- Crypto: mbedtls (+ framework submodule from GitHub), srtp, decaf, liboqs
- Utils: zlib, libxml2, jsoncpp, soci, sqlite3, xerces-c, zxing-cpp, rnnoise
- Other: hidapi, oboe, bcmatroska2, cpp-httplib

## Adding New Features — Common Patterns

### Adding a New QML File
Every `.qml` file MUST be registered in `Linphone/view/CMakeLists.txt` in the `_LINPHONEAPP_QML_FILES` list. Without this, the file won't be compiled into the Qt resource bundle and will be invisible at runtime. After adding, re-run `cmake ..` before building.

### Adding a New Settings Tab
1. Create `Linphone/view/Page/Layout/Settings/<Name>SettingsLayout.qml` extending `AbstractSettingsLayout`
2. Add the file to `Linphone/view/CMakeLists.txt` (in the settings layout section, ~line 164)
3. Add `{title: "...", layout: "<Name>SettingsLayout"}` to the `families` array in `Linphone/view/Page/Form/Settings/SettingsPage.qml`

### Adding a New Persistent Setting (full MVVM flow — 9 touch points)
All settings go through: `SettingsModel` (SDK thread) ↔ `SettingsCore` (UI thread) ↔ QML

**SettingsModel** (`Linphone/model/setting/SettingsModel.hpp/.cpp`):
1. `DECLARE_GETSET(Type, name, Name)` in .hpp
2. `DEFINE_GETSET_CONFIG_STRING(SettingsModel, name, Name, "config_key", "default")` in .cpp
3. `DEFINE_NOTIFY_CONFIG_READY(SettingsModel, Name, name)` in .cpp (inside `onConfigReady()`)

**SettingsCore** (`Linphone/core/setting/SettingsCore.hpp/.cpp`) — 9 locations:
1. `Q_PROPERTY(...)` declaration in .hpp
2. Getter/setter declarations in .hpp
3. Signal declaration in .hpp
4. Member variable in .hpp
5. `INIT_CORE_MEMBER(name, settingsModel)` in constructor (.cpp)
6. Copy in copy constructor: `mName = settingsCore.mName;` (.cpp) — **critical for save() to work**
7. `INIT_CORE_MEMBER(name, settingsModel)` in `reloadSettings()` (.cpp)
8. `makeConnectToModel(...)` + `DEFINE_CORE_GET_CONNECT(...)` for cross-thread sync (.cpp)
9. `reset()` / `writeIntoModel()` / `writeFromModel()` methods (.cpp)

If the copy constructor is missing the member, `save()` will silently lose the value.

### Settings Storage
Settings are stored in linphone's `linphonerc` config file under `[ui]` section via `linphone::Config`. Use `DEFINE_GETSET_CONFIG_STRING` macro which reads/writes with `getString("[ui]", "key", "default")`.

### Pre-commit Hook
A clang-format pre-commit hook runs on all C++ files. Before committing, run:
```bash
clang-format --style=file -i <modified .hpp/.cpp files>
```

### SDK Audio APIs (relevant for AI Agent feature)
- **Audio capture**: `CallParams::setRecordFile(path)` + `Call::startRecording()` — per-call WAV recording (PCM s16le 16kHz mono)
- **Audio injection**: `Call::getPlayer()->open(path)` + `start()` — file-based only, seeks internally (no FIFOs/pipes)
- **No External AudioDevice** in SDK 5.5.0 (`LinphoneAudioDeviceType` stops at Hdmi=12)
- **Core::setUseFiles()** is global, replaces ALL audio I/O — not suitable for per-call use

### Start Script
`./start-linphone.sh` runs from `build/OUTPUT/bin/linphone`. After building, run `cmake --install .` so the install step copies the new binary to OUTPUT.

## Network Note
gitlab.linphone.org is unreliable from this network (OVH France datacenter).
All dependencies were downloaded as ZIP archives via browser and extracted manually.
GitHub mirrors exist for some repos under BelledonneCommunications org.
