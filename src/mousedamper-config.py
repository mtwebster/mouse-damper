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

import os
import gettext
import sys
import subprocess

from xapp.SettingsWidgets import SettingsWidget, SettingsPage
from xapp.GSettingsWidgets import GSettingsSwitch, GSettingsRange, GSettingsSpinButton
from gi.repository import Gtk, Gio, GLib

_ = gettext.gettext

MOUSEDAMPER_SCHEMA_ID = "org.mtw.mousedamper"
KEY_ENABLED = "enabled"
KEY_DELTA_THRESHOLD = "delta-threshold"
KEY_OVERRIDE_GTK_DOUBLE_CLICK = "override-gtk-double-click-time"
KEY_DOUBLE_CLICK_TIME_OVERRIDE = "double-click-time-override"

gtk_settings = Gtk.Settings.get_default()
system_double_click_time = gtk_settings.get_property("gtk-double-click-time")

class Preferences(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self)
        self.set_title(_("Mousedamper Configuration"))
        self.connect("delete-event", lambda w, e: Gtk.main_quit())
        self.set_size_request(800, -1)
        self.set_default_size(800, -1)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.add(box)

        page = SettingsPage()
        box.pack_start(page, False, False, 0)

        section = page.add_section(_("Mouse Damper"))

        widget = SettingsWidget()

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        widget.pack_start(box, False, False, 0)
        label = Gtk.Label(wrap=True, xalign=0.0, use_markup=True, label=_(
            "Mousedamper helps prevent accidental clicks and drag operations caused by hand tremors "
            "or unsteady mouse movements.\n\n"
            "When you press a mouse button, the pointer is frozen in place until either you complete "
            "a double-click, the double-click timeout expires, or you move the mouse beyond the "
            "breakout threshold. This prevents the pointer from drifting between button-press and "
            "button-release, ensuring you click exactly where you intend."
        ))
        box.pack_start(label, False, False, 0)
        section.add_row(widget)

        section = page.add_section(_("Autostart"))

        switch = GSettingsSwitch(_("Launch mousedamper when the session starts"), MOUSEDAMPER_SCHEMA_ID, KEY_ENABLED)
        section.add_row(switch)

        section = page.add_section(_("Movement"))

        slider = GSettingsRange(_("Breakout threshold"), MOUSEDAMPER_SCHEMA_ID, KEY_DELTA_THRESHOLD,
            _("Lower"), _("Higher"), mini=50, maxi=1000, step=5, show_value=True,
            tooltip=_("The distance the pointer must move while frozen before the freeze is removed and normal movement resumes.")
        )
        slider.content_widget.add_mark(100, Gtk.PositionType.TOP, None)
        section.add_row(slider)

        section = page.add_section(_("Clicks"))

        switch = GSettingsSwitch(_("Override system double-click time (currently %dms)") % system_double_click_time,
            MOUSEDAMPER_SCHEMA_ID, KEY_OVERRIDE_GTK_DOUBLE_CLICK
        )
        section.add_row(switch)

        spinner = GSettingsSpinButton(_("Double-click time"), MOUSEDAMPER_SCHEMA_ID, KEY_DOUBLE_CLICK_TIME_OVERRIDE,
            mini=100, maxi=2000, step=10,
            tooltip=_("The maximum time between clicks to register a double-click, in milliseconds.")
        )
        section.add_reveal_row(spinner, MOUSEDAMPER_SCHEMA_ID, KEY_OVERRIDE_GTK_DOUBLE_CLICK)

        # Monitor GSettings for changes and restart mousedamper when settings change
        self.settings = Gio.Settings(schema_id=MOUSEDAMPER_SCHEMA_ID)
        self.settings.connect("changed", self.on_settings_changed)
        self.restart_timeout_id = 0

        theme = Gtk.IconTheme.get_default()
        if theme.has_icon("mousedamper"):
            self.set_icon_name("mousedamper")

        self.show_all()

    def on_settings_changed(self, settings, key):
        if self.restart_timeout_id != 0:
            GLib.source_remove(self.restart_timeout_id)

        self.restart_timeout_id = GLib.timeout_add(100, self.restart_mousedamper)

    def restart_mousedamper(self):
        subprocess.run(["killall", "mousedamper"], stderr=subprocess.DEVNULL, check=False)

        if self.settings.get_boolean(KEY_ENABLED):
            subprocess.Popen(["mousedamper-launch"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        self.restart_timeout_id = 0
        return GLib.SOURCE_REMOVE

if __name__ == "__main__":
    main = Preferences()

    try:
        Gtk.main()
    except Exception as e:
        print(e)
        exit(0)
