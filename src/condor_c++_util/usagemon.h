/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
