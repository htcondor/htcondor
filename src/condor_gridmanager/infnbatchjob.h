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


#ifndef INFNBATCHJOB_H
#define INFNBATCHJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"
#include "file_transfer.h"

#include "basejob.h"
#include "infnbatchresource.h"
#include "gahp-client.h"


void INFNBatchJobInit();
void INFNBatchJobReconfig();
BaseJob *INFNBatchJobCreate( ClassAd *jobad );
bool INFNBatchJobAdMatch( const ClassAd *job_ad );

class INFNBatchResource;

class INFNBatchJob : public BaseJob
{
 public:

	INFNBatchJob( ClassAd *classad );

	~INFNBatchJob();

	void Reconfig();
	void doEvaluateState();
	BaseResource *GetResource();

	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;

	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	// New variables
	int gmState;
	int remoteState;
	time_t enteredCurrentGmState;
	time_t enteredCurrentRemoteState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	int numStatusCheckAttempts;
	char *batchType;
	char *remoteSandboxId;
	char *remoteJobId;
	int lastPollTime;
	bool pollNow;

	Proxy *jobProxy;
	time_t remoteProxyExpireTime;

	INFNBatchResource *myResource;
	GahpClient *gahp;
	GahpClient *m_xfer_gahp;
	FileTransfer *m_filetrans;
	std::string m_sandboxPath;
	std::string m_xferId;

	void ProcessRemoteAd( ClassAd *remote_ad );

	void SetRemoteSandboxId( const char *sandbox_id );
	void SetRemoteJobId( const char *job_id );
	void SetRemoteIds( const char *sandbox_id, const char *job_id );
	ClassAd *buildSubmitAd();
	ClassAd *buildTransferAd();
	void CreateSandboxId();

	const char *BlahpJobDir();

		// If we're in the middle of a condor call that requires a ClassAd,
		// the ad is stored here (so that we don't have to reconstruct the
		// ad every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	ClassAd *gahpAd;
	std::string errorString;

 protected:
};

#endif

