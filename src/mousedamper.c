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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/platform.h"
#include "common/damper_core.h"

#define USEC_IN_MSEC 1000

int
main (int argc, char *argv[])
{
    if (argc != 5) {
        fprintf (stderr, "Usage: %s <verbose|quiet> <double-click-time-ms> <freeze-threshold-px> <threshold-scale>\n", argv[0]);
        return 1;
    }

    bool verbose = strcmp (argv[1], "verbose") == 0;
    int64_t double_click_time_usec = strtoll (argv[2], NULL, 10) * USEC_IN_MSEC;
    int threshold = atoi (argv[3]);
    double threshold_scale = atof (argv[4]);

    printf ("Starting mouse-damper (double-click: %ldms, threshold: %dpx, scale: %.2f)\n",
            (long)(double_click_time_usec / USEC_IN_MSEC),
            threshold,
            threshold_scale);

    const PlatformInterface *platform = platform_get_interface ();

    if (!platform->init (double_click_time_usec, threshold, verbose)) {
        fprintf (stderr, "Platform initialization failed\n");
        return 1;
    }

    /* Set threshold scale factor */
    damper_set_threshold_scale (threshold_scale);

    platform->run ();

    platform->cleanup ();

    printf ("Mouse-damper stopped\n");

    return 0;
}
