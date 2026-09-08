#!/usr/bin/env python3
import asyncio
import os

from amqtt.broker import Broker
from amqtt.plugins.authentication import AnonymousAuthPlugin as _AnonymousAuthPlugin
from amqtt.plugins.sys.broker import BrokerSysPlugin as _BrokerSysPlugin


HOST = os.environ.get("AGV_LOCAL_MQTT_HOST", "127.0.0.1")
PORT = int(os.environ.get("AGV_LOCAL_MQTT_PORT", "1883"))
_AMQTT_PLUGIN_IMPORTS = (_AnonymousAuthPlugin, _BrokerSysPlugin)


async def main() -> None:
    broker = Broker({
        "listeners": {
            "default": {
                "type": "tcp",
                "bind": f"{HOST}:{PORT}",
            },
        },
        "plugins": {
            "amqtt.plugins.authentication.AnonymousAuthPlugin": {
                "allow_anonymous": True,
            },
            "amqtt.plugins.sys.broker.BrokerSysPlugin": {
                "sys_interval": 10,
            },
        },
    })
    await broker.start()
    print(f"Local MQTT broker listening on {HOST}:{PORT}", flush=True)
    await asyncio.Event().wait()


if __name__ == "__main__":
    asyncio.run(main())
