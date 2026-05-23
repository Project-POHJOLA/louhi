# TAK Communication Plugin

**Type:** Communication  
**ID:** `tak_communication`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

Team Awareness Kit (TAK) communication plugin implementing the CoT (Cursor on
Target) protocol over TCP/TLS. Supports multiple server connections
simultaneously, each with its own certificate and identity configuration.
Receives location updates from the Location plugin and forwards them as CoT
position reports. Parses incoming CoT messages and publishes them to the NATS
message bus.

## Capabilities

- CoT
- TCP/TLS
- Multi-Server
- Position Reports
- Chat

## Topics

| Direction | Topics |
|-----------|--------|
| Publishes | `tak.>` |
| Subscribes | `tak.>`, `location.position` |

## Build Dependencies

- `plugininterface` (shared library)
- `Qt5::Widgets`, `Qt5::Core`, `Qt5::Gui`
- `Qt5::Network` — SSL/TLS sockets
- `Qt5::Xml` — CoT XML parsing/building
- `OpenSSL::Crypto` — PKCS12 certificate import

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

- Requires Qt5 Network and XML modules.
- Uses OpenSSL for PKCS12/PFX certificate loading (loads legacy provider for
  older certificate formats).
- Automatic reconnection with exponential backoff (1 s min, 60 s max).
- Subscribes to `location.position` to automatically send position reports.
- Configurable per-server: callsign, color, role, CoT type, debug logging.
