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

/*
** Events are initialized with records in the config file in the following
** forms:
**   SHUTDOWN DAY TIME DURATION BANDWIDTH CONSTRAINT RANK
**     example: SHUTDOWN MTWRFSU 04:00 3600 5 MyConst MyRank
**     example: SHUTDOWN MTWRF 23:00 3600 20 MyConst MyRank
**     notes: BANDWIDTH is in Mb/s, CONSTRAINT is on startd ads
*/

#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_classad.h"

enum ScheduledEventType { SHUTDOWN };

class ScheduledEvent : public Service {
public:
	ScheduledEvent(const char name[], const char record[], int &offset);
	~ScheduledEvent();

	// returns the name for this event
	char *Name() { return id; }

	// absolute time of next event occurrence, up to 24 hours in advance
	time_t TimeOfEvent();

	// seconds until event deadline is next scheduled to occur 
	// a maximum of 24 hours is returned
	int TimeToEvent();

	// (estimated) seconds required to complete this event
	// this depends on the type of event, and is often computed dynamically
	// based on the current state of the system
	virtual int TimeNeeded() = 0;

	// begin processing for this event
	virtual int ActivateEvent();
	virtual int DeactivateEvent() { active = false; DeactivateTid = -1; };
	bool IsActive() { return active; }
	bool IsValid() { return valid; }

protected:
	char *id;
	int daymask : 7;
	int timeofday;
	bool active;
	int DeactivateTid;
	time_t EndTime;
	int duration;
	ScheduledEventType type;
	bool valid;
};

class ScheduledShutdownEvent : public ScheduledEvent {
public:
	ScheduledShutdownEvent(const char name[], const char record[],
						   int offset=0);
	~ScheduledShutdownEvent();

	virtual int TimeNeeded();
	virtual int ActivateEvent();

	int Timeout();
	
private:
	int GetStartdList();

	// put a startd in shutdown mode using DC_CONFIG_PERSIST
	int EnterShutdownMode(const char startd_name[], const char startd_addr[]);
	// clean up config when a startd is no longer in shutdown mode
	int CleanupShutdownModeConfig(const char startd_name[],
								  const char startd_addr[]);

	ClassAdList *StartdList;
	char *constraint;
	ExprTree *rank;
	int TimeoutTid;
	int DeactivateTid;
	int SlowStartPos;
	double bandwidth;
	static int SlowStartInterval;
	static int EventInterval;
};
