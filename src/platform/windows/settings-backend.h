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

#ifndef MOUSEDAMPER_SETTINGS_BACKEND_H
#define MOUSEDAMPER_SETTINGS_BACKEND_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>

#define CONFIG_SECTION "MouseDamper"
#define CONFIG_KEY_ENABLED "Enabled"
#define CONFIG_KEY_DELTA "DeltaThreshold"
#define CONFIG_KEY_THRESHOLD_SCALE "ThresholdScaleFactor"
#define CONFIG_KEY_OVERRIDE_DBLCLICK "OverrideDoubleClickTime"
#define CONFIG_KEY_DBLCLICK_OVERRIDE "DoubleClickTimeOverride"

/* Default values */
#define DEFAULT_ENABLED 1
#define DEFAULT_DELTA 100
#define DEFAULT_THRESHOLD_SCALE 0.8
#define DEFAULT_OVERRIDE_DBLCLICK 0
#define DEFAULT_DBLCLICK_OVERRIDE 400

/* Ranges */
#define MIN_DELTA 10
#define MAX_DELTA 500
#define MIN_THRESHOLD_SCALE 0.5
#define MAX_THRESHOLD_SCALE 2.0
#define MIN_DBLCLICK 0
#define MAX_DBLCLICK 2000

typedef struct {
    bool enabled;
    int delta_threshold;
    double threshold_scale_factor;
    bool override_double_click_time;
    int double_click_time_override;
} MouseDamperConfig;

/* Get full path to config.ini in %APPDATA%\mousedamper\config.ini */
bool config_get_file_path (wchar_t *buffer, size_t buffer_size);

/* Create config directory if it doesn't exist */
bool config_ensure_directory (void);

/* Load configuration from INI file (creates with defaults if missing) */
bool config_load (MouseDamperConfig *config);

/* Save configuration to INI file */
bool config_save (const MouseDamperConfig *config);

/* Validate and clamp values to valid ranges */
void config_validate (MouseDamperConfig *config);

/* File monitoring API - no polling! */

/* Start monitoring config file directory for changes
 * Returns a handle that will be signaled when the config file changes
 * Use with RegisterWaitForSingleObject or WaitForSingleObject
 * Returns NULL on failure */
HANDLE config_start_monitoring (void);

/* Reset the monitoring handle to continue watching for changes
 * Call this after handling a change notification
 * Returns true on success */
bool config_reset_monitoring (HANDLE monitor_handle);

/* Stop monitoring and close the handle */
void config_stop_monitoring (HANDLE monitor_handle);

#endif /* MOUSEDAMPER_SETTINGS_BACKEND_H */
