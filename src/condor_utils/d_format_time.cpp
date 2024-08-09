/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"

#include <tuple>

extern "C" {

char *
d_format_time( double dsecs )
{
        int days, hours, minutes, secs;
        static char answer[25];

#define SECONDS 1
#define MINUTES (60 * SECONDS)
#define HOURS   (60 * MINUTES)
#define DAYS    (24 * HOURS)

        secs = (int)dsecs;

        days = secs / DAYS;
        secs %= DAYS;

        hours = secs / HOURS;
        secs %= HOURS;

        minutes = secs / MINUTES;
        secs %= MINUTES;

		std::ignore = snprintf(answer, sizeof(answer), "%d %02d:%02d:%02d", days, hours, minutes, secs);

        return( answer );
}

}
