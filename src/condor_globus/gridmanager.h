
#ifndef GRIDMANAGER_H
#define GRIDMANAGER_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "user_log.c++.h"
#include "classad_hashtable.h"
#include "list.h"

#include "globusjob.h"

// For developmental testing
extern int test_mode;


class GridManager : public Service
{
 public:

	GridManager();
	~GridManager();

	// initialization
	void Init();
	void Register();

	// maintainence
	void reconfig();
	
	// handlers
	int ADD_JOBS_signalHandler( int );
	int REMOVE_JOBS_signalHandler( int );
	int updateSchedd();
	int globusPoll();
	int jobProbe();

	static void gramCallbackHandler( void *, char *, int, int );

	UserLog *InitializeUserLog( GlobusJob * );
	bool WriteExecuteToUserLog( GlobusJob * );
	bool WriteAbortToUserLog( GlobusJob * );
	bool WriteTerminateToUserLog( GlobusJob * );


	// This is public because it needs to be accessible from main_init()
	char *gramCallbackContact;
	char *ScheddAddr;

 private:

	bool grabAllJobs;

	char *Owner;

	HashTable <HashKey, GlobusJob *> *JobsByContact;
	HashTable <PROC_ID, GlobusJob *> *JobsByProcID;

};

#endif
