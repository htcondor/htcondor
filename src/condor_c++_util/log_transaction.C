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

 

#define _POSIX_SOURCE

#include "condor_common.h"

#include "log_transaction.h"

LogPtrList::~LogPtrList()
{
	LogPtrListEntry	*p, *q;
	p = head;
	while(p) {
		q = p->GetNext();
		delete p;
		p = q;
	}
}


void
LogPtrList::NewEntry(LogRecord *ptr)
{
	LogPtrListEntry *entry;

	entry = new LogPtrListEntry(ptr);
	if (head == 0) {
		head = entry;
	} else {
		last->SetNext(entry);
	}
	last = entry;
}


LogPtrListEntry *
LogPtrList::FindEntry(LogRecord *target)
{
	LogPtrListEntry *ptr;

	// We maintain a cache of one target and check it first to avoid searching
	// the entire list.
	if (last_find != 0) {
		if (last_find->GetPtr() == target) {
			return last_find;
		}
	}

	for (ptr = head; ptr != 0; ptr = ptr->GetNext()) {
		if (ptr->GetPtr() == target) {
			last_find = ptr;
			return last_find;
		}
	}
	return 0;
}


LogRecord *
LogPtrList::FirstEntry()
{
	if (head != 0) { 
		return head->GetPtr();
	} else {
		return 0;
	}
}


LogRecord *
LogPtrList::NextEntry(LogRecord *prev)
{
	LogPtrListEntry *entry;

	entry = FindEntry(prev);
	if (entry != 0) {
		if (entry->GetNext() == 0) {
			return 0;
		} else {
			last_find = entry->GetNext();
			return last_find->GetPtr();
		}
	} else {
		return 0;
	}
}


void
Transaction::Commit(int fd, void *data_structure)
{
	LogRecord		*log;

	for (log = op_log.FirstEntry(); log != 0; 
		 log = op_log.NextEntry(log)) {
		if (fd >= 0) log->Write(fd);
		log->Play(data_structure);
	}
	if (fd >= 0) fsync(fd);
}


void
Transaction::AppendLog(LogRecord *log)
{
	op_log.NewEntry(log);
}
