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

#ifndef GLOBUSRESOURCE_H
#define GLOBUSRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "globusjob.h"
#include "gahp-client.h"


class GlobusJob;

class GlobusResource : public Service
{
 public:

	GlobusResource( const char *resource_name );
	~GlobusResource();

	void Reconfig();
	void RegisterJob( GlobusJob *job, bool already_submitted );
	void UnregisterJob( GlobusJob *job );
	void RequestPing( GlobusJob *job );
	bool RequestSubmit( GlobusJob *job );
	void SubmitComplete( GlobusJob *job );
	void CancelSubmit( GlobusJob *job );

	bool GridJobMonitorActive() { return monitorActive; }

	bool IsEmpty();
	bool IsDown();
	char *ResourceName();

	time_t getLastStatusChangeTime() { return lastStatusChange; }

	static const char *CanonicalName( const char *name );

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	static void setEnableGridMonitor( bool enable )
		{ enableGridMonitor = enable; }

 private:
	int DoPing();
	int CheckMonitor();
	void StopMonitor();
	bool SubmitMonitorJob();
	bool ReadMonitorJobStatusFile();
	int ReadMonitorLogFile();

	char *resourceName;
	bool resourceDown;
	bool firstPingDone;
	int pingTimerId;
	time_t lastPing;
	time_t lastStatusChange;
	Proxy *myProxy;
	List<GlobusJob> registeredJobs;
	List<GlobusJob> pingRequesters;
	// jobs that are currently executing a submit
	List<GlobusJob> submitsInProgress;
	// jobs that want to submit but can't due to submitLimit
	List<GlobusJob> submitsQueued;
	// jobs allowed to submit under jobLimit
	List<GlobusJob> submitsAllowed;
	// jobs that want to submit but can't due to jobLimit
	List<GlobusJob> submitsWanted;
	static int probeInterval;
	static int probeDelay;
	int submitLimit;		// max number of submit actions
	int jobLimit;			// max number of submitted jobs
	static int gahpCallTimeout;

	static bool enableGridMonitor;
	int checkMonitorTid;
	bool monitorActive;
	char *monitorJobStatusFile;
	char *monitorLogFile;
	int jobStatusFileLastReadTime;
	int logFileLastReadTime;

	GahpClient gahp;
};

#endif
