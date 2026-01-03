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
import sys
import locale
import gettext

# Import version info (installed alongside this script)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from config import VERSION, LOCALE_DIR

from xapp.SettingsWidgets import SettingsWidget, SettingsPage
from gi.repository import Gtk, Gio, GLib

locale.setlocale(locale.LC_ALL, '')
gettext.bindtextdomain('mousedamper', LOCALE_DIR)
gettext.textdomain('mousedamper')
_ = gettext.gettext

MOUSEDAMPER_SCHEMA_ID = "org.mtw.mousedamper"
KEY_ENABLED = "enabled"
KEY_DELTA_THRESHOLD = "delta-threshold"
KEY_OVERRIDE_GTK_DOUBLE_CLICK = "override-gtk-double-click-time"
KEY_DOUBLE_CLICK_TIME_OVERRIDE = "double-click-time-override"

gtk_settings = Gtk.Settings.get_default()
system_double_click_time = gtk_settings.get_property("gtk-double-click-time")

class MouseDamperConfig(Gtk.Application):
    def __init__(self):
        super().__init__(
            application_id="org.mtw.mousedamper.config",
            flags=Gio.ApplicationFlags.FLAGS_NONE
        )

        self.window = None

    def do_startup(self):
        Gtk.Application.do_startup(self)

    def do_activate(self):
        # If window already exists, just present it
        if self.window:
            self.window.present()
            return

        # Create the preferences window
        self.window = Gtk.Window(application=self)
        self.window.set_title(_("Mousedamper Configuration"))
        self.window.set_size_request(800, -1)
        self.window.set_default_size(800, -1)

        # Load current GSettings values
        self.settings = Gio.Settings(schema_id=MOUSEDAMPER_SCHEMA_ID)

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.window.add(box)

        page = SettingsPage()
        box.pack_start(page, False, False, 0)

        section = page.add_section(_("Mouse Damper"))

        widget = SettingsWidget()

        desc_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        widget.pack_start(desc_box, False, False, 0)
        label = Gtk.Label(wrap=True, xalign=0.0, use_markup=True, label=_(
            "Mousedamper helps prevent accidental clicks and drag operations caused by hand tremors "
            "or unsteady mouse movements.\n\n"
            "When you press a mouse button, the pointer is frozen in place until either you complete "
            "a double-click, the double-click timeout expires, or you move the mouse beyond the "
            "breakout threshold. This prevents the pointer from drifting between button-press and "
            "button-release, ensuring you click exactly where you intend."
        ))
        desc_box.pack_start(label, False, False, 0)
        section.add_row(widget)

        section = page.add_section(_("Status"))

        widget = SettingsWidget()
        box_enable = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        widget.pack_start(box_enable, True, True, 0)

        label = Gtk.Label(label=_("Enable mousedamper"), xalign=0.0)
        box_enable.pack_start(label, True, True, 0)

        self.enable_switch = Gtk.Switch()
        self.enable_switch.set_active(self.settings.get_boolean(KEY_ENABLED))
        self.enable_switch.connect("notify::active", self.on_enabled_toggled)
        box_enable.pack_start(self.enable_switch, False, False, 0)

        section.add_row(widget)

        section = page.add_section(_("Movement"))

        widget = SettingsWidget()
        box_threshold = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        widget.pack_start(box_threshold, True, True, 0)

        label = Gtk.Label(label=_("Breakout threshold"), xalign=0.0)
        box_threshold.pack_start(label, False, False, 0)

        threshold_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        box_threshold.pack_start(threshold_box, False, False, 0)

        threshold_box.pack_start(Gtk.Label(label=_("Lower")), False, False, 0)

        self.threshold_scale = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 10, 500, 5)
        self.threshold_scale.set_value(self.settings.get_int(KEY_DELTA_THRESHOLD))
        self.threshold_scale.set_draw_value(True)
        self.threshold_scale.set_value_pos(Gtk.PositionType.RIGHT)
        self.threshold_scale.add_mark(100, Gtk.PositionType.TOP, None)
        self.threshold_scale.connect("value-changed", self.on_setting_changed)
        threshold_box.pack_start(self.threshold_scale, True, True, 0)

        threshold_box.pack_start(Gtk.Label(label=_("Higher")), False, False, 0)

        tooltip_label = Gtk.Label(
            label=_("The distance the pointer must move while frozen before the freeze is removed and normal movement resumes."),
            wrap=True,
            xalign=0.0
        )
        tooltip_label.get_style_context().add_class("dim-label")
        box_threshold.pack_start(tooltip_label, False, False, 0)

        section.add_row(widget)

        section = page.add_section(_("Clicks"))

        widget = SettingsWidget()
        box_override = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        widget.pack_start(box_override, True, True, 0)

        label = Gtk.Label(
            label=_("Override system double-click time (currently %dms)") % system_double_click_time,
            xalign=0.0
        )
        box_override.pack_start(label, True, True, 0)

        self.override_switch = Gtk.Switch()
        self.override_switch.set_active(self.settings.get_boolean(KEY_OVERRIDE_GTK_DOUBLE_CLICK))
        self.override_switch.connect("notify::active", self.on_override_toggled)
        self.override_switch.connect("notify::active", self.on_setting_changed)
        box_override.pack_start(self.override_switch, False, False, 0)

        section.add_row(widget)

        # Double-click time spinner (revealed when override is on)
        widget = SettingsWidget()
        self.dblclick_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
        widget.pack_start(self.dblclick_box, True, True, 0)

        label = Gtk.Label(label=_("Double-click time"), xalign=0.0)
        self.dblclick_box.pack_start(label, True, True, 0)

        self.dblclick_spin = Gtk.SpinButton.new_with_range(100, 2000, 10)
        self.dblclick_spin.set_value(self.settings.get_int(KEY_DOUBLE_CLICK_TIME_OVERRIDE))
        self.dblclick_spin.connect("value-changed", self.on_setting_changed)
        self.dblclick_box.pack_start(self.dblclick_spin, False, False, 0)

        tooltip_label = Gtk.Label(
            label=_("The maximum time between clicks to register a double-click, in milliseconds."),
            wrap=True,
            xalign=0.0
        )
        tooltip_label.get_style_context().add_class("dim-label")
        self.dblclick_box.pack_start(tooltip_label, False, False, 0)

        self.dblclick_revealer = Gtk.Revealer()
        self.dblclick_revealer.set_transition_type(Gtk.RevealerTransitionType.SLIDE_DOWN)
        self.dblclick_revealer.add(widget)
        self.dblclick_revealer.set_reveal_child(self.override_switch.get_active())
        section.add_row(self.dblclick_revealer)

        self.apply_button = Gtk.Button(label=_("Save changes"))
        self.apply_button.set_tooltip_text(_("Apply settings and restart the daemon"))
        self.apply_button.set_sensitive(False)  # Disabled until changes are made
        self.apply_button.connect("clicked", self.on_apply_settings)
        page.pack_end(self.apply_button, False, False, 0)

        self.window.set_icon_name("mousedamper")

        self.window.show_all()

    def on_override_toggled(self, switch, gparam):
        self.dblclick_revealer.set_reveal_child(switch.get_active())

    def on_enabled_toggled(self, switch, gparam):
        # Enabled switch applies immediately (like tray menu)
        self.settings.set_boolean(KEY_ENABLED, switch.get_active())

    def on_setting_changed(self, widget, *args):
        # Enable Apply button when any setting that requires restart is changed
        self.apply_button.set_sensitive(True)

    def on_apply_settings(self, button):
        # Write non-enabled settings to GSettings
        # (Enabled is already applied immediately via on_enabled_toggled)
        self.settings.set_int(KEY_DELTA_THRESHOLD, int(self.threshold_scale.get_value()))
        self.settings.set_boolean(KEY_OVERRIDE_GTK_DOUBLE_CLICK, self.override_switch.get_active())
        self.settings.set_int(KEY_DOUBLE_CLICK_TIME_OVERRIDE, int(self.dblclick_spin.get_value()))

        button.set_sensitive(False)


if __name__ == "__main__":
    app = MouseDamperConfig()
    exit_status = app.run(sys.argv)
    sys.exit(exit_status)
