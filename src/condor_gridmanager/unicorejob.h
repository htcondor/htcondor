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

