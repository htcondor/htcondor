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

#ifndef UNICOREJOB_H
#define UNICOREJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "proxymanager.h"
#include "basejob.h"
#include "gt3resource.h"
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
	int doEvaluateState();
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

	int gmState;
	int unicoreState;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentUnicoreState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	int submitFailureCode;

	GahpClient *gahp;

	void UpdateUnicoreState( const char *update_ad_string );
	MyString *buildSubmitAd();

	void DeleteOutput();

	char *jobContact;
		// If we're in the middle of a job create call, the ad
		// string is stored here (so that we don't have to reconstruct
		// it every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	MyString *submitAd;
	MyString errorString;

};

#endif

