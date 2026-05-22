# Plugin Interface Concept

## 1. Core Plugin Structure

Each plugin must provide:
- **Metadata**: name, version, description, author
- **Type**: Communication, Map, or Screen
- **Dependencies**: required plugins or core services
- **Capabilities**: what it can do

## 2. Lifecycle Interface

```
load() → initialize() → start() → stop() → unload()
```

- **load**: Load shared libraries, verify dependencies
- **initialize**: Set up NATS subscriptions, UI elements
- **start**: Begin processing, publishing
- **stop**: Pause operations, keep subscriptions
- **unload**: Clean shutdown, release resources

## 3. Registration Contract

Each plugin registers with the core:
- **Menu entries**: top-level menu + submenus
- **Toolbar entries**: buttons in the main toolbar
- **NATS topics**: subscribe and/or publish
- **UI requirements**: shared map? dedicated screen?

### Toolbar Registration

Plugins can register toolbar buttons by overriding `getToolbarEntries()`:

```cpp
struct ToolbarEntry {
    QString id;         // Unique action identifier
    QString text;       // Button label
    QString iconPath;   // Path to icon file (empty = no icon)
    QString tooltip;    // Tooltip text
    QString group;      // Group name (buttons in same group share a separator)
};
```

**Example** — Map plugin registering zoom controls:

```cpp
QVector<ToolbarEntry> MapPlugin::getToolbarEntries() const
{
    return {
        { "map_zoom_in",  "Zoom In",  ":/icons/zoom_in.png",  "Zoom in",  "Navigation" },
        { "map_zoom_out", "Zoom Out", ":/icons/zoom_out.png", "Zoom out", "Navigation" },
    };
}
```

Buttons are grouped by `group` — same name = same visual group, separated by dividers. An empty group places the button in the default group.

When a button is clicked, the core calls `handleToolbarAction(actionId)` on the plugin:

```cpp
void MapPlugin::handleToolbarAction(const QString& actionId)
{
    if (actionId == "map_zoom_in")  zoomIn();
    if (actionId == "map_zoom_out") zoomOut();
}
```

Both methods have empty default implementations so existing plugins are not required to override them.

## 4. Communication Model

- **Core bus**: Plugins communicate via NATS topics
- **Plugin-to-plugin**: Direct topic routing or core mediation
- **Events**: Core notifies plugins of system state changes

## 5. Extension Points

- **Map providers**: Register map layers, overlays
- **Data handlers**: Custom message types
- **UI widgets**: Dockable panels, dialogs