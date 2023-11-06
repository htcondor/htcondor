/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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


#ifndef ARCJOB_H
#define ARCJOB_H

#include "condor_common.h"
#include "condor_classad.h"

#include "basejob.h"
#include "arcresource.h"
#include "proxymanager.h"
#include "gahp-client.h"

void ArcJobInit();
void ArcJobReconfig();
BaseJob *ArcJobCreate( ClassAd *jobad );
bool ArcJobAdMatch( const ClassAd *job_ad );

class ArcResource;

class ArcJob : public BaseJob
{
 public:

	ArcJob( ClassAd *classad );

	~ArcJob();

	void Reconfig();
	void doEvaluateState(int timerID);
	BaseResource *GetResource();
	void SetRemoteJobId( const char *job_id );

	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int jobInfoInterval;

	static int m_arcGahpId;

	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	void NotifyNewRemoteStatus( const char *status );

	int gmState;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	time_t lastJobInfoTime;

	std::string errorString;
	char *resourceManagerString;

	char *remoteJobId;
	std::string remoteJobState;

	std::string m_tokenFile;
	Proxy *jobProxy;
	ArcResource *myResource;
	GahpClient *gahp;

	std::string delegationId;

		// If we're in the middle of a gahp call that requires an RSL,
		// the RSL is stored here (so that we don't have to reconstruct the
		// RSL every time we test the call for completion). It should be
		// cleared once the call completes.
	std::string RSL;
		// Same as for RSL, but used by the file staging calls.
	std::vector<std::string> *stageList;
	std::vector<std::string> *stageLocalList;

	bool buildJobADL();
	std::vector<std::string> *buildStageInList(bool with_urls);
	std::vector<std::string> *buildStageOutList();
	std::vector<std::string> *buildStageOutLocalList( std::vector<std::string> *stage_list );

 protected:
};

#endif

