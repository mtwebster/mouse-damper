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

#include "../../common/platform.h"
#include "../../common/damper_core.h"
#include <windows.h>
#include <stdio.h>

#define USEC_IN_MSEC 1000

static HHOOK mouse_hook = NULL;
static DamperState damper_state;
static POINT last_pos = {0, 0};
static bool has_last_pos = false;
static volatile bool running = true;

static int64_t
get_timestamp_usec (void)
{
    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime (&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    return (int64_t)(uli.QuadPart / 10);
}

static PlatformButton
translate_button (DWORD msg)
{
    switch (msg) {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            return PLATFORM_BUTTON_LEFT;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return PLATFORM_BUTTON_RIGHT;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return PLATFORM_BUTTON_MIDDLE;
        default:
            return PLATFORM_BUTTON_LEFT;
    }
}

static LRESULT CALLBACK
low_level_mouse_proc (int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0) {
        return CallNextHookEx (mouse_hook, nCode, wParam, lParam);
    }

    MSLLHOOKSTRUCT *mouse_data = (MSLLHOOKSTRUCT *)lParam;
    PlatformEvent event;
    PlatformAction action = PLATFORM_ACTION_PASS;
    bool handled = false;

    event.timestamp_usec = get_timestamp_usec ();

    switch (wParam) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            event.type = PLATFORM_EVENT_BUTTON_PRESS;
            event.data.button.button = translate_button (wParam);
            action = damper_handle_event (&damper_state, &event);
            handled = true;
            break;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            event.type = PLATFORM_EVENT_BUTTON_RELEASE;
            event.data.button.button = translate_button (wParam);
            action = damper_handle_event (&damper_state, &event);
            handled = true;
            break;

        case WM_MOUSEMOVE:
            if (has_last_pos) {
                event.type = PLATFORM_EVENT_MOTION;
                event.data.motion.dx = mouse_data->pt.x - last_pos.x;
                event.data.motion.dy = mouse_data->pt.y - last_pos.y;

                if (event.data.motion.dx != 0 || event.data.motion.dy != 0) {
                    action = damper_handle_event (&damper_state, &event);
                    handled = true;
                }
            }
            last_pos = mouse_data->pt;
            has_last_pos = true;
            break;
    }

    if (handled && action == PLATFORM_ACTION_DROP) {
        return 1;
    }

    return CallNextHookEx (mouse_hook, nCode, wParam, lParam);
}

static BOOL WINAPI
console_ctrl_handler (DWORD ctrl_type)
{
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
        printf ("Received signal, shutting down...\n");
        running = false;
        return TRUE;
    }
    return FALSE;
}

static bool
platform_windows_init (int64_t double_click_time_usec, int threshold_px, bool verbose)
{
    damper_double_click_wait_time = double_click_time_usec;
    damper_button_freeze_delta_threshold = threshold_px;
    damper_verbose = verbose;

    damper_state_init (&damper_state);

    mouse_hook = SetWindowsHookEx (WH_MOUSE_LL,
                                    low_level_mouse_proc,
                                    GetModuleHandle (NULL),
                                    0);

    if (mouse_hook == NULL) {
        fprintf (stderr, "Failed to install mouse hook: error %lu\n", GetLastError ());
        return false;
    }

    if (!SetConsoleCtrlHandler (console_ctrl_handler, TRUE)) {
        fprintf (stderr, "Warning: Failed to set console ctrl handler\n");
    }

    printf ("Mouse hook installed successfully\n");
    return true;
}

static void
platform_windows_run (void)
{
    MSG msg;

    while (running) {
        while (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            TranslateMessage (&msg);
            DispatchMessage (&msg);
        }

        if (running) {
            Sleep (10);
        }
    }
}

static void
platform_windows_cleanup (void)
{
    if (mouse_hook != NULL) {
        UnhookWindowsHookEx (mouse_hook);
        mouse_hook = NULL;
    }
}

const PlatformInterface *
platform_get_interface (void)
{
    static const PlatformInterface windows_platform = {
        .init = platform_windows_init,
        .run = platform_windows_run,
        .cleanup = platform_windows_cleanup
    };
    return &windows_platform;
}
