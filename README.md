# mouse-damper

A utility that helps prevent accidental clicks and drag operations caused by hand tremors or unsteady mouse movements.

## The Problem

For people with hand tremors or conditions like essential tremor, using a mouse can be frustrating. Small, involuntary movements between pressing and releasing a mouse button often lead to:

- **Missed clicks** - The pointer drifts slightly, causing you to click the wrong element
- **Accidental drags** - Movement during a click is interpreted as a drag operation
- **Failed double-clicks** - The pointer moves between the first and second click, missing the target

While toolkit libraries (GTK, Qt) have some built-in tolerance for small movements, they're limited in effectiveness and don't prevent mis-targeting on button-release events.

## How It Works

Mousedamper solves this by freezing the pointer in place when you press a mouse button. The freeze continues until one of three conditions is met:

1. **Double-click completion** - You complete a double-click (press, release, press within the timeout period)
2. **Timeout expires** - The double-click timeout period passes
3. **Movement threshold exceeded** - You move the physical mouse beyond a configurable distance

This ensures that the pointer stays exactly where you clicked, preventing drift from affecting your actions. When you intentionally move the mouse beyond the threshold, normal movement resumes immediately.

## Technical Details

Mousedamper uses libevdev to intercept mouse input events at the `/dev/input` level. It automatically discovers and filters all mouse-type devices, applying the pointer freeze logic before events reach the desktop environment. The program requires root privileges to access input devices and is installed as a setuid binary.

Configuration is managed through GSettings and includes:
- Enable/disable on session start
- Breakout threshold (how far you must move to unfreeze)
- Optional double-click timeout override