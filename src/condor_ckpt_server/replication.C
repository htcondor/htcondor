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
