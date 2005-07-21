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

#define JM_COMMIT_TIMEOUT	600

class GT4Resource;

class GT4Job;
extern HashTable <HashKey, GT4Job *> GT4JobsByContact;

const char *gt4JobId( const char *contact );

void GT4JobInit();
void GT4JobReconfig();
bool GT4JobAdMustExpand( const ClassAd *jobad );
BaseJob *GT4JobCreate( ClassAd *jobad );
extern const char *GT4JobAdConst;

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
	

		// TODO should query these from GahpClient when needed
	char *gramCallbackContact;


	Proxy *jobProxy;
	GahpClient *gahp;

	void SetJobContact( const char *job_contact );
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

