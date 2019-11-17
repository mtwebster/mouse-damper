# mouse-damper
an attempt to use libevdev to dampen certain pointer events to help people with hand tremors. I'm no expert, just someone who has to deal with this, and all my observation and reasoning here is from my own experience.


For people with a hand tremor ('essential tremor' in my case) it can be difficult to be precise with a mouse
particularly when trying to target and click or double-click elements on the screen - often an unintentional drag
can be started, or the target missed on a double-click (where the initial click may have bumped the position enough
to miss the mark) - whatever the case, this is an attempt to cancel out some of these small, unintentional movements,
while still allowing full movement/response when needed.

The premise is:

- Any tiny movement (a delta of 2 or less) is halved, unless the accumulated delta exceeds a threshold within a certain
time.

- If the time limit is reached without reaching the threshold, the time mark and delta accumulator are reset.

- If the threshold is reached within the time limit, filtering is removed, and events are passed thru unmodified.
  At this point a decay timer kicks in - as long as movement continues with an event frequency inside the decay limit,
  passthru continues.

- Once the time between events exceeds the decay limit, the whole thing is started over again.

- When a button is pressed, all motion is temporarily frozen until either a) the duration from the press meets the user's gtk-double-click-time, or b) enough x or y motion is accumulated within that time limit to assume the motion is deliberate.  This is to prevent accidental dnd when attempting to double-click on a ui element.

- When a drag is initiated (movement while button pressed), motion is slightly reduced to increase accuracy.

to compile:
```
./build
```
to run:
```
mouse-damper-launch [verbose]
```
************ at his time, the setuid bit is used to allow the binary to be launched as root **************

Verbose will spew a great deal of event info

The program will crawl `/dev/input` and gather up any mouse-type devices, and begin filtering them automatically.  For now you can ctrl-c or kill the main process (it uses python multiprocessing for individual devices).

This is really really rough for now, I hope to clean it up, optimize (maybe re-write in C) once I can get the
behavior worked out.
