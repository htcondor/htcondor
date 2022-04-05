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
#define AMAZON_COMMAND_VM_ATTACH_VOLUME     "EC2_VM_ATTACH_VOLUME"
#define AMAZON_COMMAND_VM_CREATE_TAGS       "EC2_VM_CREATE_TAGS"
#define AMAZON_COMMAND_VM_SERVER_TYPE       "EC2_VM_SERVER_TYPE"

#define AMAZON_COMMAND_VM_START_SPOT        "EC2_VM_START_SPOT"
#define AMAZON_COMMAND_VM_STOP_SPOT         "EC2_VM_STOP_SPOT"
#define AMAZON_COMMAND_VM_STATUS_SPOT       "EC2_VM_STATUS_SPOT"
#define AMAZON_COMMAND_VM_STATUS_ALL_SPOT   "EC2_VM_STATUS_ALL_SPOT"

// For condor_annex.
#define AMAZON_COMMAND_BULK_START           "EC2_BULK_START"
#define AMAZON_COMMAND_PUT_RULE             "CWE_PUT_RULE"
#define AMAZON_COMMAND_PUT_TARGETS          "CWE_PUT_TARGETS"
#define AMAZON_COMMAND_BULK_STOP            "EC2_BULK_STOP"
#define AMAZON_COMMAND_DELETE_RULE          "CWE_DELETE_RULE"
#define AMAZON_COMMAND_REMOVE_TARGETS       "CWE_REMOVE_TARGETS"
#define AMAZON_COMMAND_GET_FUNCTION         "AWS_GET_FUNCTION"
#define AMAZON_COMMAND_S3_UPLOAD            "S3_UPLOAD"
#define AMAZON_COMMAND_CF_CREATE_STACK      "CF_CREATE_STACK"
#define AMAZON_COMMAND_CF_DESCRIBE_STACKS   "CF_DESCRIBE_STACKS"
#define AMAZON_COMMAND_CALL_FUNCTION        "AWS_CALL_FUNCTION"
#define AMAZON_COMMAND_BULK_QUERY           "EC2_BULK_QUERY"


#define GENERAL_GAHP_ERROR_CODE             "GAHPERROR"
#define GENERAL_GAHP_ERROR_MSG              "GAHP_ERROR"

class AmazonRequest {
    public:
        AmazonRequest( int i, const char * c, int sv = 4 ) :
            responseCode(0), includeResponseHeader(false), 
			requestID(i), requestCommand(c),
            signatureVersion(sv), httpVerb( "POST" ) {
				mutexReleased = lockGained = requestBegan = requestEnded =
					signatureTime = mutexGained = sleepBegan = liveLine = sleepEnded = {0,0};
			}
        virtual ~AmazonRequest();

        virtual bool SendRequest();
        virtual bool SendURIRequest();
        virtual bool SendJSONRequest( const std::string & payload );
        virtual bool SendS3Request( const std::string & payload );

    protected:
        typedef std::map< std::string, std::string > AttributeValueMap;
        AttributeValueMap query_parameters;
        AttributeValueMap headers;

        std::string serviceURL;
        std::string accessKeyFile;
        std::string secretKeyFile;

        std::string errorMessage;
        std::string errorCode;

        std::string resultString;
        unsigned long responseCode;

        bool includeResponseHeader;

		// For tracing.
		int requestID;
		std::string requestCommand;
		struct timespec mutexReleased;
		struct timespec lockGained;
		struct timespec requestBegan;
		struct timespec requestEnded;
		struct timespec mutexGained;
		struct timespec sleepBegan;
		struct timespec liveLine;
		struct timespec sleepEnded;

		// So that we don't bother to send expired signatures.
		struct timespec signatureTime;

		int signatureVersion;

		// For signature v4.  Use if the URL is not of the form
		// '<service>.<region>.provider.tld'.  (Includes S3.)
		std::string region;
		std::string service;

		// Some odd services (Lambda) require the use of GET.
		// Some odd services (S3) requires the use of PUT.
		std::string httpVerb;

	private:
		bool sendV2Request();
		bool sendV4Request( const std::string & payload, bool sendContentSHA = false );

		std::string canonicalizeQueryString();
		bool createV4Signature( const std::string & payload, std::string & authorizationHeader, bool sendContentSHA = false );

	// This happens to be useful for obtaining metadata information.
	protected:
		bool sendPreparedRequest(	const std::string & protocol,
									const std::string & uri,
									const std::string & payload );
};

// EC2 Commands

class AmazonVMStart : public AmazonRequest {
	public:
		AmazonVMStart( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonVMStart();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
        std::string instanceID;
        std::vector< std::string > instanceIDs;
};

class AmazonVMStartSpot : public AmazonVMStart {
    public:
		AmazonVMStartSpot( int i, const char * c ) : AmazonVMStart( i, c ) { }
        virtual ~AmazonVMStartSpot();

        virtual bool SendRequest();

        static bool ioCheck( char ** argv, int argc );
        static bool workerFunction( char ** argv, int argc, std::string & result_string );

    protected:
        std::string spotRequestID;
};

class AmazonVMStop : public AmazonRequest {
	public:
		AmazonVMStop( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonVMStop();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonVMStopSpot : public AmazonVMStop {
    public:
		AmazonVMStopSpot( int i, const char * c ) : AmazonVMStop( i, c ) { }
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
        std::string spotFleetRequestID;
        std::string annexName;

        std::vector< std::string > securityGroups;
};

class AmazonVMStatusAll : public AmazonRequest {
	public:
		AmazonVMStatusAll( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonVMStatusAll();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
        std::vector< AmazonStatusResult > results;
};

class AmazonVMStatus : public AmazonVMStatusAll {
	public:
		AmazonVMStatus( int i, const char * c ) : AmazonVMStatusAll( i, c ) { }
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
		AmazonVMStatusSpot( int i, const char * c ) : AmazonVMStatus( i, c ) { }
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
		AmazonVMStatusAllSpot( int i, const char * c ) : AmazonVMStatusSpot( i, c ) { }
        virtual ~AmazonVMStatusAllSpot();

		static bool ioCheck( char ** argv, int argc );
		static bool workerFunction( char ** argv, int argc, std::string & result_string );
};

class AmazonVMCreateKeypair : public AmazonRequest {
	public:
		AmazonVMCreateKeypair( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonVMCreateKeypair();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
    	std::string privateKeyFileName;
};

class AmazonVMDestroyKeypair : public AmazonRequest {
	public:
		AmazonVMDestroyKeypair( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonVMDestroyKeypair();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonAssociateAddress : public AmazonRequest {
    public:
		AmazonAssociateAddress( int i, const char * c ) : AmazonRequest( i, c ) { }
        virtual ~AmazonAssociateAddress();

        static bool ioCheck(char **argv, int argc);
        static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonCreateTags : public AmazonRequest {
    public:
		AmazonCreateTags( int i, const char * c ) : AmazonRequest( i, c ) { }
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
        AmazonAttachVolume( int i, const char * c ) : AmazonRequest( i, c ) { }
        virtual ~AmazonAttachVolume();

        static bool ioCheck(char **argv, int argc);
        static bool workerFunction(char **argv, int argc, std::string &result_string);
};


class AmazonVMServerType : public AmazonRequest {
	public:
        AmazonVMServerType( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonVMServerType();

		virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		std::string serverType;
};

// Spot Fleet commands
class AmazonBulkStart : public AmazonRequest {
	public:
		AmazonBulkStart( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonBulkStart();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
    	void setLaunchSpecificationAttribute( int, std::map< std::string, std::string > &, const char *, const char * = NULL );

		std::string bulkRequestID;
};

class AmazonBulkStop : public AmazonRequest {
	public:
		AmazonBulkStop( int i, const char * c ) : AmazonRequest( i, c ), success( true ) { }
		virtual ~AmazonBulkStop();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		bool success;
};

class AmazonPutRule : public AmazonRequest {
	public:
		AmazonPutRule( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonPutRule();

		virtual bool SendJSONRequest( const std::string & payload );

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
		std::string ruleARN;
};

class AmazonDeleteRule : public AmazonRequest {
	public:
		AmazonDeleteRule( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonDeleteRule();

		virtual bool SendJSONRequest( const std::string & payload );

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonPutTargets : public AmazonRequest {
	public:
		AmazonPutTargets( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonPutTargets();

		virtual bool SendJSONRequest( const std::string & payload );

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonRemoveTargets : public AmazonRequest {
	public:
		AmazonRemoveTargets( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonRemoveTargets();

		virtual bool SendJSONRequest( const std::string & payload );

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);
};

class AmazonGetFunction : public AmazonRequest {
	public:
		AmazonGetFunction( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonGetFunction();

		virtual bool SendURIRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
		std::string functionHash;
};

class AmazonS3Upload : public AmazonRequest {
	public:
		AmazonS3Upload( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonS3Upload();

		virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		std::string path;
};

class AmazonCreateStack : public AmazonRequest {
	public:
		AmazonCreateStack( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonCreateStack();

		virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		std::string stackID;
};

class AmazonDescribeStacks : public AmazonRequest {
	public:
		AmazonDescribeStacks( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonDescribeStacks();

		virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		std::string stackStatus;
		std::vector< std::string > outputs;
};

class AmazonCallFunction : public AmazonRequest {
	public:
		AmazonCallFunction( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonCallFunction();

		virtual bool SendJSONRequest( const std::string & payload );

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

    protected:
    	std::string success;
		std::string instanceID;
};

class AmazonBulkQuery : public AmazonRequest {
	public:
		AmazonBulkQuery( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonBulkQuery();

        virtual bool SendRequest();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, std::string &result_string);

	protected:
		StringList resultList;
};

class AmazonMetadataQuery : private AmazonRequest {
	public:
		AmazonMetadataQuery( int i, const char * c ) : AmazonRequest( i, c ) { }
		virtual ~AmazonMetadataQuery() { }

		virtual bool SendRequest( const std::string & uri );

		virtual std::string & getResultString() {
			return resultString;
		}
};

#endif
