# AGV -> RCS MQTT Status and Task Feedback Protocol

## Scope

This protocol standardizes AGV feedback consumed by the RCS integration sample.
The RCS service subscribes to AGV MQTT messages, updates mission/task state, and sends WCS callbacks.

## Topics

| Topic | Direction | Description |
|---|---|---|
| `agv/{agv_id}/status` | AGV -> RCS | Online/offline and telemetry events |
| `agv/{agv_id}/task/events` | AGV -> RCS | Mission/task progress events |

The RCS default subscription is `agv/+/#`, which receives both status and task event messages.
Use `RCS_AGV_MQTT_TOPIC=agv/+/task/events` if only task callbacks are required.

## Runtime Configuration

| Java property | Environment variable | Default |
|---|---|---|
| `rcs.agvMqtt.enabled` | `RCS_AGV_MQTT_ENABLED` | `false` |
| `rcs.agvMqtt.brokerUri` | `RCS_AGV_MQTT_BROKER_URI` | `tcp://127.0.0.1:1883` |
| `rcs.agvMqtt.clientId` | `RCS_AGV_MQTT_CLIENT_ID` | `rcs-agv-status-subscriber` |
| `rcs.agvMqtt.topic` | `RCS_AGV_MQTT_TOPIC` | `agv/+/#` |
| `rcs.agvMqtt.qos` | `RCS_AGV_MQTT_QOS` | `1` |
| `rcs.agvMqtt.username` | `RCS_AGV_MQTT_USERNAME` | empty |
| `rcs.agvMqtt.password` | `RCS_AGV_MQTT_PASSWORD` | empty |

## Payload

All fields use `snake_case`. The parser also accepts common camelCase aliases.

| Field | Type | Required | Description |
|---|---|---|---|
| `message_id` | string | Recommended | Unique message id, preferred idempotency key |
| `agv_id` | string | Yes | AGV id |
| `event_type` | string | Yes | Event enum |
| `mission_no` | string | For task events | RCS/openTCS order name |
| `task_no` | string | Optional | WCS task number |
| `point_id` | string | For arrived events | Current point/location |
| `battery` | int | Optional | Battery percentage |
| `online` | bool | Optional | Online state |
| `error_code` | string | For failures | Failure code |
| `error_msg` | string | For failures | Failure description |
| `seq` | long | Recommended | AGV-side monotonic sequence |
| `event_time` | string | Recommended | ISO-8601 or `yyyy-MM-dd HH:mm:ss` |

## Event Types

| Event | Meaning | RCS state effect | WCS callback |
|---|---|---|---|
| `ONLINE` | AGV is online | none | none |
| `OFFLINE` | AGV is offline | none | none |
| `BATTERY` | Battery telemetry | none | none |
| `ARRIVED_FROM` | AGV reached source point | mission `IN_PROGRESS` | `ARRIVED_FROM` |
| `PICKED` | AGV picked payload | mission `IN_PROGRESS` | `PICKED` |
| `ARRIVED_TO` | AGV reached target point | mission `IN_PROGRESS` | `ARRIVED_TO` |
| `DROPPED` | AGV dropped payload | mission `DONE` | `DROPPED` |
| `COMPLETED`/`COMPLETE`/`FINISHED` | Task completed | mission `DONE` | `DROPPED` |
| `FAILED`/`ERROR`/`EXCEPTION`/`FAULT` | Task failed | mission `FAILED` | `FAILED` |
| `ARRIVED` | Generic arrival | derived from known mission/task from/to point | derived |

`ARRIVED` is only projected when RCS can match `point_id` with a known mission or WCS task source/target.
Prefer `ARRIVED_FROM` and `ARRIVED_TO` for deterministic integration.

## Idempotency Key

RCS callback outbox uses:

1. `message_id`, when provided.
2. Otherwise: `agv_id + "|" + mission_no + "|" + event_type + "|" + event_time + "|" + seq`.

AGV implementations should provide both `message_id` and monotonic `seq`.
Once a callback idempotency key is processed successfully, repeated MQTT messages with the same key are ignored and are not sent to WCS again.

## Examples

Task completed:

```json
{
  "message_id": "AGV_01-M202606010001-0004",
  "agv_id": "AGV_01",
  "event_type": "DROPPED",
  "mission_no": "M202606010001",
  "task_no": "T202606010001",
  "point_id": "ST_OUT_01",
  "battery": 82,
  "seq": 4,
  "event_time": "2026-06-01T10:35:21Z"
}
```

Task failed:

```json
{
  "message_id": "AGV_01-M202606010001-0005",
  "agv_id": "AGV_01",
  "event_type": "FAILED",
  "mission_no": "M202606010001",
  "task_no": "T202606010001",
  "point_id": "P_WAIT_OUT_01",
  "battery": 21,
  "error_code": "LOW_BATTERY",
  "error_msg": "battery below threshold",
  "seq": 5,
  "event_time": "2026-06-01T10:36:21Z"
}
```
