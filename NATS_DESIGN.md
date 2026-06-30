# NATS Message Architecture for LOUHI

**Status:** Adopted
**Date:** 2026-06-30

---

## 1. Model

Mimics TAK's group-based access. Groups are defined and enforced entirely on
the **NATS server** — the client (LOUHI) does not know or care about groups.

The client subscribes to everything it might need (`grp.>`, `sys.>`) and
publishes to any subject plugins produce. The NATS server decides what to
route based on the client's JWT credentials.

This keeps the client dumb and the policy in one place — the server config.

---

## 2. Subject Tree

```
grp.<group>.<message_type>     — group-scoped tactical messages
sys.<message_type>             — system-wide / admin
```

| Prefix | Scope | Purpose |
|--------|-------|---------|
| `grp.<group>.>` | NATS | Group messages — routing controlled by server JWT |
| `sys.>` | NATS | System/admin messages — restricted to admin JWT |

There is no `msg.>` subject on NATS. Internal in-process routing uses
`msg.>` subjects via `PluginManager::broadcastMessage` / `emitMessageToPlugins`
and never touches the network.

---

## 3. Protocol Layer

Wire format is **CoTXML** by default (the `<event>` document from the TAK
ecosystem). Every plugin that sends or receives tactical data produces and
accepts CoTXML.

No per-plugin protocol override — if a plugin needs a custom format it must
self-describe in its metadata and the consumer must opt in.

---

## 4. Groups

### 4.1 Server side

Groups are defined in the NATS server configuration (operator/account JWT
in production, or a simple `resolver` config for development).

Each client JWT encodes which `grp.<group>.>` subjects the user can publish
to and subscribe to:

```json
{
  "sub": "USR-wolfman",
  "pub":  { "allow": ["grp.tacdata.>", "grp.casevac.>"] },
  "sub":  { "allow": ["grp.tacdata.>", "grp.casevac.>", "sys.>"] }
}
```

The server enforces these permissions. A publish to `grp.other.>` is silently
dropped at the server level.

### 4.2 Client side

LOUHI does not read, store, or care about group membership. The NATS plugin
subscribes broadly and lets the server filter.

**On connect:**
1. Subscribes to `grp.>`
2. Subscribes to `sys.>`
3. That's it.

**On publish:**
1. Any plugin calling `publish(topic, payload)` where topic matches `grp.*`
   or `sys.*` is forwarded to NATS.
2. Topics matching `msg.*` are never published to NATS (in-process only).
3. If the server rejects the publish (JWT doesn't allow that subject), the
   server drops it silently — the client never sees the rejection.

### 4.3 Development mode (no auth)

When connecting to a NATS server with no auth (e.g. local `nats-server`),
all `grp.>` and `sys.>` subjects are open. The client simply uses groups
as they appear in the subject — the group name is whatever the plugin puts
in the topic string.

---

## 5. Implementation: NATS Plugin

### 5.1 Connection config

```json
{
  "servers": [
    {
      "id": "server_1",
      "name": "Tactical NATS",
      "url": "nats://tak.gofferje.net:4222",
      "autoConnect": true
    }
  ]
}
```

Credentials (JWT + seed key or user/pass) are stored alongside the server
config, or loaded from external files.

### 5.2 Subscribe / Publish flow

```
Plugin ──publish("grp.tacdata.pos", cotXml)──→ NatsPlugin
  → natsConnection_Publish("grp.tacdata.pos", cotXml)
  → NATS server checks JWT pub.allow → routes to subscribers or drops

Incoming:
NATS server → natsConnection_Msg → NatsPlugin
  → emit messageReceived("grp.tacdata.pos", cotXml)
  → PluginManager::emitMessageToPlugins → broadcastMessage
  → matching plugins receive in deliverMessage()
```

### 5.3 EMCON

When EMCON is active, no messages are published to NATS regardless of
permissions. In-process `msg.>` routing continues unaffected.

### 5.4 Topic filtering

The NATS plugin subscribes to `grp.>` and `sys.>` on connect.
`collectAllSubscribeTopics()` from the PluginManager is **no longer needed**
for subscription management — the NATS plugin always subscribes to the
full wildcard.

However, the PluginManager still uses `collectAllSubscribeTopics()` for
in-process routing (`msg.>` subjects) between plugins running in the same
process.

---

## 6. Auth: NATS JWT (Deployment)

Production deployments use NATS JWT auth. The NATS operator issues:

- **Operator JWT** — one per deployment, signs accounts
- **Account JWT** — one per logical unit (or one shared), signs users
- **User JWT** — one per client, embeds `pub.allow` / `sub.allow`

Development: no auth, or a simple `nats-server` with `--auth` token.

---

## 7. Differences from TAK

| Aspect | TAK | LOUHI + NATS |
|--------|-----|--------------|
| Group enforcement | Server-side (TAK Server) | Server-side (NATS JWT) |
| Client group config | `MissionPackage` / config file | Not needed — client is unaware |
| Wire format | CoTXML (via TCP/UDP) | CoTXML (via NATS subjects) |
| Routing | TAK Server relays by group | NATS routes by subject |
| Presence | Client heartbeat to server | NATS client ping (built-in) |
