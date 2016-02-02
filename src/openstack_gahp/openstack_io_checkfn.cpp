/***************************************************************
 *
 * Copyright (C) 1990-2015, Condor Team, Computer Sciences Department,
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

#include "openstackgahp_common.h"
#include "openstackCommands.h"

// KEYSTONE_SERVICE <url> <username> <password> <project>
// (Because this command doest NOT communicate with the service, it is
// NOT asynchronous and needs no request ID.)
bool KeystoneService::ioCheck( char ** argv, int argc ) {
	return verify_number_args( argc, 5 ) &&
		verify_string_name( argv[1] ) &&
		verify_string_name( argv[2] ) &&
		verify_string_name( argv[3] ) &&
		verify_string_name( argv[4] );
}

// NOVA_PING <request-id> <region>
bool NovaPing::ioCheck( char ** argv, int argc ) {
	return verify_number_args( argc, 3 ) &&
		verify_request_id( argv[1] ) &&
		verify_string_name( argv[2] );
}

// NOVA_SERVER_CREATE <request-id> <region> <desc>
bool NovaServerCreate::ioCheck( char ** argv, int argc ) {
	return verify_number_args( argc, 4 ) &&
		verify_request_id( argv[1] ) &&
		verify_string_name( argv[2] ) &&
		verify_string_name( argv[3] );
}

// NOVA_SERVER_DELETE <request-id> <region> <server-id>
bool NovaServerDelete::ioCheck( char ** argv, int argc ) {
	return verify_number_args( argc, 4 ) &&
		verify_request_id( argv[1] ) &&
		verify_string_name( argv[2] ) &&
		verify_string_name( argv[3] );
}

// NOVA_SERVER_LIST <request-id> <region> <filter>
bool NovaServerList::ioCheck( char ** argv, int argc ) {
	return verify_number_args( argc, 4 ) &&
		verify_request_id( argv[1] ) &&
		verify_string_name( argv[2] ) &&
		verify_string_name( argv[3] );
}

// NOVA_SERVER_LIST_DETAIL <request-id> <region> <filter>
bool NovaServerListDetail::ioCheck( char ** argv, int argc ) {
	return verify_number_args( argc, 4 ) &&
		verify_request_id( argv[1] ) &&
		verify_string_name( argv[2] ) &&
		verify_string_name( argv[3] );
}
