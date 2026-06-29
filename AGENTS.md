# General concept for project LOUHI

## Overview

LOUHI shall be the main app of a modern Battle Management System. It will be one app which can do different functions based on plugins. Communication and data management shall be through NATS/Jetstream.

## Keywords

- QT app
- C and/or C++
- Modular
- Plugin-based

## Architecture

### Build System

- Uses CMake for building
- Plugins are compiled as shared libraries (.so files) and loaded at runtime via Qt's plugin system
- Core interface library (`libplugininterface.so`) shared between main app and plugins

### Qt Version

- **Qt 6** (migrated from Qt 5 on 2026-06-29)
- Minimum Qt 6.4 (Ubuntu 24.04 LTS / Noble)
- Build system uses `find_package(Qt6 ...)` with components: Widgets, Core, Gui, Network, SerialPort, OpenGLWidgets
- Optional: `Qt6Positioning` (for GPS support), `Qt6::Svg` (for SVG icon support)

### Directory Structure

```
/src
    main.cpp           - Application entry point
    mainwindow.cpp     - Main window class
    plugininterface.h  - Plugin base interface
    pluginmanager.cpp  - Plugin discovery and management
    configmanager.cpp  - JSON configuration management
    pluginmanagerdialog.cpp - Plugin manager UI
    mapsources.h       - Shared MapSource/TileKey structs (used by both map plugins)
    tilecache.h/.cpp   - Shared disk tile cache (~/.cache/Louhi/tiles/)
/plugins
    *.cpp, *.h         - Plugin implementations
    *.json             - Plugin metadata files
    CMakeLists.txt     - Plugin build configuration
/deps
    nats.c             - NATS C Client (git submodule, built statically)
    OpenSceneGraph     - OSG source (cloned, NOT a submodule; built with GL3 profile)
    osgearth           - osgEarth v3.8 source (cloned, NOT a submodule)
/cmake
    PortableDeploy.cmake      - CMake module for portable deployment
    portable-deploy.sh.in     - Deployment script template
    FindosgEarth.cmake        - CMake find module (makes OSGEARTH_UTIL_LIBRARY optional)
```

## Plugins

Plugins register themselves providing information about their capabilities and their requirements. Registration shall include plugin name, type, top menu item (can be shared among plugins), submenu items, NATS topic(s)

### Plugin Interface

Each plugin must implement:
- `PluginInfo getPluginInfo()` - Returns plugin metadata (name, version, type, etc.)
- `QVector<MenuEntry> getMenuEntries()` - Returns menu structure
- `bool load()`, `initialize()`, `start()`, `stop()`, `unload()` - Lifecycle methods
- `QWidget* getWidget()` - Returns the plugin's UI widget (optional)
- `void configure(QWidget* parent)` - Opens settings dialog
- `QJsonObject getConfig()`, `void setConfig(const QJsonObject&)` - JSON configuration

### Plugin Metadata

Plugins include a `.json` file with:
```json
{
    "IID": "com.louhi.plugininterface/1.0",
    "Name": "Plugin Name",
    "Description": "Description",
    "Version": "0.1",
    "Author": "Author Name"
}
```

### Plugin Types

#### Communication

Communication plugins handle the communication between the app and the rest of the world. These plugins do NOT have automatic dock widgets - they are only accessible via the Settings menu for configuration.

Examples:
- Mesh communication
- NATS Jetstream

#### Map

Map plugins draw information on a map. The map can be shared with other plugins or the plugin can require its own map screen/map view.

#### Screen

Screen plugins generally display data rather than a map. These plugins get an automatic dock widget that can be closed and reopened via the View menu.

Examples:
- Reports reading/writing
- CASEVAC handling
- Logistics handling

### Menu System

- **Communication** menu - Contains communication plugin submenus (Connect/Disconnect)
- **View** menu - Contains Screen plugin items (Show <Plugin>, Clear, etc.)
- **Settings** menu - Contains plugin configuration items
- **Plugin Manager** - Standalone action to enable/disable plugins

## Configuration

### ConfigurationManager

The app uses a JSON-based configuration system stored in `~/.config/Louhi/config.json`.

**App Configuration:**
- Window position and size
- Other app-level settings

**Plugin Configuration:**
- Each plugin can store/retrieve its own JSON configuration
- Config is automatically saved on app exit and restored on startup
- Plugins implement `getConfig()` and `setConfig(const QJsonObject&)` for this

## Building

### Dependencies

- Qt5 (development libraries) - required for building
- OpenSSL (development libraries) - required for building
- CMake 3.16+
- git (for submodule initialization)
- OpenSceneGraph (built from source with GL3 profile - see below)
- osgEarth 3.8 (built from source - see below)
- chrpath or patchelf (optional, for setting RPATH in portable bundle)

### Build Commands

```bash
# First checkout submodules (do once after cloning)
git submodule update --init --recursive

# Build OpenSceneGraph from source (GL3 profile)
cd deps
git clone --depth 1 https://github.com/openscenegraph/OpenSceneGraph.git
cd OpenSceneGraph && mkdir build && cd build
cmake .. -DOPENGL_PROFILE=GL3 -DOSG_GL_CONTEXT_VERSION=4.6
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../../..

# Build osgEarth from source
cd deps/osgearth && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release \
  -DOSGEARTH_BUILD_TOOLS=OFF -DOSGEARTH_BUILD_EXAMPLES=OFF \
  -DOSGEARTH_BUILD_TESTS=OFF -DProtobuf_PROTOC_EXECUTABLE=/usr/bin/protoc
make -j$(nproc)
sudo make install
sudo ldconfig
cd ../../..

# Normal build
mkdir build && cd build
cmake ..
make
```

The build outputs:
- `louhi` - Main executable
- `libplugininterface.so` - Plugin interface library
- `plugins/*.so` - Plugin shared libraries

### Portable Deployment

Create a self-contained folder with all dependencies bundled:

```bash
cmake -DBUILD_PORTABLE=ON ..
make portable-deploy
```

Output: `build/Louhi.app/` - a portable folder ready to copy to a USB stick.

### Running

**From build directory:**
```bash
cd build
./louhi
```

**From portable bundle:**
```bash
./Louhi.app/run.sh
```

### Dependency Management

**NATS C Client:** Added as a git submodule at `deps/nats.c` and built statically. No system package required.

**OpenSceneGraph:** Cloned from GitHub at `deps/OpenSceneGraph`. Must be built with `-DOPENGL_PROFILE=GL3` for osgEarth compatibility. Installed system-wide via `make install`.

**osgEarth:** Cloned from GitHub at `deps/osgearth` (tag `osgearth-3.8`). Must use system protoc (`-DProtobuf_PROTOC_EXECUTABLE=/usr/bin/protoc`) to match system libprotobuf version. Installed system-wide via `make install`.

**Qt5 & OpenSSL:** Resolved via `find_package` on the build system. For portable deployment, all needed `.so` files are automatically bundled by the `portable-deploy` target.

### Required System Packages (Ubuntu/Debian)
```bash
sudo apt install qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-serialport-dev qt6-positioning-dev libssl-dev cmake build-essential protobuf-compiler
```

## Cross-Platform Compatibility

LOUHI targets Linux, macOS, and Windows. All code changes must follow these rules:

### General

- **No POSIX-only APIs** — do not use `termios.h`, `fcntl.h`, `unistd.h`, `::open`, `::close`, `::read`, `::write`, `socket()` on file descriptors. Use Qt abstractions (`QSerialPort`, `QFile`, `QProcess`, `QTcpSocket`, etc.).
- **No platform `#ifdef` in business logic** — confine platform checks to CMake (conditional source files, link flags, compile definitions). If unavoidable in C++, keep them minimal and documented.
- **File paths** — use `QDir::separator()` or `/` (Qt normalizes). Never use `\` or hardcode `/`.
- **Shared library extensions** — use CMake's `MODULE` property (output name only, no extension). Plugin discovery must filter by `*.so`, `*.dylib`, `*.dll` per platform (see `src/pluginmanager.cpp`).
- **RPATH** — set via CMake, platform-conditional:
  - Linux: `$ORIGIN/../lib`
  - macOS: `@executable_path/../lib` (executable), `@loader_path/../lib` (plugins)
  - Windows: not needed (DLLs alongside .exe or in PATH)
- **Portable deploy** — use platform-native tools: `macdeployqt` (macOS), `windeployqt` (Windows), bash + `chrpath`/`patchelf` (Linux).

### Optional Dependencies

- Use `find_package(QUIET)` for optional Qt modules (e.g. `Qt5Positioning`) and guard usage with `#ifdef`.
- Never make an optional dependency `REQUIRED` without explicit agreement.

### Serial Ports

- Default device names are platform-conditional in CMake or C++:
  - Linux/macOS: `/dev/ttyUSB0`
  - Windows: `COM1`

## Translations

Every user-visible string must be translatable.

### Rules

- **Wrap all user-visible strings** in `tr()` — error messages, status text, combo box labels, default names, tooltips, dialog titles, menu entries.
- **Never use `tr()` on plugin identifiers** — config keys, `config.type`, internal names must stay locale-independent.
- **Plugin `.json` metadata** — `Name`/`Description` are English-only; they are not wrapped in `tr()` (they are static metadata, not runtime UI).
- **`lupdate` / `lrelease`** — after adding/changing any `tr()` string, run `lupdate` and `lrelease` to keep `.ts`/`.qm` files in sync:
  ```bash
  lupdate . -no-obsolete -extensions cpp,h -ts translations/louhi_en.ts translations/louhi_de.ts translations/louhi_fi.ts translations/louhi_sv.ts
  lrelease translations/*.ts
  ```
- **New `.ts` files** — if adding a new locale, add it to the list above and commit the initial `.ts` + `.qm`.
- **Config-driven UI strings** — if a string comes from config/plugin metadata and is displayed in UI (e.g. a plugin name in menus), wrap it in `tr()` at the display site.

## Cross-Platform Build Pipeline (Reference)

Native CI runners (GitHub Actions) are preferred over local cross-compilation:

| Target    | Runner            | Qt install                    | Notes |
|-----------|-------------------|-------------------------------|-------|
| Linux     | `ubuntu-latest`   | `apt install qt6-base-dev qt6-tools-dev qt6-l10n-tools qt6-serialport-dev qt6-positioning-dev` | OSG/osgEarth must be built from source; ~20-30 min |
| macOS     | `macos-latest`    | `brew install qt@6`           | Same osgEarth caveat |
| Windows   | `windows-latest`  | `aqtinstall` Qt6 + MingW/MSVC | NATS C client compiles; osgEarth requires MSVC |

Local cross-compilation (Linux→Windows MingW, Linux→macOS via `osxcross`) is possible but significantly more effort — Qt and osgEarth would need cross-compilation too. Not recommended unless CI is unavailable.

## Qt5 → Qt6 Migration Log

### 2026-06-29 — Step 1: OpenSceneGraph rebuilt with GL3 profile

**What:** Rebuilt OpenSceneGraph from source (`deps/OpenSceneGraph`) with `-DOPENGL_PROFILE=GL3 -DOSG_GL_CONTEXT_VERSION=4.6`.
**Why:** OSG was previously built with default GL2 profile. AGENTS.md requires GL3 for osgEarth compatibility. OSG has no Qt dependency, so no Qt6-related change was needed.
**Status:** Built and installed to `/usr/local/lib/`. `sudo ldconfig` run.
**Next step:** Rebuild osgEarth against the new GL3-build OSG.

### 2026-06-29 — Step 2: osgEarth rebuilt against GL3-profile OSG

**What:** Rebuilt osgEarth 3.8 from source (`deps/osgearth`) against the newly-built OSG (GL3 profile).
**Why:** Previously-installed osgEarth was linked against the old GL2-profile OSG. Rebuild required for ABI compatibility.
**Note:** osgEarth has no Qt dependency — the Qt integration is entirely on the LOUHI plugin side (`osgearthplugin`). No Qt6-related change was needed for osgEarth itself.
**Status:** Built and installed to `/usr/local/lib/`. `sudo ldconfig` run.
**Next step:** Update CMake configuration files from Qt5 to Qt6.

### 2026-06-29 — Step 3: CMake configuration migrated to Qt6

**What:** Updated all CMake files:
- Root `CMakeLists.txt`: `find_package(Qt5 ...)` → `find_package(Qt6 ...)` with `OpenGLWidgets` component; removed `Qt5Xml` (classes moved to Qt6::Core)
- `plugins/CMakeLists.txt`: All `Qt5::*` → `Qt6::*`; `Qt5Positioning_FOUND` → `Qt6Positioning_FOUND`; added `Qt6::OpenGLWidgets` for osgearthplugin
- `cmake/PortableDeploy.cmake`: `qmake` → `qmake6`; Qt5 → Qt6 fallback plugin paths
- `cmake/portable-deploy.sh.in`: `QT5_PLUGIN_DIR` → `QT6_PLUGIN_DIR`
**Status:** Project configures and links against Qt6.

### 2026-06-29 — Step 4: C++ code migrated to Qt6 APIs

**What:**
- `plugins/cotmessage.h`: Removed `#include <QDomDocument>` (not used in interface)
- `plugins/cotmessage.cpp`: Rewrote `CoTMessageParser::parse()` and `CoTMessageParser::isValid()` from `QDomDocument` to `QXmlStreamReader` (Qt6 removed QtXml module)
- `src/main.cpp`: `QLibraryInfo::location()` → `QLibraryInfo::path()` (Qt6 removed `location()`)
**Status:** All code compiles cleanly against Qt6.

### 2026-06-29 — Step 5: Full Qt6 build verified

**What:** Clean build from scratch with `rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)`.
**Result:** All targets build successfully:
- `louhi` (main executable) — links `libQt6Core`, `libQt6Gui`, `libQt6Widgets`
- `libplugininterface.so` — shared interface library
- `natsplugin.so`, `messageviewerplugin.so`, `takplugin.so`, `locationplugin.so`, `mapplugin.so`, `osgearthplugin.so` — all plugins, all link Qt6
- `nats_static.a` — NATS C client (no Qt dependency)
**Smoke test:** Binary starts, discovers all 6 plugins, loads and initializes them, CoT message parser works, TAK plugin auto-connects. No Qt5 libraries linked anywhere.
**Next step:** User testing — run `./build/louhi` from the project root and verify functionality.

### 2026-06-29 — Step 6: CoT entity rendering + icon resolution

**What:** Added tactical entity rendering to the osgEarth map plugin.

**Files changed:**
- `plugins/osgearthplugin.h/cpp` — subscribe to `msg.>` and `alert.>` NATS topics; implement CoTXML parsing in `deliverMessage()` extracting uid, type, callsign, position, iconsetpath, stale time; resolve icon via IconsetResolver; call widget entity methods
- `plugins/osgearthmapwidget.h/cpp` — add `MapEntity` struct, `addOrUpdateEntity()`/`removeEntity()`/`clearEntities()` methods; render entities as `osgEarth::PlaceNode` annotations with icon and callsign label; stale-check timer every 5s
- `plugins/iconsetresolver.h/cpp` — new class: scans `assets/icons/map/iconsets/` for `iconset.xml` files, builds `type2525b`→icon map; icon resolution priority: explicit `iconsetpath` → exact type match → 3-token prefix → 2-token → domain default (friendly/hostile/neutral/unknown)
- `plugins/CMakeLists.txt` — add iconsetresolver sources to osgearthplugin
- `NATS_DESIGN.md` — add §3 Protocol Layer: CoTXML is standard wire format

**Iconset counts (distinct type2525b codes):**
- Default: 27, FEMA Icons: 1, Generic: 0, Google: 6, OSM: 0, WASP: 0

**Result:** Build succeeds, smoke test passes, all 6 plugins load, aggregated subscriptions include `msg.>` and `alert.>`.

### 2026-06-29 — Step 7: Rewrite icon resolver — 2525B primary, iconset fallback

**What:** Changed icon resolution to use MIL-STD-2525B as the primary path, matching tak-webview-cesium reference code. Iconsets are now only used for explicit `<usericon iconsetpath>` CoT detail attributes.

**Changes:**
- `iconsetresolver.h/cpp` — Added `cotToSidc()` and `cleanSidc()` (C++ ports of JS reference), `resolve2525Icon()` with progressive SIDC fallback, 2525B file index (QSet of 3280 filenames)
- `osgearthmapwidget.h` — Added `milsymId` field to `MapEntity`
- `osgearthplugin.cpp` — Parse `__milsym`/`__milicon` from CoT detail; pass `milsymId` to resolver; load `assets/icons/map/2525/` directory

**Resolution priority:**
1. `<usericon iconsetpath>` → iconset lookup (only path that uses iconsets)
2. `__milsym`/`__milicon` detail → `cleanSidc()` → 2525/ PNG
3. CoT `type` → `cotToSidc()` → 2525/ PNG
4. Progressive SIDC shortening (strip trailing function code → generic dimension → unknown ground)
5. Iconset typeMap fallback (unchanged from before)
6. Default affiliation icon from iconset

**2525B directory:** `assets/icons/map/2525/` — 3280 pre-rendered PNGs named by 15-char lowercased 2525B SIDC (e.g. `sfgp-----------.png`).

**Result:** Builds clean, 3280 2525B icons indexed.

**Iconset counts per iconset.xml (total icons, type2525b-mapped):**
Default: 821 / 818, FEMA Icons: 40 / 40, Generic Icons: 657 / 0,
Google: 96 / 96, OSM: 347 / 0, WASP Icons: 35 / 0
