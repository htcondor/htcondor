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

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_query.h"
#include "get_daemon_addr.h"
#include "condor_config.h"

#include "scheduled_event.h"

ScheduledEvent::ScheduledEvent(const char name[], const char record[], int &i)
	: id(NULL), daymask(0), timeofday(-1), active(false), DeactivateTid(-1)
{
	// copy name
	id = strdup(name);

	while(isspace(record[i])) i++; // skip leading whitespace

	// scan the DAY section of the record
	for (; !isspace(record[i]); i++) {
		switch (record[i]) {
		case 'U': case 'u':
			daymask |= 1<<0;
			break;
		case 'M': case 'm':
			daymask |= 1<<1;
			break;
		case 'T': case 't':
			daymask |= 1<<2;
			break;
		case 'W': case 'w':
			daymask |= 1<<3;
			break;
		case 'R': case 'r':
			daymask |= 1<<4;
			break;
		case 'F': case 'f':
			daymask |= 1<<5;
			break;
		case 'S': case 's':
			daymask |= 1<<6;
			break;
		case '\0':
			dprintf(D_ALWAYS, "Error parsing day field of event record.\n");
			dprintf(D_ALWAYS, "Record \"%s\" terminates prematurely.\n", name);
			i = 0;
			return;
		default:
			dprintf(D_ALWAYS, "Error parsing day field of event record.\n");
			dprintf(D_ALWAYS, "Offending character is %c\n", record[i]);
		}
	}

	while(isspace(record[++i])); // skip the whitespace

	// read the TIME section of the record
	errno = 0;
	int hour = atoi(record+i);
	if (errno) {
		dprintf(D_ALWAYS,
				"Error parsing time field (hour) of event record.\n");
		i = 0;
		return;
	}
	while(record[i++] != ':') {	// skip the ':'
		if (record[i] == '\0') {
			dprintf(D_ALWAYS, "Error parsing time field of event record "
					"\"%s\".  No ':' found before end of string.\n", name);
			i = 0;
			return;
		}
	}
	errno = 0;
	int minute = atoi(record+i);
	if (errno) {
		dprintf(D_ALWAYS,
				"Error parsing time field (minute) of event record.\n");
		i = 0;
		return;
	}
	timeofday = (hour*60+minute)*60;

	while(!isspace(record[++i])); // skip the minute field
}

ScheduledEvent::~ScheduledEvent()
{
	if (id) free(id);
	if (DeactivateTid >= 0) {
		daemonCore->Cancel_Timer(DeactivateTid);
	}
}

const time_t TwentyFourHours = 24*60*60;

time_t
ScheduledEvent::TimeOfEvent()
{
	time_t current_time = time(0);

	if (active || !valid) {
		return current_time+TwentyFourHours;
	}

	struct tm *current_tm = localtime(&current_time);
	int current_timeofday =
		(current_tm->tm_hour*60+current_tm->tm_min)*60+current_tm->tm_sec;

	short tomorrow = current_tm->tm_wday+1;
	if (tomorrow == 7) tomorrow = 0;
	short yesterday = current_tm->tm_wday-1;
	if (yesterday = -1) yesterday = 6;

	if (current_timeofday <= timeofday &&
		daymask & (1<<(current_tm->tm_wday))) { // event is today
		return current_time - current_timeofday + timeofday;
	} else if (timeofday+duration < TwentyFourHours &&
			   current_timeofday <= timeofday+duration &&
			   daymask & (1<<(current_tm->tm_wday))) { // event has started
		return current_time - current_timeofday + timeofday;
	} else if (timeofday+duration >= TwentyFourHours &&
			   current_timeofday <= timeofday+duration-TwentyFourHours &&
			   daymask & (1<<yesterday)) { // event started yesterday
		return current_time - current_timeofday + timeofday - TwentyFourHours;
	} else {					// event may be tomorrow
		if (daymask & (1<<(tomorrow))) { // event is tomorrow
			return current_time - current_timeofday + timeofday +
				TwentyFourHours;
		}
	}

	return current_time+TwentyFourHours;
}

int
ScheduledEvent::TimeToEvent()
{
	return TimeOfEvent()-time(0);
}

int
ScheduledEvent::ActivateEvent()
{
	if (active || !valid) return -1;

	// stash event end time before we activate the event, because once
	// we activate it, the time will move to the next event
	EndTime = TimeOfEvent()+duration;

	if (DeactivateTid >= 0) {
		dprintf(D_ALWAYS,
				"Warning: Deactivate timer was set for inactive event %s!\n",
				id);
		daemonCore->Cancel_Timer(DeactivateTid);
	}
	
	DeactivateTid =
		daemonCore->Register_Timer(EndTime+1-time(0),
								   (Eventcpp)&ScheduledEvent::DeactivateEvent,
								   "ScheduledEvent::DeactivateEvent()", this);

	active = true;
	return 0;
}

int ScheduledShutdownEvent::SlowStartInterval = -1;
int ScheduledShutdownEvent::EventInterval = -1;

static int CleanupShutdownModeConfig(const char startd_machine[],
									 const char startd_addr[]);
static int CleanupShutdownModeConfigs();
static int CleanupTid = -1;

static const char ShutdownConstraint[] =
"(%s) && "
"(((ImageSize > 0) && (State != \"Preempting\")) || " // need to be vacated
" (EndDownTime =?= UNDEFINED) || " // need to be put into shutdown state
" (EndDownTime < CurrentTime))"; // need to be put into shutdown state

ScheduledShutdownEvent::ScheduledShutdownEvent(const char name[],
											   const char record[], int offset)
	: ScheduledEvent(name, record, offset), StartdList(NULL),
	  constraint(NULL), rank(NULL), ShutdownList(NULL), VacateList(NULL),
	  TimeoutTid(-1)
{
	type = SHUTDOWN;
	valid = false;

	if (offset == 0) {
		return;
	}

	while(isspace(record[offset])) offset++; // skip leading whitespace

		// read the DURATION section of the record
	errno = 0;
	int hour = atoi(record+offset);
	if (errno) {
		dprintf(D_ALWAYS,
				"Error parsing duration field (hour) of event record %s.\n",
				name);
		return;
	}
	while(record[offset++] != ':') { // skip the ':'
		if (record[offset] == '\0') {
			dprintf(D_ALWAYS, "Error parsing time field of event record "
					"\"%s\".  No ':' found before end of string.\n", name);
			return;
		}
	}
	errno = 0;
	int minute = atoi(record+offset);
	if (errno) {
		dprintf(D_ALWAYS,
				"Error parsing duration field (minute) of event record %s.\n",
				name);
		return;
	}
	duration = (hour*60+minute)*60;
	while(isdigit(record[++offset])); // skip the duration field
	while(isspace(record[++offset])); // skip the whitespace

	// read the BANDWIDTH section of the record
	errno = 0;
	bandwidth = atof(record+offset);
	if (errno) {
		dprintf(D_ALWAYS,
				"Error parsing bandwidth field of event record %s.\n",
				name);
		return;
	}

	while(!isspace(record[++offset])); // skip the bandwidth field
	while(isspace(record[++offset])); // skip the whitespace

	int len;
	for (len = 0; record[offset+len] != '\0' &&
			 !isspace(record[offset+len]); len++);
	char *tmp = (char *)malloc(len+1);
	strncpy(tmp, record+offset, len);
	tmp[len] = '\0';
	char *admin_constraint = param(tmp);
	if (!admin_constraint) {
		dprintf(D_ALWAYS, "%s undefined.\n", tmp);
		free(tmp);
		return;
	}
	constraint = (char *)malloc(strlen(admin_constraint)+
								strlen(ShutdownConstraint));
	sprintf(constraint, ShutdownConstraint, admin_constraint);
	dprintf(D_FULLDEBUG, "%s: %s\n", tmp, admin_constraint);
	free(admin_constraint);
	free(tmp);

	offset += len;			// skip the constraint field
	while(isspace(record[++offset])); // skip the whitespace

	for (len = 0; record[offset+len] != '\0' &&
			 !isspace(record[offset+len]); len++);
	tmp = (char *)malloc(len+1);
	strncpy(tmp, record+offset, len);
	tmp[len] = '\0';
	char *admin_rank = param(tmp);
	if (!admin_rank) {
		dprintf(D_ALWAYS, "%s undefined.\n", tmp);
		free(tmp);
		return;
	}
	if (Parse(admin_rank, rank)) {
		dprintf(D_ALWAYS, "Parse error in %s.\n", tmp);
		free(tmp);
		free(admin_rank);
		return;
	}
	dprintf(D_FULLDEBUG, "%s: %s\n", tmp, admin_rank);
	free(tmp);
	free(admin_rank);

	if (SlowStartInterval < 0) {
		tmp = param("EVENTD_SHUTDOWN_SLOW_START_INTERVAL");
		if (tmp) {
			SlowStartInterval = atoi(tmp);
			free(tmp);
			dprintf(D_FULLDEBUG, "Shutdown slow start interval set to %d "
					"seconds.\n", SlowStartInterval);
		} else {
			SlowStartInterval = 0;
		}
	}

	if (EventInterval < 0) {
		tmp = param("EVENTD_INTERVAL");
		if (tmp) {
			EventInterval = atoi(tmp);
			free(tmp);
		} else {
			EventInterval = 900;
		}
	}

	// start a periodic cleanup timer if we haven't already
	if (CleanupTid < 0) {
		int CleanupInterval;
		tmp = param("EVENTD_SHUTDOWN_CLEANUP_INTERVAL");
		if (tmp) {
			CleanupInterval = atoi(tmp);
			free(tmp);
			dprintf(D_FULLDEBUG, "Shutdown cleanup interval set to %d "
					"seconds.\n", CleanupInterval);
		} else {
			CleanupInterval = 3600; // one hour
		}
		// do our first cleanup soon (in 10 minutes), because we may
		// have been away for a while, but give a little time for the
		// collector to get new ads
		int firstCleanup = (CleanupInterval < 600) ? CleanupInterval : 600;
		CleanupTid =
			daemonCore->Register_Timer(firstCleanup, CleanupInterval,
									   (Event)CleanupShutdownModeConfigs,
									   "CleanupShutdownModeConfigs");
	}

	valid = true;
}

ScheduledShutdownEvent::~ScheduledShutdownEvent()
{
	if (TimeoutTid >= 0) {
		daemonCore->Cancel_Timer(TimeoutTid);
	}
	if (constraint) free(constraint);
	if (StartdList) delete StartdList;
	if (ShutdownList) delete ShutdownList;
	if (VacateList) delete VacateList;
	if (rank) delete rank;
}

int
ScheduledShutdownEvent::TimeNeeded()
{
	ClassAd		*ad;

	if (GetStartdList() < 0) return -1;
	time_t current_time = time(0);
	double TotalRequiredCheckpoints = 0.0;
	StartdList->Open();
	while ((ad = StartdList->Next()) != NULL) {
		int ImageSize = 0, JobUniverse = 1, etime = 0;
		ad->LookupInteger(ATTR_JOB_UNIVERSE, JobUniverse);
		if (JobUniverse == 1 &&
			ad->LookupInteger(ATTR_IMAGE_SIZE, ImageSize) == 1) {
			TotalRequiredCheckpoints += ImageSize;
		}
	}
	
	return (int)(TotalRequiredCheckpoints/128/bandwidth);
}

int
ShutdownSortFunc(ClassAd *m1, ClassAd *m2, void *r)
{
	ExprTree *rank = (ExprTree *)r;
	float val1=0.0, val2=0.0;
	EvalResult	result;
	rank->EvalTree(m1, m2, &result);
	if (result.type == LX_INTEGER) {
		val1 = (float)result.i;
	} else if (result.type == LX_FLOAT) {
		val1 = result.f;
	}
	rank->EvalTree(m2, m1, &result);
	if (result.type == LX_INTEGER) {
		val2 = (float)result.i;
	} else if (result.type == LX_FLOAT) {
		val2 = result.f;
	}
	return (val1 < val2);
}

int
ScheduledShutdownEvent::ActivateEvent()
{
	if (active || !valid) return -1;

	if (ScheduledEvent::ActivateEvent() < 0) return -1;

	SlowStartPos = 0;

	// put all machines in shutdown mode immediately

	// we should have a StartdList, but just to make sure...
	if (StartdList == NULL) {
		if (GetStartdList() < 0) {
			return -1;
		}
	}

	// initialize our list of shutdown machines and vacated resources
	// These lists are used to ensure that we shutdown/vacate each
	// machine/resource at most once.  Without it, we will waste time
	// contacting machines multiple times in a number of cases: (1)
	// SMP machines, (2) lost collector updates, and (3) invalid
	// permission to shutdown a given machine.  Note that ShutdownList
	// is a list of ATTR_MACHINEs and VacateList is a list of
	// ATTR_NAMEs, since we only need to change the config of an SMP
	// startd once but we need to send a separate vacate to each
	// resource the startd is managing.
	if (ShutdownList) delete ShutdownList;
	ShutdownList = new StringList;
	if (VacateList) delete VacateList;
	VacateList = new StringList;

	ClassAd *ad;
	StartdList->Open();
	while ((ad = StartdList->Next()) != NULL) {
		// if machine is not in shutdown mode already *and* we didn't
		// just put this machine into shutdown mode (i.e., an SMP
		// startd will have multiple startd ClassAds), we put it in
		// shutdown mode here
		int etime;
		if (ad->LookupInteger("EndDownTime", etime) != 1 ||
			etime < EndTime) {
			char startd_addr[ATTRLIST_MAX_EXPRESSION];
			char startd_machine[ATTRLIST_MAX_EXPRESSION];
			if (ad->LookupString(ATTR_STARTD_IP_ADDR, startd_addr) != 1 ||
				ad->LookupString(ATTR_MACHINE, startd_machine) != 1) {
				dprintf(D_ALWAYS, "Malformed startd classad in "
						"ScheduledShutdownEvent::Timeout()\n");
				continue;
			}
			if (!ShutdownList->contains(startd_machine)) {
				if (EnterShutdownMode(startd_machine, startd_addr) == 0) {
					ShutdownList->append(startd_machine);
				}
			}
		}
	}

	// set timer to start shutting down the machines

	if (TimeoutTid >= 0) {
		dprintf(D_ALWAYS, "Warning: Timer was set for inactive "
				"SHUTDOWN event %s!\n", id);
		daemonCore->Cancel_Timer(TimeoutTid);
	}

	TimeoutTid =
		daemonCore->Register_Timer(0,
				(Eventcpp)&ScheduledShutdownEvent::Timeout,
				"ScheduledShutdownEvent::Timeout()", this);

	// make sure to get an updated StartdList with the new config values
	delete StartdList;
	StartdList = NULL;
	
	return 0;
}

int
ScheduledShutdownEvent::Timeout()
{
	if (!active || !valid) {
		TimeoutTid = -1;
		return -1;
	}
	
	if (StartdList == NULL) {
		if (GetStartdList() < 0) {
			// can't contact collector, so try again in EventInterval seconds
			if (daemonCore->Reset_Timer(TimeoutTid, EventInterval) < 0) {
				dprintf(D_ALWAYS, "Failed to reset timer for SHUTDOWN event "
						"%s!\n", id);
				TimeoutTid = -1;
				return -1;
			}
			return -1;
		}
	}

	// in which order should we shutdown the machines?
	if (rank) {
		StartdList->Sort((SortFunctionType)ShutdownSortFunc, (void *)rank);
	}

	// vacate the first job in the list
	int time_to_vacate = 0;
	ClassAd *ad;
	StartdList->Open();
	while ((ad = StartdList->Next()) != NULL) {
		char startd_addr[ATTRLIST_MAX_EXPRESSION];
		char startd_name[ATTRLIST_MAX_EXPRESSION];
		char startd_state[ATTRLIST_MAX_EXPRESSION];
		char startd_machine[ATTRLIST_MAX_EXPRESSION];
		if (ad->LookupString(ATTR_STARTD_IP_ADDR, startd_addr) != 1 ||
			ad->LookupString(ATTR_MACHINE, startd_machine) != 1 ||
			ad->LookupString(ATTR_NAME, startd_name) != 1) {
			dprintf(D_ALWAYS, "Malformed startd classad in "
					"ScheduledShutdownEvent::Timeout()\n");
			continue;
		}
		// make sure all startds are in shutdown mode
		int etime = 0;
		if ((ad->LookupInteger("EndDownTime", etime) != 1 ||
			 etime < EndTime) && !ShutdownList->contains(startd_machine)) {
			if (EnterShutdownMode(startd_machine, startd_addr) == 0) {
				ShutdownList->append(startd_machine);
			}
		}
		// we keep looping to verify that all startds are in shutdown mode
		// but we only want to vacate one big job in this loop, so we
		// continue if we have already vacated a job
		if (time_to_vacate > 0) continue;
		int ImageSize = 0, JobUniverse;
		// only send vacates to startds that have jobs (i.e., which
		// have ImageSize and JobUniverse defined), which are still in
		// the claimed state, and to which we haven't already sent a
		// vacate
		if (ad->LookupString(ATTR_STATE, startd_state) != 1 ||
			strcmp(startd_state, "Claimed") != MATCH ||
			ad->LookupInteger(ATTR_IMAGE_SIZE, ImageSize) != 1 || 
			ad->LookupInteger(ATTR_JOB_UNIVERSE, JobUniverse) != 1 ||
			VacateList->contains(startd_name)) {
			continue;
		}
		ReliSock sock;
		sock.timeout(10);
		if (!sock.connect(startd_addr, 0)) {
			dprintf(D_ALWAYS, "Failed to connect to startd %s %s.\n",
					startd_name, startd_addr);
			continue;
		}
		sock.encode();
		int cmd = VACATE_CLAIM;
		if (!sock.code(cmd) || !sock.put(startd_name) || !sock.eom()) {
			dprintf(D_ALWAYS, "Failed to send VACATE_CLAIM to %s %s.\n",
					startd_name, startd_addr);
			continue;
		}
		dprintf(D_ALWAYS, "Sent VACATE_CLAIM to %s.\n", startd_name);
		VacateList->append(startd_name);
		if (JobUniverse == 1) {
			time_to_vacate += (int)(double(ImageSize)/128.0/bandwidth);
		}
		dprintf(D_FULLDEBUG, "Estimating %d seconds of vacating time "
				"for %s.\n", time_to_vacate, startd_name);
	}

	if (!time_to_vacate) {
		// we didn't vacate anyone, so simply check every EVENTD_INTERVAL
		// to see if there is any new work to do
		time_to_vacate = EventInterval;
	}

	if (daemonCore->Reset_Timer(TimeoutTid, time_to_vacate) < 0) {
		dprintf(D_ALWAYS, "Failed to reset timer for SHUTDOWN event %s!\n",
				id);
		TimeoutTid = -1;
		return -1;
	}

	// make sure to get an updated StartdList next time
	delete StartdList;
	StartdList = NULL;

	return 0;
}

int
ScheduledShutdownEvent::GetStartdList()
{
	CondorQuery StartdQuery(STARTD_AD);

	if (!valid || !constraint) return -1;

	if (StartdList) delete StartdList;
	StartdList = new ClassAdList;

	if (StartdQuery.addANDConstraint(constraint) != Q_OK) {
		return -1;
	}
	if (StartdQuery.fetchAds(*StartdList) != Q_OK) {
		return -1;
	}

	return 0;
}

static const char ShutdownConfig[] =
"EndDownTime = %d\n"
"Shutdown = (CurrentTime < EndDownTime)\n"
"START : ($(START)) && ($(Shutdown) == False)\n"
"STARTD_EXPRS = $(STARTD_EXPRS), EndDownTime\n";

static const char ShutdownAdminId[] = "ScheduledShutdownEvent";

int
ScheduledShutdownEvent::EnterShutdownMode(const char startd_name[],
										  const char startd_addr[])
{
	ReliSock sock;
	sock.timeout(10);
	if (!sock.connect((char *)startd_addr, 0)) {
		dprintf(D_ALWAYS, "Failed to connect to %s %s.\n", startd_name,
				startd_addr);
		return -1;
	}
	sock.encode();
	int cmd = DC_CONFIG_PERSIST;
	char *config = (char *)malloc(strlen(ShutdownConfig)+20);
	sprintf(config, ShutdownConfig, EndTime+SlowStartPos);
	if (!sock.code(cmd) || !sock.put((char *)ShutdownAdminId) ||
		!sock.code(config) || !sock.eom()) {
		dprintf(D_ALWAYS, "Failed to send DC_CONFIG_PERSIST to "
				"%s %s.\n", startd_name, startd_addr);
		free(config);
		return -1;
	}
	free(config);
	sock.close();
	if (!sock.connect((char *)startd_addr, 0)) {
		dprintf(D_ALWAYS, "Failed to connect to %s %s.\n", startd_name,
				startd_addr);
		return -1;
	}
	sock.encode();
	cmd = DC_RECONFIG;
	if (!sock.code(cmd) || !sock.eom()) {
		dprintf(D_ALWAYS, "Failed to send DC_RECONFIG to %s %s.\n",
				startd_name, startd_addr);
		return -1;
	}

	dprintf(D_ALWAYS, "Placed %s in shutdown mode until %d.\n",
			startd_name, EndTime+SlowStartPos);
	SlowStartPos += SlowStartInterval;

	return 0;
}

static int
CleanupShutdownModeConfig(const char startd_machine[],
						  const char startd_addr[])
{
	ReliSock sock;
	sock.timeout(10);
	if (!sock.connect((char *)startd_addr, 0)) {
		dprintf(D_ALWAYS, "Failed to connect to %s %s.\n", startd_machine,
				startd_addr);
		return -1;
	}
	sock.encode();
	int cmd = DC_CONFIG_PERSIST;
	if (!sock.code(cmd) || !sock.put((char *)ShutdownAdminId) ||
		!sock.put("") || !sock.eom()) {
		dprintf(D_ALWAYS, "Failed to send DC_CONFIG_PERSIST to "
				"%s %s.\n", startd_machine, startd_addr);
		return -1;
	}
	sock.close();
	if (!sock.connect((char *)startd_addr, 0)) {
		dprintf(D_ALWAYS, "Failed to connect to %s %s.\n", startd_machine,
				startd_addr);
		return -1;
	}
	sock.encode();
	cmd = DC_RECONFIG;
	if (!sock.code(cmd) || !sock.eom()) {
		dprintf(D_ALWAYS, "Failed to send DC_RECONFIG to %s %s.\n",
				startd_machine, startd_addr);
		return -1;
	}

	dprintf(D_ALWAYS, "Cleaned up shutdown mode config on %s.\n",
			startd_machine);

	return 0;
}

static const char CleanupConstraint[] = "(CurrentTime > EndDownTime)";

int
CleanupShutdownModeConfigs()
{
	CondorQuery	StartdQuery(STARTD_AD);
	ClassAd		*ad;

	ClassAdList StartdList;

	if (StartdQuery.addANDConstraint(CleanupConstraint) != Q_OK) {
		return -1;
	}
	if (StartdQuery.fetchAds(StartdList) != Q_OK) {
		return -1;
	}

	StartdList.Open();
	StringList Machines;
	while ((ad = StartdList.Next()) != NULL) {
		char startd_addr[ATTRLIST_MAX_EXPRESSION];
		char startd_machine[ATTRLIST_MAX_EXPRESSION];
		if (ad->LookupString(ATTR_STARTD_IP_ADDR, startd_addr) != 1 ||
			ad->LookupString(ATTR_MACHINE, startd_machine) != 1) {
			dprintf(D_ALWAYS, "Malformed startd classad in "
					"CleanupShutdownModeConfigs()\n");
			continue;
		}
		if (!Machines.contains(startd_machine)) {
			CleanupShutdownModeConfig(startd_machine, startd_addr);
			Machines.append(startd_machine);
		}
	}

	return 0;
}
