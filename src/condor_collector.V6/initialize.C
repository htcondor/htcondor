#include <iostream.h>
#include <stdio.h>

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"

// about self
static char *_FileName_ = __FILE__;

// prototypes
#ifndef WIN32
extern void reportToDevelopers (void);
extern "C" int schedule_event( int , int , int , int , int , void (*)(void));
#endif

// variables from the config file
extern char *Log;
extern char *CollectorLog;
extern char *CondorAdministrator;
extern char *CondorDevelopers;
extern int   MaxCollectorLog;
extern int   ClientTimeout; 
extern int   QueryTimeout;
extern int   MachineUpdateInterval;
extern int   MasterCheckInterval;


void
initializeParams()
{
    char *tmp;

	Log = param ("LOG");
	if (Log == NULL) 
	{
		EXCEPT ("Variable 'LOG' not found in config file.");
    }

    CollectorLog = param ("COLLECTOR_LOG");
	if (CollectorLog == NULL) 
	{
		EXCEPT ("Variable 'COLLECTOR_LOG' not found in config file.");
	}
		
    if ((CondorAdministrator = param ("CONDOR_ADMIN")) == NULL)
	{
		EXCEPT ("Variable 'CONDOR_ADMIN' not found in condfig file.");
	}

	tmp = param ("MAX_COLLECTOR_LOG");
	if( tmp ) {
		MaxCollectorLog = atoi( tmp );
		free( tmp );
	} else {
		MaxCollectorLog = 64000;
	}

	tmp = param ("CLIENT_TIMEOUT");
	if( tmp ) {
		ClientTimeout = atoi( tmp );
		free( tmp );
	} else {
		ClientTimeout = 30;
	}

	tmp = param ("QUERY_TIMEOUT");
	if( tmp ) {
		ClientTimeout = atoi( tmp );
		free( tmp );
	} else {
		ClientTimeout = 60;
	}

	tmp = param ("MACHINE_UPDATE_INTERVAL");
	if( tmp ) {
		ClientTimeout = atoi( tmp );
		free( tmp );
	} else {
		ClientTimeout = 300;
	}

	tmp = param ("MASTER_CHECK_INTERVAL");
	if( tmp ) {
		ClientTimeout = atoi( tmp );
		free( tmp );
	} else {
		ClientTimeout = 10800; 	// three hours
	}

	tmp = param ("CONDOR_DEVELOPERS");
	if (tmp == NULL) {
		CondorDevelopers = strdup("condor-admin@cs.wisc.edu");
	} else {
		if( strcmp(tmp, "NONE") == 0 ) {
			CondorDevelopers = NULL;
			free( tmp );
		} else {
			CondorDevelopers = tmp;
		}
	}
}


#ifndef WIN32
void
initializeReporter (void)
{
	const int STAR = -1;

	// schedule reports to developers
	schedule_event( STAR, 1,  0, 0, 0, reportToDevelopers );
	schedule_event( STAR, 8,  0, 0, 0, reportToDevelopers );
	schedule_event( STAR, 15, 0, 0, 0, reportToDevelopers );
	schedule_event( STAR, 23, 0, 0, 0, reportToDevelopers );
}
#endif
