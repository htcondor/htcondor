/**
 * Copyright 2015 University of Southern California
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "common.h"

char *timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    time_t t = tv.tv_sec;

    struct tm tm;
    localtime_r(&t, &tm);

    static char ts[64];
    snprintf(ts, 63, "%04d-%02d-%02d %02d:%02d:%02d.%06d",
            1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, (int)tv.tv_usec);

    return ts;
}

