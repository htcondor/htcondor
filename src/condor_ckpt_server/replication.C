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
#include "replication.h"
#include "condor_debug.h"

ReplicationEvent::ReplicationEvent()
{
	_prio = -1;
	memset((char *) &_server_addr, 0, sizeof(_server_addr));
	memset((char *) &_shadow_IP, 0, sizeof(_shadow_IP));
	_owner_name = _file = NULL;
	next = NULL;
}

ReplicationEvent::~ReplicationEvent()
{
	delete _owner_name;
	delete _file;
}

void ReplicationEvent::Owner(char *o)
{
	delete _owner_name;
	_owner_name = new char[strlen(o)+1];
	strcpy(_owner_name, o);
}

void ReplicationEvent::File(char *f)
{
	delete _file;
	_file = new char[strlen(f)+1];
	strcpy(_file, f);
}

ReplicationSchedule::ReplicationSchedule()
{
	_q = NULL;
}

ReplicationSchedule::~ReplicationSchedule()
{
	// should add code to clean up here if found to be necessary
}

int ReplicationSchedule::InsertReplicationEvent(ReplicationEvent *event)
{
	ReplicationNode *curr_rn = _q, *prev_rn = NULL;
	while (curr_rn != NULL && curr_rn->prio < event->Prio()) {
		prev_rn = curr_rn;
		curr_rn = curr_rn->next;
	}
	if (curr_rn == NULL || curr_rn->prio > event->Prio()) {
		ReplicationNode *new_rn = new ReplicationNode;
		new_rn->prio = event->Prio();
		new_rn->head = new_rn->tail = event;
		new_rn->next = curr_rn;
		if (prev_rn)
			prev_rn->next = new_rn;
		else 
			_q = new_rn;
		return 0;
	}
	else if (curr_rn->prio == event->Prio()) {
		curr_rn->tail->next = event;
		curr_rn->tail = event;
		return 0;
	}
	dprintf(D_ALWAYS, "InsertReplicationEvent failed!\n");
	return -1;
}

ReplicationEvent *ReplicationSchedule::GetNextReplicationEvent()
{
	if (_q == NULL) return NULL;

	// getting next event is easy -- head of queue
	ReplicationEvent *next_re = _q->head;

	// now remove next event from head of queue before returning
	// first case: removing last node of this priority
	if (_q->head == _q->tail) {
		ReplicationNode *old_rn = _q;
		_q = _q->next;
		delete old_rn;
	}
	// second case: more nodes of this priority remain
	else {
		_q->head = _q->head->next;
	}

	return next_re;
}
