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

  
#ifndef GCEJOB_H
#define GCEJOB_H

#include "condor_common.h"
#include "condor_classad.h"

#include "basejob.h"
#include "gceresource.h"
#include "proxymanager.h"
#include "gahp-client.h"

void GCEJobInit();
void GCEJobReconfig();
BaseJob *GCEJobCreate( ClassAd *jobad );
bool GCEJobAdMatch( const ClassAd *job_ad );

class GCEResource;

class GCEJob : public BaseJob
{
public:

	GCEJob( ClassAd *classad );
	~GCEJob();

	void Reconfig();
	void doEvaluateState();
	BaseResource *GetResource();
	void SetInstanceId( const char *instance_id );
	void SetInstanceName( const char *instance_name );
	void GCESetRemoteJobId( const char *instance_name, const char *instance_id );
	void SetRemoteVMName(const char * name);
	
	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int funcRetryInterval;
	static int maxRetryTimes;
	
	static void setSubmitInterval( int new_interval )	{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )	{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )		{ maxConnectFailures = count; }

	int gmState;
	time_t lastProbeTime;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;

	std::string errorString;
	std::string m_instanceId;
	std::string remoteJobState;

	GCEResource *myResource;
	GahpClient *gahp;

    void StatusUpdate( const char * instanceID,
                       const char * status,
                       const char * stateReasonCode,
                       const char * publicDNSName );

	friend class GCEResource;

private:
	// create dynamic input parameters
	std::string build_instance_name();

	std::string m_serviceUrl;
	
	std::string m_project;
	std::string m_zone;
	std::string m_authFile;
	std::string m_account;
	std::string m_image;
	std::string m_metadata;
	std::string m_metadataFile;
	std::string m_machineType;
	bool m_preemptible;
	std::string m_jsonFile;
	std::vector< std::pair< std::string, std::string > > m_labels;

	int m_retry_times;
	
	std::string m_instanceName;
	
	// This is actually a global.
	const char * m_failure_injection;
	
	std::string m_state_reason_code;
	
	bool probeNow;
};

#endif
