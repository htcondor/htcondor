/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#ifndef GLOBUSRESOURCE_H
#define GLOBUSRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
#include "globusjob.h"
#include "baseresource.h"
#include "gahp-client.h"


class GlobusJob;
class GlobusResource;

extern HashTable <HashKey, GlobusResource *> ResourcesByName;

class GlobusResource : public BaseResource
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

	bool resourceDown;
	bool firstPingDone;
	int pingTimerId;
	time_t lastPing;
	time_t lastStatusChange;
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

	GahpClient *gahp;
};

#endif
