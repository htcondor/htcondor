#if !defined(_LOG_TRANSACTION_H)
#define _LOG_TRANSACTION_H

/*
   This defines a transaction log for data structure operations.
   Calling Commit() will log the operations to disk and perform
   the operations on the data structure in memory.  Destroying 
   the Transaction class aborts the transaction.
*/

#include "log.h"

class LogPtrListEntry {
public:
	LogPtrListEntry(LogRecord *my_ptr) { ptr = my_ptr; next = 0; }
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
