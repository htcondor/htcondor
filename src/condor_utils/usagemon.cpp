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
#include "usagemon.h"

UsageMonitor::~UsageMonitor()
{
	for (UsageRec *rec = first, *next = NULL; rec; rec = next) {
		next = rec->next;
		delete rec;
	}
}

int
UsageMonitor::Request(double units)
{
	UsageRec *rec;

	if (interval == 0) return -1;

	time_t current_time = time(NULL);

	// clean up any old records at the head of the history list
	for (rec = first; rec && rec->timestamp < current_time - interval;
		 first = rec->next, delete rec, rec = first);
	if (first == NULL) last = NULL;

	// special case: units > max_units -- reserve larger window of time
	if (units > max_units) {
		dprintf(D_FULLDEBUG, "usagemon: %.0f > %.0f (units > max_units) "
				"special case\n", units, max_units);
		// wait until our history clears
		if (last) {
			int wait_time = last->timestamp + interval - current_time;
			dprintf(D_FULLDEBUG, "usagemon: request for %.0f must wait %d "
					"seconds\n", units, wait_time);
			return wait_time;
		}
		// okay, history is clear, so grant request
		// set time forward to reserve larger window
		time_t forward_time = current_time +
			(int)(((units/max_units)-1)*interval);
		dprintf(D_FULLDEBUG, "usagemon: request for %.0f forwarded dated by "
				"%ld seconds\n",	units, (long)(forward_time - current_time));
		// add new history record (list is empty!)
		rec = new UsageRec(units, forward_time);
		first = last = rec;
		return 0;
	}

	// first, find usage in the past interval seconds
	double usage_this_interval = 0.0;
	for (rec = first; rec; rec = rec->next) {
		usage_this_interval += rec->units;
	}

	dprintf(D_FULLDEBUG, "usagemon: request=%.0f, history=%.0f, max=%.0f\n",
			units, usage_this_interval, max_units);

	// then, check to see if this request will overflow the maximum
	double overflow = usage_this_interval + units - max_units;
	if (overflow <= 0.0) {		// request granted
		// add new history record to the end of the list
		if (last && last->timestamp == current_time) {
			last->units += units;
		} else {
			rec = new UsageRec(units, current_time);
			if (last) {
				last->next = rec;
				last = rec;
			} else {
				first = last = rec;
			}
		}
		return 0;
	}

	// if this request would overflow, find how many seconds it will take for
	// the necessary records to expire before we will allow this request
	usage_this_interval = 0;
	for (rec = first; rec; rec = rec->next) {
		usage_this_interval += rec->units;
		if (usage_this_interval > overflow) {
			int wait_time = rec->timestamp + interval - current_time;
			dprintf(D_FULLDEBUG, "usagemon: request for %.0f must wait %d "
					"seconds\n", units, wait_time);
			return wait_time;
		}
	}

	return -1;					// should never get here
}
