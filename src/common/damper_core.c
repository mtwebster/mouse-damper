/* This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2020 Michael Webster <miketwebster@gmail.com>
 */

#include "damper_core.h"
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

#define USEC_IN_MSEC 1000

int64_t damper_double_click_wait_time = 0;
int damper_button_freeze_delta_threshold = 0;
double damper_threshold_scale_factor = 1.0;
bool damper_verbose = false;

static void
log_message (const char *format, ...)
{
    if (!damper_verbose)
        return;

    va_list args;
    va_start (args, format);
    vprintf (format, args);
    printf ("\n");
    va_end (args);
}

void
damper_set_threshold_scale (double scale)
{
    damper_threshold_scale_factor = scale;
}

void
damper_state_init (DamperState *state)
{
    state->button_freeze_time = 0;
    state->first_down = false;
    state->second_down = false;
    state->motion_frozen = false;
    state->x_freeze_delta = 0;
    state->y_freeze_delta = 0;
}

void
damper_state_reset (DamperState *state)
{
    state->button_freeze_time = 0;
    state->first_down = false;
    state->second_down = false;
    state->motion_frozen = false;
    state->x_freeze_delta = 0;
    state->y_freeze_delta = 0;
}

static PlatformAction
handle_button_event (DamperState *state, const PlatformEvent *event)
{
    if (event->type == PLATFORM_EVENT_BUTTON_PRESS) {
        log_message ("Button press");
        if (!state->first_down) {
            log_message ("First down");
            state->motion_frozen = true;
            state->first_down = true;
            state->button_freeze_time = event->timestamp_usec;
        } else {
            log_message ("Second down");
            state->second_down = true;
        }
    } else if (event->type == PLATFORM_EVENT_BUTTON_RELEASE) {
        log_message ("Button release");
        if ((event->timestamp_usec - state->button_freeze_time) > damper_double_click_wait_time ||
            state->second_down) {
            log_message ("Exceeded wait time or releasing second press, resetting.");
            damper_state_reset (state);
        }
    }

    return PLATFORM_ACTION_PASS;
}

static PlatformAction
handle_motion_event (DamperState *state, const PlatformEvent *event)
{
    if (state->motion_frozen) {
        state->x_freeze_delta += event->data.motion.dx;
        state->y_freeze_delta += event->data.motion.dy;

        log_message ("Deltas: %d, %d", state->x_freeze_delta, state->y_freeze_delta);

        int64_t elapsed = event->timestamp_usec - state->button_freeze_time;
        double real_move = hypot (state->x_freeze_delta, state->y_freeze_delta);
        double scaled_threshold = damper_button_freeze_delta_threshold * damper_threshold_scale_factor;
        bool within_time = elapsed < damper_double_click_wait_time;

        if (real_move > scaled_threshold || !within_time) {
            log_message ("Thresholds reached, resetting (%dpx > %dpx [scaled from %d], %ldms > %ldms)",
                        (int)real_move, (int)scaled_threshold, damper_button_freeze_delta_threshold,
                        (long)(elapsed / USEC_IN_MSEC), (long)(damper_double_click_wait_time / USEC_IN_MSEC));
            damper_state_reset (state);
        } else {
            log_message ("Skipping event, thresholds not reached (%dpx < %dpx [scaled from %d], %ldms < %ldms)",
                        (int)real_move, (int)scaled_threshold, damper_button_freeze_delta_threshold,
                        (long)(elapsed / USEC_IN_MSEC), (long)(damper_double_click_wait_time / USEC_IN_MSEC));
            return PLATFORM_ACTION_DROP;
        }
    }

    return PLATFORM_ACTION_PASS;
}

PlatformAction
damper_handle_event (DamperState *state, const PlatformEvent *event)
{
    if (event->type == PLATFORM_EVENT_BUTTON_PRESS || event->type == PLATFORM_EVENT_BUTTON_RELEASE) {
        return handle_button_event (state, event);
    } else if (event->type == PLATFORM_EVENT_MOTION) {
        return handle_motion_event (state, event);
    }

    return PLATFORM_ACTION_PASS;
}
