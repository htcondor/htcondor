/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

 

#define _POSIX_SOURCE

#include "condor_common.h"
#include "log_transaction.h"
#include "condor_debug.h"

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
		 log = op_log.NextEntry(log)) 
	{
		if (fd >= 0) {
			if (log->Write(fd) < 0) {
				EXCEPT("write inside a transaction failed, errno = %d",errno);
			}
		}
		log->Play(data_structure);
	}
	if (fd >= 0) {
		if (fsync(fd) < 0) {
			EXCEPT("fsync inside a transaction failed, errno = %d",errno);
		}
	}
}


void
Transaction::AppendLog(LogRecord *log)
{
	op_log.NewEntry(log);
}
