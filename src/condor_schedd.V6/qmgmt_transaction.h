#if !defined(_QMGMT_TRANSACTION_H)
#define _QMGMT_TRANSACTION_H

#include "log.h"
#include "qmgmt.h"

class PtrListEntry {
public:
	PtrListEntry(void *my_ptr) { ptr = my_ptr; next = 0; }
	void *GetPtr() { return ptr; }
	PtrListEntry *GetNext() { return next; }
	void SetNext(PtrListEntry *n) { next = n; }

private:
	void	*ptr;
	PtrListEntry *next;
};

class PtrList {
public:
	PtrList() { head = 0; last = 0; last_find = 0;}
	~PtrList();
	void NewEntry(void *ptr);
	void DeleteEntry(void *ptr);
	PtrListEntry *FindEntry(void *ptr);
	int IsInList(void *ptr) { return (FindEntry(ptr) != 0); }
	void *FirstEntry();
	void *NextEntry(void *);
private:
	PtrListEntry *head;
	PtrListEntry *last;
	PtrListEntry *last_find;
};


class Transaction {
public:
//	Transaction();
//	~Transaction();
	void Commit(Stream *);
	void Commit(int fd);
	void Rollback();
	void AppendLog(LogRecord *);
	void DirtyJob(Job *);
	void DirtyCluster(Cluster *);

private:
	void CommitDirty();
	void RollBackDirty();
	PtrList op_log;
	PtrList dirty_jobs;
	PtrList dirty_clusters;
};

#endif
