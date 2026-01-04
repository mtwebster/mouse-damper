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

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#include "settings-backend.h"
#include "mousedamper-config_resource.h"
#include "gettext-helpers.h"

/* Global state */
static MouseDamperConfig g_config;
static HWND g_dialog = NULL;
static BOOL g_settings_dirty = FALSE;

/* Forward declarations */
static void init_controls (HWND hwnd);
static void update_config_from_controls (HWND hwnd);
static void apply_settings (HWND hwnd);
static void update_threshold_label (HWND hwnd);
static void update_override_checkbox_text (HWND hwnd);
static void mark_settings_dirty (HWND hwnd);
static void save_enabled_immediately (HWND hwnd);
static int get_system_double_click_time (void);

static int
get_system_double_click_time (void)
{
    return (int)GetDoubleClickTime ();
}

static void
update_threshold_label (HWND hwnd)
{
    wchar_t label[64];
    int value = (int)SendDlgItemMessage (hwnd, IDC_THRESHOLD_SLIDER, TBM_GETPOS, 0, 0);
    wchar_t *fmt = _W("%d pixels");
    snwprintf (label, 64, fmt, value);
    free(fmt);
    SetDlgItemTextW (hwnd, IDC_THRESHOLD_LABEL, label);
}

static void
update_override_checkbox_text (HWND hwnd)
{
    wchar_t label[128];
    int sys_time = get_system_double_click_time ();
    wchar_t *fmt = _W("Override system double-click time (currently %dms)");
    snwprintf (label, 128, fmt, sys_time);
    free(fmt);
    SetDlgItemTextW (hwnd, IDC_OVERRIDE_CHECK, label);
}

static void
mark_settings_dirty (HWND hwnd)
{
    g_settings_dirty = TRUE;

    /* Enable Apply button */
    EnableWindow (GetDlgItem (hwnd, IDC_APPLY_BUTTON), TRUE);

    /* Update labels immediately */
    update_threshold_label (hwnd);
}

static void
save_enabled_immediately (HWND hwnd)
{
    /* Read enabled checkbox */
    g_config.enabled = (IsDlgButtonChecked (hwnd, IDC_ENABLED_CHECK) == BST_CHECKED);

    /* Save to INI */
    if (!config_save (&g_config)) {
        wchar_t *err_msg = _W("Failed to save configuration");
        wchar_t *err_title = _W("Error");
        MessageBoxW (hwnd, err_msg, err_title, MB_OK | MB_ICONERROR);
        free(err_msg);
        free(err_title);
        return;
    }
}

static void
init_controls (HWND hwnd)
{
    /* Initialize common controls */
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof (icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx (&icc);

    /* Set dialog title */
    wchar_t *title = _W("Mousedamper Configuration");
    SetWindowTextW(hwnd, title);
    free(title);

    /* Set description text */
    wchar_t *desc = _W("Mouse Damper helps prevent accidental clicks and drag operations caused by hand tremors or unsteady mouse movements.\n\nWhen you press a mouse button, the pointer is frozen in place until either you complete a double-click, the double-click timeout expires, or you move the mouse beyond the breakout threshold.");
    SetDlgItemTextW(hwnd, IDC_DESC_TEXT, desc);
    free(desc);

    /* Set group box titles */
    wchar_t *status_group = _W("Status");
    SetDlgItemTextW(hwnd, IDC_STATUS_GROUP, status_group);
    free(status_group);

    wchar_t *movement_group = _W("Movement");
    SetDlgItemTextW(hwnd, IDC_MOVEMENT_GROUP, movement_group);
    free(movement_group);

    wchar_t *clicks_group = _W("Clicks");
    SetDlgItemTextW(hwnd, IDC_CLICKS_GROUP, clicks_group);
    free(clicks_group);

    /* Set checkbox text */
    wchar_t *enabled_txt = _W("Enable mousedamper");
    SetDlgItemTextW(hwnd, IDC_ENABLED_CHECK, enabled_txt);
    free(enabled_txt);

    /* Set static label text */
    wchar_t *threshold_label = _W("Breakout threshold:");
    SetDlgItemTextW(hwnd, IDC_THRESHOLD_LABEL_STATIC, threshold_label);
    free(threshold_label);

    wchar_t *dblclick_label = _W("Double-click time:");
    SetDlgItemTextW(hwnd, IDC_DBLCLICK_LABEL_STATIC, dblclick_label);
    free(dblclick_label);

    wchar_t *ms_label = _W("ms");
    SetDlgItemTextW(hwnd, IDC_MS_LABEL, ms_label);
    free(ms_label);

    /* Set apply button text */
    wchar_t *apply_txt = _W("Apply settings and restart the daemon");
    SetDlgItemTextW(hwnd, IDC_APPLY_BUTTON, apply_txt);
    free(apply_txt);

    /* Enabled checkbox (launcher always runs, daemon runs if enabled) */
    CheckDlgButton (hwnd, IDC_ENABLED_CHECK,
                    g_config.enabled ? BST_CHECKED : BST_UNCHECKED);

    /* Threshold slider */
    HWND slider = GetDlgItem (hwnd, IDC_THRESHOLD_SLIDER);
    SendMessage (slider, TBM_SETRANGE, TRUE, MAKELONG (MIN_DELTA, MAX_DELTA));
    SendMessage (slider, TBM_SETPOS, TRUE, g_config.delta_threshold);
    SendMessage (slider, TBM_SETTICFREQ, 50, 0);
    SendMessage (slider, TBM_SETPAGESIZE, 0, 50);
    update_threshold_label (hwnd);

    /* Override checkbox */
    CheckDlgButton (hwnd, IDC_OVERRIDE_CHECK,
                    g_config.override_double_click_time ? BST_CHECKED : BST_UNCHECKED);
    update_override_checkbox_text (hwnd);

    /* Double-click spin control */
    HWND spin = GetDlgItem (hwnd, IDC_DBLCLICK_SPIN);
    SendMessage (spin, UDM_SETRANGE32, MIN_DBLCLICK, MAX_DBLCLICK);
    SendMessage (spin, UDM_SETPOS32, 0, g_config.double_click_time_override);

    /* Enable/disable spin based on override checkbox */
    BOOL override_enabled = g_config.override_double_click_time;
    EnableWindow (GetDlgItem (hwnd, IDC_DBLCLICK_EDIT), override_enabled);
    EnableWindow (spin, override_enabled);

    /* Apply button starts disabled */
    EnableWindow (GetDlgItem (hwnd, IDC_APPLY_BUTTON), FALSE);
}

static void
update_config_from_controls (HWND hwnd)
{
    /* Read enabled checkbox */
    g_config.enabled = (IsDlgButtonChecked (hwnd, IDC_ENABLED_CHECK) == BST_CHECKED);

    /* Read threshold from slider */
    g_config.delta_threshold = (int)SendDlgItemMessage (hwnd, IDC_THRESHOLD_SLIDER,
                                                          TBM_GETPOS, 0, 0);

    /* Read override checkbox */
    g_config.override_double_click_time =
        (IsDlgButtonChecked (hwnd, IDC_OVERRIDE_CHECK) == BST_CHECKED);

    /* Read double-click time from spin */
    BOOL success;
    g_config.double_click_time_override =
        (int)SendDlgItemMessage (hwnd, IDC_DBLCLICK_SPIN, UDM_GETPOS32, 0, (LPARAM)&success);
    if (!success) {
        g_config.double_click_time_override = DEFAULT_DBLCLICK_OVERRIDE;
    }
}

static void
apply_settings (HWND hwnd)
{
    /* Update config from controls */
    update_config_from_controls (hwnd);

    /* Validate */
    config_validate (&g_config);

    /* Save to INI */
    if (!config_save (&g_config)) {
        wchar_t *err_msg = _W("Failed to save configuration");
        wchar_t *err_title = _W("Error");
        MessageBoxW (hwnd, err_msg, err_title, MB_OK | MB_ICONERROR);
        free(err_msg);
        free(err_title);
        return;
    }

    /* Mark as clean */
    g_settings_dirty = FALSE;

    /* Disable Apply button */
    EnableWindow (GetDlgItem (hwnd, IDC_APPLY_BUTTON), FALSE);
}

static INT_PTR CALLBACK
config_dialog_proc (HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
        case WM_INITDIALOG:
        {
            g_dialog = hwnd;

            /* Set window icon */
            HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_MOUSEDAMPER));
            if (hIcon) {
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
                SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
            }

            init_controls (hwnd);
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD (wp)) {
                case IDC_ENABLED_CHECK:
                    if (HIWORD (wp) == BN_CLICKED) {
                        /* Enabled checkbox: save immediately, don't mark dirty */
                        save_enabled_immediately (hwnd);
                    }
                    break;

                case IDC_OVERRIDE_CHECK:
                    if (HIWORD (wp) == BN_CLICKED) {
                        /* Enable/disable spin control */
                        BOOL checked = (IsDlgButtonChecked (hwnd, IDC_OVERRIDE_CHECK) == BST_CHECKED);
                        EnableWindow (GetDlgItem (hwnd, IDC_DBLCLICK_EDIT), checked);
                        EnableWindow (GetDlgItem (hwnd, IDC_DBLCLICK_SPIN), checked);
                        mark_settings_dirty (hwnd);
                    }
                    break;

                case IDC_DBLCLICK_EDIT:
                    if (HIWORD (wp) == EN_CHANGE) {
                        mark_settings_dirty (hwnd);
                    }
                    break;

                case IDC_APPLY_BUTTON:
                    if (HIWORD (wp) == BN_CLICKED) {
                        apply_settings (hwnd);
                    }
                    break;

                case IDCANCEL:
                    EndDialog (hwnd, 0);
                    return TRUE;
            }
            break;

        case WM_HSCROLL:
            /* Trackbar changed */
            if ((HWND)lp == GetDlgItem (hwnd, IDC_THRESHOLD_SLIDER)) {
                mark_settings_dirty (hwnd);
            }
            break;

        case WM_VSCROLL:
            /* Spin control changed */
            if ((HWND)lp == GetDlgItem (hwnd, IDC_DBLCLICK_SPIN)) {
                mark_settings_dirty (hwnd);
            }
            break;

        case WM_CLOSE:
            EndDialog (hwnd, 0);
            return TRUE;
    }

    return FALSE;
}

int WINAPI
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    HANDLE singleton_mutex;

    /* Ensure only one instance runs - use a named mutex */
    singleton_mutex = CreateMutexW(NULL, TRUE, L"Local\\MouseDamperConfig");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        /* Another instance is already running */
        if (singleton_mutex) CloseHandle(singleton_mutex);
        return 0;
    }

    /* Initialize gettext */
    init_gettext_windows();

    /* Load configuration */
    if (!config_load (&g_config)) {
        wchar_t *err_msg = _W("Failed to load configuration");
        wchar_t *err_title = _W("Error");
        MessageBoxW (NULL, err_msg, err_title, MB_OK | MB_ICONERROR);
        free(err_msg);
        free(err_title);
        return 1;
    }

    /* Show dialog */
    DialogBox (hInstance, MAKEINTRESOURCE (IDD_CONFIG_DIALOG), NULL, config_dialog_proc);

    return 0;
}
