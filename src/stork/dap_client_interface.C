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


Sock *
start_stork_command_and_authenticate (
					 const char * stork_host,
					 const int command,
					 MyString & error_reason) {

	if (stork_host == NULL) {
		error_reason.sprintf ("Invalid host %s", stork_host);
		return NULL;
	}

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



