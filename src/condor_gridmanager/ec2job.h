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

  
#ifndef EC2JOB_H
#define EC2JOB_H

#include "condor_common.h"
#include "condor_classad.h"
#include "classad_hashtable.h"

#include "basejob.h"
#include "ec2resource.h"
#include "proxymanager.h"
#include "gahp-client.h"
#include "vm_univ_utils.h"

void EC2JobInit();
void EC2JobReconfig();
BaseJob *EC2JobCreate( ClassAd *jobad );
bool EC2JobAdMatch( const ClassAd *job_ad );

class EC2Resource;

class EC2Job : public BaseJob
{
public:

	EC2Job( ClassAd *classad );
	~EC2Job();

	void Reconfig();
	void doEvaluateState();
	BaseResource *GetResource();
	void SetKeypairId( const char *keypair_id );
	void SetInstanceId( const char *instance_id );
	void SetClientToken( const char *client_token );
	void EC2SetRemoteJobId( const char *client_token, const char *instance_id );
	void SetRemoteVMName(const char * name);
	void SetRequestID( const char * requestID );
	
	static int submitInterval;
	static int gahpCallTimeout;
	static int maxConnectFailures;
	static int funcRetryInterval;
	static int pendingWaitTime;
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
	std::string m_remoteJobId;
	std::string remoteJobState;

	EC2Resource *myResource;
	GahpClient *gahp;

    void StatusUpdate( const char * instanceID,
                       const char * status,
                       const char * stateReasonCode,
                       const char * publicDNSName );

	friend class EC2Resource;

private:
	// create dynamic input parameters
	std::string build_ami_id();
	std::string build_client_token();
	std::string build_keypair();
	StringList* build_groupnames();

	std::string m_serviceUrl;
	
	std::string m_public_key_file;
	std::string m_private_key_file;
	std::string m_user_data;
	std::string m_user_data_file;
	std::string m_instance_type;
	std::string m_elastic_ip;
	std::string m_availability_zone;
	std::string m_ebs_volumes;
	std::string m_vpc_subnet;
	std::string m_vpc_ip;
	std::string m_key_pair;
	std::string m_key_pair_file;
	bool m_should_gen_key_pair;
	bool m_keypair_created;

	int m_vm_check_times;
	
	std::string m_ami_id;
	std::string m_client_token;
	StringList* m_group_names;
	
	std::string m_spot_price;
	std::string m_spot_request_id;
	// This is actually a global.
	const char * m_failure_injection;
	
	std::string m_state_reason_code;
	
	// remove created temporary keypair file
	bool remove_keypair_file(const char* filename);
	
	/**
	 * associate_n_attach - sends the gahp commands to associate addresses and volumes
	 * with a running instance.
	 */ 
	void associate_n_attach();

	// print out error codes returned from grid_manager
	void print_error_code( const char* error_code, const char* function_name );

    bool probeNow;
};

#endif
