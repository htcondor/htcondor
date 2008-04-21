/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#ifndef GLOBUSRESOURCE_H
#define GLOBUSRESOURCE_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

#include "proxymanager.h"
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
	void UnregisterJob( GlobusJob *job );

	bool RequestJM( GlobusJob *job );
	void JMComplete( GlobusJob *job );
	void JMAlreadyRunning( GlobusJob *job );
	int GetJMLimit()
		{ return jmLimit; };

	bool GridJobMonitorActive() { return monitorActive; }
	int LastGridJobMonitorUpdate() { return jobStatusFileLastUpdate; }

	static const char *CanonicalName( const char *name );
	static const char *HashName( const char *resource_name,
								 const char *proxy_subject );

	static GlobusResource *FindOrCreateResource( const char *resource_name,
												 const char *proxy_subject );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	static void setEnableGridMonitor( bool enable )
		{ enableGridMonitor = enable; }
	static bool GridMonitorEnabled()
		{ return enableGridMonitor; }

	// This should be private, but GlobusJob references it directly for now
	static HashTable <HashKey, GlobusResource *> ResourcesByName;

 private:
	void DoPing( time_t& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );
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
	static int gahpCallTimeout;

	// jobs allowed allowed to have a jobmanager running
	List<GlobusJob> jmsAllowed;
	// jobs that want a jobmanager but can't due to jmLimit
	List<GlobusJob> jmsWanted;
	int jmLimit;		// max number of running jobmanagers

		// When true, we'll try to run a grid monitor on each gt2
		// gatekeeper.
	static bool enableGridMonitor;
	int checkMonitorTid;

		// monitorStarting and monitorActive should not be true at the
		// same time.

		// When true, the grid monitor is deemed a viable source of job
		// status information and jobmanager processes can be killed.
	bool monitorActive;
		// When true, we are trying to start the grid monitor for the
		// first time or after abandoning a previous attempt to run it.
		// The monitor is not yet ready for use.
	bool monitorStarting;
		// When true, a gram job request is in progress to launch the
		// grid monitor.
	bool monitorSubmitActive;

	char *monitorDirectory;
	char *monitorJobStatusFile;
	char *monitorLogFile;
		// These two timers default to the submission time of the
		// grid_monitor.  They're updated to time() whenever the grid_monitor
		// is resubmitted or when the time in question is read.
	int jobStatusFileLastReadTime;
	int logFileLastReadTime;

		// After giving up on the grid monitor (i.e. calling
		// AbandonMonitor()), this is the time at which we should try
		// again.
	int monitorRetryTime;

		// Very simily to logFileLastReadTime, but not updated every time the
		// grid_monitor is resubmitted.  As a result it reliably reports on the
		// last time we got some sort of result back.  Used for the "Thing have
		// been failing over and over for too long" timeout.
	int logFileTimeoutLastReadTime;

		// This reports the time we saw a new complete job status file
		// from the grid monitor (and therefore sent out job status
		// updates to the job objects). This time is not updated when the
		// grid-monitor is (re)submitted.
	time_t jobStatusFileLastUpdate;

		// This is the gram job contact string for the grid monitor job.
	char *monitorGramJobId;

	GahpClient *gahp;
	GahpClient *monitorGahp;
};

#endif
