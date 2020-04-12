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

from xapp.SettingsWidgets import SettingsWidget, SettingsPage, Range
from gi.repository import Gtk, Gio

_ = gettext.gettext

gtk_settings = Gtk.Settings.get_default()
system_double_click_time = gtk_settings.get_property("gtk-double-click-time")

settings = Gio.Settings(schema_id="org.mtw.mouse-damper")
delta_val = settings.get_int("delta-threshold")
user_double_click_time = settings.get_int("double-click-time-override")
double_click_time = max(system_double_click_time, user_double_click_time)

class Preferences(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self)
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
            "<b>What this does</b>\n\n"
            "This program is intended to make mouse click actions a bit more forgiving for "
            "people suffering from essential tremor or other similar afflictions. Often when "
            "attempting to click items on the screen, due to the fact that many actions are only "
            "triggered on button-<i>release</i>, one can end up performing unintended actions if "
            "the mouse position moves from its original position between button-press and -release. "
            "This can range from accidentally invoking a drag operation to activating some adjacent "
            "element to the one you had originally intended. The toolkit libraries that "
            "graphical applications use (GTK, QT) have some preventatives for unintentional mouse "
            "movement, however they are of limited effectiveness, and do nothing to prevent mis-targeting "
            "on a button-release.\n\n"
            "The way these issues are addressed by this program is the pointer is frozen in place when "
            "a button is pressed. At this point, two things begin to be tracked - elapsed time and "
            "physical mouse movement. The freeze is removed when one of the following three events "
            "takes place:\n\n"
            "• The user proceeds to complete a double-click (a click, release, and second click within "
            "a certain duration).\n"
            "• The double-click timeout is exceeded.\n"
            "• The physical mouse movement from the freeze point exceeds a configurable threshold.\n\n"
            "While this is by no means a perfect solution, it at least allows the user to act on their "
            "intended target, without being affected by uncontrollable physical movement."
        ))
        box.pack_start(label, False, False, 0)
        section.add_row(widget)

        section = page.add_section(_("Movement threshold"))

        widget = SettingsWidget()

        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        widget.pack_start(box, False, False, 0)
        label = Gtk.Label(wrap=True, xalign=0.0, use_markup=True, label=_(
            "<b>Distance at which the mouse will become unfrozen.</b>\n\n"
            "This is the straight-line distance from the initial click point "
            "- the value does not simply accumulate all movement"
        ))
        box.pack_start(label, False, False, 0)
        section.add_row(widget)

        widget = KeyfileRange("",
                              mini=50, maxi=1000, step=10, key="delta-threshold",
                              show_value=True)

        section.add_row(widget)

        self.show_all()

class KeyfileRange(Range):

    def __init__(self, *args, **kargs):
        self.key = kargs.pop("key")

        super(KeyfileRange, self).__init__(*args, **kargs)

        self.set_spacing(6)
        self.label.hide()
        self.label.set_no_show_all(True)
        self.settings = settings

        self.content_widget.set_value(self.settings.get_int(self.key))

    def set_value(self, value):
        pass

    def get_value(self):
        pass

    def get_range(self):
        pass

    def apply_later(self, *args):
        self.settings.set_int(self.key, self.content_widget.get_value())

if __name__ == "__main__":
    main = Preferences()

    try:
        Gtk.main()
    except Exception as e:
        print(e)
        exit(0)
