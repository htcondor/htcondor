/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef INFNBATCHJOB_H
#define INFNBATCHJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "basejob.h"
#include "gahp-client.h"


class INFNBatchJob;
extern HashTable <HashKey, INFNBatchJob *> INFNBatchJobsById;

void INFNBatchJobInit();
void INFNBatchJobReconfig();
bool INFNBatchJobAdMustExpand( const ClassAd *jobad );
BaseJob *INFNBatchJobCreate( ClassAd *jobad );
extern const char *INFNBatchJobAdConst;

//class CondorResource;

class INFNBatchJob : public BaseJob
{
 public:

	INFNBatchJob( ClassAd *classad );

	~INFNBatchJob();

	void Reconfig();
	int doEvaluateState();
	BaseResource *GetResource();

	static int pollInterval;
	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;

	static void setPollInterval( int new_interval )
		{ pollInterval = new_interval; }
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
	char *remoteJobId;
	int lastPollTime;
	bool pollNow;

	GahpClient *gahp;

	void ProcessRemoteAd( ClassAd *remote_ad );

	void SetRemoteJobId( const char *job_id );
	ClassAd *buildSubmitAd();

		// If we're in the middle of a condor call that requires a ClassAd,
		// the ad is stored here (so that we don't have to reconstruct the
		// ad every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	ClassAd *gahpAd;
	MyString errorString;

 protected:
};

#endif

