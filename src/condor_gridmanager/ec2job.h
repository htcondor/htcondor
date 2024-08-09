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

#include "basejob.h"
#include "ec2resource.h"
#include "proxymanager.h"
#include "gahp-client.h"

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
	void doEvaluateState(int timerID);
	void NotifyResourceDown();
	void NotifyResourceUp();
	BaseResource *GetResource();

	void ResourceLeaseExpired(int timerID);

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
	static int maxRetryTimes;
	
	static void setSubmitInterval( int new_interval )	{ submitInterval = new_interval; }
	static void setGahpCallTimeout( int new_timeout )	{ gahpCallTimeout = new_timeout; }
	static void setConnectFailureRetry( int count )		{ maxConnectFailures = count; }

	int gmState;
	time_t lastProbeTime;
	time_t enteredCurrentGmState;
	time_t lastSubmitAttempt;
	int numSubmitAttempts;


	//
	// Hold reason codes were assigned as follows.  Hold reason sub codes
	// were assigned sequentially by order of their appearance in the code,
	// and as such are unique between hold codes.
	//
	//  37: (apparent) user error.  Example: an unspecified access key ID.
	//  38: internal error.  Example: the grid resource type was not "ec2";
	//      the GAHP should never have seen this job ad.
	//  39: administrator error.  Example: EC2_GAHP being unset.
	//  40: connection problem.  Example: E_CURL_IO.
	//  41: server error.  Example: an unrecognized or unexpected job state.
	//  42: INSTANCE MAY HAVE BEEN LOST.  Example: we tried and failed to
	//      cancel a spot instance request.
	//
	// Of these, only code 40 has a decent chance of working if tried again.
	// Code 41 may work, but probably won't (for example, the instance would
	// have to change to a recognized state).  Codes 37, 38, and 39 would need
	// to debugged; CODE 42 REQUIRES THE EC2 ACCOUNT OWNER TO INVESTIGATE.
	//
	int holdReasonCode;
	int holdReasonSubCode;


	std::string errorString;
	std::string m_remoteJobId;
	std::string remoteJobState;

	EC2Resource *myResource;
	EC2GahpClient *gahp;

    void StatusUpdate( const char * instanceID,
                       const char * status,
                       const char * stateReasonCode,
                       const char * publicDNSName,
                       const char * launchGroup );

	friend class EC2Resource;

private:
	// create dynamic input parameters
	std::string build_ami_id();
	std::string build_client_token();
	std::string build_keypair();
	std::vector<std::string>* build_groupnames();

	std::string m_serviceUrl;

	std::string m_iam_profile_arn;
	std::string m_iam_profile_name;
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
	bool m_was_job_completion;

	int m_retry_times;
	
	std::string m_ami_id;
	std::string m_client_token;
	std::string m_block_device_mapping;
	std::vector<std::string>* m_group_names;
	std::vector<std::string>* m_group_ids;
	std::vector<std::string>* m_parameters_and_values;
	
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

	int resourceLeaseTID;

	bool purgedTwice;
	bool updatedOnce;
};

#endif
