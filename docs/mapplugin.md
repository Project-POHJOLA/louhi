# Map Plugin

**Type:** Map  
**ID:** `map_plugin`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

2D tiled map plugin rendering OSM-style raster tiles using QPainter. Supports
XYZ tile sources (OSM Standard, Carto Dark) and WMS tile sources. Tiles are
fetched over HTTP and cached both in-memory (LRU) and on-disk via the shared
TileCache.

## Capabilities

- Map
- OSM
- Carto Dark
- WMS
- XYZ

## Topics

| Direction | Topics |
|-----------|--------|
| Publishes | `location.request` |
| Subscribes | `location.position`, `location.position.reply` |

## Build Dependencies

- `plugininterface` (shared library)
- `Qt5::Widgets`, `Qt5::Core`, `Qt5::Gui`
- `Qt5::Network` — HTTP tile fetching via `QNetworkAccessManager`

## Menus

| Menu | Items |
|------|-------|
| View | Show Map |
| Settings | *(direct action — opens map sources dialog)* |

## Toolbars / Buttons

None.

## Usage Notes

- Pure QPainter rendering — no OpenGL or osgEarth required.
- Shares `MapSourcesDialog` with the OsgEarth plugin for managing custom
  XYZ/WMS sources.
- Uses shared `TileCache` at `~/.cache/Louhi/tiles/` for on-disk tile caching.
- Auto-centers on first received location position (with fallback to Helsinki).
- Supports pan (click-drag) and zoom (mouse wheel) with Mercator projection.
