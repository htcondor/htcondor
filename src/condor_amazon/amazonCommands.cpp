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
#include "basename.h"
#include "vm_univ_utils.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

#define NULLSTRING "NULL"

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
			output.append(NULLSTRING);
		}
		if( status->private_dns.IsEmpty() == false ) {
			output.append(status->private_dns.Value());
		}else {
			output.append(NULLSTRING);
		}
		if( status->keyname.IsEmpty() == false ) {
			output.append(status->keyname.Value());
		}else {
			output.append(NULLSTRING);
		}
		if( status->groupnames.isEmpty() == false ) {
			char *one_group = NULL;
			status->groupnames.rewind();
			while((one_group = status->groupnames.next()) != NULL ) {
				output.append(one_group);
			}
		}else {
			output.append(NULLSTRING);
		}
	}
}


void
AmazonStatusResult::clearAll()
{
	instance_id = "";
	status = "";
	ami_id = "";
	public_dns = "";
	private_dns = "";
	keyname = "";
	groupnames.clearAll();
	instancetype = "";
}

AmazonRequest::AmazonRequest()
{
	// For gsoap
	m_soap = NULL;
	m_rsa_privk = NULL;
	m_cert = NULL;
}

AmazonRequest::~AmazonRequest() 
{
	CleanupSoap();
}

bool
AmazonRequest::SendRequest()
{
	bool tmp_result = false;

	// Handle InternalError
	int i = 0;
	for( i = 0; i < 3; i++ ) {
		// Clean up old results
		cleanupRequest();

		// Send Request
		CleanupSoap();
		tmp_result = gsoapRequest();

		if( tmp_result == true ) {
			return true;
		}

		dprintf(D_ALWAYS, "Command(%s) got error(code:%s, msg:%s\n", 
				m_request_name.Value(), m_error_code.Value(), m_error_msg.Value());

		if( strcasecmp(m_error_code.Value(), "InternalError") ) {
			// This error is not InternalError. So we don't have to retry
			break;
		}

		sleep(1);
	}

	// Error handler
	return HandleError();
}

bool
AmazonRequest::HandleError()
{
	return false;
}

/// Amazon VMStart
AmazonVMStart::AmazonVMStart()
{
	m_request_name = AMAZON_COMMAND_VM_START;
	base64_userdata = NULL;
}

AmazonVMStart::~AmazonVMStart() 
{
	if( base64_userdata ) {
		free(base64_userdata);
		base64_userdata = NULL;
	}
}

// Expecting:AMAZON_VM_START <req_id> <accesskeyfile> <secretkeyfile> <ami-id> <keypair> <userdata> <userdatafile> <instancetype> <groupname> <groupname> ..
// <groupname> are optional ones.
// we support multiple groupnames

bool AmazonVMStart::workerFunction(char **argv, int argc, MyString &result_string)
{
	int req_id = 0;
	get_int(argv[1], &req_id);
	
	dprintf (D_FULLDEBUG, "AmazonVMStart workerFunction is called\n");
	
	if( !verify_min_number_args(argc, 9) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be >= %d, but %d) to %s\n", 
				9, argc, argv[0]);
		return FALSE;
	}

	if( !verify_ami_id(argv[4]) ) {
		result_string = create_failure_result( req_id, "Invalid_AMI_ID");
		dprintf (D_ALWAYS, "Invalid AMI Id format %s to %s\n", argv[4], argv[0]);
		return FALSE;
	}
	
	AmazonVMStart request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.ami_id = argv[4];

	// we already know the number of arguments is >= 9
	if( strcasecmp(argv[5], NULLSTRING) ) {
		request.keypair = argv[5];
	}

	if( strcasecmp(argv[6], NULLSTRING) ) {
		request.user_data = argv[6];
	}

	if( strcasecmp(argv[7], NULLSTRING) ) {
		request.user_data_file = argv[7];
	}

	if( strcasecmp(argv[8], NULLSTRING) ) {
		request.instance_type = argv[8];
	}

	int i = 0;
	for( i = 9; i < argc; i++ ) {
		if( strcasecmp(argv[i], NULLSTRING) ) {
			request.groupnames.append(argv[i]);
		}
	}

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		StringList result_list;
		result_list.append(request.instance_id.Value());
		result_string = create_success_result(req_id, &result_list);
	}
	return true;

}

/// Amazon VMStop
AmazonVMStop::AmazonVMStop()
{
	m_request_name = AMAZON_COMMAND_VM_STOP;
}

AmazonVMStop::~AmazonVMStop() {}

// Expecting:AMAZON_VM_STOP <req_id> <accesskeyfile> <secretkeyfile> <instance-id>

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
	
	AmazonVMStop request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.instance_id = argv[4];

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool 
AmazonVMStop::HandleError()
{
	if( !strcasecmp(m_error_code.Value(), "InvalidInstanceID.NotFound")) {
		m_error_code = "";
		m_error_msg = "";
		return true;
	}

	return AmazonRequest::HandleError();
}

/// Amazon VMStatus
AmazonVMStatus::AmazonVMStatus()
{
	m_request_name = AMAZON_COMMAND_VM_STATUS;
}

AmazonVMStatus::~AmazonVMStatus()
{
}

// Expecting:AMAZON_VM_STATUS <req_id> <accesskeyfile> <secretkeyfile> <instance-id>

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

	AmazonVMStatus request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.instance_id = argv[4];

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		StringList result_list;
		create_status_output(&request.status_result, result_list);
		result_string = create_success_result(req_id, &result_list);
	}
	return true;
}

bool 
AmazonVMStatus::HandleError()
{
	if( !strcasecmp(m_error_code.Value(), "InvalidInstanceID.NotFound")) {
		m_error_code = "";
		m_error_msg = "";
		return true;
	}

	return AmazonRequest::HandleError();
}

/// Amazon VMStatusAll
AmazonVMStatusAll::AmazonVMStatusAll()
{
	m_request_name = AMAZON_COMMAND_VM_STATUS_ALL;
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

// Expecting:AMAZON_VM_STATUS_ALL <req_id> <accesskeyfile> <secretkeyfile> <Status>
// <Status> is optional field. If <Status> is specified, only VMs with the status will be listed

bool AmazonVMStatusAll::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMStatusAll workerFunction is called\n");
	
	if( !verify_min_number_args(argc ,4) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be >= %d, but %d) to %s\n", 
				4, argc, argv[0]);
		return FALSE;
	}

	AmazonVMStatusAll request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	if( argc >= 5 ) {
		request.vm_status = argv[4];
	}

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
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

/// Amazon VMRunningKeypair
AmazonVMRunningKeypair::AmazonVMRunningKeypair()
{
	m_request_name = AMAZON_COMMAND_VM_RUNNING_KEYPAIR;
}

AmazonVMRunningKeypair::~AmazonVMRunningKeypair()
{
}

// Expecting:AMAZON_VM_RUNNING_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <Status>
// <Status> is optional field. If <Status> is specified, the keypair which belongs to VM with the status will be listed.

bool AmazonVMRunningKeypair::workerFunction(char **argv, int argc, MyString &result_string) 
{
	int req_id = 0;
	get_int(argv[1], &req_id);

	dprintf (D_FULLDEBUG, "AmazonVMRunningKeypair workerFunction is called\n");
	
	if( !verify_min_number_args(argc ,4) ) {
		result_string = create_failure_result( req_id, "Wrong_Argument_Number");
		dprintf (D_ALWAYS, "Wrong args Number(should be >= %d, but %d) to %s\n", 
				4, argc, argv[0]);
		return FALSE;
	}

	AmazonVMRunningKeypair request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	if( argc >= 5 ) {
		request.vm_status = argv[4];
	}

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		if( request.status_num == 0 ) {
			result_string = create_success_result(req_id, NULL);
		}else {
			StringList result_list;
			int i = 0;
			for( i = 0; i < request.status_num; i++ ) {
				if( request.status_results[i].keyname.IsEmpty() ) {
					continue;
				}

				result_list.append(request.status_results[i].instance_id.Value());
				result_list.append(request.status_results[i].keyname.Value());
			}
			result_string = create_success_result(req_id, &result_list);
		}
	}
	return true;
}

/// Amazon AmazonVMCreateKeypair
AmazonVMCreateKeypair::AmazonVMCreateKeypair()
{
	m_request_name = AMAZON_COMMAND_VM_CREATE_KEYPAIR;
	has_outputfile = false;
}

AmazonVMCreateKeypair::~AmazonVMCreateKeypair() {}

// Expecting:AMAZON_VM_CREATE_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <keyname> <outputfile>

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

	AmazonVMCreateKeypair request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.keyname = argv[4];
	request.outputfile = argv[5];

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

/// Amazon VMDestroyKeypair
AmazonVMDestroyKeypair::AmazonVMDestroyKeypair()
{
	m_request_name = AMAZON_COMMAND_VM_DESTROY_KEYPAIR;
}

AmazonVMDestroyKeypair::~AmazonVMDestroyKeypair() {}

// Expecting:AMAZON_VM_DESTROY_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <keyname>

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

	AmazonVMDestroyKeypair request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];
	request.keyname = argv[4];

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		result_string = create_success_result(req_id, NULL);
	}
	return true;
}

bool 
AmazonVMDestroyKeypair::HandleError()
{
	if( !strcasecmp(m_error_code.Value(), "InvalidKeyPair.NotFound")) {
		m_error_code = "";
		m_error_msg = "";
		return true;
	}

	return AmazonRequest::HandleError();
}

/// Amazon VMKeypairNames
AmazonVMKeypairNames::AmazonVMKeypairNames()
{
	m_request_name = AMAZON_COMMAND_VM_KEYPAIR_NAMES;
}

AmazonVMKeypairNames::~AmazonVMKeypairNames() {}

// Expecting:AMAZON_VM_KEYPAIR_NAMES <req_id> <accesskeyfile> <secretkeyfile>

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

	AmazonVMKeypairNames request;

	request.accesskeyfile = argv[2];
	request.secretkeyfile = argv[3];

	// Send Request
	bool tmp_result = request.SendRequest();

	if( tmp_result == false ) {
		// Fail
		result_string = create_failure_result(req_id, request.m_error_msg.Value(), request.m_error_code.Value());
	}else {
		// Success
		result_string = create_success_result(req_id, &request.keynames);
	}
	return true;
}
