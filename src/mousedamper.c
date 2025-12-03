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

#include <glib.h>
#include <glib-unix.h>
#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#define USEC_IN_MSEC 1000
#define USEC_IN_SEC (USEC_IN_MSEC * 1000)

static gboolean verbose = FALSE;
static gint64 double_click_wait_time = 0;
static gint button_freeze_delta_threshold = 0;

typedef struct {
    gint64 button_freeze_time;
    gboolean first_down;
    gboolean second_down;
    gboolean motion_frozen;
    gint x_freeze_delta;
    gint y_freeze_delta;
} DeviceState;

typedef struct {
    struct libevdev *input_device;
    struct libevdev_uinput *output_device;
    DeviceState state;
    gint fd;
    GIOChannel *channel;
    guint watch_id;
    gchar *output_devnode;
} MouseDevice;

static GMainLoop *main_loop = NULL;
static GPtrArray *mouse_devices = NULL;

static void
device_state_reset (DeviceState *state)
{
    state->button_freeze_time = 0;
    state->first_down = FALSE;
    state->second_down = FALSE;
    state->motion_frozen = FALSE;
    state->x_freeze_delta = 0;
    state->y_freeze_delta = 0;
}

static void
log_message (const gchar *format, ...)
{
    if (!verbose)
        return;

    va_list args;
    va_start (args, format);
    g_vprintf (format, args);
    g_print ("\n");
    va_end (args);
}

static gboolean
handle_button_event (MouseDevice *device, struct input_event *event)
{
    if (event->code != BTN_LEFT && event->code != BTN_RIGHT && event->code != BTN_MIDDLE)
        return FALSE;

    gint64 etime = ((gint64)event->time.tv_sec * USEC_IN_SEC) + event->time.tv_usec;

    if (event->value == 1) {
        log_message ("Button press");
        if (!device->state.first_down) {
            log_message ("First down");
            device->state.motion_frozen = TRUE;
            device->state.first_down = TRUE;
            device->state.button_freeze_time = etime;
        } else {
            log_message ("Second down");
            device->state.second_down = TRUE;
        }
    } else if (event->value == 0) {
        log_message ("Button release");
        if ((etime - device->state.button_freeze_time) > double_click_wait_time ||
            device->state.second_down) {
            log_message ("Exceeded wait time or releasing second press, resetting.");
            device_state_reset (&device->state);
        }
    }

    libevdev_uinput_write_event (device->output_device, event->type, event->code, event->value);
    libevdev_uinput_write_event (device->output_device, EV_SYN, SYN_REPORT, 0);
    return TRUE;
}

static gboolean
handle_motion_event (MouseDevice *device, struct input_event *event)
{
    if (event->code != REL_X && event->code != REL_Y)
        return FALSE;

    gint64 etime = ((gint64)event->time.tv_sec * USEC_IN_SEC) + event->time.tv_usec;

    if (device->state.motion_frozen) {
        if (event->code == REL_X)
            device->state.x_freeze_delta += event->value;
        else
            device->state.y_freeze_delta += event->value;

        log_message ("Deltas: %d, %d", device->state.x_freeze_delta, device->state.y_freeze_delta);

        gint64 elapsed = etime - device->state.button_freeze_time;
        gdouble real_move = hypot (device->state.x_freeze_delta, device->state.y_freeze_delta);
        gboolean within_time = elapsed < double_click_wait_time;

        if (real_move > button_freeze_delta_threshold || !within_time) {
            log_message ("Thresholds reached, resetting (%dpx > %dpx, %ldms > %ldms)",
                        (int)real_move, button_freeze_delta_threshold,
                        elapsed / USEC_IN_MSEC, double_click_wait_time / USEC_IN_MSEC);
            device_state_reset (&device->state);
        } else {
            log_message ("Skipping event, thresholds not reached (%dpx < %dpx, %ldms < %ldms)",
                        (int)real_move, button_freeze_delta_threshold,
                        elapsed / USEC_IN_MSEC, double_click_wait_time / USEC_IN_MSEC);
            return TRUE;
        }
    }

    libevdev_uinput_write_event (device->output_device, event->type, event->code, event->value);
    libevdev_uinput_write_event (device->output_device, EV_SYN, SYN_REPORT, 0);
    return TRUE;
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
            if (!handle_button_event (device, &ev) &&
                !handle_motion_event (device, &ev)) {
                libevdev_uinput_write_event (device->output_device, ev.type, ev.code, ev.value);
                if (ev.type == EV_SYN)
                    libevdev_uinput_write_event (device->output_device, EV_SYN, SYN_REPORT, 0);
            }
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

    device_state_reset (&device->state);

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
                log_message ("Device at %s is our own virtual device, skipping", device_path);
            } else if (libevdev_has_event_type (dev, EV_KEY) &&
                       libevdev_has_event_code (dev, EV_KEY, BTN_LEFT)) {
                log_message ("Device at %s is a mouse", device_path);

                MouseDevice *mouse_device = create_mouse_device (device_path);
                if (mouse_device)
                    g_ptr_array_add (mouse_devices, mouse_device);
            } else {
                log_message ("Device at %s is NOT a mouse", device_path);
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

int
main (int argc, char *argv[])
{
    if (argc != 4) {
        g_printerr ("Usage: %s <verbose|quiet> <double-click-time-ms> <freeze-threshold-px>\n", argv[0]);
        return 1;
    }

    verbose = g_strcmp0 (argv[1], "verbose") == 0;
    double_click_wait_time = g_ascii_strtoll (argv[2], NULL, 10) * USEC_IN_MSEC;
    button_freeze_delta_threshold = atoi (argv[3]);

    g_print ("Starting mouse-damper (double-click: %ldms, threshold: %dpx)\n",
             double_click_wait_time / USEC_IN_MSEC,
             button_freeze_delta_threshold);

    mouse_devices = g_ptr_array_new_with_free_func ((GDestroyNotify) mouse_device_free);

    discover_mouse_devices ();

    if (mouse_devices->len == 0) {
        g_printerr ("No mouse devices found\n");
        g_ptr_array_unref (mouse_devices);
        return 1;
    }

    g_print ("Starting filters for %u device(s)\n", mouse_devices->len);

    main_loop = g_main_loop_new (NULL, FALSE);

    g_unix_signal_add (SIGINT, signal_handler, NULL);
    g_unix_signal_add (SIGTERM, signal_handler, NULL);

    g_main_loop_run (main_loop);

    g_main_loop_unref (main_loop);
    g_ptr_array_unref (mouse_devices);

    g_print ("Mouse-damper stopped\n");

    return 0;
}
