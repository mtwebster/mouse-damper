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

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PLATFORM_EVENT_BUTTON_PRESS,
    PLATFORM_EVENT_BUTTON_RELEASE,
    PLATFORM_EVENT_MOTION
} PlatformEventType;

typedef enum {
    PLATFORM_BUTTON_LEFT = 0,
    PLATFORM_BUTTON_RIGHT = 1,
    PLATFORM_BUTTON_MIDDLE = 2
} PlatformButton;

typedef struct {
    PlatformEventType type;
    int64_t timestamp_usec;
    union {
        struct {
            PlatformButton button;
        } button;
        struct {
            int dx;
            int dy;
        } motion;
    } data;
} PlatformEvent;

typedef enum {
    PLATFORM_ACTION_DROP,
    PLATFORM_ACTION_PASS
} PlatformAction;

typedef struct {
    bool (*init)(int64_t double_click_time_usec, int threshold_px, bool verbose);
    void (*run)(void);
    void (*cleanup)(void);
} PlatformInterface;

extern const PlatformInterface *platform_get_interface(void);

#endif
