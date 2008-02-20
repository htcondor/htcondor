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

// EC2 Commands
#define AMAZON_COMMAND_VM_START				"AMAZON_VM_START"
#define AMAZON_COMMAND_VM_STOP				"AMAZON_VM_STOP"
#define AMAZON_COMMAND_VM_REBOOT			"AMAZON_VM_REBOOT"
#define AMAZON_COMMAND_VM_STATUS			"AMAZON_VM_STATUS"
#define AMAZON_COMMAND_VM_STATUS_ALL		"AMAZON_VM_STATUS_ALL"
#define AMAZON_COMMAND_VM_CREATE_GROUP		"AMAZON_VM_CREATE_GROUP"
#define AMAZON_COMMAND_VM_DELETE_GROUP		"AMAZON_VM_DELETE_GROUP"
#define AMAZON_COMMAND_VM_GROUP_NAMES		"AMAZON_VM_GROUP_NAMES"
#define AMAZON_COMMAND_VM_GROUP_RULES		"AMAZON_VM_GROUP_RULES"
#define AMAZON_COMMAND_VM_ADD_GROUP_RULE	"AMAZON_VM_ADD_GROUP_RULE"
#define AMAZON_COMMAND_VM_DEL_GROUP_RULE	"AMAZON_VM_DEL_GROUP_RULE"
#define AMAZON_COMMAND_VM_CREATE_KEYPAIR	"AMAZON_VM_CREATE_KEYPAIR"
#define AMAZON_COMMAND_VM_DESTROY_KEYPAIR	"AMAZON_VM_DESTROY_KEYPAIR"
#define AMAZON_COMMAND_VM_KEYPAIR_NAMES		"AMAZON_VM_KEYPAIR_NAMES"
#define AMAZON_COMMAND_VM_REGISTER_IMAGE 	"AMAZON_VM_REGISTER_IMAGE"
#define AMAZON_COMMAND_VM_DEREGISTER_IMAGE 	"AMAZON_VM_DEREGISTER_IMAGE"

// S3 Commands
#define AMAZON_COMMAND_S3_ALL_BUCKETS		"AMAZON_S3_ALL_BUCKETS"
#define AMAZON_COMMAND_S3_CREATE_BUCKET		"AMAZON_S3_CREATE_BUCKET"
#define AMAZON_COMMAND_S3_DELETE_BUCKET		"AMAZON_S3_DELETE_BUCKET"
#define AMAZON_COMMAND_S3_LIST_BUCKET		"AMAZON_S3_LIST_BUCKET"
#define AMAZON_COMMAND_S3_UPLOAD_FILE		"AMAZON_S3_UPLOAD_FILE"
#define AMAZON_COMMAND_S3_UPLOAD_DIR		"AMAZON_S3_UPLOAD_DIR"
#define AMAZON_COMMAND_S3_DELETE_FILE		"AMAZON_S3_DELETE_FILE"
#define AMAZON_COMMAND_S3_DOWNLOAD_FILE		"AMAZON_S3_DOWNLOAD_FILE"
#define AMAZON_COMMAND_S3_DOWNLOAD_BUCKET	"AMAZON_S3_DOWNLOAD_BUCKET"


class AmazonRequest {
	public:
		AmazonRequest(const char* lib_path);
		virtual ~AmazonRequest();

		MyString m_amazon_lib_prog;
		MyString m_amazon_lib_path;

		// Error msg
		MyString error_msg;
		MyString error_code;
};

// EC2 Commands

class AmazonVMStart : public AmazonRequest {
	public:
		AmazonVMStart(const char* lib_path);
		virtual ~AmazonVMStart();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString ami_id;
		MyString keypair;

		// we support multiple group names
		StringList groupnames;

		// Result 
		MyString instance_id;

};

class AmazonVMStop : public AmazonRequest {
	public:
		AmazonVMStop(const char* lib_path);
		virtual ~AmazonVMStop();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString instance_id;

		// Result 

};

class AmazonVMReboot : public AmazonRequest {
	public:
		AmazonVMReboot(const char* lib_path);
		virtual ~AmazonVMReboot();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString instance_id;

		// Result 

};

#define AMAZON_STATUS_RUNNING "running"
#define AMAZON_STATUS_PENDING "pending"
#define AMAZON_STATUS_SHUTTING_DOWN "shutting-down"
#define AMAZON_STATUS_TERMINATED "terminated"

class AmazonStatusResult {
	public:
		void clearAll();
		MyString instance_id;
		MyString status;
		MyString ami_id;
		MyString public_dns;
		MyString private_dns;
		MyString keyname;
		StringList groupnames;
};

class AmazonVMStatus : public AmazonRequest {
	public:
		AmazonVMStatus(const char* lib_path);
		virtual ~AmazonVMStatus();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString instance_id;

		// Result 
		AmazonStatusResult status_result;

};

class AmazonVMStatusAll : public AmazonRequest {
	public:
		AmazonVMStatusAll(const char* lib_path);
		virtual ~AmazonVMStatusAll();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;

		// Result 
		AmazonStatusResult *status_results;
		int status_num;

};

class AmazonVMCreateGroup : public AmazonRequest {
	public:
		AmazonVMCreateGroup(const char* lib_path);
		virtual ~AmazonVMCreateGroup();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString groupname;
		MyString group_description;

		// Result 

};

class AmazonVMDeleteGroup : public AmazonRequest {
	public:
		AmazonVMDeleteGroup(const char* lib_path);
		virtual ~AmazonVMDeleteGroup();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString groupname;

		// Result 

};

class AmazonVMGroupNames : public AmazonRequest {
	public:
		AmazonVMGroupNames(const char* lib_path);
		virtual ~AmazonVMGroupNames();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;

		// Result 
		StringList groupnames;

};

class AmazonGroupRule {
	public:
		void clearAll();
		MyString protocol;
		MyString start_port;
		MyString end_port;
		MyString ip_range;
};

class AmazonVMGroupRules : public AmazonRequest {
	public:
		AmazonVMGroupRules(const char* lib_path);
		virtual ~AmazonVMGroupRules();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString groupname;

		// Result 
		AmazonGroupRule *rules;
		int rules_num;

};

class AmazonVMAddGroupRule : public AmazonRequest {
	public:
		AmazonVMAddGroupRule(const char* lib_path);
		virtual ~AmazonVMAddGroupRule();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString groupname;
		AmazonGroupRule rule;

		// Result 

};

class AmazonVMDelGroupRule : public AmazonRequest {
	public:
		AmazonVMDelGroupRule(const char* lib_path);
		virtual ~AmazonVMDelGroupRule();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString groupname;
		AmazonGroupRule rule;

		// Result 

};

class AmazonVMCreateKeypair : public AmazonRequest {
	public:
		AmazonVMCreateKeypair(const char* lib_path);
		virtual ~AmazonVMCreateKeypair();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString keyname;
		MyString outputfile;

		// Result 

};

class AmazonVMDestroyKeypair : public AmazonRequest {
	public:
		AmazonVMDestroyKeypair(const char* lib_path);
		virtual ~AmazonVMDestroyKeypair();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString keyname;

		// Result 

};

class AmazonVMKeypairNames : public AmazonRequest {
	public:
		AmazonVMKeypairNames(const char* lib_path);
		virtual ~AmazonVMKeypairNames();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;

		// Result 
		StringList keynames;

};

class AmazonVMRegisterImage : public AmazonRequest {
	public:
		AmazonVMRegisterImage(const char* lib_path);
		virtual ~AmazonVMRegisterImage();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString location;

		// Result 
		MyString ami_id;

};

class AmazonVMDeregisterImage : public AmazonRequest {
	public:
		AmazonVMDeregisterImage(const char* lib_path);
		virtual ~AmazonVMDeregisterImage();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString ami_id;

		// Result 

};


// S3 Commands
class AmazonS3AllBuckets: public AmazonRequest {
	public:
		AmazonS3AllBuckets(const char* lib_path);
		virtual ~AmazonS3AllBuckets();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;

		// Result 
		StringList bucketnames;

};

class AmazonS3CreateBucket: public AmazonRequest {
	public:
		AmazonS3CreateBucket(const char* lib_path);
		virtual ~AmazonS3CreateBucket();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString bucketname;

		// Result 

};

class AmazonS3DeleteBucket: public AmazonRequest {
	public:
		AmazonS3DeleteBucket(const char* lib_path);
		virtual ~AmazonS3DeleteBucket();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString bucketname;

		// Result 

};

class AmazonS3ListBucket: public AmazonRequest {
	public:
		AmazonS3ListBucket(const char* lib_path);
		virtual ~AmazonS3ListBucket();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString bucketname;
		MyString prefix;

		// Result 
		StringList keynames;

};

class AmazonS3UploadFile: public AmazonRequest {
	public:
		AmazonS3UploadFile(const char* lib_path);
		virtual ~AmazonS3UploadFile();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString filename;
		MyString bucketname;
		MyString keyname;

		// Result 

};

class AmazonS3UploadDir: public AmazonRequest {
	public:
		AmazonS3UploadDir(const char* lib_path);
		virtual ~AmazonS3UploadDir();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString dirname;
		MyString bucketname;

		// Result 

};

class AmazonS3DeleteFile: public AmazonRequest {
	public:
		AmazonS3DeleteFile(const char* lib_path);
		virtual ~AmazonS3DeleteFile();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString bucketname;
		MyString keyname;

		// Result 

};

class AmazonS3DownloadFile: public AmazonRequest {
	public:
		AmazonS3DownloadFile(const char* lib_path);
		virtual ~AmazonS3DownloadFile();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString bucketname;
		MyString keyname;
		MyString outputfile;

		// Result 

};

class AmazonS3DownloadBucket: public AmazonRequest {
	public:
		AmazonS3DownloadBucket(const char* lib_path);
		virtual ~AmazonS3DownloadBucket();

		static bool ioCheck(char **argv, int argc);
		static bool workerFunction(char **argv, int argc, MyString &result_string);

		virtual bool Request();

		// Request Args
		MyString accesskeyfile;
		MyString secretkeyfile;
		MyString bucketname;
		MyString localdir;

		// Result 

};


#endif
