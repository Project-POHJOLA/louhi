# NATS Communication Plugin

**Type:** Communication  
**ID:** `nats_communication`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

NATS transport plugin providing publish/subscribe, request-reply, and
multi-server communication via NATS JetStream. Other plugins request topic
subscriptions through the `setSubscribedTopics()` API, and the plugin subscribes
to those topics on all connected servers.

## Capabilities

- Publish
- Subscribe
- Request-Reply
- Multi-Server

## Topics

| Direction | Topics |
|-----------|--------|
| Publishes | `*` |
| Subscribes | Dynamic (via `setSubscribedTopics()`) |

## Build Dependencies

- `plugininterface` (shared library)
- `Qt5::Widgets`, `Qt5::Core`, `Qt5::Gui`
- `nats_static` — NATS C Client (git submodule at `deps/nats.c`, built statically)

## Menus

| Menu | Items |
|------|-------|
| Communication | Connect, Disconnect |
| Settings | *(direct action — opens configuration dialog)* |

## Toolbars / Buttons

None registered via `getToolbarEntries()`. The status widget provides:

- **Connect All** button
- **Disconnect All** button
- **Configure...** button

## Usage Notes

- Manages a list of NATS server connections concurrently.
- Connections are configured in the settings dialog (URL, port, auto-connect).
