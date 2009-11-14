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
#include "condor_daemon_core.h"
#include "condor_debug.h"
#include "condor_query.h"
#include "condor_config.h"
#include "dc_collector.h"
#include "daemon.h"
#include "daemon_list.h"
#include "condor_netdb.h"

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
	if (yesterday == -1) yesterday = 6;

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
	StartTime = TimeOfEvent();
	EndTime = StartTime+duration;

	if (DeactivateTid >= 0) {
		dprintf(D_ALWAYS,
				"Warning: Deactivate timer was set for inactive event %s!\n",
				id);
		daemonCore->Cancel_Timer(DeactivateTid);
	}
	
	DeactivateTid =
		daemonCore->Register_Timer(EndTime+1-time(0),
								   (TimerHandlercpp)&ScheduledEvent::DeactivateEvent,
								   "ScheduledEvent::DeactivateEvent()", this);

	active = true;

	return 0;
}

int ScheduledShutdownEvent::SlowStartInterval = -1;
int ScheduledShutdownEvent::EventInterval = -1;
int ScheduledShutdownEvent::NumScheduledShutdownEvents = 0;
Router ScheduledShutdownEvent::NetRouter;
NetworkCapacity ScheduledShutdownEvent::NetCap;
NetworkReservations ScheduledShutdownEvent::NetRes;
int ScheduledShutdownEvent::SimulateShutdowns = 0;

static int CleanupShutdownModeConfig(const char startd_machine[],
									 const char startd_addr[]);
static void CleanupShutdownModeConfigs();
static int CleanupTid = -1;
static int StartdValidWindow = 600;
static int RescheduleInterval = 60;

ScheduledShutdownEvent::ScheduledShutdownEvent(const char name[],
											   const char record[],
											   bool runtime, bool standard,
											   int offset)
	: ScheduledEvent(name, record, offset), StartdList(NULL),
	  constraint(NULL), ShutdownList(NULL), VacateList(NULL), TimeoutTid(-1),
	  RuntimeConfig(runtime), StandardJobsOnly(standard)
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

	int len;
	for (len = 0; record[offset+len] != '\0' &&
			 !isspace(record[offset+len]); len++);
	char *tmp = (char *)malloc(len+1);
	strncpy(tmp, record+offset, len);
	tmp[len] = '\0';
	constraint = param(tmp);
	if (!constraint) {
		dprintf(D_ALWAYS, "%s undefined.\n", tmp);
		free(tmp);
		return;
	}
	dprintf(D_FULLDEBUG, "%s: %s\n", tmp, constraint);
	free(tmp);

	if (NumScheduledShutdownEvents == 0) {

		SlowStartInterval = param_integer("EVENTD_SHUTDOWN_SLOW_START_INTERVAL",0);
		dprintf(D_FULLDEBUG, "Shutdown slow start interval set to %d "
				"seconds.\n", SlowStartInterval);

		EventInterval = param_integer("EVENTD_INTERVAL",900);
		
		tmp = param("EVENTD_ROUTING_INFO");
		if (tmp) {
			if (NetRouter.LoadRoutingTable(tmp) < 0) {
				dprintf(D_ALWAYS, "Failed to load EVENTD_ROUTING_INFO.  "
						"No network scheduling will be performed.\n");
				free(tmp);
				return;
			}
			free(tmp);
		} else {
			dprintf(D_ALWAYS, "EVENTD_ROUTING_INFO undefined.  "
					"No network scheduling will be performed.\n");
			return;
		}

		tmp = param("EVENTD_CAPACITY_INFO");
		if (tmp) {
			if (NetCap.LoadNetworkCapacity(tmp) < 0) {
				dprintf(D_ALWAYS, "Failed to load EVENTD_CAPACITY_INFO.  "
						"No network scheduling will be performed.\n");
				free(tmp);
				return;
			}
			free(tmp);
		} else {
			dprintf(D_ALWAYS, "EVENTD_CAPACITY_INFO undefined.  "
					"No network scheduling will be performed.\n");
			return;
		}
		NetRes.BindRouter(&NetRouter);
		NetRes.BindCapacity(&NetCap);

		tmp = param("EVENTD_SIMULATE_SHUTDOWNS");
		SimulateShutdowns = 0;
		if (tmp) {
			if (tmp[0] == 'T' || tmp[0] == 't') {
				SimulateShutdowns = 1;
				dprintf(D_FULLDEBUG,
						"Simulating shutdowns.  No commands will be sent.\n");
			}
			free(tmp);
		}

		StartdValidWindow = param_integer("UPDATE_INTERVAL",300);
		StartdValidWindow *= 2;

		RescheduleInterval = param_integer("EVENTD_MIN_RESCHEDULE_INTERVAL",60);
	}

	NumScheduledShutdownEvents++;

	// start a periodic cleanup timer if we haven't already
	if (CleanupTid < 0) {
		int CleanupInterval;
		CleanupInterval = param_integer("EVENTD_SHUTDOWN_CLEANUP_INTERVAL",3600);
		dprintf(D_FULLDEBUG, "Shutdown cleanup interval set to %d "
				"seconds.\n", CleanupInterval);
		// do our first cleanup soon (in 10 minutes), because we may
		// have been away for a while, but give a little time for the
		// collector to get new ads
		int firstCleanup = (CleanupInterval < 600) ? CleanupInterval : 600;
		CleanupTid =
			daemonCore->Register_Timer(firstCleanup, CleanupInterval,
									   CleanupShutdownModeConfigs,
									   "CleanupShutdownModeConfigs");
	}

	last_schedule_update = 0;
	valid = true;
	earliest_vacate = 0;
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
	NumScheduledShutdownEvents--;
	schedule.Rewind();
	for (ShutdownRecord *rec = schedule.Next(); rec; rec = schedule.Next()) {
		NetRes.CancelReservation(rec->id);
		delete rec;
	}
}

static int
ScheduleCompar(const ScheduledShutdownEvent::ShutdownRecord **r1,
			   const ScheduledShutdownEvent::ShutdownRecord **r2)
{
	return ((*r1)->vacate_duration) - ((*r2)->vacate_duration);
}

int
ScheduledShutdownEvent::TimeNeeded()
{
	StartTime = TimeOfEvent();

	UpdateSchedule();

	if (schedule.IsEmpty()) {
		return 0;
	}
	
	return StartTime-earliest_vacate;
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

	// Initialize our list of shutdown machines and vacated resources.
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

	time_t current_time = time(0);
	ClassAd *ad;
	StartdList->Open();
	while ((ad = StartdList->Next()) != NULL) {
		// if machine is not in shutdown mode already *and* we didn't
		// just put this machine into shutdown mode (i.e., an SMP
		// startd will have multiple startd ClassAds), we put it in
		// shutdown mode here
		int last_heard_from = 0;
		ad->LookupInteger(ATTR_LAST_HEARD_FROM, last_heard_from);
		if (current_time - last_heard_from > StartdValidWindow) {
			char startd_name[ATTRLIST_MAX_EXPRESSION];
			startd_name[0] = '\0';
			ad->LookupString(ATTR_NAME, startd_name);
			dprintf(D_ALWAYS, "ClassAd for %s is %d seconds old!\n",
					startd_name, current_time - last_heard_from);
		}
		int etime;
		if (ad->LookupInteger("EndDownTime", etime) != 1 ||
			etime < EndTime) {
			char startd_addr[ATTRLIST_MAX_EXPRESSION];
			char startd_machine[ATTRLIST_MAX_EXPRESSION];
			if (ad->LookupString(ATTR_STARTD_IP_ADDR, startd_addr) != 1 ||
				ad->LookupString(ATTR_MACHINE, startd_machine) != 1) {
				dprintf(D_ALWAYS, "Malformed startd ClassAd in "
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

	// set timer to start vacating
	if (TimeoutTid >= 0) {
		dprintf(D_ALWAYS, "Warning: Timer was set for inactive "
				"SHUTDOWN event %s!\n", id);
		daemonCore->Cancel_Timer(TimeoutTid);
	}

	TimeoutTid =
		daemonCore->Register_Timer(0,
				(TimerHandlercpp)&ScheduledShutdownEvent::Timeout,
				"ScheduledShutdownEvent::Timeout()", this);

	// make sure to get an updated StartdList with the new config values
	delete StartdList;
	StartdList = NULL;
	
	num_records_past_deadline = 0;

	return 0;
}

void
ScheduledShutdownEvent::Timeout()
{
	if (!active || !valid) {
		TimeoutTid = -1;
		return;
	}
	
	time_t current_time = time(0);
	// updating the schedule is expensive (collector query plus any network
	// scheduling), so don't do it more than once per RescheduleInterval
	if (forceScheduleUpdate ||
		(current_time - last_schedule_update > RescheduleInterval)) {
		UpdateSchedule();
	}
	forceScheduleUpdate = false;
	
	// perform any scheduled vacates
	schedule.Rewind();
	ShutdownRecord *rec;
	for (rec = schedule.Next(); rec && rec->vacate_time <= current_time;
		 rec = schedule.Next()) {
		if (Vacate(rec->startd_name, rec->startd_addr) == 0) {
			VacateList->append(rec->startd_name);
		}
		// remove the shutdown record even if we failed to vacate the 
		// resource; we will reschedule the vacate in UpdateSchedule()
		// if the job is still running
		RemoveRecord(rec);
		delete rec;
		current_time = time(0);	// in case vacate is slow
	}

	// set timer for next scheduled vacates
	int next_vacate;
	if (rec) {
		next_vacate = rec->vacate_time-current_time;
	} else {
		// check every EVENTD_INTERVAL to see if there is any new work to do
		next_vacate = EventInterval;
	}

	// make sure we call Timeout() at StartTime so we can vacate jobs
	// with no checkpoint files
	int time_to_start = StartTime - current_time;
	if (time_to_start > 0 && time_to_start < next_vacate) {
		forceScheduleUpdate = true;
		next_vacate = time_to_start;
	}

	if (daemonCore->Reset_Timer(TimeoutTid, next_vacate) < 0) {
		dprintf(D_ALWAYS, "Failed to reset timer for SHUTDOWN event %s!\n",
				id);
		TimeoutTid = -1;
		return;
	}

	// Make sure that all startds are in shutdown mode.
	if (StartdList == NULL) {
		if (GetStartdList() < 0) {
			return;			// Oh well, try again next time...
		}
	}

	ClassAd *ad;
	StartdList->Open();
	while ((ad = StartdList->Next()) != NULL) {
		int last_heard_from = 0;
		ad->LookupInteger(ATTR_LAST_HEARD_FROM, last_heard_from);
		if (current_time - last_heard_from > StartdValidWindow) {
			char startd_name[ATTRLIST_MAX_EXPRESSION];
			startd_name[0] = '\0';
			ad->LookupString(ATTR_NAME, startd_name);
			dprintf(D_ALWAYS, "ClassAd for %s is %d seconds old!\n",
					startd_name, current_time - last_heard_from);
		}
		char startd_addr[ATTRLIST_MAX_EXPRESSION];
		char startd_machine[ATTRLIST_MAX_EXPRESSION];
		if (ad->LookupString(ATTR_STARTD_IP_ADDR, startd_addr) != 1 ||
			ad->LookupString(ATTR_MACHINE, startd_machine) != 1) {
			dprintf(D_ALWAYS, "Malformed startd ClassAd in "
					"ScheduledShutdownEvent::Timeout()\n");
			continue;
		}
		int etime = 0;
		if ((ad->LookupInteger("EndDownTime", etime) != 1 ||
			 etime < EndTime) && !ShutdownList->contains(startd_machine)) {
			if (EnterShutdownMode(startd_machine, startd_addr) == 0) {
				ShutdownList->append(startd_machine);
			}
		}
	}

	// make sure to get an updated StartdList next time
	delete StartdList;
	StartdList = NULL;
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

	CollectorList * collectors = CollectorList::create();
	if (collectors->query (StartdQuery, *StartdList) != Q_OK ) {
		delete collectors;
		return -1;
	}
	delete collectors;

	return 0;
}

void
ScheduledShutdownEvent::UpdateSchedule()
{
	if (GetStartdList() < 0) return;

	// Walk through list of startds, building up list of scheduling changes
	// and vacating jobs with no checkpoints if it is time to do so.
	time_t current_time = time(0);
	ClassAd *ad;
	ShutdownRecord **SchedulingChanges = NULL;
	bool AnyCancelled = false;
	int NumSchedulingChanges = 0;
	int NumStartds = 0;
	StartdList->Open();
	while ((ad = StartdList->Next()) != NULL) {
		NumStartds++;
		char startdName[128];
		if (!ad->LookupString(ATTR_NAME, startdName)) {
			dprintf(D_ALWAYS, "Could not lookup %s\n", ATTR_NAME);
			continue;
		}
		char startdMachine[128];
		if (!ad->LookupString(ATTR_MACHINE, startdMachine)) {
			dprintf(D_ALWAYS, "Could not lookup %s\n", ATTR_MACHINE);
			continue;
		}
		// If we have already vacated this slot, then we don't need to
		// try to schedule a vacate.  Note that the VacateList is only
		// up-to-date if we are active.
		if (active && VacateList && VacateList->contains(startdName)) {
			continue;
		}
		// If we're setting runtime configs, then the startd may have
		// restarted, removing the configuration.  To detect this
		// case, check if the startd classad has no EndDownTime
		// attribute (or an old one) even though we think we've shut
		// it down.  In that case, we don't want to vacate the job.
		if (active && RuntimeConfig) {
			int etime;
			if ((!ad->LookupInteger("EndDownTime", etime) ||
				 etime < EndTime) && ShutdownList->contains(startdMachine)) {
				continue;
			}
		}
		// Write a debugging message if we get a stale ad, so we know that
		// an ad may be timed out soon.
		int last_heard_from = 0;
		ad->LookupInteger(ATTR_LAST_HEARD_FROM, last_heard_from);
		if (current_time - last_heard_from > StartdValidWindow) {
			dprintf(D_ALWAYS, "ClassAd for %s is %d seconds old!\n",
					startdName, current_time - last_heard_from);
		}
		// we only need to vacate the startd if it is claimed
		char state[20];
		if (!ad->LookupString(ATTR_STATE, state)) {
			dprintf(D_ALWAYS, "Could not lookup %s for %s\n", ATTR_STATE,
					startdName);
			continue;
		}
		if (strcmp(state, "Claimed")) {
			if (DeleteRecord(startdName)) {
				AnyCancelled = true;
				dprintf(D_FULLDEBUG, "Cancelling vacate of %s, now in %s "
						"state.\n",	startdName, state);
			}
			continue;
		}
		int CkptSize=0, ExecutableSize=0, JobUniverse = CONDOR_UNIVERSE_STANDARD;
		ad->LookupInteger(ATTR_JOB_UNIVERSE, JobUniverse);
		ad->LookupInteger(ATTR_IMAGE_SIZE, CkptSize);
		ad->LookupInteger(ATTR_EXECUTABLE_SIZE, ExecutableSize);
		CkptSize -= ExecutableSize;
		char startdAddr[128];
		if (!ad->LookupString(ATTR_STARTD_IP_ADDR, startdAddr)) {
			dprintf(D_ALWAYS, "Could not lookup %s for %s\n",
					ATTR_STARTD_IP_ADDR, startdName);
			continue;
		}
		if (JobUniverse != CONDOR_UNIVERSE_STANDARD && StandardJobsOnly) continue;
		// We don't need to get a network reservation to vacate jobs
		// which don't have checkpoints.  We still need to vacate
		// these jobs though, so we make sure that UpdateSchedule() is
		// called at StartTime by Timeout(), and if we have reached
		// StartTime, we go ahead and vacate the jobs here.
		if (JobUniverse != CONDOR_UNIVERSE_STANDARD || CkptSize <= 0) {
			if (DeleteRecord(startdName)) {
				AnyCancelled = true;
				dprintf(D_FULLDEBUG, "Cancelling pre-scheduled vacate of %s.  "
						"New job has est. ckpt size of zero.\n", startdName);
			}
			if (active && current_time >= StartTime) {
				if (Vacate(startdName, startdAddr) == 0) {
					VacateList->append(startdName);
				}
			}
			continue;
		}
		char startdIP[128];
		strcpy(startdIP, startdAddr+1);	// skip the leading '<'
		char *colon = strchr(startdIP, ':');
		if (!colon) {
			dprintf(D_ALWAYS, "Invalid %s: %s for %s\n", ATTR_STARTD_IP_ADDR,
					startdAddr, startdName);
			continue;
		}
		*colon = '\0';
		char ckptServer[128], ckptServerIP[16];
		if (!ad->EvalString(ATTR_CKPT_SERVER, NULL, ckptServer)) {
			dprintf(D_ALWAYS, "Failed to evaluate %s for %s\n",
					ATTR_CKPT_SERVER, startdName);
			continue;
		}
		struct hostent *hp = condor_gethostbyname(ckptServer);
		if (!hp) {
			dprintf(D_ALWAYS, "DNS lookup for %s %s failed!\n",
					ATTR_CKPT_SERVER, ckptServer);
			continue;
		}
		struct in_addr ckpt_dest = *((struct in_addr *)hp->h_addr);
		strcpy(ckptServerIP, inet_ntoa(ckpt_dest));
		// We don't need a network reservation to transfer the checkpoint
		// to/from the same machine.
		if (!strcmp(startdIP, ckptServerIP)) {
			if (DeleteRecord(startdName)) {
				AnyCancelled = true;
				dprintf(D_FULLDEBUG, "Cancelling pre-scheduled vacate of %s.  "
						"Checkpoint will be stored locally now.\n",
						startdName);
			}
			if (active && current_time >= StartTime) {
				if (Vacate(startdName, startdAddr) == 0) {
					VacateList->append(startdName);
				}
			}
			continue;
		}
		ShutdownRecord *rec = LookupRecord(startdName);
		if (rec) {
			if (rec->ckpt_size != CkptSize) {
				dprintf(D_FULLDEBUG, "Rescheduling vacate of %s due to new "
						"checkpoint size (%d vs. %d).\n", rec->startd_name,
						CkptSize, rec->ckpt_size);
			} else if (strcmp(rec->startd_ip, startdIP)) {
				dprintf(D_FULLDEBUG, "Rescheduling vacate of %s due to new "
						"startd IP (%s vs. %s).\n", rec->startd_name,
						startdIP, rec->startd_ip);
			} else if (strcmp(rec->ckpt_server_ip, ckptServerIP)) {
				dprintf(D_FULLDEBUG, "Rescheduling vacate of %s due to new "
						"checkpoint server (%s vs. %s).\n", rec->startd_name,
						ckptServerIP, rec->ckpt_server_ip);
			} else {
				rec->timestamp = current_time;
				continue;		// no change in schedule
			}
			AnyCancelled = true;
			RemoveRecord(rec);
			delete rec;
		}
		int duration = 
			NetRes.TransferDuration(double(CkptSize)*1024,
									startdIP, ckptServerIP);
		if (duration <= 0) {
			// Something went seriously wrong.
			dprintf(D_ALWAYS, "Network routing failure from %s to %s!\n",
					startdIP, ckptServerIP);
			continue;
		}
		rec = new ShutdownRecord(startdName, startdAddr, startdIP,
								 ckptServerIP, CkptSize, 0, duration,
								 NetRes.NewID(), current_time);

		if (NumSchedulingChanges == 0) {
			SchedulingChanges =
				new ShutdownRecord* [StartdList->MyLength()];
		}
		SchedulingChanges[NumSchedulingChanges++] = rec;
	}

	// remove stale records
	schedule.Rewind();
	for (ShutdownRecord *rec = schedule.Next(); rec; rec = schedule.Next()) {
		if (current_time - rec->timestamp > StartdValidWindow) {
			NetRes.CancelReservation(rec->id);
			dprintf(D_FULLDEBUG, "Scheduled vacate of %s at %d cancelled.  "
					"ClassAd timed out.\n",
					rec->startd_name, (int)rec->vacate_time);
			schedule.DeleteCurrent();
			delete rec;
		}
	}

	// implement any needed scheduling changes
	if (NumSchedulingChanges) {
		// sort SchedulingChanges by vacate_duration, so we schedule shorter
		// transfers nearer to StartTime
		qsort((void *)SchedulingChanges, NumSchedulingChanges,
			  sizeof(ShutdownRecord *),
			  (int (*)(const void *, const void *))ScheduleCompar);

		for (int i=0; i < NumSchedulingChanges; i++) {
			double rate = 0.0;
			time_t end_time = StartTime; // xfer must end before shutdown start
			ShutdownRecord *rec = SchedulingChanges[i];
			int rval =
				NetRes.ReserveCapacity(double(rec->ckpt_size)*1024,
									   rec->startd_ip, rec->ckpt_server_ip,
									   false,
									   &rec->vacate_time, &end_time,
									   rec->id, "", &rate);
			if (rval != 1) {
				// We failed to schedule the transfer before the deadline.
				// We still need to perform the transfer, so find the
				// earliest avialable transfer window.
				rec->vacate_time = current_time;
				end_time = 0;
				rval = NetRes.ReserveCapacity(double(rec->ckpt_size)*1024,
											  rec->startd_ip,
											  rec->ckpt_server_ip,
											  true,
											  &rec->vacate_time, &end_time,
											  rec->id, "", &rate);
				if (rval == 1) num_records_past_deadline++;
			}
			if (rval == 1) {
				if (schedule.IsEmpty() || earliest_vacate > rec->vacate_time) {
					earliest_vacate = rec->vacate_time;
				}
				InsertRecord(rec);
				dprintf(D_FULLDEBUG, "Vacate of %s (%s --> %s at %.0f Mb/s) "
						"scheduled from %d to %d.\n", rec->startd_name,
						rec->startd_ip, rec->ckpt_server_ip, rate,
						(int)rec->vacate_time, (int)end_time);
			} else {
				// We should always find a schedule.  This is a serious error.
				dprintf(D_ALWAYS, "Network scheduling failure for %.0f bytes "
						"from %s to %s by %s!\n", double(rec->ckpt_size)*1024,
						rec->startd_ip,	rec->ckpt_server_ip, ctime(&EndTime));
				delete rec;
			}
		}

		delete [] SchedulingChanges;
	}

	// if we cancelled any scheduled vacates, try to reschedule any
	// transfers which are past the deadline
	if (AnyCancelled && num_records_past_deadline > 0) {
		schedule.Rewind();
		for (ShutdownRecord *rec = schedule.Next(); rec;
			 rec = schedule.Next()) {
			if (rec->vacate_time+rec->vacate_duration > StartTime) {
				ShutdownRecord *nrec =
					new ShutdownRecord(rec->startd_name, rec->startd_addr,
									   rec->startd_ip, rec->ckpt_server_ip,
									   rec->ckpt_size, 0, rec->vacate_duration,
									   rec->id, rec->timestamp);
				double rate = 0.0;
				time_t end_time = StartTime; // xfer must end b4 shutdwn start
				int rval =
					NetRes.ReserveCapacity(double(nrec->ckpt_size)*1024,
										   nrec->startd_ip,
										   nrec->ckpt_server_ip,
										   false,
										   &nrec->vacate_time, &end_time,
										   NetRes.NewID(), "", &rate);
				if (rval == 1) { // we can meet the deadline now!
					dprintf(D_FULLDEBUG, "Rescheduling vacate of %s before "
							"deadline!\nVacate of %s (%s --> %s at %.0f "
							"Mb/s) scheduled from %d to %d.\n",
							nrec->startd_name,
							nrec->startd_name, nrec->startd_ip,
							nrec->ckpt_server_ip, rate,
							(int)nrec->vacate_time, (int)end_time);
					num_records_past_deadline--;
					NetRes.CancelReservation(rec->id); // delete old record
					schedule.DeleteCurrent();
					delete rec;
					if (schedule.IsEmpty() ||
						earliest_vacate > nrec->vacate_time) {
						earliest_vacate = nrec->vacate_time;
					}
					InsertRecord(nrec);	// insert new record
				} else {
					delete nrec; // no new schedule
				}
			}
		}
	}

	last_schedule_update = current_time;
	dprintf(D_FULLDEBUG, "%s: Query returned %d startd ClassAds.\n",
			id, NumStartds);
}

static const char ShutdownConfig[] =
"EndDownTime = %d\n"
"Shutdown = (CurrentTime < EndDownTime)\n"
"START = ($(START)) && ($(Shutdown) == False)\n"
"STARTD_EXPRS = $(STARTD_EXPRS), EndDownTime\n";

static const char StandardShutdownConfig[] =
"EndDownTime = %d\n"
"Shutdown = (CurrentTime < EndDownTime)\n"
"START = ($(START)) && (($(Shutdown) == False) || (TARGET.JobUniverse != 1))\n"
"STARTD_EXPRS = $(STARTD_EXPRS), EndDownTime\n";

static const char ShutdownAdminId[] = "eventd_shutdown";

int
ScheduledShutdownEvent::EnterShutdownMode(const char startd_name[],
										  const char startd_addr[])
{
	if (!SimulateShutdowns) {
		int cmd = (RuntimeConfig) ? DC_CONFIG_RUNTIME : DC_CONFIG_PERSIST;
		char *config;
		if (StandardJobsOnly) {
			config = (char *)malloc(strlen(StandardShutdownConfig)+20);
			sprintf(config, StandardShutdownConfig, (int)EndTime+SlowStartPos);
		} else {
			config = (char *)malloc(strlen(ShutdownConfig)+20);
			sprintf(config, ShutdownConfig, (int)EndTime+SlowStartPos);
		}
		Daemon startd(DT_STARTD, startd_addr, NULL);
		Sock *sock = startd.startCommand(cmd, Stream::reli_sock, 10);
		if (!sock || !sock->put((char *)ShutdownAdminId) ||
			!sock->code(config) || !sock->eom()) {
			dprintf(D_ALWAYS, "Failed to send DC_CONFIG_PERSIST to "
					"%s %s.\n", startd_name, startd_addr);
			free(config);
			delete sock;
			return -1;
		}
		free(config);
		delete sock;

		if (!startd.sendCommand(DC_RECONFIG, Stream::reli_sock, 10)) {
			dprintf(D_ALWAYS, "Failed to send DC_RECONFIG to %s %s.\n",
					startd_name, startd_addr);
			return -1;
		}
	}
	dprintf(D_ALWAYS, "Placed %s in shutdown mode until %d.\n",
			startd_name, EndTime+SlowStartPos);
	SlowStartPos += SlowStartInterval;

	return 0;
}

int
ScheduledShutdownEvent::Vacate(char startd_name[], char startd_addr[])
{
	if (!SimulateShutdowns) {
		Daemon startd(DT_STARTD, startd_addr, NULL);
		Sock *sock = startd.startCommand(VACATE_CLAIM, Stream::reli_sock, 10);
		if (!sock || !sock->put(startd_name) || !sock->eom()) {
			dprintf(D_ALWAYS, "Failed to send VACATE_CLAIM to %s %s.\n",
					startd_name, startd_addr);
			delete sock;
			return -1;
		}
		delete sock;
	}
	dprintf(D_ALWAYS, "Sent VACATE_CLAIM to %s.\n", startd_name);

	return 0;
}

ScheduledShutdownEvent::ShutdownRecord::
ShutdownRecord(const char name[], const char addr[], const char startd_IP[],
			   const char ckpt_server_IP[], int size, time_t vtime,
			   int duration,  int new_id, time_t tstamp) 
{
	startd_name = strdup(name);
	startd_addr = strdup(addr);
	strcpy(startd_ip, startd_IP);
	strcpy(ckpt_server_ip, ckpt_server_IP);
	ckpt_size = size;
	vacate_time = vtime;
	vacate_duration = duration;
	id = new_id;
	timestamp = tstamp;
	
}

ScheduledShutdownEvent::ShutdownRecord::~ShutdownRecord()
{
	free(startd_name);
	free(startd_addr);
}

ScheduledShutdownEvent::ShutdownRecord *
ScheduledShutdownEvent::LookupRecord(const char startd_name[])
{
	schedule.Rewind();
	for (ShutdownRecord *rec = schedule.Next(); rec; rec = schedule.Next()) {
		if (!strcmp(rec->startd_name, startd_name)) {
			return rec;
		}
	}
	return NULL;
}

void
ScheduledShutdownEvent::InsertRecord(ShutdownRecord *new_rec)
{
	// insert in order of vacate_time
	schedule.Rewind();
	ShutdownRecord *rec = NULL;
	for (rec = schedule.Next(); rec && rec->vacate_time < new_rec->vacate_time;
		 rec = schedule.Next());
	if (rec) schedule.Insert(new_rec);
	else schedule.Append(new_rec);
}

void
ScheduledShutdownEvent::RemoveRecord(ShutdownRecord *r)
{
	schedule.Rewind();
	for (ShutdownRecord *rec = schedule.Next(); rec; rec = schedule.Next()) {
		if (r == rec) {
			NetRes.CancelReservation(rec->id);
			schedule.DeleteCurrent();
			return;
		}
	}
}

bool
ScheduledShutdownEvent::DeleteRecord(const char startd_name[])
{
	schedule.Rewind();
	for (ShutdownRecord *rec = schedule.Next(); rec; rec = schedule.Next()) {
		if (!strcmp(rec->startd_name, startd_name)) {
			NetRes.CancelReservation(rec->id);
			schedule.DeleteCurrent();
			delete rec;
			return true;
		}
	}
	return false;
}

static int
CleanupShutdownModeConfig(const char startd_machine[],
						  const char startd_addr[])
{
	Daemon startd(DT_STARTD, startd_addr, NULL);
	Sock *sock = startd.startCommand(DC_CONFIG_PERSIST, Stream::safe_sock, 10);
	if (!sock || !sock->put((char *)ShutdownAdminId) ||
		!sock->put("") || !sock->eom()) {
		dprintf(D_ALWAYS, "Failed to send DC_CONFIG_PERSIST to "
				"%s %s.\n", startd_machine, startd_addr);
		delete sock;
		return -1;
	}
	delete sock;

	sock = startd.startCommand(DC_CONFIG_RUNTIME, Stream::safe_sock, 10);
	if (!sock || !sock->put((char *)ShutdownAdminId) ||
		!sock->put("") || !sock->eom()) {
		dprintf(D_ALWAYS, "Failed to send DC_CONFIG_RUNTIME to "
				"%s %s.\n", startd_machine, startd_addr);
		delete sock;
		return -1;
	}
	delete sock;

	if (!startd.sendCommand(DC_RECONFIG, Stream::safe_sock, 10)) {
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
	CollectorList * collectors = CollectorList::create();
	if (collectors->query (StartdQuery,StartdList) != Q_OK) { 
		delete collectors;
		return -1;
	}
	delete collectors;

	StartdList.Open();
	StringList Machines;
	while ((ad = StartdList.Next()) != NULL) {
		char startd_addr[ATTRLIST_MAX_EXPRESSION];
		char startd_machine[ATTRLIST_MAX_EXPRESSION];
		if (ad->LookupString(ATTR_STARTD_IP_ADDR, startd_addr) != 1 ||
			ad->LookupString(ATTR_MACHINE, startd_machine) != 1) {
			dprintf(D_ALWAYS, "Malformed startd ClassAd in "
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
