#include "client_common.h"
#include "condor_common.h"
#include "internet.h"
#include "MyString.h"
#include "condor_error.h"
#include "daemon.h"

int 
start_command_and_authenticate (const char * server_host, 
								int command, 
								ReliSock*& result_sock) {
	
	char * server_sinful = NULL;
	if (server_host) {
		server_sinful = hostname_to_string (server_host, 0);
		if (!server_sinful) {
			fprintf (stderr, "Invalid host %s\n", server_host);
			return FALSE;
		}	
	}

	CondorError errorstack;
	Daemon my_credd(DT_CREDD, server_sinful, NULL);
	Sock * sock = my_credd.startCommand (command, 
										 Stream::reli_sock, 
										 0,
										 &errorstack);

	if (!sock) {
		fprintf (stderr, "Unable to connect to host %s (%s)\n", 
				 server_host, 
				 errorstack.getFullText());
		return FALSE;
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
			fprintf (stderr, "Unable to authenticate, qutting\n");
			delete reli_sock;
			return FALSE;
		}
	}
  

	result_sock = reli_sock;
	return TRUE;
}
