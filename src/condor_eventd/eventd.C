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

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "string_list.h"
#include "condor_config.h"

#include "eventd.h"

EventDaemon _EventD, *EventD = &_EventD;

EventDaemon::EventDaemon() : EventInterval(-1), MaxEventPreparation(0),
	TimeoutTid(-1),	EventList(NULL), NumEvents(-1)
{
}

EventDaemon::~EventDaemon()
{
	if (TimeoutTid >= 0) {
		daemonCore->Cancel_Timer(TimeoutTid);
	}
	for (int i=0; i < NumEvents; i++) {
		delete EventList[i];
	}
	delete [] EventList;
}

int
EventDaemon::Config()
{
	char *tmp;

	for (int i=0; i < NumEvents; i++) {
		delete EventList[i];
	}
	delete [] EventList;

	tmp = param("EVENT_LIST");
	if (!tmp || tmp[0] == '\0') {
		dprintf(D_ALWAYS, "EVENT_LIST is empty or not defined.  "
				"EventDaemon will do nothing.\n");
		if (tmp) free(tmp);
		return 0;
	}
	StringList event_list(tmp);
	free(tmp);

	NumEvents = event_list.number();
	EventList = new ScheduledEvent* [NumEvents];

	event_list.rewind();
	char *item;
	for (int offset = 0; (item = event_list.next()) != NULL; offset++) {
		tmp = param(item);
		if (!tmp) {
			dprintf(D_ALWAYS, "Event %s undefined!\n", item);
			EventList[offset] = NULL;
		} else {
			dprintf(D_FULLDEBUG, "%s: %s\n", item, tmp);
			EventList[offset] = NewEvent(item, tmp);
		}
		free(tmp);
	}

	tmp = param("EVENTD_INTERVAL");
	if (tmp) {
		EventInterval = atoi(tmp);
		free(tmp);
		dprintf(D_FULLDEBUG, "Event interval set to %d seconds.\n",
				EventInterval);
	} else {
		EventInterval = 900;
	}

	tmp = param("EVENTD_MAX_PREPARATION");
	if (tmp) {
		MaxEventPreparation = atoi(tmp);
		free(tmp);
		dprintf(D_FULLDEBUG, "Max event preparation set to %d minutes.\n",
				MaxEventPreparation);
		MaxEventPreparation *= 60; // convert to seconds
	} else {
		MaxEventPreparation = 0;
	}

	if (TimeoutTid >= 0) {
		daemonCore->Cancel_Timer(TimeoutTid);
	}

	TimeoutTid = daemonCore->Register_Timer(0, EventInterval,
											(Eventcpp)&EventDaemon::Timeout,
											"Timeout", this);

	return 0;
}

int
EventDaemon::Timeout()
{
	int TimeToNearestEvent = -1;
	bool EventActive = false;
	for (int ev=0; ev < NumEvents; ev++) {
		if (EventList[ev] && EventList[ev]->IsValid()) {
			if (EventList[ev]->IsActive()) {
				EventActive = true;
			} else {
				int TimeToEvent = EventList[ev]->TimeToEvent();
				if (TimeToNearestEvent > TimeToEvent ||
					TimeToNearestEvent == -1) {
					TimeToNearestEvent = TimeToEvent;
				}
				if (MaxEventPreparation == 0 ||
					TimeToEvent <= MaxEventPreparation) {
					int TimeNeeded = EventList[ev]->TimeNeeded();
					dprintf(D_FULLDEBUG, "%s: TimeToEvent=%d, TimeNeeded=%d\n",
							EventList[ev]->Name(), TimeToEvent, TimeNeeded);
					if (TimeNeeded + EventInterval >= TimeToEvent) {
						dprintf(D_ALWAYS, "%s: Activating...\n",
								EventList[ev]->Name());
						EventList[ev]->ActivateEvent();
					}
				} else {
					dprintf(D_FULLDEBUG, "%s: TimeToEvent=%d\n",
							EventList[ev]->Name(), TimeToEvent);
				}
			}
		}
	}
	if (MaxEventPreparation > 0 && !EventActive &&
		TimeToNearestEvent > MaxEventPreparation) {
		dprintf(D_FULLDEBUG, "All events are a long way off.\n");
		dprintf(D_FULLDEBUG, "Next Timeout() will occur in %d seconds.\n",
				TimeToNearestEvent-MaxEventPreparation);
		daemonCore->Reset_Timer(TimeoutTid,
								TimeToNearestEvent-MaxEventPreparation,
								EventInterval);
	}

	return 0;
}

ScheduledEvent *
EventDaemon::NewEvent(const char name[], const char record[])
{
	if (strncmp(record, "SHUTDOWN", 8) == 0) {
			// handle RUNTIME and STANDARD options
		bool RuntimeConfig = false;
		bool StandardJobsOnly = false;
		int offset = 8;
		while (record[offset] == '_') {
			offset++;
			if (strncmp(&(record[offset]), "RUNTIME", 7) == 0) {
				RuntimeConfig = true;
				offset += 7;
			} else if (strncmp(&(record[offset]), "STANDARD", 8) == 0) {
				StandardJobsOnly = true;
				offset += 8;
			}
		}
		return new ScheduledShutdownEvent(name, record+offset, RuntimeConfig,
										  StandardJobsOnly);
	}

	dprintf(D_ALWAYS, "Unknown event type in event %s.\n", name);
	return NULL;
}
