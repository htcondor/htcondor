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


#ifndef MIRRORJOB_H
#define MIRRORJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "basejob.h"
#include "mirrorresource.h"
#include "gahp-client.h"


class MirrorJob;
extern HashTable <HashKey, MirrorJob *> MirrorJobsById;

void MirrorJobInit();
void MirrorJobReconfig();
BaseJob *MirrorJobCreate( ClassAd *jobad );
bool MirrorJobAdMatch( const ClassAd *job_ad );

class MirrorResource;

class MirrorJob : public BaseJob
{
 public:

	MirrorJob( ClassAd *classad );

	~MirrorJob();

	void Reconfig();
	int doEvaluateState();
	BaseResource *GetResource();

	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int leaseInterval;

	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }
	static void setLeaseInterval( int count )
		{ leaseInterval = count; }

	// New variables
	int gmState;
	int remoteState;
	time_t enteredCurrentGmState;
	time_t enteredCurrentRemoteState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	int submitFailureCode;
	char *mirrorScheddName;
	PROC_ID mirrorJobId;
	char *remoteJobIdString;
	bool mirrorActive;
	bool mirrorReleased;
	char *submitterId;

	MirrorResource *myResource;
	GahpClient *gahp;

	ClassAd *newRemoteStatusAd;
	int newRemoteStatusServerTime;
	int lastRemoteStatusServerTime;

	void NotifyNewRemoteStatus( ClassAd *update_ad );

	void ProcessRemoteAdInactive( ClassAd *remote_ad );
	void ProcessRemoteAdActive( ClassAd *remote_ad );

	void SetRemoteJobId( const char *job_id );
	ClassAd *buildSubmitAd();
	ClassAd *buildStageInAd();

		// If we're in the middle of a condor call that requires a ClassAd,
		// the ad is stored here (so that we don't have to reconstruct the
		// ad every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	ClassAd *gahpAd;
	MyString errorString;

 protected:
};

#endif

