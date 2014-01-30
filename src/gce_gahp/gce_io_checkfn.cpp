/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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
#include "gcegahp_common.h"
#include "gceCommands.h"

// Expecting:GCE_PING <req_id> <serviceurl> <authfile> <project> <zone>
bool GcePing::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}

// Expecting:GCE_INSTACE_INSERT <req_id> <serviceurl> <authfile> <project> <zone>
//     <instance_name> <machine_type> <image> <metadata> <metadata_file>
bool GceInstanceInsert::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 11) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_string_name(argv[6]) &&
		verify_string_name(argv[7]) &&
		verify_string_name(argv[8]) &&
		verify_string_name(argv[9]) &&
		verify_string_name(argv[10]);
}

// Expecting:GCE_INSTACE_DELETE <req_id> <serviceurl> <authfile> <project> <zone> <instance_name>
bool GceInstanceDelete::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 7) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]) &&
		verify_string_name(argv[6]);
}

// Expecting:GCE_INSTANCE_LIST <req_id> <serviceurl> <authfile> <project> <zone>
bool GceInstanceList::ioCheck(char **argv, int argc)
{
	return verify_number_args(argc, 6) &&
		verify_request_id(argv[1]) &&
		verify_string_name(argv[2]) &&
		verify_string_name(argv[3]) &&
		verify_string_name(argv[4]) &&
		verify_string_name(argv[5]);
}
