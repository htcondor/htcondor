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

#ifndef __USAGEMON_H__
#define __USAGEMON_H__

/*
** The UsageMonitor grants and denies requests for units of a resource
** such that a configured maximum amount is not exceeded over a configured
** time interval.
*/

class UsageMonitor {
public:
	UsageMonitor() : max_units(0.0), interval(0), first(NULL), last(NULL) {}
	~UsageMonitor();

	UsageMonitor(double Units, int Interval) // max. units in Interval seconds
	{ UsageMonitor(); SetMax(Units, Interval); }
	void SetMax(double Units, int Interval) // max. units in Interval seconds
	{ max_units = Units; interval = Interval; }

	int Request(double units);	// returns 0 if request is granted; -1 if
								// an error occurred; and the number of seconds
								// before this request can be granted otherwise
private:
	struct UsageRec {
		UsageRec() : units(0.0), timestamp(0), next(NULL) {}
		UsageRec(double u, time_t t) : units(u), timestamp(t), next(NULL) {}
		double units; time_t timestamp;
		UsageRec *next;
	};

	double max_units;
	int interval;
	UsageRec *first, *last;		// ordered history of UsageRecs
};

#endif
