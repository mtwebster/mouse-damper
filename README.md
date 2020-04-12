# mouse-damper
This is an attempt to use libevdev to dampen certain pointer events to help
people with hand tremors. I'm no expert, just someone who has to deal with this.

For people with a hand tremor it can be difficult to be precise with a mouse,
particularly when trying to target and click or double-click elements on the
screen - often an unintentional drag can be started, or the target missed on a
double-click (where the initial click may have bumped the position enough to
miss the intended target) - whatever the case, this is an attempt to cancel
out some of these small, unintentional movements, while trying not to get in the
way of normal movement and response when needed.

This is really really quite rough at the moment, no warranty. :)

### How it works
The program will crawl `/dev/input` and gather up any mouse-type devices, and
begin filtering them automatically. For now you can ctrl-c or kill the main
process (it uses python multiprocessing for individual devices).

The toolkit libraries that graphical applications use (like GTK, QT) have some
preventatives for unintentional mouse movement. They are of limited effectiveness,
however, and do nothing to prevent mis-targeting on a button-release.

The way these issues are addressed by this program is the pointer is frozen in
place when a button is pressed. At this point, two things begin to be tracked -
elapsed time and physical mouse movement. The freeze is removed when one of the
following three events takes place:

- The user proceeds to complete a double-click (a click, release, and second 
  click within a certain duration).
- The double-click timeout is exceeded.
- The physical mouse movement from the freeze point exceeds a configurable
  threshold. While this is by no means a perfect solution, it at least allows
  the user to act on their intended target, without being affected by
  uncontrollable physical movement.