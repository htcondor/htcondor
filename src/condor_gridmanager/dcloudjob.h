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

  
#ifndef DCLOUDJOB_H
#define DCLOUDJOB_H
  
#include "condor_common.h"
#include "condor_classad.h"
#include "MyString.h"
#include "classad_hashtable.h"

#include "basejob.h"
#include "dcloudresource.h"
#include "proxymanager.h"
#include "gahp-client.h"
#include "vm_univ_utils.h"

void DCloudJobInit();
void DCloudJobReconfig();
BaseJob *DCloudJobCreate( ClassAd *jobad );
bool DCloudJobAdMatch( const ClassAd *job_ad );

class DCloudResource;

class DCloudJob : public BaseJob
{
public:

	DCloudJob( ClassAd *classad );
	~DCloudJob();

	void Reconfig();
	void doEvaluateState();
	BaseResource *GetResource();
	void SetInstanceName( const char *instance_name );
	void SetInstanceId( const char *instance_id );
	void SetRemoteJobId( const char *instance_name, const char *instance_id );

	void StatusUpdate( const char *new_status );

	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int funcRetryInterval;
	static int pendingWaitTime;
	static int maxRetryTimes;
	
	static void setSubmitInterval( int new_interval )	{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )	{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )		{ maxConnectFailures = count; }

	static HashTable<HashKey, DCloudJob *> JobsByInstanceId;

	int gmState;
	bool probeNow;
	time_t probeErrorTime;
	time_t lastProbeTime;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	MyString errorString;
	MyString remoteJobState;

	DCloudResource *myResource;
	GahpClient *gahp;

private:
	char *m_serviceUrl;
	char *m_instanceId;
	char *m_instanceName;
	char *m_imageId;
	char *m_realmId;
	char *m_hwpId;
	char *m_hwpCpu;
	char *m_hwpMemory;
	char *m_hwpStorage;
	char *m_username;
	char *m_password;
	char *m_keyname;
	char *m_userdata;

	bool m_needstart;

	void ProcessInstanceAttrs( StringList &attrs );
	MyString build_instance_name( int max_length );
};

#endif
