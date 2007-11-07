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
#include "condor_debug.h"
#include "gtodc.h"

/* This is a gettimeofday cache. The reason it is here and not in image.[Ch]
	is because of C/C++ ABI headaches. */

GTOD_Cache _condor_global_gtodc_object;
GTOD_Cache *_condor_global_gtodc = &_condor_global_gtodc_object;

static void _condor_stabalize_timeval(struct timeval *t);

void 
_condor_gtodc_init(GTOD_Cache *gtodc)
{
	gtodc->initialized = FALSE;

	gtodc->exe_machine.tv_sec = 0;
	gtodc->exe_machine.tv_usec = 0;

	gtodc->sub_machine.tv_sec = 0;
	gtodc->sub_machine.tv_usec = 0;
}

int 
_condor_gtodc_is_initialized(GTOD_Cache *gtodc)
{
	return gtodc->initialized;
}

void
_condor_gtodc_set_initialized(GTOD_Cache *gtodc, int truth)
{
	gtodc->initialized = truth;
}

void
_condor_gtodc_set_now(GTOD_Cache *gtodc, struct timeval *exe_machine, 
				struct timeval *sub_machine)
{
	gtodc->exe_machine.tv_sec = exe_machine->tv_sec;
	gtodc->exe_machine.tv_usec = exe_machine->tv_usec;

	gtodc->sub_machine.tv_sec = sub_machine->tv_sec;
	gtodc->sub_machine.tv_usec = sub_machine->tv_usec;
}

/* compute the time difference between the raw time and the initial "now"
	from the execute machine, then add that time differential to the "now"
	from the submit machine and store the results into the cooked 
	timeval. This function assumes the cache has been prepared.
*/
void
_condor_gtodc_synchronize(GTOD_Cache *gtodc, struct timeval *exe_machine_cooked, 
						struct timeval *exe_machine_raw)
{
	double raw_exe_sec;
	double sub_sec;
	double old_exe_sec;
	double cooked_sec;
	double factor = 1000000.0;
	double integral, fractional;

	_condor_stabalize_timeval(exe_machine_raw);
	_condor_stabalize_timeval(&gtodc->exe_machine);
	_condor_stabalize_timeval(&gtodc->sub_machine);

	raw_exe_sec = exe_machine_raw->tv_sec + 	
					(exe_machine_raw->tv_usec / factor);
	
	sub_sec = gtodc->sub_machine.tv_sec + 	
					(gtodc->sub_machine.tv_usec / factor);

	old_exe_sec = gtodc->exe_machine.tv_sec + 	
					(gtodc->exe_machine.tv_usec / factor);
	
	/* compute the time difference wrt the submit machine */
	cooked_sec = sub_sec + (raw_exe_sec - old_exe_sec);

	fractional = modf(cooked_sec, &integral);

	exe_machine_cooked->tv_sec = (long)integral;
	exe_machine_cooked->tv_usec = (long)(fractional * factor);

	/* in case something went wrong in the math, at least provide in bounds 
		values to the user */
	_condor_stabalize_timeval(exe_machine_cooked);
}


/* just incase someboday does something funny and the values end up negative
	or out of bounds */
void 
_condor_stabalize_timeval(struct timeval *t)
{
	while (t->tv_usec < 0) {
		t->tv_sec--;
		t->tv_usec += 1000000L;
	}

	if (t->tv_usec >= 1000000L) {
		t->tv_sec += (t->tv_usec)/1000000L;
		t->tv_usec %= 1000000L;
	}
}


