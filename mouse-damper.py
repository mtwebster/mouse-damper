#!/usr/bin/env python3

import sys
import os
import libevdev
import fcntl
import setproctitle
import ctypes
import signal
import multiprocessing
import time

setproctitle.setproctitle("mouse-damper")

USEC_IN_MSEC = 1000
USEC_IN_SEC = USEC_IN_MSEC * 1000

CANCEL_VALUE = 2 # relative value at or below which events are initially zeroed out
DELTA_THRESHOLD = 10 # absolute distance until tiny events are no longer cancelled (jitter becomes intentional movement)
THRESHOLD_TIMEOUT = 500 * USEC_IN_MSEC # time to cancel tiny events

DECAY_TIME = 100 * USEC_IN_MSEC # time after reaching threshold before resetting everything (settle time)

BUTTONS = (libevdev.EV_KEY.BTN_LEFT, libevdev.EV_KEY.BTN_RIGHT, libevdev.EV_KEY.BTN_MIDDLE)

VERBOSE = len(sys.argv) == 2 and sys.argv[1] == "verbose"

def log(txt):
    if not VERBOSE:
        return

    print(txt, flush=True)

class EventHandler(object):
    def __init__(self, in_device, out_device, eventcode):
        self.code = eventcode
        self.in_device = in_device
        self.out_device = out_device

        self.in_device.enable(self.code)

        self.delta_accumulator = 0
        self.start_accumulator_time = 0
        self.decay_time = 0
        self.passthru = False

    def handle_event(self, event):
        # if event.code in BUTTONS:
            # handle dampen for duration of potential double-click

        if event.code != self.code:
            return False

        etime = (event.sec * USEC_IN_SEC + event.usec)
        aval = abs(event.value)

        decay_time_not_reached = (etime - self.decay_time) < DECAY_TIME
        within_time_threshold = (etime - self.start_accumulator_time) < THRESHOLD_TIMEOUT
        within_move_threshold = abs(self.delta_accumulator) < DELTA_THRESHOLD

        new_event = event

        if self.passthru:
            if decay_time_not_reached:
                log("continue passthru %s: %d" % (str(self.code), etime))
                self.decay_time = etime
            else:
                log("reset passthru %s: %d" % (str(self.code), etime))
                self.set_passthru(False, 0)
        elif aval > CANCEL_VALUE:
            # if the move is large enough, pass it thru, reset everything
            log("large delta, passthru %s: %d" % (str(self.code), etime))
            self.set_passthru(True, etime)
        elif abs(self.delta_accumulator) == 0 or (within_time_threshold and within_move_threshold):
            # cancel the move, accumulate vector, record time if necessary
            log("accumulating %s (delta total: %d)" % (str(self.code), self.delta_accumulator))
            if self.start_accumulator_time == 0:
                self.start_accumulator_time = etime
            self.delta_accumulator += event.value

            new_event = libevdev.InputEvent(self.code, value=0, sec=event.sec, usec=event.usec)
        elif not within_time_threshold and within_move_threshold:
            # if we reach the timeout without hitting the cumulative threshold, skip the event, and reset
            log("accumulate time threshold reached for %s, reset" % str(self.code))
            self.start_accumulator_time = 0
            self.delta_accumulator = 0

            new_event = libevdev.InputEvent(self.code, value=0, sec=event.sec, usec=event.usec)
        else:
            # we've reached threshold, pass events unchanged, until decay is reached.
            log("reached threshold, passthru start %s" % str(self.code))
            self.set_passthru(True, etime)

        self.send_event(new_event)
        return True

    def set_passthru(self, passthru, etime):
        self.passthru = passthru

        self.start_accumulator_time = 0
        self.delta_accumulator = 0

        if passthru:
            self.decay_time = etime
        else:
            self.decay_time = 0

    def send_event(self, event):
        self.out_device.send_events([event])

class MouseDevice(multiprocessing.Process):
    def __init__(self, group=None, target=None, name=None,
                 args=(), kwargs=None):
        super(MouseDevice, self).__init__(group=group, target=target, name=name)
        input_path = self.name

        fd = open(input_path, 'rb')
        self.input_device = libevdev.Device(fd)
        self.output_device = self.input_device.create_uinput_device()

        print("Mousedevice Init for %s: redirected from %s to %s" % (self.input_device.name, self.name, self.output_device.devnode))

        self.x_handler = EventHandler(self.input_device, self.output_device, libevdev.EV_REL.REL_X)
        self.y_handler = EventHandler(self.input_device, self.output_device, libevdev.EV_REL.REL_Y)

    def run(self):
        log("Run loop process: %s" % self.input_device.name)
        self.input_device.grab()

        try:
            while True:
                for e in self.input_device.events():
                    if self.x_handler.handle_event(e) or self.y_handler.handle_event(e):
                        continue

                    self.output_device.send_events([e])
        finally:
            log("Thread interrupted for %s" % self.input_device.name)

    def close(self):
        print("Closing %s (redirected to %s)" % (self.input_device.name, self.name))
        self.terminate()
        self.join()

        self.input_device.fd.close()


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
            device.close()

        exit(0)

if __name__ == "__main__":
    main = Main()

    try:
        main.start()
    except Exception as e:
        print(e)
        exit(0)