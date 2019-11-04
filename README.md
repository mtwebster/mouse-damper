# mouse-damper
an attempt to use libevdev to dampen small pointer movements to help people with hand tremors


For people with a hand tremor ('essential tremor' in my case) it can be difficult to be precise with a mouse
particularly when trying to target and click or double-click elements on the screen - often an unintentional drag
can be started, or the target missed on a double-click (where the initial click may have bumped the position enough
to miss the mark) - whatever the case, this is an attempt to cancel out some of these small, unintentional movements,
while still allowing full movement/response when needed.

The premise is:

- Any tiny movement (a delta of 2 or less) is ignored, unless the accumulated delta exceeds a threshold within a certain
time.

- If the time limit is reached without reaching the threshold, the time mark and delta accumulator are reset.

- If the threshold is reached within the time limit, filtering is removed, and events are passed thru unmodified.
  At this point a decay timer kicks in - as long as movement continues with an event frequency inside the decay limit,
  passthru continues.

- Once the time between events exceeds the decay limit, the whole thing is started over again.

This is tracked separately for both x and y

for now, to run:

```
cat /proc/bus/input/devices  # look for your mouse, look at the Handlers line, and the event code (event11 for instance)

# as root or with sudo 

sudo ./mouse-damper.py /dev/input/event11 
```

This is really really rough for now, I hope to clean it up, optimize (maybe re-write in C) once I can get the
behavior worked out.