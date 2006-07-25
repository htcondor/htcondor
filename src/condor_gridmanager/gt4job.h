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

#ifndef GT4JOB_H
#define GT4JOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "globus_utils.h"
#include "classad_hashtable.h"

#include "proxymanager.h"
#include "basejob.h"
#include "gt4resource.h"
#include "gahp-client.h"
#include "gridftpmanager.h"

#define JM_COMMIT_TIMEOUT	600

class GT4Resource;
class GridftpServer;

void GT4JobInit();
void GT4JobReconfig();
BaseJob *GT4JobCreate( ClassAd *jobad );
bool GT4JobAdMatch( const ClassAd *job_ad );

class GT4Job : public BaseJob
{
 public:

	GT4Job( ClassAd *classad );

	~GT4Job();

	void Reconfig();
	int doEvaluateState();
	void UpdateGlobusState( int new_state, const char *new_fault );
	void GramCallback( const char *new_state, const char *new_fault,
					   const int new_exit_code );
	bool GetCallbacks();
	void ClearCallbacks();
	BaseResource *GetResource();
	void SetRemoteJobId( const char *job_id );

	static int probeInterval;
	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )
		{ maxConnectFailures = count; }

	// New variables
	int gmState;
	int globusState;
	MyString globusStateFaultString;
	int callbackGlobusState;
	MyString callbackGlobusStateFaultString;
	GT4Resource *myResource;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentGlobusState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	time_t jmProxyExpireTime;
	time_t jmLifetime;
	char *resourceManagerString;
	char * submit_id;
	char * jobmanagerType;
	GridftpServer *gridftpServer;

		// TODO should query these from GahpClient when needed
	char *gramCallbackContact;


	Proxy *jobProxy;
	GahpClient *gahp;

	MyString *buildSubmitRSL();
	void DeleteOutput();

	char *jobContact;
		// If we're in the middle of a globus call that requires an RSL,
		// the RSL is stored here (so that we don't have to reconstruct the
		// RSL every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	MyString *RSL;
	MyString errorString;
	char *localOutput;
	char *localError;
	bool streamOutput;
	bool streamError;
	bool stageOutput;
	bool stageError;
	MyString globusErrorString;

	int numGlobusSubmits;

	char * delegatedCredentialURI;

 protected:
	bool callbackRegistered;
	int connect_failure_counter;
	bool AllowTransition( int new_state, int old_state );
	const char * printXMLParam (const char * name, const char * value);
	const char * getDummyJobScratchDir();
};

#endif

