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

class GlobusResource : public BaseResource
{
 protected:
	GlobusResource( const char *resource_name, const char *proxy_subject );
	~GlobusResource();

 public:
	bool Init();
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
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static GlobusResource *FindOrCreateResource( const char *resource_name,
												 const char *proxy_subject );

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }

	static void setProbeDelay( int new_delay )
		{ probeDelay = new_delay; }

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	static void setEnableGridMonitor( bool enable )
		{ enableGridMonitor = enable; }

	// This should be private, but GlobusJob references it directly for now
	static HashTable <HashKey, GlobusResource *> ResourcesByName;

 private:
	int DoPing();
	int CheckMonitor();
	void StopMonitor();
	bool SubmitMonitorJob();
	void StopMonitorJob();
	void CleanupMonitorJob();
	enum ReadFileStatus { RFS_OK, RFS_PARTIAL, RFS_ERROR };
	ReadFileStatus ReadMonitorJobStatusFile();
	int ReadMonitorLogFile();
	void AbandonMonitor();

	bool initialized;

	char *proxySubject;
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
		// These two timers default to the submission time of the
		// grid_monitor.  They're updated to time() whenever the grid_monitor
		// is resubmitted or when the time in question is read.
	int jobStatusFileLastReadTime;
	int logFileLastReadTime;

		// Very simily to logFileLastReadTime, but not updated every time the
		// grid_monitor is resubmitted.  As a result it reliably reports on the
		// last time we got some sort of result back.  Used for the "Thing have
		// been failing over and over for too long" timeout.
	int logFileTimeoutLastReadTime;

		// When true, the monitor is submitted as if it was from scratch.
		// (Say, the first time, or after a _long_ delay after encountering
		// problems.)
	bool initialMonitorStart;

	GahpClient *gahp;
};

#endif
