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
