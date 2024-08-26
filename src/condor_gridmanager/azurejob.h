/***************************************************************
 *
 * Copyright (C) 1990-2017, Condor Team, Computer Sciences Department,
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

  
#ifndef AZUREJOB_H
#define AZUREJOB_H

#include "condor_common.h"
#include "condor_classad.h"

#include "basejob.h"
#include "azureresource.h"
#include "proxymanager.h"
#include "gahp-client.h"

void AzureJobInit();
void AzureJobReconfig();
BaseJob *AzureJobCreate( ClassAd *jobad );
bool AzureJobAdMatch( const ClassAd *job_ad );

class AzureResource;

class AzureJob : public BaseJob
{
public:

	AzureJob( ClassAd *classad );
	~AzureJob();

	void Reconfig();
	void doEvaluateState(int timerID);
	BaseResource *GetResource();
	void SetRemoteJobId( const char *vm_name );
	
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
	std::string m_vmName;
	std::string remoteJobState;

	AzureResource *myResource;
	GahpClient *gahp;

    void StatusUpdate( const char *status );

	friend class AzureResource;

private:
	// create dynamic input parameters
	std::string build_vm_name();

	// Assemble parameters for AZURE_VM_CREATE command
	bool BuildVmParams();

	std::string m_subscription;
	std::string m_location;
	std::string m_authFile;

	int m_retry_times;
	
	std::vector<std::string> m_vmParams;

	// This is actually a global.
	const char * m_failure_injection;

	bool probeNow;
};

#endif
