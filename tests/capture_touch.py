#!/usr/bin/env python3
"""Capture SDL pad coordinates for offline replay; never injects mouse input.

Uses the bundled SDL DLL. Close Steam Input's controller configuration before
capturing. --list only enumerates controllers. A capture runs for --seconds and
writes buffered data after sampling, so disk I/O does not affect sample cadence.
"""
import argparse
import csv
import ctypes as c
from pathlib import Path
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--seconds', type=float, default=20)
    parser.add_argument('--list', action='store_true')
    args = parser.parse_args()
    if not args.list and args.output is None:
        parser.error('--output is required for a capture')
    sdl = c.CDLL(str(args.dll.resolve()))
    def bind(name, result, *params):
        fn = getattr(sdl, name)
        fn.restype, fn.argtypes = result, params
        return fn
    hint = bind('SDL_SetHint', c.c_bool, c.c_char_p, c.c_char_p)
    init = bind('SDL_Init', c.c_bool, c.c_uint32)
    quit_sdl = bind('SDL_Quit', None)
    ids = bind('SDL_GetGamepads', c.POINTER(c.c_uint32), c.POINTER(c.c_int))
    free = bind('SDL_free', None, c.c_void_p)
    name = bind('SDL_GetGamepadNameForID', c.c_char_p, c.c_uint32)
    open_pad = bind('SDL_OpenGamepad', c.c_void_p, c.c_uint32)
    close_pad = bind('SDL_CloseGamepad', None, c.c_void_p)
    pads = bind('SDL_GetNumGamepadTouchpads', c.c_int, c.c_void_p)
    update = bind('SDL_UpdateGamepads', None)
    pump = bind('SDL_PumpEvents', None)
    get = bind('SDL_GetGamepadTouchpadFinger', c.c_bool, c.c_void_p,
               c.c_int, c.c_int, c.POINTER(c.c_bool), c.POINTER(c.c_float),
               c.POINTER(c.c_float), c.POINTER(c.c_float))
    error = bind('SDL_GetError', c.c_char_p)
    hint(b'SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS', b'1')
    hint(b'SDL_JOYSTICK_HIDAPI', b'1')
    hint(b'SDL_JOYSTICK_THREAD', b'1')
    if not init(0x2000):  # SDL_INIT_GAMEPAD
        raise RuntimeError(error().decode())
    controller = None
    try:
        count = c.c_int()
        found = ids(c.byref(count))
        controller_ids = list(found[:count.value])
        free(found)
        for device_id in controller_ids:
            print(device_id, name(device_id).decode(), flush=True)
        if args.list:
            return 0
        for device_id in controller_ids:
            candidate = open_pad(device_id)
            if candidate and pads(candidate) >= 2:
                controller = candidate
                break
            if candidate:
                close_pad(candidate)
        if not controller:
            raise RuntimeError('No two-pad controller visible to SDL (check HidHide access).')
        down, x, y, pressure = c.c_bool(), c.c_float(), c.c_float(), c.c_float()
        rows = []
        print('Recording right pad now for', args.seconds, 'seconds.', flush=True)
        start = previous = deadline = time.perf_counter()
        while (now := time.perf_counter()) - start < args.seconds:
            if now < deadline:
                time.sleep(max(0, deadline-now))
            pump()
            update()
            if not get(controller, 1, 0, c.byref(down), c.byref(x), c.byref(y), c.byref(pressure)):
                raise RuntimeError(error().decode())
            now = time.perf_counter()
            rows.append((now-start, now-previous, int(down.value), x.value, y.value, pressure.value))
            previous = now
            deadline = max(deadline+.001, now)
        with args.output.open('w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow(('time', 'dt', 'down', 'x', 'y', 'pressure'))
            writer.writerows(rows)
        print('Saved', len(rows), 'polls to', args.output, flush=True)
    finally:
        if controller:
            close_pad(controller)
        quit_sdl()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
