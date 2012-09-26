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


#ifndef UNICOREJOB_H
#define UNICOREJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "proxymanager.h"
#include "basejob.h"
#include "gahp-client.h"


void UnicoreJobInit();
void UnicoreJobReconfig();
BaseJob *UnicoreJobCreate( ClassAd *jobad );
bool UnicoreJobAdMatch( const ClassAd *job_ad );

class UnicoreJob : public BaseJob
{
 public:

	UnicoreJob( ClassAd *classad );

	~UnicoreJob();

	void Reconfig();
	void doEvaluateState();
	BaseResource *GetResource();

	static int probeInterval;
	static int submitInterval;
	static int gahpCallTimeout;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	static HashTable<HashKey, UnicoreJob *> JobsByUnicoreId;

	static void UnicoreGahpCallbackHandler( const char *update_ad_string );

	int gmState;
	int unicoreState;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentUnicoreState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	int submitFailureCode;
	ClassAd *newRemoteStatusAd;

	GahpClient *gahp;

	void UpdateUnicoreState( const char *update_ad_string );
	void UpdateUnicoreState( ClassAd *update_ad );
	std::string *buildSubmitAd();
	void SetRemoteJobId( const char *job_id );

	void DeleteOutput();

	char *resourceName;
	char *jobContact;
		// If we're in the middle of a job create call, the ad
		// string is stored here (so that we don't have to reconstruct
		// it every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	std::string *submitAd;
	std::string errorString;

};

#endif

