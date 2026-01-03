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

#ifndef MOUSEDAMPER_LAUNCHER_RESOURCE_H
#define MOUSEDAMPER_LAUNCHER_RESOURCE_H

/* Icon resource ID */
#define IDI_MOUSEDAMPER            100

/* Menu resource ID */
#define IDR_TRAY_MENU              200

/* Menu item IDs */
#define IDM_ENABLE                 2001
#define IDM_DISABLE                2002
#define IDM_CONFIGURE              2003
#define IDM_ABOUT                  2004
#define IDM_QUIT                   2005

/* Dialog IDs */
#define IDD_ABOUT_DIALOG           201

/* Control IDs */
#define IDC_ABOUT_TITLE            2101
#define IDC_ABOUT_VERSION          2102
#define IDC_ABOUT_DESCRIPTION      2103
#define IDC_ABOUT_DESCRIPTION2     2104
#define IDC_ABOUT_COPYRIGHT        2105
#define IDC_ABOUT_LICENSE          2106

/* Timer IDs */
#define IDT_CONFIG_CHECK           3001

/* Custom window messages */
#define WM_TRAYICON                (WM_APP + 1)
#define WM_DAEMON_EXITED           (WM_APP + 2)
#define WM_CONFIG_CHANGED          (WM_APP + 3)

#endif /* MOUSEDAMPER_LAUNCHER_RESOURCE_H */
