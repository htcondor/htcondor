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

