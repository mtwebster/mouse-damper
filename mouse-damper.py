#!/usr/bin/env python3

import sys
import libevdev

USEC_IN_MSEC = 1000
USEC_IN_SEC = USEC_IN_MSEC * 1000

CANCEL_VALUE = 2 # relative value at or below which events are initially zeroed out
DELTA_THRESHOLD = 10 # absolute distance until tiny events are no longer cancelled (jitter becomes intentional movement)
THRESHOLD_TIMEOUT = 500 * USEC_IN_MSEC # time to cancel tiny events

DECAY_TIME = 100 * USEC_IN_MSEC # time after reaching threshold before resetting everything (settle time)

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
                print("continue passthru %s: %d" % (str(self.code), etime))
                self.decay_time = etime
            else:
                print("reset passthru %s: %d" % (str(self.code), etime))
                self.set_passthru(False, 0)
        elif aval > CANCEL_VALUE:
            # if the move is large enough, pass it thru, reset everything
            print("large delta, passthru %s: %d" % (str(self.code), etime))
            self.set_passthru(True, etime)
        elif abs(self.delta_accumulator) == 0 or (within_time_threshold and within_move_threshold):
            # cancel the move, accumulate vector, record time if necessary
            print("accumulating %s (delta total: %d)" % (str(self.code), self.delta_accumulator))
            if self.start_accumulator_time == 0:
                self.start_accumulator_time = etime
            self.delta_accumulator += event.value

            new_event = libevdev.InputEvent(self.code, value=0, sec=event.sec, usec=event.usec)
        elif not within_time_threshold and within_move_threshold:
            # if we reach the timeout without hitting the cumulative threshold, skip the event, and reset
            print("accumulate time threshold reached for %s, reset" % str(self.code))
            self.start_accumulator_time = 0
            self.delta_accumulator = 0

            new_event = libevdev.InputEvent(self.code, value=0, sec=event.sec, usec=event.usec)
        else:
            # we've reached threshold, pass events unchanged, until decay is reached.
            print("reached threshold, passthru start %s" % str(self.code))
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

class Main(object):
    def __init__(self, args):
        self.path = args[1]

        self.fd = open(self.path, 'rb')
        self.in_device = libevdev.Device(self.fd)
        self.in_device.grab()
        self.out_device = self.in_device.create_uinput_device()
        print('Device is at {}'.format(self.out_device.devnode))

        self.x_handler = EventHandler(self.in_device, self.out_device, libevdev.EV_REL.REL_X)
        self.y_handler = EventHandler(self.in_device, self.out_device, libevdev.EV_REL.REL_Y)

    def start_events(self):
        while True:
            for e in self.in_device.events():
                if (not self.x_handler.handle_event(e)) and (not self.y_handler.handle_event(e)):
                    self.out_device.send_events([e])

    def stop_events(self):
        close(fd)

if __name__ == "__main__":
    Main(sys.argv).start_events()

