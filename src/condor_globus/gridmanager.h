
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
	int ADD_JOBS_signalHandler( int = 0 );
	int SUBMIT_JOB_signalHandler( int );
	int REMOVE_JOBS_signalHandler( int = 0 );
	int CANCEL_JOB_signalHandler( int );
	int COMMIT_JOB_signalHandler( int );
	int RESTART_JM_signalHandler( int );
	void addRestartJM( GlobusJob * );
	int updateSchedd();
	int globusPoll();
	int jobProbe();
	int checkProxy();

	static void gramCallbackHandler( void *, char *, int, int );

	UserLog *InitializeUserLog( GlobusJob * );
	bool WriteExecuteToUserLog( GlobusJob * );
	bool WriteAbortToUserLog( GlobusJob * );
	bool WriteTerminateToUserLog( GlobusJob * );
	bool WriteEvictToUserLog( GlobusJob * );
	bool WriteGlobusSubmitEventToUserLog( GlobusJob * );
	bool WriteGlobusSubmitFailedEventToUserLog( GlobusJob * );
	bool WriteGlobusResourceUpEventToUserLog( GlobusJob * );
	bool WriteGlobusResourceDownEventToUserLog( GlobusJob * );


	// This is public because it needs to be accessible from main_init()
	char *gramCallbackContact;
	char *ScheddAddr;
	char *X509Proxy;
	bool useDefaultProxy;
	int m_cluster;
	int m_proc;

	HashTable <HashKey, GlobusJob *> *JobsByContact;
	HashTable <PROC_ID, GlobusJob *> *JobsByProcID;
	HashTable <HashKey, char *> *DeadMachines;

	List <GlobusJob> JobsToSubmit;
	List <GlobusJob> JobsToCancel;
	List <GlobusJob> JobsToCommit;
	List <GlobusJob> JMsToRestart;

	List <GlobusJob> WaitingToSubmit;
	List <GlobusJob> WaitingToCancel;
	List <GlobusJob> WaitingToCommit;
	List <GlobusJob> WaitingToRestart;

	List <char *> MachinesToProbe;
	List <GlobusJob> JobsToProbe;

 private:

	bool grabAllJobs;

	char *Owner;

	int checkProxy_tid;
	int checkProxy_interval;
	int minProxy_time;

	time_t Proxy_Expiration_Time;
	time_t Initial_Proxy_Expiration_Time;

	void cancelAllPendingEvents(GlobusJob *);
	bool stopJM(GlobusJob *);
	void RecordRebirth(char *rmContact);
	void RecordDeath(char *rmContact, char *second);

};

#endif
