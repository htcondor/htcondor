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

#ifndef CREAMJOB_H
#define CREAMJOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "proxymanager.h"
#include "basejob.h"
#include "creamresource.h"
#include "gahp-client.h"
#include "gridftpmanager.h"

#define JM_COMMIT_TIMEOUT	600

class CreamResource;
class GridftpServer;

void CreamJobInit();
void CreamJobReconfig();
BaseJob *CreamJobCreate( ClassAd *jobad );
bool CreamJobAdMatch( const ClassAd *job_ad );

class CreamJob : public BaseJob
{
 public:

	CreamJob( ClassAd *classad );

	~CreamJob();

	void Reconfig();
	int doEvaluateState();
	void SetRemoteJobState( const char *new_state, int exit_code,
							const char *failure_reason );
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
	MyString remoteState;
	MyString remoteStateFaultString;
	CreamResource *myResource;
	time_t lastProbeTime;
	bool probeNow;
	time_t enteredCurrentGmState;
	time_t enteredCurrentRemoteState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;
	time_t jmProxyExpireTime;
	time_t jmLifetime;
	char *resourceManagerString;
	char *resourceBatchSystemString;
	char *resourceQueueString;
	char *uploadUrl;
	GridftpServer *gridftpServer;


	Proxy *jobProxy;
	GahpClient *gahp;

	ClassAd *buildSubmitAd();

	char *remoteJobId;
		// If we're in the middle of a gahp call that requires a classad,
		// the ad is stored here (so that we don't have to reconstruct the
		// ad every time we test the call for completion). It should be
		// freed and reset to NULL once the call completes.
	ClassAd *gahpAd;
	MyString errorString;
	char *localOutput;
	char *localError;
	bool streamOutput;
	bool streamError;
	bool stageOutput;
	bool stageError;
	MyString gahpErrorString;

	char * delegatedCredentialURI;

 protected:
	int connect_failure_counter;
};

#endif

