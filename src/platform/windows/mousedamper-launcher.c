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

#include "config.h"
#include "settings-backend.h"
#include "mousedamper-launcher_resource.h"
#include "gettext-helpers.h"
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

#define MAX_RESTART_ATTEMPTS 3
#define RESTART_WINDOW_MS 30000  // 30 seconds

#define WINDOW_CLASS_NAME L"MouseDamperLauncherWindow"

typedef struct {
    HWND hwnd;
    NOTIFYICONDATAW nid;
    HANDLE daemon_process;
    HANDLE daemon_wait_handle;
    HANDLE config_monitor_handle;
    HANDLE config_wait_handle;
    DWORD daemon_pid;
    MouseDamperConfig config;
    int restart_attempts;
    DWORD last_restart_time;
    bool verbose;
} TrayAppState;

/* Forward declarations */
static LRESULT CALLBACK tray_window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK about_dialog_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static VOID CALLBACK config_change_callback(PVOID context, BOOLEAN timeout);
static void reload_config_and_restart(TrayAppState *state);
static void on_enable(TrayAppState *state);
static void on_disable(TrayAppState *state);
static void update_menu_for_state(HMENU menu, TrayAppState *state);

static int
get_system_double_click_time (void)
{
    UINT time_ms = GetDoubleClickTime ();
    return (int)time_ms;
}

static bool
kill_existing_processes (void)
{
    HANDLE snapshot;
    PROCESSENTRY32W pe32;
    DWORD current_pid = GetCurrentProcessId ();
    bool found_any = false;

    snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    pe32.dwSize = sizeof (PROCESSENTRY32W);

    if (!Process32FirstW (snapshot, &pe32)) {
        CloseHandle (snapshot);
        return false;
    }

    do {
        /* Look for mousedamper.exe (case insensitive) */
        if (_wcsicmp (pe32.szExeFile, L"mousedamper.exe") == 0) {
            /* Don't kill ourselves */
            if (pe32.th32ProcessID == current_pid) {
                continue;
            }

            found_any = true;

            /* Try to terminate the process */
            HANDLE process = OpenProcess (PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
            if (process != NULL) {
                TerminateProcess (process, 0);
                CloseHandle (process);
            }
        }
    } while (Process32NextW (snapshot, &pe32));

    CloseHandle (snapshot);
    return found_any;
}

static HANDLE
launch_daemon_and_get_handle (bool verbose, int dblclick_ms, int threshold_px, double threshold_scale, DWORD *out_pid)
{
    wchar_t cmd_line[MAX_PATH * 2];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    /* Build command line: mousedamper.exe verbose/quiet <dblclick-ms> <threshold-px> <threshold-scale> */
    snwprintf (cmd_line, MAX_PATH * 2, L"\"%s\" %s %d %d %.2f",
               MOUSEDAMPER_DAEMON_PATH,
               verbose ? L"verbose" : L"quiet",
               dblclick_ms,
               threshold_px,
               threshold_scale);

    /* Setup startup info */
    ZeroMemory (&si, sizeof (si));
    si.cb = sizeof (si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  /* Hide console window */

    ZeroMemory (&pi, sizeof (pi));

    /* Launch the daemon */
    if (!CreateProcessW (MOUSEDAMPER_DAEMON_PATH,
                         cmd_line,
                         NULL,           /* Process handle not inheritable */
                         NULL,           /* Thread handle not inheritable */
                         FALSE,          /* Don't inherit handles */
                         CREATE_NO_WINDOW, /* No console window */
                         NULL,           /* Use parent environment */
                         NULL,           /* Use parent directory */
                         &si,
                         &pi)) {
        return NULL;
    }

    /* Close thread handle, but keep process handle */
    CloseHandle (pi.hThread);

    if (out_pid) {
        *out_pid = pi.dwProcessId;
    }

    return pi.hProcess;
}

static void
show_balloon_notification (TrayAppState *state, const wchar_t *title, const wchar_t *msg)
{
    if (!state || !state->hwnd) return;

    state->nid.uFlags = NIF_INFO;
    snwprintf (state->nid.szInfoTitle, 64, L"%s", title);
    snwprintf (state->nid.szInfo, 256, L"%s", msg);
    state->nid.dwInfoFlags = NIIF_INFO;

    Shell_NotifyIconW (NIM_MODIFY, &state->nid);

    /* Clear the flags for next time */
    state->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
}

static void
update_tray_tooltip (TrayAppState *state, const wchar_t *text)
{
    if (!state || !state->hwnd) return;

    snwprintf (state->nid.szTip, 128, L"%s", text);
    Shell_NotifyIconW (NIM_MODIFY, &state->nid);
}

static VOID CALLBACK
daemon_exit_callback (PVOID context, BOOLEAN timeout)
{
    TrayAppState *state = (TrayAppState *)context;

    /* Post message to main window (thread-safe) */
    /* Don't call UI functions directly from callback! */
    if (state && state->hwnd) {
        PostMessage (state->hwnd, WM_DAEMON_EXITED, 0, 0);
    }
}

static bool
register_process_wait (TrayAppState *state)
{
    if (!state->daemon_process) return false;

    /* Register asynchronous wait on process handle */
    /* Callback fires immediately when process exits */
    return RegisterWaitForSingleObject (
        &state->daemon_wait_handle,
        state->daemon_process,
        daemon_exit_callback,
        state,
        INFINITE,
        WT_EXECUTEONLYONCE
    );
}

static bool
restart_daemon_with_throttle (TrayAppState *state)
{
    DWORD current_time = GetTickCount ();
    int dblclick_ms;

    /* Reset counter if outside time window */
    if (current_time - state->last_restart_time > RESTART_WINDOW_MS) {
        state->restart_attempts = 0;
    }

    /* Check rate limit */
    if (state->restart_attempts >= MAX_RESTART_ATTEMPTS) {
        wchar_t *title = _W("Mouse Damper Error");
        wchar_t *msg = _W("Daemon crashed multiple times. Please check configuration.");
        wchar_t *tooltip = _W("Mouse Damper - Stopped (Too many crashes)");
        show_balloon_notification (state, title, msg);
        update_tray_tooltip (state, tooltip);
        free(title);
        free(msg);
        free(tooltip);
        return false;
    }

    state->restart_attempts++;
    state->last_restart_time = current_time;

    /* Determine double-click time */
    if (state->config.override_double_click_time) {
        dblclick_ms = state->config.double_click_time_override;
    } else {
        dblclick_ms = get_system_double_click_time ();
    }

    /* Launch daemon */
    wchar_t *restarting_txt = _W("Mouse Damper - Restarting...");
    update_tray_tooltip (state, restarting_txt);
    free(restarting_txt);

    state->daemon_process = launch_daemon_and_get_handle (
        state->verbose,
        dblclick_ms,
        state->config.delta_threshold,
        state->config.threshold_scale_factor,
        &state->daemon_pid
    );

    if (state->daemon_process) {
        /* Register wait for new process */
        register_process_wait (state);

        wchar_t *active_txt = _W("Mouse Damper - Active");
        wchar_t *success_title = _W("Mouse Damper");
        wchar_t *success_msg = _W("Daemon restarted successfully");
        update_tray_tooltip (state, active_txt);
        show_balloon_notification (state, success_title, success_msg);
        free(active_txt);
        free(success_title);
        free(success_msg);
        return true;
    } else {
        wchar_t *failed_txt = _W("Mouse Damper - Failed to restart");
        wchar_t *err_title = _W("Mouse Damper Error");
        wchar_t *err_msg = _W("Failed to restart daemon");
        update_tray_tooltip (state, failed_txt);
        show_balloon_notification (state, err_title, err_msg);
        free(failed_txt);
        free(err_title);
        free(err_msg);
        return false;
    }
}

static bool
add_tray_icon (TrayAppState *state)
{
    ZeroMemory (&state->nid, sizeof (state->nid));
    state->nid.cbSize = sizeof (NOTIFYICONDATAW);
    state->nid.hWnd = state->hwnd;
    state->nid.uID = 1;
    state->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    state->nid.uCallbackMessage = WM_TRAYICON;
    state->nid.hIcon = LoadIcon (GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MOUSEDAMPER));

    wchar_t *active_tip = _W("Mouse Damper - Active");
    snwprintf (state->nid.szTip, 128, L"%s", active_tip);
    free(active_tip);

    return Shell_NotifyIconW (NIM_ADD, &state->nid);
}

static bool
remove_tray_icon (TrayAppState *state)
{
    return Shell_NotifyIconW (NIM_DELETE, &state->nid);
}

static void
on_configure (TrayAppState *state)
{
    /* Launch config GUI (non-blocking) */
    STARTUPINFOW si = {0};
    si.cb = sizeof (si);
    PROCESS_INFORMATION pi = {0};

    if (CreateProcessW (MOUSEDAMPER_CONFIG_PATH, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle (pi.hProcess);
        CloseHandle (pi.hThread);
    }
}

static void
on_about (TrayAppState *state)
{
    DialogBox (GetModuleHandle (NULL),
              MAKEINTRESOURCE (IDD_ABOUT_DIALOG),
              state->hwnd,
              about_dialog_proc);
}

static void
on_quit (TrayAppState *state)
{
    /* Unregister wait */
    if (state->daemon_wait_handle) {
        UnregisterWait (state->daemon_wait_handle);
        state->daemon_wait_handle = NULL;
    }

    /* Kill daemon gracefully */
    if (state->daemon_process) {
        TerminateProcess (state->daemon_process, 0);
        WaitForSingleObject (state->daemon_process, 2000);
        CloseHandle (state->daemon_process);
        state->daemon_process = NULL;
    }

    /* Remove tray icon */
    remove_tray_icon (state);

    /* Exit application */
    PostQuitMessage (0);
}

static VOID CALLBACK
config_change_callback (PVOID context, BOOLEAN timeout)
{
    TrayAppState *state = (TrayAppState *)context;

    /* Post message to main window (thread-safe) */
    /* Don't call UI functions directly from callback! */
    if (state && state->hwnd) {
        PostMessage (state->hwnd, WM_CONFIG_CHANGED, 0, 0);
    }
}

static void
reload_config_and_restart (TrayAppState *state)
{
    /* Reload configuration */
    if (!config_load(&state->config)) {
        return;
    }

    /* Unregister wait if daemon is running */
    if (state->daemon_wait_handle) {
        UnregisterWait(state->daemon_wait_handle);
        state->daemon_wait_handle = NULL;
    }

    /* Stop existing daemon */
    if (state->daemon_process) {
        TerminateProcess(state->daemon_process, 0);
        WaitForSingleObject(state->daemon_process, 2000);
        CloseHandle(state->daemon_process);
        state->daemon_process = NULL;
    }

    /* Update tooltip based on enabled state */
    if (state->config.enabled) {
        wchar_t *restarting = _W("Mouse Damper - Restarting...");
        update_tray_tooltip(state, restarting);
        free(restarting);

        /* Determine double-click time */
        int dblclick_ms = state->config.override_double_click_time ?
                          state->config.double_click_time_override :
                          get_system_double_click_time();

        /* Launch daemon with new settings */
        state->daemon_process = launch_daemon_and_get_handle(
            state->verbose,
            dblclick_ms,
            state->config.delta_threshold,
            state->config.threshold_scale_factor,
            &state->daemon_pid
        );

        if (state->daemon_process) {
            register_process_wait(state);

            wchar_t *active = _W("Mouse Damper - Active");
            wchar_t *title = _W("Mouse Damper");
            wchar_t *msg = _W("Settings applied - daemon restarted");
            update_tray_tooltip(state, active);
            show_balloon_notification(state, title, msg);
            free(active);
            free(title);
            free(msg);
        } else {
            wchar_t *failed = _W("Mouse Damper - Failed to start");
            update_tray_tooltip(state, failed);
            free(failed);
        }
    } else {
        wchar_t *disabled = _W("Mouse Damper - Disabled");
        update_tray_tooltip(state, disabled);
        free(disabled);
    }
}

static void
on_enable (TrayAppState *state)
{
    /* Enable in config */
    state->config.enabled = true;

    /* Save config */
    if (!config_save(&state->config)) {
        wchar_t *err_title = _W("Error");
        wchar_t *err_msg = _W("Failed to save configuration");
        show_balloon_notification(state, err_title, err_msg);
        free(err_title);
        free(err_msg);
        return;
    }

    /* Start daemon */
    reload_config_and_restart(state);
}

static void
on_disable (TrayAppState *state)
{
    /* Disable in config */
    state->config.enabled = false;

    /* Save config */
    if (!config_save(&state->config)) {
        wchar_t *err_title = _W("Error");
        wchar_t *err_msg = _W("Failed to save configuration");
        show_balloon_notification(state, err_title, err_msg);
        free(err_title);
        free(err_msg);
        return;
    }

    /* Stop daemon */
    if (state->daemon_wait_handle) {
        UnregisterWait(state->daemon_wait_handle);
        state->daemon_wait_handle = NULL;
    }

    if (state->daemon_process) {
        TerminateProcess(state->daemon_process, 0);
        WaitForSingleObject(state->daemon_process, 2000);
        CloseHandle(state->daemon_process);
        state->daemon_process = NULL;
    }

    wchar_t *disabled = _W("Mouse Damper - Disabled");
    wchar_t *title = _W("Mouse Damper");
    wchar_t *msg = _W("Daemon stopped");
    update_tray_tooltip(state, disabled);
    show_balloon_notification(state, title, msg);
    free(disabled);
    free(title);
    free(msg);
}

static void
update_menu_for_state (HMENU menu, TrayAppState *state)
{
    /* Enable/disable menu items based on current state */
    EnableMenuItem(menu, IDM_ENABLE, state->config.enabled ? MF_GRAYED : MF_ENABLED);
    EnableMenuItem(menu, IDM_DISABLE, state->config.enabled ? MF_ENABLED : MF_GRAYED);
}

static void
show_context_menu (TrayAppState *state)
{
    POINT pt;
    GetCursorPos (&pt);

    HMENU hmenu = LoadMenu (GetModuleHandle (NULL), MAKEINTRESOURCE (IDR_TRAY_MENU));
    if (!hmenu) return;

    HMENU popup = GetSubMenu (hmenu, 0);

    /* Set menu item text from translations */
    MENUITEMINFOW mii = {0};
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_STRING;

    wchar_t *enable_txt = _W("Enable");
    mii.dwTypeData = enable_txt;
    SetMenuItemInfoW(popup, IDM_ENABLE, FALSE, &mii);
    free(enable_txt);

    wchar_t *disable_txt = _W("Disable");
    mii.dwTypeData = disable_txt;
    SetMenuItemInfoW(popup, IDM_DISABLE, FALSE, &mii);
    free(disable_txt);

    wchar_t *config_txt = _W("Configure...");
    mii.dwTypeData = config_txt;
    SetMenuItemInfoW(popup, IDM_CONFIGURE, FALSE, &mii);
    free(config_txt);

    wchar_t *about_txt = _W("About...");
    mii.dwTypeData = about_txt;
    SetMenuItemInfoW(popup, IDM_ABOUT, FALSE, &mii);
    free(about_txt);

    wchar_t *quit_txt = _W("Quit");
    mii.dwTypeData = quit_txt;
    SetMenuItemInfoW(popup, IDM_QUIT, FALSE, &mii);
    free(quit_txt);

    /* Update menu items based on enabled state */
    update_menu_for_state(popup, state);

    /* Required for menu to work correctly from taskbar */
    SetForegroundWindow (state->hwnd);

    TrackPopupMenu (popup, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, state->hwnd, NULL);

    /* Required cleanup */
    PostMessage (state->hwnd, WM_NULL, 0, 0);
    DestroyMenu (hmenu);
}

static void
on_tray_icon_event (TrayAppState *state, LPARAM lParam)
{
    switch (lParam) {
        case WM_LBUTTONDBLCLK:
            on_configure (state);
            break;

        case WM_RBUTTONUP:
            show_context_menu (state);
            break;
    }
}

static LRESULT CALLBACK
tray_window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TrayAppState *state = (TrayAppState *)GetWindowLongPtr (hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE:
        {
            /* Store state pointer */
            CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
            state = (TrayAppState *)cs->lpCreateParams;
            SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR)state);
            state->hwnd = hwnd;

            /* Add tray icon */
            if (!add_tray_icon (state)) {
                wchar_t *err_msg = _W("Failed to create tray icon");
                wchar_t *err_title = _W("Error");
                MessageBoxW (NULL, err_msg, err_title, MB_OK | MB_ICONERROR);
                free(err_msg);
                free(err_title);
                return -1;
            }

            /* Start config file monitoring (event-driven, no polling!) */
            state->config_monitor_handle = config_start_monitoring ();
            if (state->config_monitor_handle) {
                /* Register wait for config changes */
                RegisterWaitForSingleObject (
                    &state->config_wait_handle,
                    state->config_monitor_handle,
                    config_change_callback,
                    state,
                    INFINITE,
                    WT_EXECUTEONLYONCE
                );
            }

            /* Kill any existing daemon processes */
            kill_existing_processes ();
            Sleep (100);

            /* Launch daemon only if enabled */
            if (state->config.enabled) {
                /* Determine double-click time */
                int dblclick_ms;
                if (state->config.override_double_click_time) {
                    dblclick_ms = state->config.double_click_time_override;
                } else {
                    dblclick_ms = get_system_double_click_time ();
                }

                /* Launch daemon */
                state->daemon_process = launch_daemon_and_get_handle (
                    state->verbose,
                    dblclick_ms,
                    state->config.delta_threshold,
                    state->config.threshold_scale_factor,
                    &state->daemon_pid
                );

                if (state->daemon_process) {
                    /* Register wait for process exit */
                    register_process_wait (state);
                } else {
                    wchar_t *err_msg = _W("Failed to launch mousedamper daemon");
                    wchar_t *err_title = _W("Error");
                    wchar_t *failed_tip = _W("Mouse Damper - Failed to start");
                    MessageBoxW (NULL, err_msg, err_title, MB_OK | MB_ICONERROR);
                    update_tray_tooltip (state, failed_tip);
                    free(err_msg);
                    free(err_title);
                    free(failed_tip);
                }
            } else {
                wchar_t *disabled_tip = _W("Mouse Damper - Disabled");
                update_tray_tooltip (state, disabled_tip);
                free(disabled_tip);
            }

            return 0;
        }

        case WM_TRAYICON:
            if (state) {
                on_tray_icon_event (state, lParam);
            }
            return 0;

        case WM_DAEMON_EXITED:
            if (state) {
                /* Unregister wait */
                if (state->daemon_wait_handle) {
                    UnregisterWait (state->daemon_wait_handle);
                    state->daemon_wait_handle = NULL;
                }

                /* Close process handle */
                if (state->daemon_process) {
                    CloseHandle (state->daemon_process);
                    state->daemon_process = NULL;
                }

                /* Attempt restart with throttling */
                restart_daemon_with_throttle (state);
            }
            return 0;

        case WM_COMMAND:
            if (state) {
                switch (LOWORD (wParam)) {
                    case IDM_ENABLE:
                        on_enable (state);
                        break;
                    case IDM_DISABLE:
                        on_disable (state);
                        break;
                    case IDM_CONFIGURE:
                        on_configure (state);
                        break;
                    case IDM_ABOUT:
                        on_about (state);
                        break;
                    case IDM_QUIT:
                        on_quit (state);
                        break;
                }
            }
            return 0;

        case WM_CONFIG_CHANGED:
            if (state) {
                /* Unregister the wait (it's one-shot) */
                if (state->config_wait_handle) {
                    UnregisterWait (state->config_wait_handle);
                    state->config_wait_handle = NULL;
                }

                /* Reset the monitoring to continue watching */
                if (state->config_monitor_handle) {
                    config_reset_monitoring (state->config_monitor_handle);

                    /* Re-register the wait */
                    RegisterWaitForSingleObject (
                        &state->config_wait_handle,
                        state->config_monitor_handle,
                        config_change_callback,
                        state,
                        INFINITE,
                        WT_EXECUTEONLYONCE
                    );
                }

                /* Reload config and restart daemon */
                reload_config_and_restart (state);
            }
            return 0;

        case WM_DESTROY:
            if (state) {
                /* Stop config monitoring */
                if (state->config_wait_handle) {
                    UnregisterWait (state->config_wait_handle);
                    state->config_wait_handle = NULL;
                }
                if (state->config_monitor_handle) {
                    config_stop_monitoring (state->config_monitor_handle);
                    state->config_monitor_handle = NULL;
                }

                /* Unregister daemon wait */
                if (state->daemon_wait_handle) {
                    UnregisterWait (state->daemon_wait_handle);
                    state->daemon_wait_handle = NULL;
                }

                /* Close process handle */
                if (state->daemon_process) {
                    CloseHandle (state->daemon_process);
                    state->daemon_process = NULL;
                }

                remove_tray_icon (state);
            }
            PostQuitMessage (0);
            return 0;
    }

    return DefWindowProc (hwnd, msg, wParam, lParam);
}

static INT_PTR CALLBACK
about_dialog_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
        case WM_INITDIALOG:
        {
            /* Set dialog title */
            wchar_t *title = _W("About Mouse Damper");
            SetWindowTextW(hwnd, title);
            free(title);

            /* Set version title */
            wchar_t version_buf[128];
            wchar_t *version_fmt = _W("Mouse Damper v%s");
            snwprintf(version_buf, 128, version_fmt, MOUSEDAMPER_VERSION_W);
            free(version_fmt);
            SetDlgItemTextW(hwnd, IDC_ABOUT_TITLE, version_buf);

            /* Set description paragraphs */
            wchar_t *desc1 = _W("Prevents accidental clicks and drag operations caused by hand tremors or unsteady mouse movements.");
            SetDlgItemTextW(hwnd, IDC_ABOUT_DESCRIPTION, desc1);
            free(desc1);

            wchar_t *desc2 = _W("When you press a mouse button, the pointer is frozen in place until you complete a double-click, the double-click timeout expires, or you move the mouse beyond the configured threshold.");
            SetDlgItemTextW(hwnd, IDC_ABOUT_DESCRIPTION2, desc2);
            free(desc2);

            /* Set copyright and license */
            wchar_t *copyright = _W("Copyright 2020 Michael Webster");
            SetDlgItemTextW(hwnd, IDC_ABOUT_COPYRIGHT, copyright);
            free(copyright);

            wchar_t *license = _W("Licensed under GPL-3.0");
            SetDlgItemTextW(hwnd, IDC_ABOUT_LICENSE, license);
            free(license);

            /* Set OK button text */
            wchar_t *ok_txt = _W("OK");
            SetDlgItemTextW(hwnd, IDOK, ok_txt);
            free(ok_txt);

            return TRUE;
        }

        case WM_COMMAND:
            if (LOWORD (wParam) == IDOK || LOWORD (wParam) == IDCANCEL) {
                EndDialog (hwnd, LOWORD (wParam));
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static bool
create_window (TrayAppState *state)
{
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof (WNDCLASSEXW);
    wc.lpfnWndProc = tray_window_proc;
    wc.hInstance = GetModuleHandle (NULL);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW (&wc)) {
        return false;
    }

    /* Create hidden window */
    state->hwnd = CreateWindowExW (
        WS_EX_TOOLWINDOW,  /* Don't show in taskbar */
        WINDOW_CLASS_NAME,
        L"Mouse Damper Launcher",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,
        NULL,
        GetModuleHandle (NULL),
        state  /* Pass state as creation parameter */
    );

    return (state->hwnd != NULL);
}

int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    TrayAppState state = {0};
    MSG msg;
    HANDLE singleton_mutex;

    /* Ensure only one instance runs - use a named mutex */
    singleton_mutex = CreateMutexW(NULL, TRUE, L"Local\\MouseDamperLauncher");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Another instance is already running */
        if (singleton_mutex) CloseHandle(singleton_mutex);
        return 0;
    }

    /* Initialize gettext */
    init_gettext_windows();

    /* Parse command line for verbose flag */
    if (lpCmdLine && (strstr (lpCmdLine, "-v") || strstr (lpCmdLine, "--verbose"))) {
        state.verbose = true;
    }

    /* Load configuration */
    if (!config_load (&state.config)) {
        wchar_t *err_msg = _W("Failed to load configuration");
        wchar_t *err_title = _W("Error");
        MessageBoxW (NULL, err_msg, err_title, MB_OK | MB_ICONERROR);
        free(err_msg);
        free(err_title);
        return 1;
    }

    /* Exit silently if not enabled */
    if (!state.config.enabled) {
        return 0;
    }

    /* Create window */
    if (!create_window (&state)) {
        wchar_t *err_msg = _W("Failed to create window");
        wchar_t *err_title = _W("Error");
        MessageBoxW (NULL, err_msg, err_title, MB_OK | MB_ICONERROR);
        free(err_msg);
        free(err_title);
        return 1;
    }

    /* Message loop */
    while (GetMessage (&msg, NULL, 0, 0)) {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }

    return (int)msg.wParam;
}
