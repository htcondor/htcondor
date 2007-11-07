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


#ifndef ORACLEJOB_H
#define ORACLEJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "oci.h"

#include "basejob.h"
#include "oracleresource.h"

#define JOB_COMMIT_TIMEOUT	600

// Oracle queue job states
#define ORACLE_JOB_UNQUEUED		1
#define ORACLE_JOB_SUBMIT		2
#define ORACLE_JOB_BROKEN		3
#define ORACLE_JOB_IDLE			4
#define ORACLE_JOB_ACTIVE		5

class OciSession;

void OracleJobInit();
void OracleJobReconfig();
BaseJob *OracleJobCreate( ClassAd *jobad );
bool OracleJobAdMatch( const ClassAd *job_ad );

extern OCIEnv *GlobalOciEnvHndl;
extern OCIError *GlobalOciErrHndl;

void print_error( sword status, OCIError *error_handle );

class OracleJob : public BaseJob
{
 public:

	OracleJob( ClassAd *classad );

	~OracleJob();

	void Reconfig();
	int doEvaluateState();
	void UpdateGlobusState( int new_state, int new_error_code );
	BaseResource *GetResource();

	static int probeInterval;
	static int submitInterval;

	static void setProbeInterval( int new_interval )
		{ probeInterval = new_interval; }
	static void setSubmitInterval( int new_interval )
		{ submitInterval = new_interval; }

	int gmState;
	OciSession *ociSession;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	char *resourceManagerString;
	char *dbName;
	char *dbUsername;
	char *dbPassword;

	char *remoteJobId;
	bool jobRunPhase;
	int remoteJobState;

	OCIError *ociErrorHndl;

	bool newRemoteStateUpdate;
	int newRemoteState;

	void UpdateRemoteState( int new_state );

	char *doSubmit1();
	int doSubmit2();
	int doSubmit3();
	int doRemove();

 protected:
};

#endif

