# Location Plugin

**Type:** Communication  
**ID:** `location_communication`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

Location provider plugin supporting four backends with automatic failover:
Serial GPS (NMEA sentences via QSerialPort), GPSD (TCP connection to gpsd
daemon), Manual (fixed position from config), and System Location (OS location
services via Qt Positioning). Broadcasts location data on the message bus and
responds to request-reply queries.

## Capabilities

- Serial GPS
- GPSD
- Manual
- System Location
- Failover
- Request-Reply

## Topics

| Direction | Topics |
|-----------|--------|
| Publishes | `location.position`, `location.position.reply` |
| Subscribes | `location.request` |

## Build Dependencies

- `plugininterface` (shared library)
- `Qt5::Widgets`, `Qt5::Core`, `Qt5::Gui`
- `Qt5::Network` — TCP socket for GPSD provider
- `Qt5::SerialPort` — cross-platform serial port access
- `Qt5::Positioning` *(optional)* — system location provider via
  `QGeoPositionInfoSource` (GeoClue2 on Linux, Core Location on macOS,
  Windows Location API). The plugin builds and runs without it; the System
  Location option is simply absent from the settings dialog.

## Menus

| Menu | Items |
|------|-------|
| Communication | Connect, Disconnect |
| Settings | *(direct action — opens configuration dialog)* |

## Toolbars / Buttons

None registered via `getToolbarEntries()`. The status widget provides:

- **Configure...** button

## Usage Notes

- Serial GPS uses Qt's `QSerialPort` (cross-platform). Default port:
  `/dev/ttyUSB0` on Linux/macOS, `COM1` on Windows.
- GPSD provider connects to a gpsd daemon over TCP and parses JSON TPV
  messages.
- System Location provider wraps `QGeoPositionInfoSource` and uses the
  operating system's native location services. Requires `Qt5::Positioning`
  at build time. No configuration needed — it auto-detects the platform's
  location backend.
- All NMEA providers parse GPGGA, GPRMC, and GPGSA sentences.
- Automatic failover: switches from main to fallback provider on
  disconnect or error.
- Broadcasts location at a configurable interval.
