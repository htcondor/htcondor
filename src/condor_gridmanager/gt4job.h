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
	void doEvaluateState();
	void UpdateGlobusState( const MyString &new_state,
							const MyString &new_fault );
	void GramCallback( const char *new_state, const char *new_fault,
					   const int new_exit_code );
	bool GetCallbacks();
	void ClearCallbacks();
	BaseResource *GetResource();
	void SetRemoteJobId( const char *job_id );

	bool SwitchToGram42();

	int ProxyCallback();

	static int probeInterval;
	static int submitInterval;
	static int gahpCallTimeout;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )
		{ gahpCallTimeout = new_timeout; }

	// New variables
	int gmState;
	MyString globusState;
	MyString globusStateFaultString;
	MyString callbackGlobusState;
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

	bool m_isGram42;

 protected:
	bool callbackRegistered;
	int connect_failure_counter;
	bool AllowTransition( const MyString &new_state,
						  const MyString &old_state );
	const char * printXMLParam (const char * name, const char * value);
	const char * getDummyJobScratchDir();
};

#endif

