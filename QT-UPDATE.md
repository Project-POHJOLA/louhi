# Qt5 → Qt6 Migration Overview

**Date:** 2026-06-27
**OS:** Ubuntu 24.04 LTS (Noble)
**Current Qt:** 5.15.13 (from `qtbase5-dev`)

---

## 1. System Packages

### Packages to install (replacing Qt5)

| Qt5 package | Qt6 replacement | Ubuntu Noble status |
|---|---|---|
| `qtbase5-dev` | `qt6-base-dev` | ✅ Available `6.4.2+dfsg-*` |
| `qttools5-dev-tools` (lrelease) | `qt6-tools-dev` + `qt6-l10n-tools` | ✅ Available |
| `qt5-serialport-dev` | `qt6-serialport-dev` | ✅ Available |
| `qt5-positioning-dev` | `qt6-positioning-dev` | ✅ Available |
| — | `qt6-svg-dev` (needed for SVG icon support) | ✅ Available |
| `libssl-dev` | unchanged | — |
| `cmake` 3.16+ → 3.22+ recommended | — | ✅ Noble has 3.28 |

**Qt6 version in Noble:** `6.4.2`. All required Qt6 modules ship in the distro.

### Before / After install commands

```
# Current (Qt5):
sudo apt install qtbase5-dev libssl-dev cmake build-essential protobuf-compiler
# plus: qttools5-dev-tools (for lrelease), libqt5serialport5-dev, libqt5positioning5-dev

# Qt6 replacement:
sudo apt install qt6-base-dev qt6-tools-dev qt6-l10n-tools \
                 qt6-serialport-dev qt6-positioning-dev \
                 libssl-dev cmake build-essential protobuf-compiler
```

### osgEarth / OSG caveat

osgEarth 3.8 (currently used) was built against Qt5. Upgrading to Qt6 requires:

1. Rebuild **OpenSceneGraph** — no change needed; OSG doesn't depend on Qt.
2. Rebuild **osgEarth** against Qt6 — needs osgEarth **3.10+** (the first release with Qt6 support). Building from source is required; Ubuntu Noble does not ship osgEarth packages.
3. The osgEarth CMake integration (`FindosgEarth.cmake`) is module-agnostic and needs no changes.

**If osgEarth is dropped or disabled,** the remaining map plugins (mapplugin) only depend on Qt6::Network and Qt6::Widgets — no extra complication.

---

## 2. CMake Build System Changes

### 2.1 Root `CMakeLists.txt`

| Location | Qt5 | Qt6 |
|---|---|---|
| L15 | `find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui Network Xml SerialPort)` | `find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Network SerialPort)` — **remove `Xml`** |
| L16 | `find_package(Qt5Positioning QUIET)` | `find_package(Qt6Positioning QUIET)` |
| L64 | `target_link_libraries(plugininterface Qt5::Core Qt5::Widgets ...)` | `target_link_libraries(plugininterface Qt6::Core Qt6::Widgets ...)` |
| L114 | `target_link_libraries(louhi ... Qt5::Widgets Qt5::Gui)` | `target_link_libraries(louhi ... Qt6::Widgets Qt6::Gui)` |

**QtXml module removed in Qt6** — it was listed in `find_package(Qt5 ... Xml ...)` on L15. The `QXmlStream*` classes it provided moved into Qt6::Core; they are always available. No separate module needed.

### 2.2 Plugins `CMakeLists.txt`

Every `Qt5::*` → `Qt6::*` across all `target_link_libraries` calls:

| Plugin | Change |
|---|---|
| natsplugin | `Qt5::Widgets Qt5::Core Qt5::Gui` → `Qt6::Widgets Qt6::Core Qt6::Gui` |
| messageviewerplugin | same |
| takplugin | + `Qt5::Network Qt5::Xml` → `Qt6::Network` (drop `Qt6::Xml`) |
| locationplugin | `Qt5::Network Qt5::SerialPort` → `Qt6::Network Qt6::SerialPort`; `Qt5Positioning_FOUND` → `Qt6Positioning_FOUND` |
| mapplugin | `Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network` → `Qt6::*` |
| osgearthplugin | same + drop `Qt5::Xml` if present |

### 2.3 Translations

- `lrelease` comes from `qt6-l10n-tools` (was `qttools5-dev-tools`)
- The `find_program(LRELEASE lrelease)` call still works (same binary name)
- `lupdate` command updates: use `qt6-tools-dev` version

### 2.4 Minimum CMake version

Qt6 requires CMake ≥ 3.16 (already satisfied). For best practice with Qt6's `qt_*` commands, CMake ≥ 3.21 is recommended, but the classic approach used here (`find_package` + `target_link_libraries`) works from 3.16.

---

## 3. C++ Code Changes

### 3.1 QDomDocument / QDomElement — **REWRITE REQUIRED**

**Files:** `plugins/cotmessage.h`, `plugins/cotmessage.cpp`

Qt6 **removed** the entire `QtXml` module, including `QDomDocument`, `QDomElement`, and all DOM tree APIs. The XML **writing** side already uses `QXmlStreamWriter` and needs no change. The **parsing** side (`CoTMessageParser::parse` and `CoTMessageParser::isValid`) uses `QDomDocument::setContent()` and `QDomElement` traversal — must be rewritten to `QXmlStreamReader`.

**Estimated effort:** ~150 lines in `cotmessage.cpp`.

Pattern: event-driven `QXmlStreamReader` loop checking `isStartElement()`, `name()`, `attributes()`, `readNext()`. The CoT XML structure is well-defined (flat `<event>` → `<point>` / `<detail>`), so the rewrite is mechanical.

**Additional changes:**
- `#include <QDomDocument>` → remove (or replace with `#include <QXmlStreamReader>`)
- `#include <QDomElement>` → remove
- Link to `Qt6::Xml` **not needed** — `QXmlStreamReader` is in `Qt6::Core`

### 3.2 QOverload usage — **HARMLESS, CAN CLEAN UP**

**Files:** `plugins/locationsettingsdialog.cpp`, `plugins/osgearthmapwidget.cpp`, `plugins/takserverconnection.cpp`

Qt6 removed some overloaded signals (e.g. `QComboBox::currentIndexChanged(const QString&)`), making the `QOverload<int>::of(...)` wrapper unnecessary for those cases. The code still compiles — the wrapper just disambiguates a non-existent overload which is a no-op. Cosmetic cleanup only.

### 3.3 QPluginLoader — **NO CHANGE**

`QPluginLoader`, `metaData()`, `instance()`, `unload()` — all unchanged API surface. The `Q_PLUGIN_METADATA` macro with `IID` and `FILE` also works identically.

### 3.4 QAction / QActionGroup — **NO CHANGE**

All `QAction` and `QActionGroup` usage in the codebase is through `Qt::Widgets` module, which exists in Qt6. Signal signatures used match Qt6.

### 3.5 QProcess — **NO CHANGE**

The single use (`QProcess::startDetached(...)`) is stable across versions.

---

## 4. Portable Deployment

### 4.1 `cmake/PortableDeploy.cmake`

| Location | Qt5 | Qt6 |
|---|---|---|
| L19 | `"Install Qt5 development tools."` | `"Install Qt6 development tools."` |
| L36 | same | same |
| L56–61 | `find_program(QT_QMAKE_EXECUTABLE qmake)` → `qmake -query QT_INSTALL_PLUGINS` → `QT5_PLUGIN_DIR` | `find_program(QT_QMAKE_EXECUTABLE qmake6)` → `qmake6 -query QT_INSTALL_PLUGINS` → `QT6_PLUGIN_DIR` |
| L65–76 | Fallback paths `/usr/lib/x86_64-linux-gnu/qt5/plugins`, `/usr/lib/qt5/plugins` | `/usr/lib/x86_64-linux-gnu/qt6/plugins`, `/usr/lib/qt6/plugins` |
| L68 | `"qt5/plugins"` | `"qt6/plugins"` |
| L79 | variable naming | rename to `QT6_PLUGIN_DIR` |

### 4.2 `cmake/portable-deploy.sh.in`

| Location | Qt5 | Qt6 |
|---|---|---|
| L15 | `QT5_PLUGIN_DIR="@QT5_PLUGIN_DIR@"` | `QT6_PLUGIN_DIR="@QT6_PLUGIN_DIR@"` |
| L119–138 | `${QT5_PLUGIN_DIR}` | `${QT6_PLUGIN_DIR}` |
| L120 | `${QT5_PLUGIN_DIR}/platforms/*.so` | `${QT6_PLUGIN_DIR}/platforms/*.so` |
| L128 | `${QT5_PLUGIN_DIR}/imageformats/*.so` | `${QT6_PLUGIN_DIR}/imageformats/*.so` |

### 4.3 macOS / Windows

- `macdeployqt` → `macdeployqt6`
- `windeployqt` → `windeployqt6`

---

## 5. Summary of Effort

| Category | Files | Type | Effort |
|---|---|---|---|
| CMake target renames | `CMakeLists.txt`, `plugins/CMakeLists.txt` | Mechanical search‑replace (~20 lines) | Small |
| CMake module/package renames | `cmake/PortableDeploy.cmake`, `cmake/portable-deploy.sh.in` | Manual (~30 lines) | Small |
| QDom → QXmlStreamReader rewrite | `plugins/cotmessage.cpp`, `plugins/cotmessage.h` | Rewrite (~150 lines) | Medium |
| QOverload cleanup (optional) | 3 files | Delete wrapper, keep code | Trivial |
| System package install | — | `apt install` replacing 5 packages | Small |
| osgEarth rebuild | — | Rebuild osgEarth 3.10+ with Qt6 | Medium–Large |

**Total code changes:** ~20 lines of CMake, ~150 lines of C++ rewrite, ~30 lines of deploy scripts.

**Blocking path:** The `QDomDocument` rewrite in `cotmessage.cpp` is the only code change that prevents compilation. Everything else is mechanical.

**osgEarth dependency:** If osgEarth is still needed, it must be rebuilt from source with Qt6 support (osgEarth 3.10+). If osgEarth is not currently active (our system has it not installed), the migration can proceed without touching it.
