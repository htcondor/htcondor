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
#include "basename.h"
#include "amazongahp_common.h"
#include "amazonCommands.h"

// Expecting:AMAZON_VM_START <req_id> <accesskeyfile> <secretkeyfile> <ami-id> <keypair> <userdata> <userdatafile> <instancetype> <groupname> <groupname> ..
// <groupname> are optional ones.
// we support multiple groupnames
bool AmazonVMStart::ioCheck(char **argv, int argc)
{
	return verify_min_number_args(argc, 9) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_ami_id(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_string_name(argv[6]) &&
		verify_string_name(argv[7]) &&
		verify_string_name(argv[8]);
}

// Expecting:AMAZON_VM_STOP <req_id> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMStop::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_instance_id(argv[4]);
}

// Expecting:AMAZON_VM_STATUS <req_id> <accesskeyfile> <secretkeyfile> <instance-id>
bool AmazonVMStatus::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_instance_id(argv[4]);
}

// Expecting:AMAZON_VM_STATUS_ALL <req_id> <accesskeyfile> <secretkeyfile>
bool AmazonVMStatusAll::ioCheck(char **argv, int argc)
{
	return verify_min_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

// Expecting:AMAZON_VM_RUNNING_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <Status>
// <Status> is optional field. If <Status> is specified, the keypair which belongs to VM with the status will be listed.

bool AmazonVMRunningKeypair::ioCheck(char **argv, int argc)
{
	return verify_min_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}

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

// Expecting:AMAZON_VM_DESTROY_KEYPAIR <req_id> <accesskeyfile> <secretkeyfile> <keyname>
bool AmazonVMDestroyKeypair::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 5) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]);
}

// Expecting:AMAZON_VM_KEYPAIR_NAMES <req_id> <accesskeyfile> <secretkeyfile>
bool AmazonVMKeypairNames::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 4) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]);
}
