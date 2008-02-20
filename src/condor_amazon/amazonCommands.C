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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "condor_arglist.h"
#include "MyString.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_popen.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

#define PERL_SCRIPT_SUCCESS_TAG "#SUCCESS"
#define PERL_SCRIPT_ERROR_TAG "#ERROR"
#define PERL_SCRIPT_ERROR_CODE "#ECODE"

static bool
__systemCommand(ArgList &args, StringList &output, MyString &error_code)
{
	FILE *fp = NULL;
	fp = my_popen(args, "r", FALSE);

	if( !fp ) {
		MyString args_string;
		args.GetArgsStringForDisplay(&args_string,0);
		dprintf(D_ALWAYS, "Failed to execute my_popen: %s\n", args_string.Value());
		return false;
	}

	bool read_something = false;
	char buf[2048];
	MyString one_line;

	while( fgets(buf, 2048, fp) ) {
		one_line = buf;
		one_line.chomp();
		one_line.trim();

		output.append(one_line.Value());
		read_something = true;

		dprintf(D_FULLDEBUG, "__systemCommand got : %s\n", one_line.Value());
	}
	my_pclose(fp);

	if( !read_something ) {
		MyString args_string;
		args.GetArgsStringForDisplay(&args_string,0);
		dprintf(D_ALWAYS, "Error: '%s' did not produce any valid output\n", args_string.Value());
		return false;
	}

	// delete the last line
	if( !strcmp(one_line.Value(), PERL_SCRIPT_SUCCESS_TAG) ||
		!strcmp(one_line.Value(), PERL_SCRIPT_ERROR_TAG) ) {
		output.remove(one_line.Value());
	}

	// Check if the last line is PERL_SCRIPT_ERROR_TAG
	// find error code if available
	if( !strcmp(one_line.Value(), PERL_SCRIPT_ERROR_TAG) ) {
		char* x = NULL;	

		output.rewind();
		while( (x = output.next()) != NULL ) {
			if( !strncmp(x, PERL_SCRIPT_ERROR_CODE, 
						strlen(PERL_SCRIPT_ERROR_CODE)) ) {
				// Found error code
				MyString name;
				MyString value;
				parse_param_string(x, name, value, true);
				error_code = value;
				break;
			}
		}
		return false;
	}

	// Check if the last line is PERL_SCRIPT_SUCCESS_TAG
	if( strcmp(one_line.Value(), PERL_SCRIPT_SUCCESS_TAG) ) {
		return false;
	}
		
	return true;
}

static bool 
systemCommand(ArgList &args, const char *chdir_path, StringList &output, MyString &ecode)
{
	bool tmp_result = false;

	if( chdir_path ) {
		if( chdir(chdir_path) < 0 ) {
			dprintf(D_ALWAYS, "Cannot switch dir to %s\n", chdir_path);
			chdir(get_working_dir());
			return -1;
		}
	}

	output.clearAll();
	tmp_result = __systemCommand(args, output, ecode);

	if( chdir_path ) {
		chdir(get_working_dir());
	}

	return tmp_result;
}

/*
   The one line format for status should be like 
   INSTANCEID=i-21789a48,AMIID=ami-2bb65342,STATUS=running,PRIVATEDNS=domU-12-31-36-00-2D-63.z-1.compute-1.internal,PUBLICDNS=ec2-72-44-51-9.z-1.compute-1.amazonaws.com,KEYNAME=gsg-keypair,GROUP=mytestgroup
   STATUS must be one of "pending", "running", "shutting-down", "terminated"
   PRIVATEDNS and PUBLICDNS may be empty until the instance enters a running state
*/
static bool parseAmazonStatusResult( const char* result_line, AmazonStatusResult &output)
{
	output.clearAll();

	if( !result_line ) {
		return false;
	}

	dprintf(D_FULLDEBUG, "get status: %s\n", result_line);

	StringList result_string(result_line, ",");
	if( result_string.isEmpty() ) {
		dprintf(D_ALWAYS, "Invalid status format(%s)\n", result_line);
		return false;
	}

	result_string.rewind();
	char *field = NULL;
	MyString one_field;
	MyString name;
	MyString value;

	while( (field = result_string.next() ) != NULL ) {
		one_field = field;
		if( one_field.Length() == 0 ) {
			continue;
		}

		parse_param_string(one_field.Value(), name, value, true);
		if( !name.Length() || !value.Length() ) {
			continue;
		}

		if( !strcasecmp(name.Value(), "INSTANCEID") ) {
			output.instance_id = value;
			continue;
		}
		if( !strcasecmp(name.Value(), "AMIID") ) {
			output.ami_id = value;
			continue;
		}
		if( !strcasecmp(name.Value(), "STATUS") ) {
			output.status = value;
			continue;
		}
		if( !strcasecmp(name.Value(), "PRIVATEDNS") ) {
			output.private_dns = value;
			continue;
		}
		if( !strcasecmp(name.Value(), "PUBLICDNS") ) {
			output.public_dns = value;
			continue;
		}
		if( !strcasecmp(name.Value(), "KEYNAME") ) {
			output.keyname = value;
			continue;
		}

		// It is possible that there are multiple groups
		if( !strcasecmp(name.Value(), "GROUP") ) {
			output.groupnames.append(value.Value());
			continue;
		}
	}

	if( output.status.IsEmpty() ) {
		dprintf(D_ALWAYS, "Failed to get valid status\n");
		return false;
	}

	return true;
}

/*
	Each line will should be like 
	0.0.0.0/0<tab>tcp<tab>80<tab>80
	128.105.167.49/24<tab>tcp<tab>80<tab>90
*/

static bool parseAmazonGroupRuleResult( const char* result_line, AmazonGroupRule &output)
{
	output.clearAll();

	if( !result_line ) {
		return false;
	}

	dprintf(D_FULLDEBUG, "get status: %s\n", result_line);

	StringList result_string(result_line, "\t");
	if( result_string.isEmpty() ) {
		dprintf(D_ALWAYS, "Invalid group rule format(%s)\n", result_line);
		return false;
	}

	result_string.rewind();
	if( result_string.number() != 4 ) {
		dprintf(D_ALWAYS, "Invalid group rule format(%s)\n", result_line);
	}

	output.ip_range = result_string.next();
	output.protocol = result_string.next();
	output.start_port = result_string.next();
	output.end_port = result_string.next();
	return true;
}

// Format is "<instance_id> <status> <ami_id> <public_dns> <private_dns> <keypairname> <group> <group> <group> ...
static void create_status_output(AmazonStatusResult *status, StringList &output)
{
	output.clearAll();

	if( !status ) {
		return;
	}
	
	output.append(status->instance_id.Value());
	output.append(status->status.Value());
	output.append(status->ami_id.Value());

	if( !strcasecmp(status->status.Value(), AMAZON_STATUS_RUNNING)) {
		if( status->public_dns.IsEmpty() == false ) {
			output.append(status->public_dns.Value());
		}else {
			output.append("NULL");
		}
		if( status->private_dns.IsEmpty() == false ) {
			output.append(status->private_dns.Value());
		}else {
			output.append("NULL");
		}
		if( status->keyname.IsEmpty() == false ) {
			output.append(status->keyname.Value());
		}else {
			output.append("NULL");
		}
		if( status->groupnames.isEmpty() == false ) {
			char *one_group = NULL;
			status->groupnames.rewind();
			while((one_group = status->groupnames.next()) != NULL ) {
				output.append(one_group);
			}
		}else {
			output.append("NULL");
		}
	}
}


void
AmazonStatusResult::clearAll()
{
	instance_id = "";
	ami_id = "";
	status = "";
	private_dns = "";
	public_dns = "";
	keyname = "";
	groupnames.clearAll();
}

void
AmazonGroupRule::clearAll()
{
	protocol = "";
	start_port = "";
	end_port = "";
	ip_range = "";
}

AmazonRequest::AmazonRequest(const char* lib_path)
{
	m_amazon_lib_path = lib_path;
	m_amazon_lib_prog = AMAZON_SCRIPT_NAME;
}

AmazonRequest::~AmazonRequest() {}

/// Amazon VMStart
AmazonVMStart::AmazonVMStart(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMStart::~AmazonVMStart() {}

// Expecting:AMAZON_VM_START <req_id> <accesskeyfile> <secretkeyfile> <ami-id> <keypair> <groupname> <groupname> ..
// <keypair> and <groupname> are optional ones.
// we support multiple groupnames
bool AmazonVMStart::ioCheck(char **argv, int argc)
{
	return verify_min_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_ami_id(argv[4]);
}

bool AmazonVMStart::workerFunction(char **argv, int argc, MyString &result_string)
{
	int req_id = 0;
	get_int(argv[1], &req_id);
	
	dprintf (D_FULLDEBUG, "AmazonVMStart workerFunction is called\n");
	
	if( !verify_min_number_args(argc ,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be >= %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	if( !verify_ami_id(argv[4]) ) {
		result_string = create_failure_result( req_id, "Invalid_AMI_ID");
		dprintf (D_ALWAYS, "Invalid AMI Id format %s to %s\n", argv[4], argv[0]);
		return FALSE;
	}
	
	AmazonVMStart request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.ami_id = argv[4];

	if( argc >= 6 ) {
		if( strcasecmp(argv[5], "NULL") ) {
			request.keypair = argv[5];
		}

		int i = 0;
		for( i = 6; i < argc; i++ ) {
			if( strcasecmp(argv[i], "NULL") ) {
				request.groupnames.append(argv[i]);
			}
		}
	}

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		StringList result_list;
		result_list.append(request.instance_id.Value());
		result_string = create_success_result(req_id, &result_list);
	}
	return true;

}

bool AmazonVMStart::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", error_msg.Value());
		return false;
	}

	if( ami_id.IsEmpty() ) {
		error_msg = "Empty_AMI_ID";
		dprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMStart Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("start");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-id");
	systemcmd.AppendArg(ami_id);

	if( keypair.IsEmpty() == false ) {
		systemcmd.AppendArg("-key");
		systemcmd.AppendArg(keypair);
	}
	if( groupnames.isEmpty() == false ) {
		char *one_group = NULL;
		groupnames.rewind();
		while((one_group = groupnames.next()) != NULL ) {
			systemcmd.AppendArg("-group");
			systemcmd.AppendArg(one_group);
		}
	}

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// Read output file
	instance_id = output.next();

	if( instance_id.IsEmpty() ) {
		dprintf(D_ALWAYS, "Failed to get instance id during AmazonVMStart\n");
		return false;
	}

	return true;
}

/// Amazon VMStop
AmazonVMStop::AmazonVMStop(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMStop::~AmazonVMStop() {}

// Expecting:AMAZON_VM_STOP <req_id> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMStop::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_instance_id(argv[4]);
}

bool AmazonVMStop::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMStop workerFunction is called\n");

	if( !verify_number_args(argc ,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	if( !verify_instance_id(argv[4]) ) {
		result_string = create_failure_result( req_id, "Invalid_Instance_Id");
		dprintf (D_ALWAYS, "Invalid Instance Id format %s to %s\n", argv[4], argv[0]);
		return FALSE;
	}
	
	AmazonVMStop request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.instance_id = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMStop::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		error_msg = "Empty_Instance_ID";
		dprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMStop Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("stop");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-id");
	systemcmd.AppendArg(instance_id);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon VMReboot
AmazonVMReboot::AmazonVMReboot(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMReboot::~AmazonVMReboot() {}

// Expecting:AMAZON_VM_REBOOT <req_id> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMReboot::ioCheck(char **argv, int argc) 
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_instance_id(argv[4]);
}

bool AmazonVMReboot::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);
	
	dprintf (D_FULLDEBUG, "AmazonVMReboot workerFunction is called\n");

	if( !verify_number_args(argc ,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	if( !verify_instance_id(argv[4]) ) {
		result_string = create_failure_result( req_id, "Invalid_Instance_Id");
		dprintf (D_ALWAYS, "Invalid Instance Id format %s to %s\n", argv[4], argv[0]);
		return FALSE;
	}

	AmazonVMReboot request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.instance_id = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMReboot::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMReboot Error: %s\n", error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		error_msg = "Empty_Instance_ID";
		dprintf(D_ALWAYS, "AmazonVMReboot Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMReboot Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("reboot");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-id");
	systemcmd.AppendArg(instance_id);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon VMStatus
AmazonVMStatus::AmazonVMStatus(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMStatus::~AmazonVMStatus() {}

// Expecting:AMAZON_VM_STATUS <req_id> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMStatus::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_instance_id(argv[4]);
}

bool AmazonVMStatus::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);
	
	dprintf (D_FULLDEBUG, "AmazonVMStatus workerFunction is called\n");

	if( !verify_number_args(argc ,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	if( !verify_instance_id(argv[4]) ) {
		result_string = create_failure_result( req_id, "Invalid_Instance_Id");
		dprintf (D_ALWAYS, "Invalid Instance Id format %s to %s\n", argv[4], argv[0]);
		return FALSE;
	}

	AmazonVMStatus request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.instance_id = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		StringList result_list;
		create_status_output(&request.status_result, result_list);
		result_string = create_success_result(req_id, &result_list);
	}
	return true;
}

bool AmazonVMStatus::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", error_msg.Value());
		return false;
	}

	if( instance_id.IsEmpty() ) {
		error_msg = "Empty_Instance_ID";
		dprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMStatus Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("status");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-instanceid");
	systemcmd.AppendArg(instance_id);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();
	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	char *result_line = output.next();
	if( !result_line ) {
		dprintf(D_ALWAYS, "Failed to get status output during AmazonVMStatus\n");
		return false;
	}

	if( parseAmazonStatusResult(result_line, status_result) == false ) {
		return false;
	}

	return true;
}

/// Amazon VMStatusAll
AmazonVMStatusAll::AmazonVMStatusAll(const char* lib_path) : AmazonRequest(lib_path)
{
	status_results = NULL;
	status_num = 0;
}

AmazonVMStatusAll::~AmazonVMStatusAll()
{
	if( status_results ) {
		delete[] status_results;
		status_results = NULL;
		status_num = 0;
	}
}

// Expecting:AMAZON_VM_STATUS_ALL <req_id> <accesskeyfile> <secretkeyfile>
bool AmazonVMStatusAll::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

bool AmazonVMStatusAll::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMStatusAll workerFunction is called\n");
	
	if( !verify_number_args(argc ,4) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				4, argc, argv[0]);
		return FALSE;
	}

	AmazonVMStatusAll request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		if( request.status_num == 0 ) {
			result_string = create_success_result(req_id, NULL);
		}else {
			StringList result_list;
			int i = 0;
			for( i = 0; i < request.status_num; i++ ) {
				result_list.append(request.status_results[i].instance_id.Value());
				result_list.append(request.status_results[i].status.Value());
				result_list.append(request.status_results[i].ami_id.Value());
			}
			result_string = create_success_result(req_id, &result_list);
		}
	}
	return true;
}

bool AmazonVMStatusAll::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMStatusAll Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMStatusAll Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("status");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	/*
	 * Each line will have the status of each instance.
	 * The format of each line is same to that for VMstatus
	 */

	if( output.number() == 0 ) {
		if( status_results ) {
			delete[] status_results;
			status_results = NULL;
		}
		status_num = 0;
		return true;
	}

	status_results = new AmazonStatusResult[output.number()];
	ASSERT(status_results);

	int index = 0;
	char *one_instance = NULL;
	while( (one_instance = output.next()) != NULL ) {
		if( parseAmazonStatusResult(one_instance, status_results[index]) == false ) {
			continue;
		}
		index++;
	}
	status_num = index;
	if( index == 0 ) {
		delete[] status_results;
		status_results = NULL;
	}

	return true;
}

/// Amazon VMCreateGroup
AmazonVMCreateGroup::AmazonVMCreateGroup(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMCreateGroup::~AmazonVMCreateGroup() {}

// Expecting:AMAZON_VM_CREATE_GROUP <req_id> <accesskeyfile> <secretkeyfile> <groupname> <group description>
bool AmazonVMCreateGroup::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}

bool AmazonVMCreateGroup::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMCreateGroup workerFunction is called\n");

	if( !verify_number_args(argc,6) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				6, argc, argv[0]);
		return FALSE;
	}

	AmazonVMCreateGroup request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.groupname = argv[4];
	request.group_description = argv[5];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMCreateGroup::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMCreateGroup Error: %s\n", error_msg.Value());
		return false;
	}

	if( groupname.IsEmpty() ) {
		error_msg = "Empty_Group_Name";
		dprintf(D_ALWAYS, "AmazonVMCreateGroup Error: %s\n", error_msg.Value());
		return false;
	}

	if( group_description.IsEmpty() ) {
		error_msg = "Empty_Group_Description";
		dprintf(D_ALWAYS, "AmazonVMCreateGroup Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMCreateGroup Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("creategroup");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-group");
	systemcmd.AppendArg(groupname);
	systemcmd.AppendArg("-desc");
	systemcmd.AppendArg(group_description);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon VMDeleteGroup
AmazonVMDeleteGroup::AmazonVMDeleteGroup(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMDeleteGroup::~AmazonVMDeleteGroup() {}

// Expecting:AMAZON_VM_DELETE_GROUP <req_id> <accesskeyfile> <secretkeyfile> <groupname>
bool AmazonVMDeleteGroup::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonVMDeleteGroup::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMDeleteGroup workerFunction is called\n");

	if( !verify_number_args(argc,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonVMDeleteGroup request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.groupname = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMDeleteGroup::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMDeleteGroup Error: %s\n", error_msg.Value());
		return false;
	}

	if( groupname.IsEmpty() ) {
		error_msg = "Empty_Group_Name";
		dprintf(D_ALWAYS, "AmazonVMDeleteGroup Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMDeleteGroup Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("delgroup");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-group");
	systemcmd.AppendArg(groupname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon VMGroupnames
AmazonVMGroupNames::AmazonVMGroupNames(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMGroupNames::~AmazonVMGroupNames() {}

// Expecting:AMAZON_VM_GROUP_NAMES <req_id> <accesskeyfile> <secretkeyfile>
bool AmazonVMGroupNames::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

bool AmazonVMGroupNames::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMGroupNames workerFunction is called\n");

	if( !verify_number_args(argc,4) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				4, argc, argv[0]);
		return FALSE;
	}

	AmazonVMGroupNames request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, &request.groupnames);
	}
	return true;
}

bool AmazonVMGroupNames::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMGroupNames Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMGroupNames Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("groupnames");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	if( output.number() == 0 ) {
		// At least there must be "default" group.
		dprintf(D_ALWAYS, "Failed to get group name during AmazonVMGroupNames\n");
		return false;
	}

	/*
	 * Each line will have <group name>\t<group description>
	 */
	char *one_group = NULL;
	while( (one_group = output.next()) != NULL ) {
		int tab_pos = -1;
		MyString tmp_str;

		tmp_str = one_group;
		tab_pos = tmp_str.FindChar('\t', 0);
		if( tab_pos > 0 ) {
			tmp_str.setChar(tab_pos, '\0');
		}
		groupnames.append(tmp_str.Value());
	}

	return true;
}

/// Amazon AmazonVMGroupRules
AmazonVMGroupRules::AmazonVMGroupRules(const char* lib_path) : AmazonRequest(lib_path) 
{
	rules = NULL;
	rules_num = 0;
}

AmazonVMGroupRules::~AmazonVMGroupRules() 
{
	if( rules ) {
		delete[] rules;
		rules = NULL;
		rules_num = 0;
	}
}

// Expecting:AMAZON_VM_GROUP_RULES <req_id> <accesskeyfile> <secretkeyfile> <groupname>
bool AmazonVMGroupRules::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonVMGroupRules::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMGroupRules workerFunction is called\n");

	if( !verify_number_args(argc,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be = %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonVMGroupRules request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.groupname = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		if( request.rules_num == 0 ) {
			result_string = create_success_result(req_id, NULL);
		}else {
			StringList result_list;
			int i = 0;
			for( i = 0; i < request.rules_num; i++ ) {
				result_list.append(request.rules[i].ip_range.Value());
				result_list.append(request.rules[i].protocol.Value());
				result_list.append(request.rules[i].start_port.Value());
				result_list.append(request.rules[i].end_port.Value());
			}
			result_string = create_success_result(req_id, &result_list);
		}
	}
	return true;
}

bool AmazonVMGroupRules::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMGroupRules Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMGroupRules Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("grouprules");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-group");
	systemcmd.AppendArg(groupname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	/* Each line will should be like 
	 * 0.0.0.0/0       			tcp     80      80
	 * 128.105.167.49/24		tcp		80		90
	 */

	if( output.number() == 0 ) {
		if( rules ) {
			delete[] rules;
			rules = NULL;
		}
		rules_num = 0;
		return true;
	}

	rules = new AmazonGroupRule[output.number()];
	ASSERT(rules);

	int index = 0;
	char *one_rule = NULL;
	while( (one_rule = output.next()) != NULL ) {
		if( parseAmazonGroupRuleResult(one_rule, rules[index]) == false ) {
			continue;
		}
		index++;
	}
	rules_num = index;
	if( index == 0 ) {
		delete[] rules;
		rules = NULL;
	}

	return true;
}

/// Amazon AmazonVMAddGroupRule
AmazonVMAddGroupRule::AmazonVMAddGroupRule(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMAddGroupRule::~AmazonVMAddGroupRule() {}

// Expecting:AMAZON_VM_ADD_GROUP_RULE <req_id> <accesskeyfile> <secretkeyfile> <groupname> <protocol> <start_port> <end_port> <ip_range>
// <ip_range> is optional one.
bool AmazonVMAddGroupRule::ioCheck(char **argv, int argc)
{
	return verify_min_number_args(argc, 8) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_number(argv[6]) &&
		verify_number(argv[7]) && 
		( (argc == 8) || 
		  ((argc == 9) && is_valid_network(argv[8], NULL, NULL)));
}

bool AmazonVMAddGroupRule::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMAddGroupRule workerFunction is called\n");

	if( !verify_min_number_args(argc,8) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be >=%d, but %d) to %s\n", 
				8, argc, argv[0]);
		return FALSE;
	}

	AmazonVMAddGroupRule request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.groupname = argv[4];
	request.rule.protocol = argv[5];
	request.rule.start_port = argv[6];
	request.rule.end_port = argv[7];

	if( argc == 9 ) {
		request.rule.ip_range = argv[8];
	}

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMAddGroupRule::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMAddGroupRule Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMAddGroupRule Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("setgroupingress");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-group");
	systemcmd.AppendArg(groupname);
	systemcmd.AppendArg("-protocol");
	systemcmd.AppendArg(rule.protocol);
	systemcmd.AppendArg("-fromport");
	systemcmd.AppendArg(rule.start_port);
	systemcmd.AppendArg("-toport");
	systemcmd.AppendArg(rule.end_port);

	if( rule.ip_range.IsEmpty() == false ) {
		systemcmd.AppendArg("-iprange");
		systemcmd.AppendArg(rule.ip_range);
	}

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon AmazonVMDelGroupRule
AmazonVMDelGroupRule::AmazonVMDelGroupRule(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMDelGroupRule::~AmazonVMDelGroupRule() {}

// Expecting:AMAZON_VM_DEL_GROUP_RULE <req_id> <accesskeyfile> <secretkeyfile> <groupname> <protocol> <start_port> <end_port> <ip_range>
// <ip_range> is optional one.
bool AmazonVMDelGroupRule::ioCheck(char **argv, int argc)
{
	return verify_min_number_args(argc, 8) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_number(argv[6]) &&
		verify_number(argv[7]) && 
		( (argc == 8) || 
		  ((argc == 9) && is_valid_network(argv[8], NULL, NULL)));
}

bool AmazonVMDelGroupRule::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMDelGroupRule workerFunction is called\n");

	if( !verify_min_number_args(argc,8) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be >=%d, but %d) to %s\n", 
				8, argc, argv[0]);
		return FALSE;
	}

	AmazonVMDelGroupRule request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.groupname = argv[4];
	request.rule.protocol = argv[5];
	request.rule.start_port = argv[6];
	request.rule.end_port = argv[7];

	if( argc == 9 ) {
		request.rule.ip_range = argv[8];
	}

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMDelGroupRule::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMDelGroupRule Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMDelGroupRule Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("delgroupingress");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-group");
	systemcmd.AppendArg(groupname);
	systemcmd.AppendArg("-protocol");
	systemcmd.AppendArg(rule.protocol);
	systemcmd.AppendArg("-fromport");
	systemcmd.AppendArg(rule.start_port);
	systemcmd.AppendArg("-toport");
	systemcmd.AppendArg(rule.end_port);

	if( rule.ip_range.IsEmpty() == false ) {
		systemcmd.AppendArg("-iprange");
		systemcmd.AppendArg(rule.ip_range);
	}

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon AmazonVMCreateKeypair
AmazonVMCreateKeypair::AmazonVMCreateKeypair(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMCreateKeypair::~AmazonVMCreateKeypair() {}

// Expecting:AMAZON_VM_CREATE_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <keyname> <outputfile>
bool AmazonVMCreateKeypair::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}

bool AmazonVMCreateKeypair::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMCreateKeypair workerFunction is called\n");

	if( !verify_number_args(argc,6) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				6, argc, argv[0]);
		return FALSE;
	}

	AmazonVMCreateKeypair request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.keyname = argv[4];
	request.outputfile = argv[5];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMCreateKeypair::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMCreateKeyPair Error: %s\n", error_msg.Value());
		return false;
	}

	if( keyname.IsEmpty() ) {
		error_msg = "Empty_Keyname";
		dprintf(D_ALWAYS, "AmazonVMCreateKeyPair Error: %s\n", error_msg.Value());
		return false;
	}

	// check if output file could be created
	if( check_create_file(outputfile.GetCStr()) == false ) {
		error_msg = "No_permission_for_keypair_outputfile";
		dprintf(D_ALWAYS, "AmazonVMCreateKeypair Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMCreateKeypair Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("createloginkey");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-keyname");
	systemcmd.AppendArg(keyname);
	systemcmd.AppendArg("-output");
	systemcmd.AppendArg(outputfile);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon VMDestroyKeypair
AmazonVMDestroyKeypair::AmazonVMDestroyKeypair(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMDestroyKeypair::~AmazonVMDestroyKeypair() {}

// Expecting:AMAZON_VM_DESTROY_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <keyname>
bool AmazonVMDestroyKeypair::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonVMDestroyKeypair::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMDestroyKeypair workerFunction is called\n");

	if( !verify_number_args(argc,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonVMDestroyKeypair request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.keyname = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMDestroyKeypair::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", error_msg.Value());
		return false;
	}

	if( keyname.IsEmpty() ) {
		error_msg = "Empty_Keyname";
		dprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMDestroyKeypair Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("deleteloginkey");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-keyname");
	systemcmd.AppendArg(keyname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}

/// Amazon VMKeypairNames
AmazonVMKeypairNames::AmazonVMKeypairNames(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMKeypairNames::~AmazonVMKeypairNames() {}

// Expecting:AMAZON_VM_KEYPAIR_NAMES <req_id> <accesskeyfile> <secretkeyfile>
bool AmazonVMKeypairNames::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

bool AmazonVMKeypairNames::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMKeypairNames workerFunction is called\n");

	if( !verify_number_args(argc,4) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				4, argc, argv[0]);
		return FALSE;
	}

	AmazonVMKeypairNames request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, &request.keynames);
	}
	return true;
}

bool AmazonVMKeypairNames::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMKeypairNames Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMKeypairNames Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("loginkeynames");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	/*
	 * Each line will have <keyname>\t<key fingerprint>
	 */
	char *one_key = NULL;
	while( (one_key = output.next()) != NULL ) {
		int tab_pos = -1;
		MyString tmp_str;

		tmp_str = one_key;
		tab_pos = tmp_str.FindChar('\t', 0);
		if( tab_pos > 0 ) {
			tmp_str.setChar(tab_pos, '\0');
		}
		keynames.append(tmp_str.Value());
	}

	return true;
}

/// Amazon VMRegisterImage
AmazonVMRegisterImage::AmazonVMRegisterImage(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMRegisterImage::~AmazonVMRegisterImage() {}

// Expecting:AMAZON_VM_REGISTER_IMAGE <req_id> <accesskeyfile> <secretkeyfile> <location on S3>
bool AmazonVMRegisterImage::ioCheck(char **argv, int argc) 
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonVMRegisterImage::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);
	
	dprintf (D_FULLDEBUG, "AmazonVMRegisterImage workerFunction is called\n");

	if( !verify_number_args(argc ,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonVMRegisterImage request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.location = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		StringList result_list;
		result_list.append(request.ami_id.Value());
		result_string = create_success_result(req_id, &result_list);
	}
	return true;
}

bool AmazonVMRegisterImage::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMRegisterImage Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMRegisterImage Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("registerimage");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-location");
	systemcmd.AppendArg(location);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// Read output file
	ami_id = output.next();

	if( ami_id.IsEmpty() ) {
		dprintf(D_ALWAYS, "Failed to get ami id during AmazonVMRegisterImage\n");
		return false;
	}
	return true;
}

/// Amazon VMDeregisterImage
AmazonVMDeregisterImage::AmazonVMDeregisterImage(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonVMDeregisterImage::~AmazonVMDeregisterImage() {}

// Expecting:AMAZON_VM_DEREGISTER_IMAGE <req_id> <accesskeyfile> <secretkeyfile> <ami-id>
bool AmazonVMDeregisterImage::ioCheck(char **argv, int argc) 
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_ami_id(argv[4]);
}

bool AmazonVMDeregisterImage::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMDeregisterImage workerFunction is called\n");
	
	if( !verify_number_args(argc ,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonVMDeregisterImage request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.ami_id = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonVMDeregisterImage::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonVMDeregisterImage Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonVMDeregisterImage Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("deregisterimage");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-id");
	systemcmd.AppendArg(ami_id);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	// There is no output for success
	return true;
}



/// Amazon S3AllBuckets
AmazonS3AllBuckets::AmazonS3AllBuckets(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3AllBuckets::~AmazonS3AllBuckets() {}

// Expecting:AMAZON_S3_ALL_BUCKETS <req_id> <accesskeyfile> <secretkeyfile>
bool AmazonS3AllBuckets::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

bool AmazonS3AllBuckets::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3AllBuckets workerFunction is called\n");

	if( !verify_number_args(argc,4) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				4, argc, argv[0]);
		return FALSE;
	}

	AmazonS3AllBuckets request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, &request.bucketnames);
	}
	return true;
}

bool AmazonS3AllBuckets::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3AllBuckets Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3AllBuckets Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("listallbuckets");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	/*
	 * Each line will have <bucketname>
	 */
	char *bucket = NULL;
	while( (bucket = output.next()) != NULL ) {
		bucketnames.append(bucket);
	}

	return true;
}

/// Amazon S3CreateBucket
AmazonS3CreateBucket::AmazonS3CreateBucket(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3CreateBucket::~AmazonS3CreateBucket() {}

// Expecting:AMAZON_S3_CREATE_BUCKET <req_id> <accesskeyfile> <secretkeyfile> <bucketname>
bool AmazonS3CreateBucket::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonS3CreateBucket::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3CreateBucket workerFunction is called\n");

	if( !verify_number_args(argc,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonS3CreateBucket request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.bucketname = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3CreateBucket::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3CreateBucket Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3CreateBucket Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("createbucket");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}

/// Amazon S3DeleteBucket
AmazonS3DeleteBucket::AmazonS3DeleteBucket(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3DeleteBucket::~AmazonS3DeleteBucket() {}

// Expecting:AMAZON_S3_DELETE_BUCKET <req_id> <accesskeyfile> <secretkeyfile> <bucketname>
bool AmazonS3DeleteBucket::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonS3DeleteBucket::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3DeleteBucket workerFunction is called\n");

	if( !verify_number_args(argc,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonS3DeleteBucket request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.bucketname = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3DeleteBucket::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3DeleteBucket Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3DeleteBucket Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("deletebucket");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);
	systemcmd.AppendArg("-force");

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}

/// Amazon S3ListBucket
AmazonS3ListBucket::AmazonS3ListBucket(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3ListBucket::~AmazonS3ListBucket() {}

// Expecting:AMAZON_S3_LIST_BUCKET <req_id> <accesskeyfile> <secretkeyfile> <bucketname>
bool AmazonS3ListBucket::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

bool AmazonS3ListBucket::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3ListBucket workerFunction is called\n");

	if( !verify_number_args(argc,5) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				5, argc, argv[0]);
		return FALSE;
	}

	AmazonS3ListBucket request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.bucketname = argv[4];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, &request.keynames);
	}
	return true;
}

bool AmazonS3ListBucket::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3ListBucket Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3ListBucket Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("listbucket");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}


	/*
	 * Each line will have <keyname>\t<size>
	 */
	char *one_keyname = NULL;
	while( (one_keyname = output.next()) != NULL ) {
		int tab_pos = -1;
		MyString tmp_str;

		tmp_str = one_keyname;
		tab_pos = tmp_str.FindChar('\t', 0);
		if( tab_pos > 0 ) {
			tmp_str.setChar(tab_pos, '\0');
		}
		keynames.append(tmp_str.Value());	
	}

	return true;
}

/// Amazon S3UploadFile
AmazonS3UploadFile::AmazonS3UploadFile(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3UploadFile::~AmazonS3UploadFile() {}

// Expecting:AMAZON_S3_UPLOAD_FILE <req_id> <accesskeyfile> <secretkeyfile> <filename> <bucketname> <keyname>
bool AmazonS3UploadFile::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 7) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_string_name(argv[6]);
}

bool AmazonS3UploadFile::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3UploadFile workerFunction is called\n");

	if( !verify_number_args(argc,7) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				7, argc, argv[0]);
		return FALSE;
	}

	AmazonS3UploadFile request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.filename = argv[4];
	request.bucketname = argv[5];
	request.keyname = argv[6];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3UploadFile::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3UploadFile Error: %s\n", error_msg.Value());
		return false;
	}

	if( !check_read_access_file( filename.GetCStr()) ) {
		error_msg.sprintf("Cannot read the file to upload(%s)", filename.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3UploadFile Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("uploadfile");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-file");
	systemcmd.AppendArg(filename);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);
	systemcmd.AppendArg("-keyname");
	systemcmd.AppendArg(keyname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}

/// Amazon S3UploadDir
AmazonS3UploadDir::AmazonS3UploadDir(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3UploadDir::~AmazonS3UploadDir() {}

// Expecting:AMAZON_S3_UPLOAD_DIR <req_id> <accesskeyfile> <secretkeyfile> <dirname> <bucketname>
bool AmazonS3UploadDir::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}

bool AmazonS3UploadDir::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3UploadDir workerFunction is called\n");

	if( !verify_number_args(argc,6) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				6, argc, argv[0]);
		return FALSE;
	}

	AmazonS3UploadDir request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.dirname = argv[4];
	request.bucketname = argv[5];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3UploadDir::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3UploadDir Error: %s\n", error_msg.Value());
		return false;
	}

	if( !check_read_access_file( dirname.GetCStr()) ) {
		error_msg.sprintf("Cannot read the directory to upload(%s)", dirname.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3UploadDir Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("uploaddir");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-dir");
	systemcmd.AppendArg(dirname);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);
	systemcmd.AppendArg("-ec2");

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}

/// Amazon S3DeleteFile
AmazonS3DeleteFile::AmazonS3DeleteFile(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3DeleteFile::~AmazonS3DeleteFile() {}

// Expecting:AMAZON_S3_DELETE_FILE <req_id> <accesskeyfile> <secretkeyfile> <keyname> <bucketname>
bool AmazonS3DeleteFile::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}

bool AmazonS3DeleteFile::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3DeleteFile workerFunction is called\n");

	if( !verify_number_args(argc,6) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				6, argc, argv[0]);
		return FALSE;
	}

	AmazonS3DeleteFile request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.keyname = argv[4];
	request.bucketname = argv[5];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3DeleteFile::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3DeleteFile Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3DeleteFile Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("deletefile");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-name");
	systemcmd.AppendArg(keyname);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}

/// Amazon S3DownloadFile
AmazonS3DownloadFile::AmazonS3DownloadFile(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3DownloadFile::~AmazonS3DownloadFile() {}

// Expecting:AMAZON_S3_DOWNLOAD_FILE <req_id> <accesskeyfile> <secretkeyfile> <keyname> <bucketname> <outputfile>
bool AmazonS3DownloadFile::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 7) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_string_name(argv[6]);
}

bool AmazonS3DownloadFile::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3DownloadFile workerFunction is called\n");

	if( !verify_number_args(argc,7) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				7, argc, argv[0]);
		return FALSE;
	}

	AmazonS3DownloadFile request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.keyname = argv[4];
	request.bucketname = argv[5];
	request.outputfile = argv[6];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3DownloadFile::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3DownloadFile Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3DownloadFile Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("downloadfile");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-name");
	systemcmd.AppendArg(keyname);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);
	systemcmd.AppendArg("-output");
	systemcmd.AppendArg(outputfile);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}

/// Amazon S3DownloadBucket
AmazonS3DownloadBucket::AmazonS3DownloadBucket(const char* lib_path) : AmazonRequest(lib_path) {}

AmazonS3DownloadBucket::~AmazonS3DownloadBucket() {}

// Expecting:AMAZON_S3_DOWNLOAD_BUCKET <req_id> <accesskeyfile> <secretkeyfile> <bucketname> <localdir>
bool AmazonS3DownloadBucket::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}

bool AmazonS3DownloadBucket::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonS3DownloadBucket workerFunction is called\n");

	if( !verify_number_args(argc,6) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be %d, but %d) to %s\n", 
				6, argc, argv[0]);
		return FALSE;
	}

	AmazonS3DownloadBucket request(get_amazon_lib_path());

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.bucketname = argv[4];
	request.localdir = argv[5];

	// Send Request
	bool tmp_result = request.Request();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.error_msg.GetCStr(), request.error_code.GetCStr());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool AmazonS3DownloadBucket::Request()
{
	if( !check_access_and_secret_key_file(accesskeyfile.GetCStr(), 
				secretkeyfile.GetCStr(), error_msg) ) {
		dprintf(D_ALWAYS, "AmazonS3DownloadBucket Error: %s\n", error_msg.Value());
		return false;
	}

	if( m_amazon_lib_path.IsEmpty() ) {
		error_msg = "No_Amazon_Library";
		dprintf(D_ALWAYS, "AmazonS3DownloadBucket Error: %s\n", error_msg.Value());
		return false;
	}

	ArgList systemcmd;
	systemcmd.AppendArg(AMAZON_SCRIPT_INTERPRETER);
	systemcmd.AppendArg(m_amazon_lib_prog);
	systemcmd.AppendArg("downloadbucket");
	systemcmd.AppendArg("-a");
	systemcmd.AppendArg(accesskeyfile);
	systemcmd.AppendArg("-s");
	systemcmd.AppendArg(secretkeyfile);
	systemcmd.AppendArg("-bucket");
	systemcmd.AppendArg(bucketname);
	systemcmd.AppendArg("-outputdir");
	systemcmd.AppendArg(localdir);

	StringList output;
	MyString ecode;
	bool tmp_result = systemCommand(systemcmd, m_amazon_lib_path.GetCStr(), output, ecode);

	output.rewind();

	if( tmp_result == false ){
		error_msg = output.next();
		error_code = ecode;
		return false;
	}

	return true;
}
