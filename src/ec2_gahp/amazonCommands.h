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

#ifndef AMAZON_COMMANDS_H
#define AMAZON_COMMANDS_H

#include "condor_common.h"
#include "condor_string.h"
#include "MyString.h"
#include "string_list.h"

#include <map>

// EC2 Commands
#define AMAZON_COMMAND_VM_START             "EC2_VM_START"
#define AMAZON_COMMAND_VM_STOP              "EC2_VM_STOP"
#define AMAZON_COMMAND_VM_REBOOT            "EC2_VM_REBOOT"
#define AMAZON_COMMAND_VM_STATUS            "EC2_VM_STATUS"
#define AMAZON_COMMAND_VM_STATUS_ALL        "EC2_VM_STATUS_ALL"
#define AMAZON_COMMAND_VM_RUNNING_KEYPAIR   "EC2_VM_RUNNING_KEYPAIR"
#define AMAZON_COMMAND_VM_CREATE_GROUP      "EC2_VM_CREATE_GROUP"
#define AMAZON_COMMAND_VM_DELETE_GROUP      "EC2_VM_DELETE_GROUP"
#define AMAZON_COMMAND_VM_GROUP_NAMES       "EC2_VM_GROUP_NAMES"
#define AMAZON_COMMAND_VM_GROUP_RULES       "EC2_VM_GROUP_RULES"
#define AMAZON_COMMAND_VM_ADD_GROUP_RULE    "EC2_VM_ADD_GROUP_RULE"
#define AMAZON_COMMAND_VM_DEL_GROUP_RULE    "EC2_VM_DEL_GROUP_RULE"
#define AMAZON_COMMAND_VM_CREATE_KEYPAIR    "EC2_VM_CREATE_KEYPAIR"
#define AMAZON_COMMAND_VM_DESTROY_KEYPAIR   "EC2_VM_DESTROY_KEYPAIR"
#define AMAZON_COMMAND_VM_KEYPAIR_NAMES     "EC2_VM_KEYPAIR_NAMES"
#define AMAZON_COMMAND_VM_REGISTER_IMAGE    "EC2_VM_REGISTER_IMAGE"
#define AMAZON_COMMAND_VM_DEREGISTER_IMAGE  "EC2_VM_DEREGISTER_IMAGE"
#define AMAZON_COMMAND_VM_ASSOCIATE_ADDRESS "EC2_VM_ASSOCIATE_ADDRESS"
//#define AMAZON_COMMAND_VM_DISASSOCIATE_ADDRESS   "EC2_VM_DISASSOCIATE_ADDRESS"
#define AMAZON_COMMAND_VM_ATTACH_VOLUME		"EC_VM_ATTACH_VOLUME"
#define AMAZON_COMMAND_VM_CREATE_TAGS		"EC2_VM_CREATE_TAGS"
#define AMAZON_COMMAND_VM_SERVER_TYPE		"EC2_VM_SERVER_TYPE"

#define AMAZON_COMMAND_VM_START_SPOT        "EC2_VM_START_SPOT"
#define AMAZON_COMMAND_VM_STOP_SPOT         "EC2_VM_STOP_SPOT"
#define AMAZON_COMMAND_VM_STATUS_SPOT       "EC2_VM_STATUS_SPOT"
#define AMAZON_COMMAND_VM_STATUS_ALL_SPOT   "EC2_VM_STATUS_ALL_SPOT"

// S3 Commands
#define AMAZON_COMMAND_S3_ALL_BUCKETS       "AMAZON_S3_ALL_BUCKETS"
#define AMAZON_COMMAND_S3_CREATE_BUCKET     "AMAZON_S3_CREATE_BUCKET"
#define AMAZON_COMMAND_S3_DELETE_BUCKET     "AMAZON_S3_DELETE_BUCKET"
#define AMAZON_COMMAND_S3_LIST_BUCKET       "AMAZON_S3_LIST_BUCKET"
#define AMAZON_COMMAND_S3_UPLOAD_FILE       "AMAZON_S3_UPLOAD_FILE"
#define AMAZON_COMMAND_S3_UPLOAD_DIR        "AMAZON_S3_UPLOAD_DIR"
#define AMAZON_COMMAND_S3_DELETE_FILE       "AMAZON_S3_DELETE_FILE"
#define AMAZON_COMMAND_S3_DOWNLOAD_FILE     "AMAZON_S3_DOWNLOAD_FILE"
#define AMAZON_COMMAND_S3_DOWNLOAD_BUCKET   "AMAZON_S3_DOWNLOAD_BUCKET"

#define GENERAL_GAHP_ERROR_CODE             "GAHPERROR"
#define GENERAL_GAHP_ERROR_MSG              "GAHP_ERROR"

class AmazonRequest {
    public:
        AmazonRequest();
        virtual ~AmazonRequest();

        virtual bool SendRequest();

    protected:    
        typedef std::map< std::string, std::string > AttributeValueMap;
        AttributeValueMap query_parameters;
        
        std::string serviceURL;
        std::string accessKeyFile;
        std::string secretKeyFile;
        
        std::string errorMessage;
        std::string errorCode;
        
        std::string resultString;

		bool includeResponseHeader;
};

// EC2 Commands

class AmazonVMStart : public AmazonRequest {
	public:
		AmazonVMStart();
		virtual ~AmazonVMStart();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
        std::string instanceID;
};

class AmazonVMStartSpot : public AmazonVMStart {
    public:
        AmazonVMStartSpot();
        virtual ~AmazonVMStartSpot();
        
        virtual bool SendRequest();
        
        static bool ioCheck( char ** argv, int argc );
        static bool workerFunction( char ** argv, int argc, std::string & result_string );

    protected:
        std::string spotRequestID;
};

class AmazonVMStop : public AmazonRequest {
	public:
		AmazonVMStop();
		virtual ~AmazonVMStop();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonVMStopSpot : public AmazonVMStop {
    public:
        AmazonVMStopSpot();
        virtual ~AmazonVMStopSpot();
        
        // EC2_VM_STOP_SPOT uses the same argument structure as EC2_VM_STOP.
		// static bool ioCheck( char ** argv, int argc );
		static bool workerFunction( char ** argv, int argc, std::string & result_string );
};

#define AMAZON_STATUS_RUNNING "running"
#define AMAZON_STATUS_PENDING "pending"
#define AMAZON_STATUS_SHUTTING_DOWN "shutting-down"
#define AMAZON_STATUS_TERMINATED "terminated"

class AmazonStatusResult {
	public:
		std::string instance_id;
		std::string status;
		std::string ami_id;
		std::string public_dns;
		std::string private_dns;
		std::string keyname;
		std::string instancetype;
        std::string stateReasonCode;
        std::string clientToken;

        std::vector< std::string > securityGroups;
};

class AmazonVMStatusAll : public AmazonRequest {
	public:
		AmazonVMStatusAll();
		virtual ~AmazonVMStatusAll();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
        std::vector< AmazonStatusResult > results;
};

class AmazonVMStatus : public AmazonVMStatusAll {
	public:
		AmazonVMStatus();
		virtual ~AmazonVMStatus();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonStatusSpotResult {
    public:
        std::string state;
        std::string launch_group;
        std::string request_id;
        std::string instance_id;
        std::string status_code;
};

class AmazonVMStatusSpot : public AmazonVMStatus {
    public:
        AmazonVMStatusSpot();
        virtual ~AmazonVMStatusSpot();

        virtual bool SendRequest();
        
        // EC2_VM_STATUS_SPOT uses the same argument structure as EC2_VM_STATUS_SPOT.
		// static bool ioCheck( char ** argv, int argc );
		static bool workerFunction( char ** argv, int argc, std::string & result_string );

    protected:
        std::vector< AmazonStatusSpotResult > spotResults;
};

class AmazonVMStatusAllSpot : public AmazonVMStatusSpot {
    public:
        AmazonVMStatusAllSpot();
        virtual ~AmazonVMStatusAllSpot();

		static bool ioCheck( char ** argv, int argc );
		static bool workerFunction( char ** argv, int argc, std::string & result_string );
};

class AmazonVMRunningKeypair : public AmazonVMStatusAll {
	public:
		AmazonVMRunningKeypair();
		virtual ~AmazonVMRunningKeypair();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonVMCreateKeypair : public AmazonRequest {
	public:
		AmazonVMCreateKeypair();
		virtual ~AmazonVMCreateKeypair();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
    	std::string privateKeyFileName;
};

class AmazonVMDestroyKeypair : public AmazonRequest {
	public:
		AmazonVMDestroyKeypair();
		virtual ~AmazonVMDestroyKeypair();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonVMKeypairNames : public AmazonRequest {
	public:
		AmazonVMKeypairNames();
		virtual ~AmazonVMKeypairNames();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
        StringList keyNames;
};

class AmazonAssociateAddress : public AmazonRequest {
    public:
        AmazonAssociateAddress();
        virtual ~AmazonAssociateAddress();

        static bool ioCheck(char **argv, int argc);
        static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonCreateTags : public AmazonRequest {
    public:
        AmazonCreateTags();
        virtual ~AmazonCreateTags();

        static bool ioCheck(char **argv, int argc);
        static bool workerFunction(char **argv, int argc, std::string &result_string);
};

/**
 * AmazonAttachVolume - Will attempt to attach a running instance to an EBS volume
 * @see http://docs.amazonwebservices.com/AWSEC2/latest/APIReference/index.html?ApiReference-query-AttachVolume.html
 */
class AmazonAttachVolume : public AmazonRequest {
    public:
        AmazonAttachVolume();
        virtual ~AmazonAttachVolume();

        static bool ioCheck(char **argv, int argc);
        static bool workerFunction(char **argv, int argc, std::string &result_string);
};


class AmazonVMServerType : public AmazonRequest {
	public:
		AmazonVMServerType();
		virtual ~AmazonVMServerType();

		virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		std::string serverType;
};

#endif
