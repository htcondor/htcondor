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

    MaxCollectorLog = (tmp = param ("MAX_COLLECTOR_LOG")) ? atoi (tmp) : 64000;

    ClientTimeout = (tmp = param ("CLIENT_TIMEOUT")) ? atoi (tmp) : 30;

	QueryTimeout = (tmp = param ("QUERY_TIMEOUT")) ? atoi (tmp) : 60;

    MachineUpdateInterval = (tmp = param ("MACHINE_UPDATE_INTERVAL")) ?
		atoi (tmp) : 300;
    
	MasterCheckInterval = (tmp = param ("MASTER_CHECK_INTERVAL")) ? 
		atoi (tmp) : 10800; 	// three hours

	tmp = param ("CONDOR_DEVELOPERS");
	if (tmp == NULL) {
		tmp = "condor@cs.wisc.edu";
	} else
	if (strcmp (tmp, "NONE") == 0) {
		tmp = NULL;
	}
	CondorDevelopers = tmp;
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
