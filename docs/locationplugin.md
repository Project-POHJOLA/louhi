# Location Plugin

**Type:** Communication  
**ID:** `location_communication`  
**Version:** 0.1  
**Author:** LOUHI Team

## Description

Location provider plugin supporting three GPS backends with automatic failover:
Serial GPS (NMEA sentences over POSIX serial), GPSD (TCP connection to gpsd
daemon), and Manual (fixed position from config). Broadcasts location
data on the message bus and responds to request-reply queries.

## Capabilities

- Serial GPS
- GPSD
- Manual
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
- POSIX system headers (serial): `fcntl.h`, `unistd.h`, `termios.h`,
  `sys/ioctl.h`, `errno.h`

## Menus

| Menu | Items |
|------|-------|
| Communication | Connect, Disconnect |
| Settings | *(direct action — opens configuration dialog)* |

## Toolbars / Buttons

None registered via `getToolbarEntries()`. The status widget provides:

- **Configure...** button

## Usage Notes

- No external GPS library required — serial GPS uses POSIX APIs directly.
- GPSD provider connects to a gpsd daemon over TCP and parses JSON TPV messages.
- Serial provider parses NMEA sentences (GPGGA, GPRMC, GPGSA).
- Automatic failover: switches from main to fallback provider on disconnect/error.
- Broadcasts location at a configurable interval.
