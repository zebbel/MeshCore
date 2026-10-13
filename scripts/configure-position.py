#!/usr/bin/env python3
"""Configure the T1000-E position button over BLE; never prints channel keys."""
import argparse
import asyncio
import getpass


def arguments():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--address', required=True, help='T1000-E Bluetooth address')
    p.add_argument('--ask-pin', action='store_true', help='Prompt for the BLE pairing PIN')
    p.add_argument('--list', action='store_true', help='List stored channel names')
    p.add_argument('--channel', help='Exact existing private channel name')
    switch = p.add_mutually_exclusive_group()
    switch.add_argument('--enable', action='store_true')
    switch.add_argument('--disable', action='store_true')
    return p.parse_args()


async def configure(args):
    from meshcore import MeshCore, EventType
    pin = getpass.getpass('BLE PIN: ') if args.ask_pin else None
    mc = await MeshCore.create_ble(args.address, pin=pin, only_error=True,
                                   default_timeout=15, auto_reconnect=False)
    if mc is None:
        raise RuntimeError('Could not connect. Disconnect the phone app and try again.')

    async def checked(awaitable, expected, action):
        event = await awaitable
        if event is None or event.type != expected:
            raise RuntimeError(action + ' failed. Check the firmware, channel name and connection.')
        return event.payload

    try:
        values = await checked(mc.commands.get_custom_vars(), EventType.CUSTOM_VARS, 'Reading settings')
        if 'position_button' not in values:
            raise RuntimeError('This device does not expose the custom position-button settings.')
        if args.list:
            for slot in range(40):
                ch = await checked(mc.commands.get_channel(slot), EventType.CHANNEL_INFO, 'Reading channels')
                name = ch.get('channel_name', '')
                if name:
                    print(f'{slot}: {name}')
        if args.disable:
            await checked(mc.commands.set_custom_var('position_button', '0'), EventType.OK, 'Disabling button')
        if args.channel is not None:
            await checked(mc.commands.set_custom_var('position_channel', args.channel), EventType.OK, 'Selecting channel')
        if args.enable:
            await checked(mc.commands.set_custom_var('position_button', '1'), EventType.OK, 'Enabling button')
        values = await checked(mc.commands.get_custom_vars(), EventType.CUSTOM_VARS, 'Reading settings')
        for name in ('position_channel', 'position_button', 'position_status'):
            print(f'{name}: {values.get(name, "unavailable")}')
    finally:
        await mc.disconnect()


if __name__ == '__main__':
    try:
        asyncio.run(configure(arguments()))
    except (RuntimeError, ImportError) as exc:
        raise SystemExit(str(exc))
