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


#ifndef GLOBUSJOB_H
#define GLOBUSJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"
#include "HashTable.h"

#include "proxymanager.h"
#include "basejob.h"
#include "globusresource.h"
#include "gahp-client.h"

#define JM_COMMIT_TIMEOUT	600

class GlobusResource;

class GlobusJob;
extern HashTable <std::string, GlobusJob *> JobsByContact;

char *globusJobId( const char *contact );
void gramCallbackHandler( void *user_arg, char *job_contact, int state,
						  int errorcode );

void GlobusJobInit();
void GlobusJobReconfig();
BaseJob *GlobusJobCreate( ClassAd *jobad );
bool GlobusJobAdMatch( const ClassAd *job_ad );

class GlobusJob : public BaseJob
{
 public:

	GlobusJob( ClassAd *classad );

	~GlobusJob();

	void Reconfig();
	void doEvaluateState();
	void CommunicationTimeout();
	void NotifyResourceDown();
	void NotifyResourceUp();
	void UpdateGlobusState( int new_state, int new_error_code );
	void GramCallback( int new_state, int new_error_code );
	bool GetCallbacks();
	void ClearCallbacks();
	BaseResource *GetResource();
	void GlobusSetRemoteJobId( const char *job_id, bool is_gt5 );

	/* If true, then ATTR_ON_EXIT_BY_SIGNAL, ATTR_ON_EXIT_SIGNAL, and
	   ATTR_ON_EXIT_CODE are valid.  If false, no exit status is available.
	   At the moment this only returns true for gridshell jobs.

	   This is virtual as a reminder for when we merge in Jaime's gridmanager
	   reorg.  GlobusJob derives from a base Job class.  I imagine that the
	   base job class will provide a virtual version that simply returns false,
	   and GlobusJob will return true if the grid shell is active.  Heck, maybe
	   gridshell jobs should be "class GlobusJobGridshell : public GlobusJob",
	   but that only makes sense if the gridshell is permenantly bound to
	   GlobusJob.
	*/
	virtual bool IsExitStatusValid();

	void ProxyCallback();

	static int submitInterval;
	static int restartInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int outputWaitGrowthTimeout;

	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setRestartInterval( int new_interval )
		{ restartInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	// New variables
	int gmState;
	int globusState;
	int globusStateErrorCode;
	int globusStateBeforeFailure;
	int callbackGlobusState;
	int callbackGlobusStateErrorCode;
	bool jmUnreachable;
	bool jmDown;
	GlobusResource *myResource;
	int communicationTimeoutTid;
	time_t lastProbeTime;
	time_t lastRemoteStatusUpdate;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentGlobusState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
		// Globus error code that caused the failure of the previous
		// submit attempt/submission
	int previousGlobusError;
	int lastRestartReason;
	time_t lastRestartAttempt;
	int numRestartAttempts;
	int numRestartAttemptsThisSubmit;
	time_t jmProxyExpireTime;
	bool proxyRefreshRefused;
	time_t outputWaitLastGrowth;
	int outputWaitOutputSize;
	int outputWaitErrorSize;
	// HACK!
	bool retryStdioSize;
	char *resourceManagerString;
	bool useGridJobMonitor;

		// TODO should query these from GahpClient when needed
	char *gassServerUrl;
	char *gramCallbackContact;


	Proxy *jobProxy;
	GahpClient *gahp;

	std::string *buildSubmitRSL();
	std::string *buildRestartRSL();
	std::string *buildStdioUpdateRSL();
	bool GetOutputSize( int& output, int& error );
	void DeleteOutput();

	char *jobContact;
		// If we're in the middle of a globus call that requires an RSL,
		// the RSL is stored here (so that we don't have to reconstruct the
		// RSL every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	std::string *RSL;
	std::string errorString;
	char *localOutput;
	char *localError;
	bool streamOutput;
	bool streamError;
	bool stageOutput;
	bool stageError;
	int globusError;

	bool restartingJM;
	time_t restartWhen;

	std::string outputClassadFilename;
	bool useGridShell;

	int jmShouldBeStoppingTime;

	bool m_lightWeightJob;

 protected:
	bool callbackRegistered;
	int connect_failure_counter;
	bool AllowTransition( int new_state, int old_state );

	bool RetryFailureOnce( int error_code );
	bool RetryFailureAlways( int error_code );
//	bool FailureIsRestartable( int error_code );
	bool FailureNeedsCommit( int error_code );
	bool JmShouldSleep();

private:
	// Copy constructor not implemented.  Don't call.
	GlobusJob( GlobusJob& copy );

	bool mergedGridShellOutClassad;
};

bool WriteGlobusSubmitEventToUserLog( ClassAd *job_ad );
bool WriteGlobusSubmitFailedEventToUserLog( ClassAd *job_ad,
											int failure_code,
											const char *failure_mesg);

const char *rsl_stringify( const std::string& src );
const char *rsl_stringify( const char *string );

#endif

