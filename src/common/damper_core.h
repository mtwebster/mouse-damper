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

#ifndef DAMPER_CORE_H
#define DAMPER_CORE_H

#include "platform.h"

typedef struct {
    int64_t button_freeze_time;
    bool first_down;
    bool second_down;
    bool motion_frozen;
    int x_freeze_delta;
    int y_freeze_delta;
} DamperState;

extern int64_t damper_double_click_wait_time;
extern int damper_button_freeze_delta_threshold;
extern double damper_threshold_scale_factor;
extern bool damper_verbose;

void damper_state_init(DamperState *state);
void damper_state_reset(DamperState *state);
void damper_set_threshold_scale(double scale);
PlatformAction damper_handle_event(DamperState *state, const PlatformEvent *event);

#endif
