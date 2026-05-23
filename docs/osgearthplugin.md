# OsgEarth Map Plugin

**Type:** Map  
**ID:** `osgearth_map`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

3D globe map plugin embedding osgEarth (OpenSceneGraph-based terrain rendering)
inside a QOpenGLWidget. Supports XYZ and WMS image layers with an
EarthManipulator for intuitive 3D navigation (pan, zoom, rotate). Subscribes to
location position topics for initial centering.

## Capabilities

- Map
- 3D
- osgEarth
- OSM
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
- `Qt5::Network` — tile fetching
- `${OSGEARTH_LIBRARIES}` — osgEarth libraries
- `${OSGEARTH_INCLUDE_DIRS}` — osgEarth include paths
- **Requires osgEarth 3.8 and OpenSceneGraph (GL3 profile)** built from source

## Menus

| Menu | Items |
|------|-------|
| View | Show 3D Map |
| Map | Basemap |
| Settings | *(direct action — opens map sources dialog)* |

## Toolbars / Buttons

| ID | Text | Tooltip | Group |
|----|------|---------|-------|
| `osgearth_basemap` | Basemap | Select basemap for the globe | Map |

## Additional Docks

- **BasemapDockWidget** — QDockWidget listing basemap sources (OSM, Carto Dark,
  custom) for quick switching.

## Usage Notes

- **Conditional build** — only compiled when `osgEarth_FOUND` is true.
- Requires osgEarth and OpenSceneGraph installed system-wide (built from source
  with `-DOPENGL_PROFILE=GL3`).
- Uses OpenGL 3.3 Core Profile via QOpenGLWidget.
- Shares `MapSourcesDialog` and `TileCache` with the 2D Map plugin.
