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

#include "settings-backend.h"
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>

bool
config_get_file_path (wchar_t *buffer, size_t buffer_size)
{
    wchar_t appdata_path[MAX_PATH];

    /* Get %APPDATA% path */
    if (SHGetFolderPathW (NULL, CSIDL_APPDATA, NULL, 0, appdata_path) != S_OK) {
        fprintf (stderr, "Failed to get APPDATA path\n");
        return false;
    }

    /* Build full path: %APPDATA%\mousedamper\config.ini */
    snwprintf (buffer, buffer_size, L"%s\\mousedamper\\config.ini", appdata_path);

    return true;
}

bool
config_ensure_directory (void)
{
    wchar_t appdata_path[MAX_PATH];
    wchar_t config_dir[MAX_PATH];

    /* Get %APPDATA% path */
    if (SHGetFolderPathW (NULL, CSIDL_APPDATA, NULL, 0, appdata_path) != S_OK) {
        fprintf (stderr, "Failed to get APPDATA path\n");
        return false;
    }

    /* Build directory path: %APPDATA%\mousedamper */
    snwprintf (config_dir, MAX_PATH, L"%s\\mousedamper", appdata_path);

    /* Create directory (ignore error if it already exists) */
    if (!CreateDirectoryW (config_dir, NULL)) {
        DWORD error = GetLastError ();
        if (error != ERROR_ALREADY_EXISTS) {
            fprintf (stderr, "Failed to create config directory: error %lu\n", error);
            return false;
        }
    }

    return true;
}

bool
config_load (MouseDamperConfig *config)
{
    wchar_t config_path[MAX_PATH];
    DWORD file_attrs;
    bool file_exists;

    if (!config) {
        return false;
    }

    /* Get config file path */
    if (!config_get_file_path (config_path, MAX_PATH)) {
        return false;
    }

    /* Check if file exists */
    file_attrs = GetFileAttributesW (config_path);
    file_exists = (file_attrs != INVALID_FILE_ATTRIBUTES);

    /* If file doesn't exist, use defaults and create it */
    if (!file_exists) {
        config->enabled = DEFAULT_ENABLED;
        config->delta_threshold = DEFAULT_DELTA;
        config->threshold_scale_factor = DEFAULT_THRESHOLD_SCALE;
        config->override_double_click_time = DEFAULT_OVERRIDE_DBLCLICK;
        config->double_click_time_override = DEFAULT_DBLCLICK_OVERRIDE;

        /* Create directory and save defaults */
        if (!config_ensure_directory ()) {
            return false;
        }

        return config_save (config);
    }

    /* Read values from INI file */
    config->enabled = GetPrivateProfileIntW (
        L"" CONFIG_SECTION,
        L"" CONFIG_KEY_ENABLED,
        DEFAULT_ENABLED,
        config_path
    ) != 0;

    config->delta_threshold = GetPrivateProfileIntW (
        L"" CONFIG_SECTION,
        L"" CONFIG_KEY_DELTA,
        DEFAULT_DELTA,
        config_path
    );

    /* Read threshold_scale_factor as string and parse to double */
    {
        wchar_t scale_buf[32];
        wchar_t default_scale_buf[32];
        snwprintf (default_scale_buf, 32, L"%.2f", DEFAULT_THRESHOLD_SCALE);

        GetPrivateProfileStringW (
            L"" CONFIG_SECTION,
            L"" CONFIG_KEY_THRESHOLD_SCALE,
            default_scale_buf,
            scale_buf,
            32,
            config_path
        );
        config->threshold_scale_factor = wcstod (scale_buf, NULL);
    }

    config->override_double_click_time = GetPrivateProfileIntW (
        L"" CONFIG_SECTION,
        L"" CONFIG_KEY_OVERRIDE_DBLCLICK,
        DEFAULT_OVERRIDE_DBLCLICK,
        config_path
    ) != 0;

    config->double_click_time_override = GetPrivateProfileIntW (
        L"" CONFIG_SECTION,
        L"" CONFIG_KEY_DBLCLICK_OVERRIDE,
        DEFAULT_DBLCLICK_OVERRIDE,
        config_path
    );

    /* Validate and clamp values */
    config_validate (config);

    return true;
}

bool
config_save (const MouseDamperConfig *config)
{
    wchar_t config_path[MAX_PATH];
    wchar_t value_buf[32];

    if (!config) {
        return false;
    }

    /* Ensure directory exists */
    if (!config_ensure_directory ()) {
        return false;
    }

    /* Get config file path */
    if (!config_get_file_path (config_path, MAX_PATH)) {
        return false;
    }

    /* Write Enabled */
    snwprintf (value_buf, 32, L"%d", config->enabled ? 1 : 0);
    if (!WritePrivateProfileStringW (L"" CONFIG_SECTION, L"" CONFIG_KEY_ENABLED, value_buf, config_path)) {
        fprintf (stderr, "Failed to write Enabled: error %lu\n", GetLastError ());
        return false;
    }

    /* Write DeltaThreshold */
    snwprintf (value_buf, 32, L"%d", config->delta_threshold);
    if (!WritePrivateProfileStringW (L"" CONFIG_SECTION, L"" CONFIG_KEY_DELTA, value_buf, config_path)) {
        fprintf (stderr, "Failed to write DeltaThreshold: error %lu\n", GetLastError ());
        return false;
    }

    /* Write ThresholdScaleFactor */
    snwprintf (value_buf, 32, L"%.2f", config->threshold_scale_factor);
    if (!WritePrivateProfileStringW (L"" CONFIG_SECTION, L"" CONFIG_KEY_THRESHOLD_SCALE, value_buf, config_path)) {
        fprintf (stderr, "Failed to write ThresholdScaleFactor: error %lu\n", GetLastError ());
        return false;
    }

    /* Write OverrideDoubleClickTime */
    snwprintf (value_buf, 32, L"%d", config->override_double_click_time ? 1 : 0);
    if (!WritePrivateProfileStringW (L"" CONFIG_SECTION, L"" CONFIG_KEY_OVERRIDE_DBLCLICK, value_buf, config_path)) {
        fprintf (stderr, "Failed to write OverrideDoubleClickTime: error %lu\n", GetLastError ());
        return false;
    }

    /* Write DoubleClickTimeOverride */
    snwprintf (value_buf, 32, L"%d", config->double_click_time_override);
    if (!WritePrivateProfileStringW (L"" CONFIG_SECTION, L"" CONFIG_KEY_DBLCLICK_OVERRIDE, value_buf, config_path)) {
        fprintf (stderr, "Failed to write DoubleClickTimeOverride: error %lu\n", GetLastError ());
        return false;
    }

    return true;
}

void
config_validate (MouseDamperConfig *config)
{
    if (!config) {
        return;
    }

    /* Clamp delta_threshold to valid range */
    if (config->delta_threshold < MIN_DELTA) {
        config->delta_threshold = MIN_DELTA;
    } else if (config->delta_threshold > MAX_DELTA) {
        config->delta_threshold = MAX_DELTA;
    }

    /* Clamp threshold_scale_factor to valid range */
    if (config->threshold_scale_factor < MIN_THRESHOLD_SCALE) {
        config->threshold_scale_factor = MIN_THRESHOLD_SCALE;
    } else if (config->threshold_scale_factor > MAX_THRESHOLD_SCALE) {
        config->threshold_scale_factor = MAX_THRESHOLD_SCALE;
    }

    /* Clamp double_click_time_override to valid range */
    if (config->double_click_time_override < MIN_DBLCLICK) {
        config->double_click_time_override = MIN_DBLCLICK;
    } else if (config->double_click_time_override > MAX_DBLCLICK) {
        config->double_click_time_override = MAX_DBLCLICK;
    }
}

HANDLE
config_start_monitoring (void)
{
    wchar_t dir_path[MAX_PATH];

    /* Get config directory path */
    if (FAILED (SHGetFolderPathW (NULL, CSIDL_APPDATA, NULL, 0, dir_path))) {
        return NULL;
    }

    wcscat (dir_path, L"\\mousedamper");

    /* Start monitoring the directory for file changes
     * FILE_NOTIFY_CHANGE_LAST_WRITE: Notified when files are written to
     * This returns a handle that gets signaled when changes occur */
    HANDLE monitor = FindFirstChangeNotificationW (
        dir_path,
        FALSE,  /* Don't watch subdirectories */
        FILE_NOTIFY_CHANGE_LAST_WRITE
    );

    if (monitor == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    return monitor;
}

bool
config_reset_monitoring (HANDLE monitor_handle)
{
    if (!monitor_handle || monitor_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    /* Reset the notification so it will signal again on the next change */
    return FindNextChangeNotification (monitor_handle);
}

void
config_stop_monitoring (HANDLE monitor_handle)
{
    if (monitor_handle && monitor_handle != INVALID_HANDLE_VALUE) {
        FindCloseChangeNotification (monitor_handle);
    }
}
