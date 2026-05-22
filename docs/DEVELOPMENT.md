# LOUHI Development Guide

## Plugin Development

### Overview

LOUHI uses Qt's plugin system to load plugins at runtime as shared libraries (`.so` files). Each plugin must implement the `PluginInterface` and provide a JSON metadata file.

### Plugin Interface

All plugins inherit from `PluginInterface`, defined in `src/plugininterface.h`. The interface defines the contract between the core application and plugins.

#### Plugin Types

Plugins declare their type via `PluginInfo::type`. Three types are supported:

| Type | Purpose | UI Behavior |
|------|---------|-------------|
| `Communication` | Handles external communication (NATS, mesh, etc.) | No automatic dock widget; accessible via Settings menu |
| `Map` | Renders data on a map view | May share map with other plugins or request a dedicated view |
| `Screen` | Displays data in dockable panels | Gets an automatic dock widget with View menu integration |

#### Core Structures

**PluginInfo** - Metadata returned by `getPluginInfo()`:

```cpp
struct PluginInfo {
    QString id;                  // Unique identifier (e.g. "nats")
    QString name;                // Display name (e.g. "NATS Communication")
    QString version;             // Semantic version (e.g. "0.1")
    QString description;         // Human-readable description
    QString author;              // Author or team name
    PluginType type;             // Communication, Map, or Screen
    bool enabled;                // Whether plugin is enabled
    QStringList dependencies;    // Required plugin IDs
    QStringList capabilities;    // What the plugin can do
    QStringList subscribeTopics; // NATS topics to subscribe to
    QStringList publishTopics;   // NATS topics the plugin publishes to
};
```

**MenuEntry** - Menu structure returned by `getMenuEntries()`:

```cpp
struct MenuEntry {
    QString topMenu;             // Top-level menu name (e.g. "Communication")
    QStringList subMenus;        // Submenu items (e.g. ["Connect", "Disconnect"])
};
```

Multiple plugins can share the same `topMenu` name; their submenu items will be merged.

#### Required Methods

Every plugin must implement these methods:

**Metadata & Menus:**
- `PluginInfo getPluginInfo() const` - Returns plugin metadata
- `QVector<MenuEntry> getMenuEntries() const` - Returns menu structure

**Lifecycle (called in order):**
- `bool load()` - Load resources, verify dependencies. Called when plugin is discovered.
- `bool initialize()` - Set up internal state, create UI elements, prepare NATS subscriptions.
- `bool start()` - Begin active operations (publishing, processing, connecting).
- `bool stop()` - Pause operations while keeping subscriptions and UI intact.
- `bool unload()` - Full cleanup, release all resources. Called when plugin is disabled or app exits.

**UI & Configuration:**
- `QWidget* getWidget()` - Returns the plugin's UI widget. Return `nullptr` for plugins without a dock widget (e.g. Communication plugins).
- `void configure(QWidget* parent)` - Opens a settings dialog. `parent` is the main window for proper dialog parenting.
- `QJsonObject getConfig() const` - Serializes plugin settings to JSON. Called on app exit.
- `void setConfig(const QJsonObject& config)` - Restores plugin settings from JSON. Called on app startup.

#### Signals

Plugins can emit these signals (defined in `PluginInterface`):

- `void messageReceived(const QString& topic, const QString& payload)` - Emitted when a message arrives on a subscribed topic
- `void statusChanged(const QString& status)` - Emitted when plugin status changes (e.g. "Connected", "Disconnected", "Error")
- `void showWidgetRequested()` - Emitted to request the dock widget be shown
- `void connectionStatusChanged(const QString& connectionName, const QString& status)` - Emitted for connection LED status in the status bar. `connectionName` is the display name (e.g. "NATS", "TAK"). `status` format: `"<name>:<Connected|Disconnected>"` (e.g. `"localhost:Connected"`, `"MyTAKServer:Disconnected"`)

#### Optional Methods

- `void setSubscribedTopics(const QStringList& topics)` - Called by PluginManager to set aggregated subscription topics on Communication plugins. Override to update your NATS subscriptions dynamically.

### NATS Topic Wildcards

The message bus supports NATS-style wildcard matching for `subscribeTopics`:

| Wildcard | Meaning | Example |
|----------|---------|---------|
| `>` | Matches all subtopics | `>` matches everything, `tak.>` matches `tak.server1`, `tak.server1.data`, etc. |
| `*` | Matches exactly one token | `tak.*` matches `tak.server1` but not `tak.server1.data` |

The `PluginManager` evaluates wildcards when routing messages from Communication plugins to subscriber plugins.

### Creating a New Plugin

#### 1. File Structure

Create these files in the `plugins/` directory:

```
plugins/
    myplugin.h           - Plugin class declaration
    myplugin.cpp         - Plugin implementation
    myplugin.json        - Qt plugin metadata
```

#### 2. Metadata File (`myplugin.json`)

```json
{
    "IID": "com.louhi.plugininterface/1.0",
    "Name": "My Plugin",
    "Description": "Description of what this plugin does",
    "Version": "0.1",
    "Author": "Your Name"
}
```

#### 3. Header File (`myplugin.h`)

```cpp
#ifndef MYPLUGIN_H
#define MYPLUGIN_H

#include "../src/plugininterface.h"
#include <QtPlugin>

class MyPlugin : public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
    Q_PLUGIN_METADATA(IID "com.louhi.plugininterface/1.0" FILE "myplugin.json")

public:
    MyPlugin(QObject* parent = nullptr);
    ~MyPlugin();

    PluginInfo getPluginInfo() const override;
    QVector<MenuEntry> getMenuEntries() const override;

    bool load() override;
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool unload() override;

    QWidget* getWidget() override;
    void configure(QWidget* parent) override;

    QJsonObject getConfig() const override;
    void setConfig(const QJsonObject& config) override;

    void setSubscribedTopics(const QStringList& topics) override;  // For Communication plugins

private:
    PluginInfo m_info;
    // Add plugin-specific members
};

#endif
```

#### 4. Implementation File (`myplugin.cpp`)

Key implementation patterns:

**Constructor:** Initialize `m_info` with all fields:

```cpp
MyPlugin::MyPlugin(QObject* parent) : PluginInterface(parent) {
    m_info.id = "myplugin";
    m_info.name = "My Plugin";
    m_info.version = "0.1";
    m_info.description = "Description";
    m_info.author = "Author";
    m_info.type = PluginType::Screen;  // or Communication, Map
    m_info.enabled = true;
    m_info.dependencies = {};
    m_info.capabilities = {"capability1", "capability2"};
    m_info.subscribeTopics = {};
    m_info.publishTopics = {};
}
```

**getPluginInfo() and getMenuEntries():**

```cpp
PluginInfo MyPlugin::getPluginInfo() const {
    return m_info;
}

QVector<MenuEntry> MyPlugin::getMenuEntries() const {
    return {
        {"My Menu", {"Action 1", "Action 2"}}
    };
}
```

**Lifecycle methods:** Return `true` on success, `false` on failure:

```cpp
bool MyPlugin::load() {
    // Verify dependencies, load resources
    return true;
}

bool MyPlugin::initialize() {
    // Create UI, set up subscriptions
    return true;
}

bool MyPlugin::start() {
    // Begin active operations
    return true;
}

bool MyPlugin::stop() {
    // Pause operations
    return true;
}

bool MyPlugin::unload() {
    // Full cleanup
    return true;
}
```

**Configuration (auto-saved on exit, restored on startup):**

```cpp
QJsonObject MyPlugin::getConfig() const {
    QJsonObject config;
    // config["someSetting"] = m_someValue;
    return config;
}

void MyPlugin::setConfig(const QJsonObject& config) {
    // m_someValue = config["someSetting"].toString();
}
```

#### 5. CMake Configuration

Add the plugin to `plugins/CMakeLists.txt`:

```cmake
add_library(myplugin SHARED
    myplugin.h
    myplugin.cpp
)

target_link_libraries(myplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui)

set_target_properties(myplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
)
```

### Plugin Lifecycle Flow

```
App Start
    |
    v
Plugin Discovery (scan plugins/ directory)
    |
    v
load() ────────────────────────┐
    |                           |
    v                           |
initialize() ───────────────────┤
    |                           |
    v                           |
start() ◄──── stop() ───────────┤ (toggle during runtime)
    |                           |
    v                           |
App Exit                        |
    |                           |
    v                           v
unload() ◄─────────────────────┘
```

### Communication via NATS

Plugins communicate through NATS topics. Use the topic lists in `PluginInfo` to declare which topics you subscribe to and publish on:

- `subscribeTopics` - Topics the plugin listens to
- `publishTopics` - Topics the plugin sends messages to

The `messageReceived` signal delivers incoming messages. Connect to it or override handling in your plugin.

### Best Practices

- Keep plugins focused on a single responsibility
- Use `capabilities` to advertise what your plugin provides
- Declare `dependencies` if your plugin requires another plugin
- Return `nullptr` from `getWidget()` for Communication plugins
- Always clean up resources in `unload()`
- Store configuration in `QJsonObject` for persistence
- Use `statusChanged` signal to report connection/operational state
- Communication plugins should emit `connectionStatusChanged` to update status bar LEDs (format: `"<serverName>:<Connected|Disconnected>"`)
