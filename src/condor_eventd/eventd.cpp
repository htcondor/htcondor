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

	EventInterval = param_integer("EVENTD_INTERVAL",900);
	dprintf(D_FULLDEBUG, "Event interval set to %d seconds.\n",
			EventInterval);

	MaxEventPreparation = param_integer("EVENTD_MAX_PREPARATION",0);
	dprintf(D_FULLDEBUG, "Max event preparation set to %d minutes.\n",
			MaxEventPreparation);
	MaxEventPreparation *= 60; // convert to seconds

	if (TimeoutTid >= 0) {
		daemonCore->Cancel_Timer(TimeoutTid);
	}

	TimeoutTid = daemonCore->Register_Timer(0, EventInterval,
											(TimerHandlercpp)&EventDaemon::Timeout,
											"Timeout", this);

	return 0;
}

void
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
