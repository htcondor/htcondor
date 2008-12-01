/***************************************************************
 *
 * Copyright (C) 1990-2008, Condor Team, Computer Sciences Department,
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
#include "sock.h"
#include "dap_constants.h"
#include "dap_client_interface.h"
#include "dap_error.h"
#include "internet.h"
#include "../condor_c++_util/my_hostname.h"
#include "daemon_types.h"
#include "daemon.h"
#include "dap_server_interface.h"
#include "condor_version.h"
#include "condor_debug.h"
#include "condor_netdb.h"

char * 
get_stork_sinful_string (const char * hostname) {
  MyString _hostname;
  if ((hostname == NULL) || (strcmp (hostname, "localhost") == 0)) {
    _hostname = my_hostname();
  } else {
    _hostname = hostname;
  }

  char * host = getHostFromAddr (_hostname.Value());

  // Lookup ip
  struct hostent *phe = NULL;
  if (host) {
    phe=condor_gethostbyname(host);
  }
  if (!phe) {
    return NULL;
  }
  struct in_addr addr;
  memcpy(&addr, phe->h_addr_list[0], sizeof(struct in_addr));
  char * ip = inet_ntoa(addr);
  
  int port = getPortFromAddr (hostname);
  if (port == 0)
    port = CLI_AGENT_SUBMIT_TCP_PORT;

  char * stork_host = (char*)malloc(100);
  sprintf (stork_host, "<%s:%d>", ip, port);
  return stork_host;
}

void
stork_version()
{
    printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	return;
}

void
stork_print_usage(
	FILE *stream,
	const char *argv0,
	const char *usage,
	bool remote_connect
)
{
	const char *my_name = NULL;
	my_name = strrchr( argv0, DIR_DELIM_CHAR );
	if( !my_name ) {
		my_name = argv0;
	} else {
		my_name++;
	}
	fprintf( stream, "usage: %s  [option]... %s\n", my_name, usage);
	fprintf( stream, "\t-help\t\t\tprint this help information\n");
	fprintf( stream, "\t-version\t\tprint version information\n");
	if (remote_connect) {
	fprintf( stream, "\t-debug\t\t\tprint debugging information to console\n");
	fprintf( stream, "\t-name stork_server\tstork server\n");
	}
	return;
}

void
stork_parse_global_opts(
	int  & argc,
	char *argv[],
	const char *usage,
	struct stork_global_opts *parsed,
	bool remote_connect
)
{
	const char *argv0 = argv[0];
	int local_argc = 0;	// count of args that are not global
	memset(parsed, 0, sizeof(*parsed) );	// clear parsed opts.
	char **tmp1 = argv;

	for( tmp1++; *tmp1; tmp1++ ) {
		if ( (*tmp1)[0] != '-' ) {
			// If it doesn't start with '-', skip it
			argv[ ++local_argc ] = *tmp1;	// retain this argument
			continue;
        }
		switch( (*tmp1)[1] ) {
			case 'h':
				// -help
				stork_print_usage(stdout, argv0, usage, remote_connect);
				exit(0);
			case 'v':
				stork_version();
				exit(0);
			case 'd':
				// -debug
				Termlog = 1;
				dprintf_config ("TOOL");
				argc--;	// Account for one global arg processed
				break;
			case 'n':
				if (remote_connect) {
					// -name remoteServerName
					argc--;	// Account for one global arg processed
					tmp1++;
					if ( tmp1 && *tmp1 ) {
						parsed->server = *tmp1;
						argc--;	// Account for one global arg processed
					} else {
						fprintf(stderr, "-name requires another argument\n");
						stork_print_usage(stderr,argv0,usage,remote_connect);
						exit(1);
					}
				} else {
					argv[ ++local_argc ] = *tmp1;	// retain this argument
				}
				break;
			default:
				argv[ ++local_argc ] = *tmp1;	// retain this argument
				break;
		}
	}
	argv[ ++local_argc ] = NULL;
}

Sock *
start_stork_command_and_authenticate (
					 const char * stork_host,
					 const int command,
					 MyString & error_reason) {

	Daemon my_stork(DT_STORK, stork_host, NULL);

	Sock* sock = my_stork.startCommand (command, 
										Stream::reli_sock, 
										0);
	if (!sock) {
		error_reason.sprintf (
				 "Unable to start STORK_SUBMIT command to host %s\n", 
				 stork_host);
		return NULL;
	}


	ReliSock * reli_sock = (ReliSock*)sock;

	if (!reli_sock->triedAuthentication()) { 
		CondorError errstack;
		if( ! SecMan::authenticate_sock(reli_sock, CLIENT_PERM, &errstack) ) {
			error_reason = "Authentication error";
			delete reli_sock;
			return NULL;
		}
	}

	return sock;
}





int 
stork_submit (
			Sock * sock,
			const classad::ClassAd * request,
			const char * host, 
			const char * cred, 
			const int cred_size, 
		    char *& id,
			char * & _error_reason) {

	bool cleanup_sock = false;

	// If passed in an existing steam, then use it.  Otherwise create a new
	// stream, and remember to clean it up before returning.
	if (sock == NULL) {
		// Get a connection
		MyString error_reason;
		sock = start_stork_command_and_authenticate (
			host, 
			STORK_SUBMIT,
			error_reason);

		if (!sock) {
			_error_reason = strdup (error_reason.Value());
			return FALSE;
		}
		cleanup_sock = true;
	}

	sock->encode();

	classad::ClassAdUnParser unparser;
	std::string adbuffer = "";
    unparser.Unparse(adbuffer, request);

	char *_request = strdup(adbuffer.c_str());	// to un-const
	if (!sock->code (_request)) {
		_error_reason = strdup("Client send error");
		if (cleanup_sock) delete sock;
		return FALSE;
	}
	free (_request);
	
	int _cred_size = cred_size;		//de-const

	if ( !sock->code (&_cred_size) ) {
		_error_reason = strdup("Client send error");
		if (cleanup_sock) delete sock;
		return FALSE;
	}
	if (cred_size) {
		char * _cred = strdup (cred);  
		sock->code_bytes (_cred, cred_size);
		free (_cred);
	}


	sock->eom();
	sock->decode();

	char *line = NULL;
	if (!sock->code (line)) {
		_error_reason = strdup("Client recv error");
		if (cleanup_sock) delete sock;
		return FALSE;
	}
	sock->eom();

	_error_reason = NULL;
	id=line;

	if (cleanup_sock) {
		sock->close();
		delete sock;
	}
	
	return TRUE;
}

int
stork_rm (
			const char * id, 
			const char * host, 
			char * & result,
			char * & _error_reason) {

		// Get a connection
	MyString error_reason;
	Sock * sock = 
		start_stork_command_and_authenticate (
											  host, 
											  STORK_REMOVE,
											  error_reason);

	if (!sock) {
		_error_reason = strdup (error_reason.Value());
		return FALSE;
	}

	sock->encode();

	if (!sock->put (id)) {
		_error_reason = strdup ("Client send error");
		delete sock;
		return FALSE;
	}

	sock->eom();
	sock->decode();

	int rc=0;
  
		//listen to the reply from dap server
	char *buff = NULL;
	if (!(sock->code(rc) && sock->code(buff))) {
		_error_reason = strdup ("Client recv error");
		delete sock;
		return FALSE;
	}

	if (rc) {
		result = buff;
		_error_reason = NULL;
	} else {
		result = NULL;
		_error_reason = buff;
	}

	sock->close();
	delete sock;
	return (rc)?TRUE:FALSE;
}


int
stork_status (
			const char * id, 
			const char * host, 
			classad::ClassAd * & result,
			char * & _error_reason) {

		// Get a connection
	MyString error_reason;
	Sock * sock = 
		start_stork_command_and_authenticate (
											  host, 
											  STORK_STATUS,
											  error_reason);

	if (!sock) {
		_error_reason = strdup (error_reason.Value());
		return FALSE;
	}

	sock->encode();

	if (!sock->put (id)) {
		_error_reason = strdup ("Client send error");
		delete sock;
		return FALSE;
	}

	sock->eom();
	sock->decode();
 
	int rc = 0;
 		//listen to the reply from dap server
	MyString	buf;
	if (!(sock->code(rc) && sock->code(buf))) {
		_error_reason = strdup ("Client recv error");
		delete sock;
		return FALSE;
	}


	if (rc) {
			classad::ClassAdParser parser;
			result = parser.ParseClassAd ( buf.Value() );
			if (!result) {
				_error_reason=strdup("Unable to parse result");
				rc=0;
			} else {
				_error_reason = NULL;
			}
	} else {
		result = NULL;
		_error_reason = buf.StrDup( );
	}

	sock->close();
	delete sock;
	return (rc)?TRUE:FALSE;
}




int
stork_list (
			const char * host, 
			int & result_size, 
			classad::ClassAd ** & result,
			char * & _error_reason) {

		// Get a connection
	MyString error_reason;
	Sock * sock = 
		start_stork_command_and_authenticate (
											  host, 
											  STORK_LIST,
											  error_reason);

	if (!sock) {
		_error_reason = strdup (error_reason.Value());
		return FALSE;
	}
	sock->decode();

	int rc = 0;
	if (!sock->code(rc)) {
		_error_reason = strdup ("Client recv error");
		delete sock;
		return FALSE;
	}

	result_size = rc;
	int i;

	MyString	buf;
	classad::ClassAdParser parser;

	result = new classad::ClassAd*[result_size];
	for (i=0; i<result_size; i++) {
		if (!sock->code(buf)) {
			_error_reason = strdup ("Client recv error");
			delete sock;
			return FALSE;
		}
		
		result[i] = parser.ParseClassAd( buf.Value() );
	}
	_error_reason = NULL;

	sock->close();
	delete sock;
	return (rc >= 0)?TRUE:FALSE;
}



