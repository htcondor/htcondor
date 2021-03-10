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
#include "condor_daemon_core.h"
#include <map>

#include "proxymanager.h"
#include "baseresource.h"
#include "gahp-client.h"

#define DEFAULT_GM_DISABLE_LENGTH (60*60)

class GlobusJob;
class GlobusResource;

class GlobusResource : public BaseResource
{
 protected:
	GlobusResource( const char *resource_name, const Proxy *proxy,
					bool is_gt5 );
	~GlobusResource();

 public:
	bool Init();
	const char *ResourceType();
	void Reconfig();
	void UnregisterJob( BaseJob *job );

	bool IsGt5() const { return m_isGt5; }

	bool RequestJM( GlobusJob *job, bool is_submit );
	void JMComplete( GlobusJob *job );
	void JMAlreadyRunning( GlobusJob *job );
	int GetJMLimit( bool for_submit )
		{ return for_submit ? submitJMLimit : restartJMLimit; };

	bool GridJobMonitorActive() const { return monitorActive; }
	int LastGridJobMonitorUpdate() const { return jobStatusFileLastUpdate; }
	bool GridMonitorFirstStartup() const { return monitorFirstStartup; }

	static const char *CanonicalName( const char *name );
	static std::string & HashName( const char *resource_name,
								 const char *proxy_subject );

	static GlobusResource *FindOrCreateResource( const char *resource_name,
												 const Proxy *proxy,
												 bool is_gt5 );

	const char *GetHashName();

	void PublishResourceAd( ClassAd *resource_ad );

	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	static void setEnableGridMonitor( bool enable )
		{ enableGridMonitor = enable; }
	static bool GridMonitorEnabled()
		{ return enableGridMonitor; }
	static void setGridMonitorDisableLength( int len )
	{ monitorDisableLength = len; }

	void gridMonitorCallback( int state, int errorcode );

	// This should be private, but GlobusJob references it directly for now
	static std::map <std::string, GlobusResource *> ResourcesByName;

		// This is the gram job contact string for the grid monitor job.
	char *monitorGramJobId;

	virtual void SetJobPollInterval();

 private:
	void DoPing( unsigned& ping_delay, bool& ping_complete,
				 bool& ping_succeeded  );
	void CheckMonitor();
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
	char *proxyFQAN;
	static int gahpCallTimeout;

	bool m_isGt5;
	bool m_versionKnown;

	// We limit the number of jobmanagers we're willing to run at a time
	// on each resource. We keep seperate queues for initial job
	// submission and restarts for jobs already submitted. Each queue
	// is guaranteed half of the overall limit. If either queue isn't
	// using its full share, the other queue can borrow the unused slots
	// until the original queue needs them again.

	// jobs allowed allowed to have a jobmanager running for submission
	List<GlobusJob> submitJMsAllowed;
	// jobs that want a jobmanager for submission but can't due to jmLimit
	List<GlobusJob> submitJMsWanted;
	// max number of running jobmanagers for submission
	int submitJMLimit;

	// jobs allowed allowed to have a jobmanager running for restart
	List<GlobusJob> restartJMsAllowed;
	// jobs that want a jobmanager for restart but can't due to jmLimit
	List<GlobusJob> restartJMsWanted;
	// max number of running jobmanagers for restart
	int restartJMLimit;

		// When true, we'll try to run a grid monitor on each gt2
		// gatekeeper.
	static bool enableGridMonitor;
	static int monitorDisableLength;
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
	bool monitorFirstStartup;

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

		// This reports the time we saw a new complete job status file
		// from the grid monitor (and therefore sent out job status
		// updates to the job objects). This time is not updated when the
		// grid-monitor is (re)submitted.
	time_t jobStatusFileLastUpdate;

	int monitorGramJobStatus;
	int monitorGramErrorCode;

	GahpClient *gahp;
	GahpClient *monitorGahp;
};

#endif
