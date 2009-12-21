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

#ifndef _DAP_CLIENT_INTERFACE_H
#define _DAP_CLIENT_INTERFACE_H

#include "condor_common.h"
#include "sock.h"

#define WANT_CLASSAD_NAMESPACE
#include <iostream>
#include "classad/classad_distribution.h"

/** dap_submit()
	Submits a specified request to Stork server

	@param request - the ClassAd to submit
	@param stork_server_sin - the sinful string of Stork server
	@param cred - the buffer containing the credential
	@param cred_size - the size of the credential in bytes
	@param id - an out-parameter specifying the id of new request
	   (only valid if return code is TRUE)
	   -must be deleted with free() by caller
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

Sock *
start_stork_command_and_authenticate (
					 const char * stork_host,
					 const int command,
					 MyString & error_reason);

int stork_submit (Sock * sock,
		const classad::ClassAd * request,
				const char * stork_server_sin, 
				const char * cred, 
				const int cred_size, 
				char *& id,
				char *& error_reason);


/** dap_rm()
	Removes a request with specified id from server

	@param id - id of the request
	@param stork_server_sin - the sinful string of Stork server
	@param result - result of the request - currently empty
	   (only valid if return code is TRUE)
	   -must be deleted with free() by caller
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_rm (const char * id, 
			const char * stork_server_sin,
			char *& result,
			char *& error_reason);

/** dap_status()
	Queries for the status (ClassAd) of a given request

	@param id - id of the request
	@param stork_server_sin - the sinful string of Stork server
	@param result - an out-parameter ClassAd result of the request
	   (only valid if return code is TRUE)
	   -must be deleted with free() by caller
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_status (const char * id, 
				const char * stork_server_sin, 
				classad::ClassAd * & result,
				char *& error_reason);

/** dap_list()
	List all active requests for specified user

	@param id - id of the request
	@param stork_server_sin - the sinful string of Stork server
	@param result - an out-parameter, an array of  ClassAds
	   corresponding to each active request
	   (only valid if return code is TRUE)
	   -each element and the array must be deleted with free()/delete
	   respectively
	@param error_reason - an out-parameter specifying error reason
	   (only valid if return code is FALSE)
	   -must be deleted with free() by caller
	@return - TRUE if success, FALSE if failure
*/

int stork_list (const char * stork_server_sin,
			  int & size, 
			  classad::ClassAd ** & result,
			  char *& error_reason);



/** get_stork_sinful_string()
	Returns a sinful string (<xxx.xxx.xxx.xxx:1234>) for the Stork server,
given a host name. This string is what needs to be passed to dap_* commands.
    @param hostname - the hostname of the server. 
	Can be null, may or may not contain port
*/

char * 
get_stork_sinful_string (const char * hostname);

/** stork_version(void)
	Print Stork version information.
*/

void
stork_version(void);

/** stork_print_usage()
	Print Stork usage.
    @param stream - in-parameter specifying output stream
    @param name - in-parameter specifying program name.  Leading
		directory path is stripped off, if present.
    @param usage - in-parameter usage text, which is printed to the
		output stream.  Global options are automatically appended.
    @param remote_connect - in-parameter, true if tool connects to a
		remote stork server.
*/

void
stork_print_usage(
	FILE *stream,
	const char *argv0,
	const char *usage,
	bool remote_connect=false
);

// stork_parse_global_opts() parses global options here.
struct stork_global_opts {
	const char *server;
	// add more parsed stork global options here
};

/** stork_parse_global_opts()
	Parse and handle stork tool global options.  All global options
	are removed from the argument list before this function returns.
    @param argc - an in-out-parameter indicating argument count,
		including program name.  argc will be reduced by number of
		global options processed.
    @param argv - an in-out-parameter indicating argument vector,
		including program name.  argv will be reduced by number of
		global options processed.
    @param usage - in-parameter usage text, which is printed to the
		output stream.  Global options are automatically appended.
    @param opts - in-parameter, pointing to output parsed options
		structure
    @param remote_connect - in-parameter, true if tool connects to a
		remote stork server.  Defaults to false.
*/

void
stork_parse_global_opts(
	int & argc,
	char *argv[],
	const char *usage,
	struct stork_global_opts *parsed,
	bool remote_connect=false
);

#endif
