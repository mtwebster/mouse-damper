#!/usr/bin/env python3

import sys
import libevdev

CANCEL_VALUE = 2 # relative value at or below which events are initially zeroed out
THRESHOLD = 20 # absolute distance until tiny events are no longer cancelled (jitter becomes intentional movement)
TIMEOUT = 500 # ms  - time to cancel tiny events
DECAY_TIME = 100 # ms  - time after reaching threshold before resetting everything (settle time)

def to_ms(event):
    sec_to_ms = event.sec * 1000
    usec_to_ms = int(event.usec / 1000)

    return sec_to_ms + usec_to_ms

def main(args):
    path = args[1]

    fd = open(path, 'rb')
    d = libevdev.Device(fd)
    d.grab()

    # create a duplicate of our input device
    d.enable(libevdev.EV_REL.REL_X)  # make sure the code we map to is available
    d.enable(libevdev.EV_REL.REL_Y)  # make sure the code we map to is available
    uidev = d.create_uinput_device()
    print('Device is at {}'.format(uidev.devnode))

    x_accumulator = 0
    y_accumulator = 0
    x_start_ms = 0
    y_start_ms = 0
    x_decay_ms = 0
    y_decay_ms = 0
    x_passthru = False
    y_passthru = False


    while True:
        for e in d.events():
            if e.code in (libevdev.EV_REL.REL_X, libevdev.EV_REL.REL_Y):
                oldval = e.value

                aval = abs(e.value)
                val = e.value

                if e.code == libevdev.EV_REL.REL_X:
                    decay_time_not_reached = (to_ms(e) - x_decay_ms) < DECAY_TIME

                    within_time_threshold = (to_ms(e) - x_start_ms) < TIMEOUT
                    within_move_threshold = abs(x_accumulator) < THRESHOLD

                    if x_passthru:
                        if decay_time_not_reached:
                            print("continue passthru x: %d" % to_ms(e))
                            x_decay_ms = to_ms(e)
                        else:
                            print("reset passthru x: %d" % to_ms(e))

                            x_passthru = False

                    elif aval > CANCEL_VALUE:
                        # if the move is large enough, pass it thru, reset everything
                        x_start_ms = 0
                        x_accumulator = 0
                        x_passthru = True
                        x_decay_ms = to_ms(e)

                        print("large delta, passthru x: %d" % to_ms(e))
                    elif abs(x_accumulator) == 0 or (within_time_threshold and within_move_threshold):
                        # cancel the move, accumulate vector, record time if necessary
                        print("accumulating x (%d)" % x_accumulator)

                        if x_start_ms == 0:
                            x_start_ms = to_ms(e)
                        x_accumulator += val
                        e = libevdev.InputEvent(libevdev.EV_REL.REL_X, value=0, sec=e.sec, usec=e.usec)
                    # if we reach the timeout without hitting the cumulative threshold, skip the event, and reset
                    elif not within_time_threshold and within_move_threshold:
                        print("accumulate time threshold reached, reset x: %d" % to_ms(e))
                        x_start_ms = 0
                        x_accumulator = 0
                        e = libevdev.InputEvent(libevdev.EV_REL.REL_X, value=0, sec=e.sec, usec=e.usec)
                        uidev.send_events([e])
                    else:
                        print("reached threshold, passthru start x: %d" % to_ms(e))
                        # we've reached threshold, pass events unchanged, until decay is reached.
                        x_passthru = True
                        x_decay_ms = to_ms(e)
                        x_start_ms = 0
                        x_accumulator = 0

                if e.code == libevdev.EV_REL.REL_Y:
                    decay_time_not_reached = (to_ms(e) - y_decay_ms) < DECAY_TIME

                    within_time_threshold = (to_ms(e) - y_start_ms) < TIMEOUT
                    within_move_threshold = abs(y_accumulator) < THRESHOLD

                    if y_passthru:
                        if decay_time_not_reached:
                            print("continue passthru y: %d" % to_ms(e))
                            y_decay_ms = to_ms(e)
                        else:
                            print("reset passthru y: %d" % to_ms(e))

                            y_passthru = False

                    elif aval > CANCEL_VALUE:
                        # if the move is large enough, pass it thru, reset everything
                        y_start_ms = 0
                        y_accumulator = 0
                        y_passthru = True
                        y_decay_ms = to_ms(e)

                        print("large delta, passthru y: %d" % to_ms(e))
                    elif abs(y_accumulator) == 0 or (within_time_threshold and within_move_threshold):
                        # cancel the move, accumulate vector, record time if necessary
                        print("accumulating y (%d)" % y_accumulator)

                        if y_start_ms == 0:
                            y_start_ms = to_ms(e)
                        y_accumulator += val
                        e = libevdev.InputEvent(libevdev.EV_REL.REL_Y, value=0, sec=e.sec, usec=e.usec)
                    # if we reach the timeout without hitting the cumulative threshold, skip the event, and reset
                    elif not within_time_threshold and within_move_threshold:
                        print("accumulate time threshold reached, reset y: %d" % to_ms(e))
                        y_start_ms = 0
                        y_accumulator = 0
                        e = libevdev.InputEvent(libevdev.EV_REL.REL_Y, value=0, sec=e.sec, usec=e.usec)
                        uidev.send_events([e])
                    else:
                        print("reached threshold, passthru start y: %d" % to_ms(e))
                        # we've reached threshold, pass events unchanged, until decay is reached.
                        y_passthru = True
                        y_decay_ms = to_ms(e)
                        y_start_ms = 0
                        y_accumulator = 0
            uidev.send_events([e])

if __name__ == "__main__":
    main(sys.argv)