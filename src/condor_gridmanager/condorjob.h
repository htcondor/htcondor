/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#ifndef CONDORJOB_H
#define CONDORJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "basejob.h"
#include "condorresource.h"
#include "gahp-client.h"


class CondorJob;

void CondorJobInit();
void CondorJobReconfig();
BaseJob *CondorJobCreate( ClassAd *jobad );
bool CondorJobAdMatch( const ClassAd *job_ad );

class CondorResource;

class CondorJob : public BaseJob
{
 public:

	CondorJob( ClassAd *classad );

	~CondorJob();

	void Reconfig();
	int doEvaluateState();
	BaseResource *GetResource();
	int JobLeaseSentExpired();

	static int submitInterval;
	static int removeInterval;
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
	time_t lastRemoveAttempt;
	int numSubmitAttempts;
	int submitFailureCode;
	char *remoteScheddName;
	char *remotePoolName;
	PROC_ID remoteJobId;
	char *submitterId;
	int connectFailureCount;

	Proxy *jobProxy;
	time_t remoteProxyExpireTime;
	time_t lastProxyRefreshAttempt;
	CondorResource *myResource;
	GahpClient *gahp;

	ClassAd *newRemoteStatusAd;
	int newRemoteStatusServerTime;
	int lastRemoteStatusServerTime;
	bool doActivePoll;

	void NotifyNewRemoteStatus( ClassAd *update_ad );

	void ProcessRemoteAd( ClassAd *remote_ad );

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

