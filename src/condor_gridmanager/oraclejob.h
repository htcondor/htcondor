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

