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
#if !defined(_LOG_TRANSACTION_H)
#define _LOG_TRANSACTION_H

/*
   This defines a transaction log for data structure operations.
   Calling Commit() will log the operations to disk and perform
   the operations on the data structure in memory.  Destroying 
   the Transaction class aborts the transaction.  Users are
   encouraged to log a BeginTransaction record before calling
   Commit() and an EndTransaction record after calling Commit()
   to handle crashes in the middle of a commit.  This interface
   does not include BeginTransaction and EndTransaction records
   so they can be defined as deemed appropriate by the user.
*/

#include "log.h"

class LogPtrListEntry {
public:
	LogPtrListEntry(LogRecord *my_ptr) { ptr = my_ptr; next = 0; }
	~LogPtrListEntry() { delete ptr; }
	LogRecord *GetPtr() { return ptr; }
	LogPtrListEntry *GetNext() { return next; }
	void SetNext(LogPtrListEntry *n) { next = n; }
private:
	LogRecord	*ptr;
	LogPtrListEntry *next;
};

class LogPtrList {
public:
	LogPtrList() { head = 0; last = 0; last_find = 0;}
	~LogPtrList();
	void NewEntry(LogRecord *ptr);
	LogPtrListEntry *FindEntry(LogRecord *ptr);
	int IsInList(LogRecord *ptr) { return (FindEntry(ptr) != 0); }
	LogRecord *FirstEntry();
	LogRecord *NextEntry(LogRecord *ptr);
private:
	LogPtrListEntry *head;
	LogPtrListEntry *last;
	LogPtrListEntry *last_find;
};


class Transaction {
public:
	void Commit(int fd, void *data_structure);
	void AppendLog(LogRecord *);
	LogRecord *FirstEntry() { return op_log.FirstEntry(); }
	LogRecord *NextEntry(LogRecord *ptr) { return op_log.NextEntry(ptr); }
private:
	LogPtrList op_log;
};

#endif
