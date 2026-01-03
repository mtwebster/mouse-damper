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
#include <glib-unix.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define USEC_IN_SEC 1000000

typedef struct {
    struct libevdev *input_device;
    struct libevdev_uinput *output_device;
    DamperState state;
    gint fd;
    GIOChannel *channel;
    guint watch_id;
    gchar *output_devnode;
} MouseDevice;

static GMainLoop *main_loop = NULL;
static GPtrArray *mouse_devices = NULL;

static PlatformButton
translate_button_code (guint code)
{
    switch (code) {
        case BTN_LEFT:
            return PLATFORM_BUTTON_LEFT;
        case BTN_RIGHT:
            return PLATFORM_BUTTON_RIGHT;
        case BTN_MIDDLE:
            return PLATFORM_BUTTON_MIDDLE;
        default:
            return PLATFORM_BUTTON_LEFT;
    }
}

static gboolean
device_event_callback (GIOChannel *source, GIOCondition condition, gpointer user_data)
{
    MouseDevice *device = user_data;
    struct input_event ev;
    int rc;

    if (condition & (G_IO_HUP | G_IO_ERR)) {
        g_warning ("Device disconnected or error occurred");
        return G_SOURCE_REMOVE;
    }

    do {
        rc = libevdev_next_event (device->input_device, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
            PlatformEvent platform_ev;
            PlatformAction action = PLATFORM_ACTION_PASS;
            gboolean is_handled = FALSE;

            if (ev.type == EV_KEY &&
                (ev.code == BTN_LEFT || ev.code == BTN_RIGHT || ev.code == BTN_MIDDLE)) {
                platform_ev.type = (ev.value == 1) ? PLATFORM_EVENT_BUTTON_PRESS : PLATFORM_EVENT_BUTTON_RELEASE;
                platform_ev.timestamp_usec = ((gint64)ev.time.tv_sec * USEC_IN_SEC) + ev.time.tv_usec;
                platform_ev.data.button.button = translate_button_code (ev.code);

                action = damper_handle_event (&device->state, &platform_ev);
                is_handled = TRUE;
            } else if (ev.type == EV_REL && (ev.code == REL_X || ev.code == REL_Y)) {
                platform_ev.type = PLATFORM_EVENT_MOTION;
                platform_ev.timestamp_usec = ((gint64)ev.time.tv_sec * USEC_IN_SEC) + ev.time.tv_usec;
                platform_ev.data.motion.dx = (ev.code == REL_X) ? ev.value : 0;
                platform_ev.data.motion.dy = (ev.code == REL_Y) ? ev.value : 0;

                action = damper_handle_event (&device->state, &platform_ev);
                is_handled = TRUE;
            }

            if (is_handled && action == PLATFORM_ACTION_DROP) {
                continue;
            }

            libevdev_uinput_write_event (device->output_device, ev.type, ev.code, ev.value);
            if (ev.type == EV_SYN)
                libevdev_uinput_write_event (device->output_device, EV_SYN, SYN_REPORT, 0);

        } else if (rc == LIBEVDEV_READ_STATUS_SYNC) {
            g_warning ("Events dropped, resyncing");
            while (rc == LIBEVDEV_READ_STATUS_SYNC) {
                rc = libevdev_next_event (device->input_device, LIBEVDEV_READ_FLAG_SYNC, &ev);
                if (rc == LIBEVDEV_READ_STATUS_SYNC || rc == LIBEVDEV_READ_STATUS_SUCCESS) {
                    libevdev_uinput_write_event (device->output_device, ev.type, ev.code, ev.value);
                    if (ev.type == EV_SYN)
                        libevdev_uinput_write_event (device->output_device, EV_SYN, SYN_REPORT, 0);
                }
            }
        }
    } while (rc == LIBEVDEV_READ_STATUS_SUCCESS);

    return G_SOURCE_CONTINUE;
}

static void
mouse_device_free (MouseDevice *device)
{
    if (device->watch_id > 0)
        g_source_remove (device->watch_id);

    if (device->channel) {
        g_io_channel_shutdown (device->channel, FALSE, NULL);
        g_io_channel_unref (device->channel);
    }

    if (device->output_device)
        libevdev_uinput_destroy (device->output_device);

    if (device->input_device)
        libevdev_free (device->input_device);

    if (device->fd >= 0)
        close (device->fd);

    g_free (device->output_devnode);
    g_free (device);
}

static MouseDevice *
create_mouse_device (const gchar *device_path)
{
    MouseDevice *device;
    int rc;

    device = g_new0 (MouseDevice, 1);
    device->fd = -1;

    device->fd = open (device_path, O_RDONLY | O_NONBLOCK);
    if (device->fd < 0) {
        g_warning ("Failed to open %s: %s", device_path, strerror (errno));
        mouse_device_free (device);
        return NULL;
    }

    rc = libevdev_new_from_fd (device->fd, &device->input_device);
    if (rc < 0) {
        g_warning ("Failed to initialize libevdev for %s: %s", device_path, strerror (-rc));
        mouse_device_free (device);
        return NULL;
    }

    rc = libevdev_uinput_create_from_device (device->input_device,
                                             LIBEVDEV_UINPUT_OPEN_MANAGED,
                                             &device->output_device);
    if (rc < 0) {
        g_warning ("Failed to create uinput device for %s: %s", device_path, strerror (-rc));
        mouse_device_free (device);
        return NULL;
    }

    device->output_devnode = g_strdup (libevdev_uinput_get_devnode (device->output_device));

    g_print ("Device init for %s: redirected from %s to %s\n",
             libevdev_get_name (device->input_device),
             device_path,
             device->output_devnode);

    damper_state_init (&device->state);

    rc = libevdev_grab (device->input_device, LIBEVDEV_GRAB);
    if (rc < 0) {
        g_warning ("Failed to grab device %s: %s", device_path, strerror (-rc));
        mouse_device_free (device);
        return NULL;
    }

    device->channel = g_io_channel_unix_new (device->fd);
    g_io_channel_set_encoding (device->channel, NULL, NULL);
    g_io_channel_set_buffered (device->channel, FALSE);

    device->watch_id = g_io_add_watch (device->channel,
                                       G_IO_IN | G_IO_HUP | G_IO_ERR,
                                       device_event_callback,
                                       device);

    return device;
}

static gboolean
output_devnode_equal (gconstpointer a, gconstpointer b)
{
    const MouseDevice *device = a;
    const gchar *path = b;
    return g_strcmp0 (device->output_devnode, path) == 0;
}

static void
discover_mouse_devices (void)
{
    gint fd_code = 0;

    while (TRUE) {
        gchar *device_path = g_strdup_printf ("/dev/input/event%d", fd_code);
        gint fd = open (device_path, O_RDONLY);

        if (fd < 0) {
            g_free (device_path);
            break;
        }

        struct libevdev *dev = NULL;
        int rc = libevdev_new_from_fd (fd, &dev);

        if (rc >= 0) {
            guint idx;
            gboolean already_handled = g_ptr_array_find_with_equal_func (mouse_devices,
                                                                          device_path,
                                                                          output_devnode_equal,
                                                                          &idx);

            if (already_handled) {
                if (damper_verbose)
                    g_print ("Device at %s is our own virtual device, skipping\n", device_path);
            } else if (libevdev_has_event_type (dev, EV_KEY) &&
                       libevdev_has_event_code (dev, EV_KEY, BTN_LEFT)) {
                if (damper_verbose)
                    g_print ("Device at %s is a mouse\n", device_path);

                MouseDevice *mouse_device = create_mouse_device (device_path);
                if (mouse_device)
                    g_ptr_array_add (mouse_devices, mouse_device);
            } else {
                if (damper_verbose)
                    g_print ("Device at %s is NOT a mouse\n", device_path);
            }
            libevdev_free (dev);
        }

        close (fd);
        g_free (device_path);
        fd_code++;
    }
}

static gboolean
signal_handler (gpointer user_data)
{
    g_print ("Received signal, shutting down...\n");
    g_main_loop_quit (main_loop);
    return G_SOURCE_REMOVE;
}

static bool
platform_linux_init (int64_t double_click_time_usec, int threshold_px, bool verbose)
{
    damper_double_click_wait_time = double_click_time_usec;
    damper_button_freeze_delta_threshold = threshold_px;
    damper_verbose = verbose;

    mouse_devices = g_ptr_array_new_with_free_func ((GDestroyNotify) mouse_device_free);

    discover_mouse_devices ();

    if (mouse_devices->len == 0) {
        g_printerr ("No mouse devices found\n");
        g_ptr_array_unref (mouse_devices);
        return false;
    }

    g_print ("Starting filters for %u device(s)\n", mouse_devices->len);

    return true;
}

static void
platform_linux_run (void)
{
    main_loop = g_main_loop_new (NULL, FALSE);

    g_unix_signal_add (SIGINT, signal_handler, NULL);
    g_unix_signal_add (SIGTERM, signal_handler, NULL);

    g_main_loop_run (main_loop);

    g_main_loop_unref (main_loop);
}

static void
platform_linux_cleanup (void)
{
    g_ptr_array_unref (mouse_devices);
}

const PlatformInterface *
platform_get_interface (void)
{
    static const PlatformInterface linux_platform = {
        .init = platform_linux_init,
        .run = platform_linux_run,
        .cleanup = platform_linux_cleanup
    };
    return &linux_platform;
}
