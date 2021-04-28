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


#ifndef CREAMJOB_H
#define CREAMJOB_H

#include "condor_common.h"
#include "condor_classad.h"

#include "proxymanager.h"
#include "basejob.h"
#include "creamresource.h"
#include "gahp-client.h"
#include "transferrequest.h"

class CreamResource;

void CreamJobInit();
void CreamJobReconfig();
BaseJob *CreamJobCreate( ClassAd *jobad );
bool CreamJobAdMatch( const ClassAd *job_ad );

class CreamJob : public BaseJob
{
 public:

	CreamJob( ClassAd *classad );

	~CreamJob();

	void Reconfig();
	void doEvaluateState();
	void NewCreamState( const char *new_state, int exit_code,
						const char *failure_reason );
	BaseResource *GetResource();
	void SetRemoteJobId( const char *job_id );

	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;

	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	void ProxyCallback();

	bool IsConnectionError( const char *msg );

	static std::string getFullJobId(const char * resourceManager, const char * job_id);

	// New variables
	int gmState;
	std::string remoteState;
	std::string remoteStateFaultString;
	CreamResource *myResource;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentRemoteState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	time_t jmProxyExpireTime;
	char *resourceManagerString;
	char *resourceBatchSystemString;
	char *resourceQueueString;
	char *uploadUrl;
	char *downloadUrl;
	int connectFailureCount;
	int m_numCleanupAttempts;

	Proxy *jobProxy;
	GahpClient *gahp;

	char *buildSubmitAd();

	char *remoteJobId;
		// If we're in the middle of a gahp call that requires a classad,
		// the ad is stored here (so that we don't have to reconstruct the
		// ad every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	char *creamAd;
	std::string errorString;
	char *localOutput;
	char *localError;
	bool streamOutput;
	bool streamError;
	bool stageOutput;
	bool stageError;
	std::string gahpErrorString;

	char * delegatedCredentialURI;
	char *leaseId;
	TransferRequest *m_xfer_request;

private:
	// If true, we should poll for status ourselves, _once_
	// instead of relying on CreamResource to handle it.
	bool doActivePoll;

	TransferRequest *MakeStageInRequest();
	TransferRequest *MakeStageOutRequest();
};

#endif

