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
#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "collector_engine.h"
#include "condor_io.h"
#include "sched.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// about self
static char *_FileName_ = __FILE__;

// prototypes
#ifndef WIN32
extern void reportToDevelopers (void);
extern "C" int schedule_event( int , int , int , int , int , void (*)(void));
#endif

// variables from the config file
extern char *CondorAdministrator;
extern char *CondorDevelopers;
extern int   ClientTimeout;
extern int   QueryTimeout;
extern char *CollectorName;

extern CollectorEngine collector;
extern SafeSock updateSock;
extern int sendCollectorAd();
extern void init_classad(int interval);

static int UpdateTimerId = -1;

void
initializeParams()
{
    char *tmp;
	char *tmp1;
	int		ClassadLifetime;
	int		MasterCheckInterval;
	int i;

	if (CondorAdministrator) free (CondorAdministrator);
    if ((CondorAdministrator = param ("CONDOR_ADMIN")) == NULL)
	{
		EXCEPT ("Variable 'CONDOR_ADMIN' not found in condfig file.");
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
		QueryTimeout = atoi( tmp );
		free( tmp );
	} else {
		QueryTimeout = 60;
	}

	tmp = param ("CLASSAD_LIFETIME");
	if( tmp ) {
		ClassadLifetime = atoi( tmp );
		free( tmp );
	} else {
		ClassadLifetime = 900;
	}

	tmp = param ("MASTER_CHECK_INTERVAL");
	if( tmp ) {
		MasterCheckInterval = atoi( tmp );
		free( tmp );
	} else {
		MasterCheckInterval = 10800; 	// three hours
	}

	if (CondorDevelopers) free (CondorDevelopers);
	tmp = param ("CONDOR_DEVELOPERS");
	if (tmp == NULL) {
		tmp = strdup("condor-admin@cs.wisc.edu");
	} else
	if (stricmp (tmp, "NONE") == 0) {
		free (tmp);
		tmp = NULL;
	}
	CondorDevelopers = tmp;


	if (CollectorName) free (CollectorName);
	CollectorName = param("COLLECTOR_NAME");

	// handle params for Collector updates
	if ( UpdateTimerId >= 0 ) {
		daemonCore->Cancel_Timer(UpdateTimerId);
		UpdateTimerId = -1;
	}
	updateSock.close();
	tmp = param ("CONDOR_DEVELOPERS_COLLECTOR");
	if (tmp == NULL) {
		tmp = strdup("condor.cs.wisc.edu");
	}
	if (stricmp(tmp,"NONE") == 0 ) {
		free(tmp);
		tmp = NULL;
	}
	tmp1 = param("COLLECTOR_UPDATE_INTERVAL");
	if ( tmp1 ) {
		i = atoi(tmp1);
	} else {
		i = 900;		// default to 15 minutes
	}
	if ( tmp && i ) {
		if ( updateSock.connect(tmp,COLLECTOR_PORT) == TRUE ) {
			UpdateTimerId = daemonCore->Register_Timer(1,i,
				(TimerHandler)sendCollectorAd, "sendCollectorAd");
			init_classad(i);
		}
	}
	if (tmp)
		free(tmp);
	if (tmp1)
		free(tmp1);

	// set the appropriate parameters in the collector engine
	collector.setClientTimeout( ClientTimeout );
	collector.scheduleHousekeeper( ClassadLifetime );
	collector.scheduleDownMasterCheck( MasterCheckInterval );
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
