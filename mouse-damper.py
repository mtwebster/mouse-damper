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

CANCEL_VALUE = 4 # relative value at or below which events are initially zeroed out
DELTA_THRESHOLD = 20 # absolute distance until tiny events are no longer halved (jitter becomes intentional movement)
THRESHOLD_TIMEOUT = 500 * USEC_IN_MSEC # time to cancel tiny events

DECAY_TIME = 100 * USEC_IN_MSEC # time after reaching threshold before resetting everything (settle time)

DOUBLE_CLICK_WAIT_TIME = 400 * USEC_IN_MSEC # GtkSettings::gtk-double-click-time default value

BUTTONS = (libevdev.EV_KEY.BTN_LEFT, libevdev.EV_KEY.BTN_RIGHT, libevdev.EV_KEY.BTN_MIDDLE)

VERBOSE = len(sys.argv) == 2 and sys.argv[1] == "verbose"

def log(txt):
    if not VERBOSE:
        return

    print(txt, flush=True)

def reduce_vector(value):
    reduction = int(abs(value) / 2) + 1

    if value < 0:
        reduction *= -1

    return reduction


class ClickEventHandler(object):
    def __init__(self, device_obj, in_device, out_device):
        self.device_obj = device_obj
        self.in_device = in_device
        self.out_device = out_device

    def handle_event(self, event):
        etime = (event.sec * USEC_IN_SEC + event.usec)

        if event.code not in BUTTONS:
            return False

        if self.device_obj.button_freeze_for_dc:
            if etime - self.device_obj.button_freeze_time > DOUBLE_CLICK_WAIT_TIME:
                self.device_obj.button_freeze_time = 0
                self.device_obj.button_freeze_for_dc = False

        if event.value == 1:
            self.device_obj.button_down = True
            self.device_obj.button_freeze_time = etime
            self.device_obj.button_freeze_for_dc = True
        elif event.value == 0:
            self.device_obj.button_down = False

        self.send_event(event)
        return True

    def send_event(self, event):
        self.out_device.send_events([event])


class MotionEventHandler(object):
    def __init__(self, device_obj, in_device, out_device, eventcode):
        self.code = eventcode
        self.device_obj = device_obj
        self.in_device = in_device
        self.out_device = out_device

        self.in_device.enable(self.code)

        self.delta_accumulator = 0
        self.start_accumulator_time = 0
        self.decay_time = 0
        self.passthru = False

    def handle_event(self, event):
        etime = (event.sec * USEC_IN_SEC + event.usec)

        if event.code != self.code:
            return False

        aval = abs(event.value)
        decay_time_not_reached = (etime - self.decay_time) < DECAY_TIME
        within_time_threshold = (etime - self.start_accumulator_time) < THRESHOLD_TIMEOUT
        within_move_threshold = abs(self.delta_accumulator) < DELTA_THRESHOLD

        new_event = event

        if self.device_obj.button_down:
            log("button pressed, cancel any passthru, force reduction: %d" % etime)

            reduction = reduce_vector(event.value)
            new_event = libevdev.InputEvent(self.code, value=reduction, sec=event.sec, usec=event.usec)
        elif self.passthru:
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

            reduction = reduce_vector(event.value)
            self.delta_accumulator += reduction

            new_event = libevdev.InputEvent(self.code, value=reduction, sec=event.sec, usec=event.usec)
        elif not within_time_threshold and within_move_threshold:
            # if we reach the timeout without hitting the cumulative threshold, skip the event, and reset
            log("accumulate time threshold reached for %s, reset" % str(self.code))
            self.start_accumulator_time = 0
            self.delta_accumulator = 0

            new_event = libevdev.InputEvent(self.code, value=reduce_vector(event.value), sec=event.sec, usec=event.usec)
        else:
            # we've reached threshold, pass events unchanged, until decay is reached.
            log("reached threshold, passthru start %s" % str(self.code))
            self.set_passthru(True, etime)

        self.send_event(new_event)
        return True

    def set_passthru(self, passthru, etime=0):
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

        # These need to be available to all 3 handlers, not nice for now
        self.button_freeze_time = 0
        self.button_freeze_for_dc = False
        self.button_down = False

        print("Mousedevice Init for %s: redirected from %s to %s" % (self.input_device.name, self.name, self.output_device.devnode))

        self.x_handler = MotionEventHandler(self, self.input_device, self.output_device, libevdev.EV_REL.REL_X)
        self.y_handler = MotionEventHandler(self, self.input_device, self.output_device, libevdev.EV_REL.REL_Y)
        self.click_handler = ClickEventHandler(self, self.input_device, self.output_device)

    def run(self):
        log("Run loop process: %s" % self.input_device.name)
        self.input_device.grab()

        try:
            while True:
                for e in self.input_device.events():
                    if any([self.click_handler.handle_event(e),
                           self.x_handler.handle_event(e),
                           self.y_handler.handle_event(e)]):
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