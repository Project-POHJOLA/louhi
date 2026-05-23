# Message Viewer Plugin

**Type:** Screen  
**ID:** `message_viewer`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

Screen-type debug plugin that displays all messages flowing through the plugin
message bus. Shows messages in a list (topic + first 50 characters) with a
detail view for full content. Users can add/remove topic subscription filters
to narrow displayed messages.

## Capabilities

- View Messages
- Filter Messages

## Topics

| Direction | Topics |
|-----------|--------|
| Publishes | *(none)* |
| Subscribes | `>` (all topics) |

## Build Dependencies

- `plugininterface` (shared library)
- `Qt5::Widgets`, `Qt5::Core`, `Qt5::Gui`

## Menus

| Menu | Items |
|------|-------|
| View | Show Message Viewer, Clear Messages |

## Toolbars / Buttons

None.

## Usage Notes

- No external dependencies beyond Qt5.
- Gets an automatic dock widget from the main application (standard for Screen
  plugins).
- Message list is capped at a configurable maximum (default 100 messages).
- Useful diagnostic tool for development and debugging message flow.
