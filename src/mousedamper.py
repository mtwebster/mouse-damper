#!/usr/bin/env python3

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Copyright 2020 Michael Webster <miketwebster@gmail.com>

import os
import sys
import math
import libevdev
import setproctitle
import signal
import multiprocessing

from gi.repository import Gio, GLib

setproctitle.setproctitle("mousedamper")

USEC_IN_MSEC = 1000
USEC_IN_SEC = USEC_IN_MSEC * 1000

VERBOSE = sys.argv[1] == "verbose"
DOUBLE_CLICK_WAIT_TIME = int(sys.argv[2]) * USEC_IN_MSEC # GtkSettings::gtk-double-click-time
BUTTON_FREEZE_DELTA_THRESHOLD = int(sys.argv[3])

BUTTONS = (libevdev.EV_KEY.BTN_LEFT, libevdev.EV_KEY.BTN_RIGHT, libevdev.EV_KEY.BTN_MIDDLE)

def log(txt):
    if not VERBOSE:
        return

    print(txt)

class DeviceState():
    def __init__(self):
        self.reset()

    def reset(self):
        self.button_freeze_time = 0
        self.first_down = False
        self.second_down = False
        self.motion_frozen = False
        self.x_freeze_delta = 0
        self.y_freeze_delta = 0

class ClickEventHandler(object):
    def __init__(self, device_state, in_device, out_device):
        self.device_state = device_state
        self.in_device = in_device
        self.out_device = out_device

    def handle_event(self, event):
        if event.code not in BUTTONS:
            return False

        etime = (event.sec * USEC_IN_SEC + event.usec)

        if event.value == 1:
            log("Button press")
            if not self.device_state.first_down:
                log("First down")
                self.device_state.motion_frozen = True
                self.device_state.first_down = True
                self.device_state.button_freeze_time = etime
            else:
                log("Second down")
                self.device_state.second_down = True
        elif event.value == 0:
            log("Button release")
            if (etime - self.device_state.button_freeze_time) > DOUBLE_CLICK_WAIT_TIME \
                or self.device_state.second_down:
                log("Exceeded wait time or releasing second press, resetting.")
                self.device_state.reset()

        self.out_device.send_events([event])
        return True

class MotionEventHandler(object):
    def __init__(self, device_state, in_device, out_device):
        self.codes = (libevdev.EV_REL.REL_X, libevdev.EV_REL.REL_Y)
        self.device_state = device_state
        self.in_device = in_device
        self.out_device = out_device

        for code in self.codes:
            self.in_device.enable(code)

    def handle_event(self, event):
        if event.code not in self.codes:
            return False

        etime = (event.sec * USEC_IN_SEC + event.usec)

        if self.device_state.motion_frozen:
            if event.code == libevdev.EV_REL.REL_X:
                self.device_state.x_freeze_delta += event.value
            else:
                self.device_state.y_freeze_delta += event.value
            log("Deltas: %d, %d" % (self.device_state.x_freeze_delta, self.device_state.y_freeze_delta))

            elapsed = etime - self.device_state.button_freeze_time
            real_move = math.hypot(self.device_state.x_freeze_delta, self.device_state.y_freeze_delta)
            within_time = elapsed < DOUBLE_CLICK_WAIT_TIME

            if real_move > BUTTON_FREEZE_DELTA_THRESHOLD or not within_time:
                log("Thresholds reached, resetting (%dpx > %dpx, %dms > %dms)"
                        % (
                              real_move, BUTTON_FREEZE_DELTA_THRESHOLD,
                              elapsed / USEC_IN_MSEC, DOUBLE_CLICK_WAIT_TIME / USEC_IN_MSEC)
                          )
                self.device_state.reset()
            else:
                log("Skipping event, thresholds not reached (%dpx < %dpx, %dms < %dms)"
                        % (
                              real_move, BUTTON_FREEZE_DELTA_THRESHOLD,
                              elapsed / USEC_IN_MSEC, DOUBLE_CLICK_WAIT_TIME / USEC_IN_MSEC)
                          )
                return True

        self.out_device.send_events([event])
        return True

class MouseDevice(multiprocessing.Process):
    def __init__(self, group=None, target=None, name=None,
                 args=(), kwargs=None):
        super(MouseDevice, self).__init__(group=group, target=target, name=name)
        input_path = self.name

        fd = open(input_path, 'rb')
        self.input_device = libevdev.Device(fd)
        self.output_device = self.input_device.create_uinput_device()

        print("Mousedevice Init for %s: redirected from %s to %s" % (self.input_device.name, self.name, self.output_device.devnode))

        self.state = DeviceState()

        self.motion_handler = MotionEventHandler(self.state, self.input_device, self.output_device)
        self.click_handler = ClickEventHandler(self.state, self.input_device, self.output_device)

    def run(self):
        log("Run loop process: %s" % self.input_device.name)
        self.input_device.grab()

        try:
            while True:
                for e in self.input_device.events():
                    if any([self.click_handler.handle_event(e),
                           self.motion_handler.handle_event(e)]):
                        continue

                    self.output_device.send_events([e])
        finally:
            log("Thread interrupted for %s" % self.input_device.name)


class Main(object):
    def __init__(self):
        signal.signal(signal.SIGINT, self.stop)

        fd_code = 0
        self.mouse_devices = []

        mouse_ev_paths = []

        while True:
            devpath = "/dev/input/event%d" % fd_code
            try:
                fd = open(devpath, 'rb')
            except FileNotFoundError:
                break

            dev = libevdev.Device(fd)
            try:
                if dev.evbits[libevdev.EV_KEY] and libevdev.EV_KEY.BTN_LEFT in dev.evbits[libevdev.EV_KEY]:
                    log("Device at %s is a mouse" % devpath)
                    mouse_ev_paths.append(devpath)
            except KeyError:
                log("Device at %s is NOT a mouse" % devpath)

            fd.close()
            fd_code += 1

        log("Starting filters")

        for path in mouse_ev_paths:
            log("creating MouseDevice for %s" % path)
            d = MouseDevice(name=path)
            self.mouse_devices.append(d)

    def start(self):
        for device in self.mouse_devices:
            device.start()

    def stop(self, sig, frame):
        for device in self.mouse_devices:
            device.terminate()
            device.join()

        exit(0)

if __name__ == "__main__":
    main = Main()

    try:
        main.start()
    except Exception as e:
        print(e)
        exit(0)