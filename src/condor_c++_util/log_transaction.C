/* 
** Copyright 1996, 1997 by Condor
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Jim Pruyne
**
** Fixed on 7/8/97 by Derek Wright
** Modified by Jim Basney to implement general classad logging
**
*/ 

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