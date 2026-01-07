#!/usr/bin/env python3

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Copyright 2020 Michael Webster <miketwebster@gmail.com>

import sys
import os
import subprocess
import locale
import gettext

# Import config (installed alongside this script)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import VERSION, DAEMON_EXEC, CONFIG_EXEC, LOCALE_DIR

import gi
gi.require_version("Gtk", "3.0")
gi.require_version("XApp", "1.0")
from gi.repository import Gtk, Gio, GLib, XApp

locale.setlocale(locale.LC_ALL, '')
gettext.bindtextdomain('mousedamper', LOCALE_DIR)
gettext.textdomain('mousedamper')
_ = gettext.gettext

MOUSEDAMPER_SCHEMA_ID = "org.mtw.mousedamper"
KEY_ENABLED = "enabled"
KEY_DELTA_THRESHOLD = "delta-threshold"
KEY_OVERRIDE_GTK_DOUBLE_CLICK = "override-gtk-double-click-time"
KEY_DOUBLE_CLICK_TIME_OVERRIDE = "double-click-time-override"

class MouseDamperManager(Gtk.Application):
    def __init__(self):
        super().__init__(
            application_id="org.mtw.mousedamper.manager",
            flags=Gio.ApplicationFlags.HANDLES_COMMAND_LINE
        )

        self.verbose = False
        self.add_main_option("verbose", ord("v"), GLib.OptionFlags.NONE,
                            GLib.OptionArg.NONE, "Enable verbose output", None)

    def do_startup(self):
        Gtk.Application.do_startup(self)

        # GSettings monitoring
        self.settings = Gio.Settings(schema_id=MOUSEDAMPER_SCHEMA_ID)
        self.settings.connect("changed", self.on_settings_changed)

        # Process management
        self.daemon_process = None
        self.restart_count = 0
        self.restart_window_start = GLib.get_monotonic_time()
        self.restart_timeout_id = 0

        # XAppStatusIcon
        self.status_icon = XApp.StatusIcon()
        self.setup_tray_icon()

    def do_activate(self):
        # Start daemon if enabled
        if self.settings.get_boolean(KEY_ENABLED):
            self.start_daemon()
        self.hold()

    def do_command_line(self, command_line):
        options = command_line.get_options_dict()
        self.verbose = options.contains("verbose")

        self.activate()
        return 0

    def setup_tray_icon(self):
        # Icon and tooltip
        self.status_icon.set_icon_name("mousedamper-symbolic")

        self.update_tooltip()
        self.status_icon.set_visible(True)

        # Left-click: open config UI
        self.status_icon.connect("activate", self.on_activate_icon)

        # Right-click menu
        menu = Gtk.Menu()

        self.enable_item = Gtk.CheckMenuItem.new_with_label(_("Enable"))
        self.enable_item.set_active(self.settings.get_boolean(KEY_ENABLED))
        self.enable_item.connect("toggled", self.on_toggle_enabled)
        menu.append(self.enable_item)

        menu.append(Gtk.SeparatorMenuItem())

        config_item = Gtk.MenuItem.new_with_label(_("Configure..."))
        config_item.connect("activate", self.on_configure)
        menu.append(config_item)

        menu.append(Gtk.SeparatorMenuItem())

        about_item = Gtk.MenuItem.new_with_label(_("About..."))
        about_item.connect("activate", self.on_about)
        menu.append(about_item)

        quit_item = Gtk.MenuItem.new_with_label(_("Quit"))
        quit_item.connect("activate", self.on_quit)
        menu.append(quit_item)

        menu.show_all()
        self.status_icon.set_secondary_menu(menu)

    def start_daemon(self):
        # Kill any existing instances first
        subprocess.run(["killall", "mousedamper"], stderr=subprocess.DEVNULL, check=False)

        # Get configuration
        delta_val = self.settings.get_int(KEY_DELTA_THRESHOLD)
        if self.settings.get_boolean(KEY_OVERRIDE_GTK_DOUBLE_CLICK):
            double_click_time = self.settings.get_int(KEY_DOUBLE_CLICK_TIME_OVERRIDE)
        else:
            double_click_time = Gtk.Settings.get_default().get_property("gtk-double-click-time")

        threshold_scale = 1.0  # Hardcoded per user requirement

        # Build command
        cmd = [
            DAEMON_EXEC,
            "verbose" if self.verbose else "terse",
            str(double_click_time),
            str(delta_val),
            str(threshold_scale)
        ]

        # Launch as subprocess using Gio.Subprocess for async monitoring
        try:
            flags = Gio.SubprocessFlags.NONE
            if not self.verbose:
                flags = Gio.SubprocessFlags.STDOUT_SILENCE | Gio.SubprocessFlags.STDERR_SILENCE

            self.daemon_process = Gio.Subprocess.new(cmd, flags)

            # Monitor for exit asynchronously
            self.daemon_process.wait_async(None, self.on_daemon_exited)

            self.update_tooltip()

            if self.verbose:
                print(f"Started mousedamper daemon: PID {self.daemon_process.get_identifier()}")
        except Exception as e:
            print(f"Failed to start mousedamper: {e}")
            self.send_notification(_("Mouse Damper Error"), f"Failed to start: {e}")
            self.update_tooltip()

    def stop_daemon(self):
        if self.daemon_process:
            try:
                self.daemon_process.force_exit()
                if self.verbose:
                    print("Stopped mousedamper daemon")
            except:
                subprocess.run(["killall", "mousedamper"], stderr=subprocess.DEVNULL, check=False)
            self.daemon_process = None

        self.update_tooltip()

    def on_daemon_exited(self, process, result):
        try:
            process.wait_finish(result)
            exit_code = process.get_exit_status()
        except:
            exit_code = -1

        self.daemon_process = None

        if self.verbose:
            print(f"Mousedamper daemon exited with code {exit_code}")

        if self.settings.get_boolean(KEY_ENABLED):
            # Auto-restart with throttling
            self.handle_daemon_crash()
        else:
            self.update_tooltip()

    def handle_daemon_crash(self):
        current_time = GLib.get_monotonic_time()

        # Reset counter if outside 30-second window (monotonic time is in microseconds)
        if (current_time - self.restart_window_start) > (30 * 1000000):
            self.restart_count = 0
            self.restart_window_start = current_time

        self.restart_count += 1

        if self.restart_count <= 3:
            print(f"Mousedamper crashed, restarting (attempt {self.restart_count}/3)")
            self.send_notification(_("Mouse Damper"), f"Daemon crashed, restarting... ({self.restart_count}/3)")
            self.start_daemon()
        else:
            print("Mousedamper crashed too many times, giving up")
            self.send_notification(_("Mouse Damper Error"), _("Daemon crashed too many times. Please check logs."))
            self.update_tooltip()

    def on_settings_changed(self, settings, key):
        # Restart daemon on any GSettings change
        # Debounce to handle multiple rapid changes
        if self.restart_timeout_id:
            GLib.source_remove(self.restart_timeout_id)

        self.restart_timeout_id = GLib.timeout_add(100, self.restart_daemon_delayed)

    def restart_daemon_delayed(self):
        if self.verbose:
            print("GSettings changed, restarting daemon...")

        self.stop_daemon()

        if self.settings.get_boolean(KEY_ENABLED):
            self.start_daemon()

        # Update menu checkbox
        self.enable_item.set_active(self.settings.get_boolean(KEY_ENABLED))

        self.restart_timeout_id = 0
        return GLib.SOURCE_REMOVE

    def update_tooltip(self):
        if self.daemon_process:
            tooltip = _("Mouse Damper - Active")
        elif self.settings.get_boolean(KEY_ENABLED):
            tooltip = _("Mouse Damper - Starting...")
        else:
            tooltip = _("Mouse Damper - Disabled")

        self.status_icon.set_tooltip_text(tooltip)

    def send_notification(self, title, body):
        notification = Gio.Notification.new(title)
        notification.set_body(body)
        notification.set_priority(Gio.NotificationPriority.NORMAL)
        Gio.Application.send_notification(self, None, notification)

    def on_activate_icon(self, icon, button, time):
        # Left-click: open config UI
        self.on_configure(None)

    def on_toggle_enabled(self, item):
        enabled = item.get_active()
        self.settings.set_boolean(KEY_ENABLED, enabled)

    def on_configure(self, item):
        subprocess.Popen([CONFIG_EXEC])

    def on_about(self, item):
        dialog = Gtk.AboutDialog()
        dialog.set_program_name(_("Mouse Damper"))
        dialog.set_version(VERSION)
        dialog.set_comments(_("Prevents unintended mouse movements during clicks"))
        dialog.set_copyright(_("Copyright 2020 Michael Webster"))
        dialog.set_license_type(Gtk.License.GPL_3_0)
        dialog.run()
        dialog.destroy()

    def on_quit(self, item):
        if self.verbose:
            print("Quitting manager...")
        self.stop_daemon()
        self.quit()


if __name__ == "__main__":
    app = MouseDamperManager()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
