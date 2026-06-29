# Changelog

## [Unreleased]

### Icon & Entity System (2026-06-29)

- **Icon resolution rewrite**: MIL-STD-2525B is now the primary icon path.
  CoT type → `cotToSidc()` → 15-char SIDC → pre-rendered PNG from
  `assets/icons/map/2525/` (3280 icons). Iconsets are only used for
  explicit `<usericon iconsetpath>` CoT attributes.
- **2525B SIDC conversion**: `cotToSidc()` and `cleanSidc()` ported from
  tak-webview-cesium JavaScript reference, with progressive SIDC fallback
  (full SIDC → strip function code → generic dimension → unknown ground).
- **Milsym parsing**: `OsgEarthPlugin::parseCotMessage()` now extracts
  `__milsym`, `__milicon`, `milsym`, `milicon` from CoT detail for
  direct 2525B SIDC lookup.
- **CoT entity rendering**: `OsgEarthMapWidget` renders tactical entities
  as `osgEarth::PlaceNode` annotations with icon and callsign label;
  5-second stale-check timer removes expired entities.
- **Iconset counting fixed**: `loadIconset()` now reports total icon count
  and type2525b-mapped count separately instead of collapsing same-key
  entries (FEMA: 40 icons / 40 mappings, WASP: 35 / 0, etc.).
- **OSM HTTP User-Agent**: Set `osgEarth::HTTPClient::setUserAgent()`
  so OSM tile servers accept requests.

### Bug Fixes (2026-06-29)

- **Black globe on startup**: `m_mapInitialized` was an uninitialized
  garbage `bool`. If non-zero, `initializeGL()` returned immediately
  without any osgEarth setup. Also fixed uninitialized `m_updateTimer`.
- **Iconsets dir discovery**: Replaced fragile `applicationDirPath() +
  "/../assets/..."` fallback chain with `findIconsBaseDir()` that searches
  build-tree, portable-bundle, system data dirs
  (`QStandardPaths::GenericDataLocation`, cross-platform), and per-user
  writable override.

### Qt6 Migration (2026-06-29)

- **CMake**: `Qt5::` → `Qt6::`, removed Qt5Xml, added Qt6OpenGLWidgets.
- **API migration**: `QDomDocument` → `QXmlStreamReader` in CoT parser;
  `QLibraryInfo::location()` → `path()`.
- **System deps**: `qt6-base-dev`, `qt6-serialport-dev`,
  `qt6-positioning-dev`, `qt6-tools-dev`, `qt6-l10n-tools` on Ubuntu 24.04.
- **OSG/osgEarth**: Rebuilt with GL3 profile, Qt6-free osgEarth 3.8.

### NATS Architecture (2026-06-29)

- **NATS_DESIGN.md**: Full subject tree (`msg.*`, `alert.*`), JWT auth
  model (one NATS account per role archetype), JetStream streams,
  standard echelon group chats, DM/group chat patterns.
- **Protocol layer**: CoTXML declared as the standard wire format over
  NATS. Plugins may explicitly register a different protocol.

### EMCON (2026-06-27)

- **Emission Control**: Global toggle suppresses all outbound NATS
  transmissions while keeping inbound reception. Red EMCON badge in
  status bar, "Go Dark" menu toggle, per-plugin `publish()` method
  gated by `PluginManager::m_emconActive`.

### Cross-Platform Support (2026-05-23)

- **Serial ports**: Replaced POSIX `termios.h` with `QSerialPort`.
- **Plugin discovery**: Added `*.dll` filter for Windows alongside
  `*.so` / `*.dylib`.
- **Platform-conditional RPATH**: Linux `$ORIGIN/../lib`, macOS
  `@executable_path/../lib` / `@loader_path/../lib`, Windows none.
- **Location provider**: `Qt6::Positioning` optional dependency,
  wraps platform GPS (GeoClue on Linux, CoreLocation on macOS, Win
  Location API on Windows).
- **Defaults**: Serial device names platform-conditional
  (`/dev/ttyUSB0`, `COM1`) in CMake.

### Translations (2026-05-23)

- All user-visible strings wrapped in `tr()`. `.ts`/`.qm` files for
  en/de/fi/sv. `lupdate`/`lrelease` integrated into build.

### Plugin Menu System (2026-05-22)

- **Plugin Manager UI**: Enable/disable plugins, info dialog with
  author/description/version.
- **Toolbar groups**: Each plugin group gets its own `QToolBar` stacked
  on the right border. System toolbar (About, Exit) stays separate.
- **Basemap dock**: `BasemapDockWidget` for single-click image/terrain
  source switching in osgEarth plugin.
- **Direct action menu entries**: `MenuEntry::addAsDirectAction` flag
  bypasses submenu nesting.

### Map & Tiles (2026-05-22)

- **Map plugin (tile-based)**: OSM/Carto Dark/WMS/XYZ tile rendering,
  pan/zoom, LRU disk cache at `~/.cache/Louhi/tiles/`.
- **osgEarth 3D globe**: Embedded in `QOpenGLWidget` via
  `osgViewer::GraphicsWindowEmbedded`. osgEarth 3.8 + OSG 3.7 with
  GL3 profile.
- **Shared map sources**: `mapsources.h` struct shared between tile and
  3D map plugins. `MapSourcesDialog` in `libplugininterface`.
- **Tile cache**: `TileCache` class with per-source subdirectories,
  clear cache action.

### TAK Communication (2026-05-21)

- **CoT over TLS**: TCP+TLS connection to TAK servers, auto-reconnect
  with exponential backoff.
- **CoT XML parsing**: TCP stream-aware parser (`cotmessage.cpp`),
  extracts position, callsign, type, staleness, iconset, color.
- **Position reporting**: Subscribes to `location.position`, sends
  CoT position reports with `<takv>`, `<__group>`, and configurable
  CoT type per server.
- **Per-server config**: Server name, address, port, callsign,
  protocol, enrollment cert, debug logging toggle.

### NATS Communication (2026-05-21)

- **NATS C Client**: Git submodule at `deps/nats.c` (v3.12.0), built
  statically with `-fPIC`. No system `libnats-dev` needed.
- **Multi-server UI**: Add/remove servers, per-server connection
  status LEDs, connection status in status bar.
- **Wildcard topic routing**: NATS-style `>` and `*` in subscribe
  topics, aggregated across all plugins by `PluginManager`.
- **EMCON support**: Publish gating, per-server EMCON state in config.

### Location Plugin (2026-05-21)

- **Providers**: Serial GPS (NMEA), GPSD, Manual lat/lon input.
- **Failover**: Automatic main/fallback provider switching.
- **Publishes**: `location.position` and `location.position.reply` topics.

### Core Architecture (2026-05-21)

- **Plugin system**: `PluginInterface` with lifecycle (`load`/`init`/
  `start`/`stop`/`unload`), menu entries, toolbar entries, message
  delivery, JSON config.
- **PluginManager**: Discovery via QPluginLoader, lifecycle management,
  message routing (NATS wildcard matching), topic aggregation.
- **MainWindow**: Dock-only layout (`setDockNestingEnabled(true)`),
  dock geometry saved/restored in config.
- **ConfigManager**: JSON config at `~/.config/Louhi/config.json`,
  shared cross-plugin config section.
- **Portable deploy**: `cmake -DBUILD_PORTABLE=ON && make
  portable-deploy` → self-contained `Louhi.app/` bundle.
