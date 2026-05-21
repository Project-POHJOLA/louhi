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
- **NATS topics**: subscribe and/or publish
- **UI requirements**: shared map? dedicated screen?

## 4. Communication Model

- **Core bus**: Plugins communicate via NATS topics
- **Plugin-to-plugin**: Direct topic routing or core mediation
- **Events**: Core notifies plugins of system state changes

## 5. Extension Points

- **Map providers**: Register map layers, overlays
- **Data handlers**: Custom message types
- **UI widgets**: Dockable panels, dialogs