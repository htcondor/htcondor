/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
    phe=gethostbyname(host);
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
	char *my_name = NULL;
	my_name = strrchr( argv0, DIR_DELIM_CHAR );
	if( !my_name ) {
		my_name = (char *)argv0;
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
				dprintf_config ("TOOL", 2 );
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

	if (!reli_sock->isAuthenticated()) { 
		char * p = SecMan::getSecSetting ("SEC_%s_AUTHENTICATION_METHODS", "CLIENT");
		MyString methods;
		if (p) {
			methods = p;
			free (p);
		} else {
			methods = SecMan::getDefaultAuthenticationMethods();
		}
		CondorError errstack;

		if( ! reli_sock->authenticate(methods.Value(), &errstack) ) {
			error_reason = "Authentication error";
			delete reli_sock;
			return NULL;
		}
	}

	return sock;
}





int 
stork_submit (
			const classad::ClassAd * request,
			const char * host, 
			const char * cred, 
			const int cred_size, 
		    char *& id,
			char * & _error_reason) {

		// Get a connection
	MyString error_reason;
	Sock * sock = 
		start_stork_command_and_authenticate (
											  host, 
											  STORK_SUBMIT,
											  error_reason);

	if (!sock) {
		_error_reason = strdup (error_reason.Value());
		return FALSE;
	}


	char line[MAX_TCP_BUF];

	sock->encode();

	classad::ClassAdUnParser unparser;
	std::string adbuffer = "";
    unparser.Unparse(adbuffer, request);

	char *_request = strdup(adbuffer.c_str());	// to un-const
	if (!sock->code (_request)) {
		_error_reason = strdup("Client send error");
		delete sock;
		return FALSE;
	}
	free (_request);

	sock->code ((int)cred_size);
	if (cred_size) {
		char * _cred = strdup (cred);  
		sock->code_bytes (_cred, cred_size);
		free (_cred);
	}


	sock->eom();
	sock->decode();

	char * pline = (char*)line;
	if (!sock->code (pline)) {
		_error_reason = strdup("Client recv error");
		delete sock;
		return FALSE;
	}
	
	sock->eom();

	_error_reason = NULL;
	id=strdup(line);

	sock->close();
	delete sock;
	
	return TRUE;
}

int
stork_rm (
			const char * id, 
			const char * host, 
			char * & result,
			char * & _error_reason) {

	char buff[MAX_TCP_BUF];
	char * pbuff = (char*)buff;

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

	strcpy (buff, id);
	if (!sock->code (pbuff)) {
		_error_reason = strdup ("Client send error");
		delete sock;
		return FALSE;
	}

	sock->eom();
	sock->decode();

	int rc=0;
  
		//listen to the reply from dap server
	if (!(sock->code(rc) && sock->code(pbuff))) {
		_error_reason = strdup ("Client recv error");
		delete sock;
		return FALSE;
	}

	if (rc) {
		result = strdup (pbuff);
		_error_reason = NULL;
	} else {
		result = NULL;
		_error_reason = strdup(pbuff);
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

	char buff[MAX_TCP_BUF];
	char * pbuff = (char*)buff;

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

	strcpy (buff, id);
	if (!sock->code (pbuff)) {
		_error_reason = strdup ("Client send error");
		delete sock;
		return FALSE;
	}

	sock->eom();
	sock->decode();
 
	int rc = 0;
 		//listen to the reply from dap server
	if (!(sock->code(rc) && sock->code(pbuff))) {
		_error_reason = strdup ("Client recv error");
		delete sock;
		return FALSE;
	}


	if (rc) {
			classad::ClassAdParser parser;
			result = parser.ParseClassAd (pbuff);
			if (!result) {
				_error_reason=strdup("Unable to parse result");
				rc=0;
			} else {
				_error_reason = NULL;
			}
	} else {
		result = NULL;
		_error_reason = strdup(pbuff);
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

	char buff[MAX_TCP_BUF];
	char * pbuff = (char*)buff;

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

	int rc;
	if (!sock->code(rc)) {
		_error_reason = strdup ("Client recv error");
		delete sock;
		return FALSE;
	}

	result_size = rc;
	int i;

	classad::ClassAdParser parser;

//	result = (ClassAd**)malloc (sizeof(ClassAd*) * result_size);
	result = new classad::ClassAd*[result_size];
	for (i=0; i<result_size; i++) {
		if (!sock->code(pbuff)) {
			_error_reason = strdup ("Client recv error");
			delete sock;
			return FALSE;
		}
		
		result[i] = parser.ParseClassAd(pbuff);
	}
	_error_reason = NULL;

	sock->close();
	delete sock;
	return (rc >= 0)?TRUE:FALSE;
}



