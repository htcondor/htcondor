/* 
** Copyright 1996 by Miron Livny, and Jim Pruyne
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
**
*/ 

#define _POSIX_SOURCE

#include "condor_common.h"

#include "qmgmt_transaction.h"

PtrList::~PtrList()
{
	PtrListEntry	*p, *q;
	p = head;
	while(p) {
		q = p->GetNext();
		delete p;
		p = q;
	}
}


void
PtrList::NewEntry(void *ptr)
{
	PtrListEntry *entry;

	entry = new PtrListEntry(ptr);
	if (head == 0) {
		head = entry;
	} else {
		last->SetNext(entry);
	}
	last = entry;
}


void
PtrList::DeleteEntry(void *ptr)
{
}


PtrListEntry *
PtrList::FindEntry(void *target)
{
	PtrListEntry *ptr;

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


void *
PtrList::FirstEntry()
{
	if (head != 0) { 
		return head->GetPtr();
	} else {
		return 0;
	}
}


void *
PtrList::NextEntry(void *prev)
{
	PtrListEntry *entry;

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
Transaction::Commit(Stream *s)
{
	LogRecord		*log;

	for (log = (LogRecord *) op_log.FirstEntry(); log != 0; 
		 log = (LogRecord *) op_log.NextEntry(log)) {
		log->Write(s);
	}
	CommitDirty();
}


void
Transaction::Commit(int fd)
{
	LogRecord		*log;

	for (log = (LogRecord *) op_log.FirstEntry(); log != 0; 
		 log = (LogRecord *) op_log.NextEntry(log)) {
		log->Write(fd);
	}
	fsync(fd);
	CommitDirty();
}


void
Transaction::Rollback()
{
	RollBackDirty();
}


void
Transaction::AppendLog(LogRecord *log)
{
	op_log.NewEntry(log);
}


void
Transaction::DirtyJob(Job *job)
{
	if (job != 0) {
		if (!dirty_jobs.IsInList(job)) {
			dirty_jobs.NewEntry(job);
		}
	}
}


void
Transaction::DirtyCluster(Cluster *cl)
{
	if (cl != 0) {
		if (!dirty_clusters.IsInList(cl)) {
			dirty_clusters.NewEntry(cl);
		}
	}
}


void
Transaction::CommitDirty()
{
	Cluster *cl;
	Job		*job;

	for (cl = (Cluster *) dirty_clusters.FirstEntry(); cl != 0; 
		 cl = (Cluster *) dirty_clusters.NextEntry(cl)) {
		cl->Commit();
	}

	for (job = (Job *) dirty_jobs.FirstEntry(); job != 0; 
		 job = (Job *) dirty_jobs.NextEntry(job)) {
		job->Commit();
	}
}


void
Transaction::RollBackDirty()
{
	Cluster *cl;
	Job		*job;

	for (cl = (Cluster *) dirty_clusters.FirstEntry(); cl != 0; 
		 cl = (Cluster *) dirty_clusters.NextEntry(cl)) {
		cl->Rollback();
	}

	for (job = (Job *) dirty_jobs.FirstEntry(); job != 0; 
		 job = (Job *) dirty_jobs.NextEntry(job)) {
		job->Rollback();
	}
}

