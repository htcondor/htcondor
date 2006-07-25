/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
