/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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
#include "condor_io.h"
#include "condor_debug.h"
#include "daemon.h"
#include "condor_uid.h"
#include "lsa_mgr.h"
#include "store_cred.h"
#include <conio.h>



extern "C" FILE *DebugFP;
extern "C" int DebugFlags;


static int code_store_cred(Stream *socket, char* &user, char* &pw, int &mode) {
	
	int result;
	
	result = socket->code(user);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv user.\n");
		return FALSE;
	}
	
	result = socket->code(pw);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv pw.\n");
		return FALSE;
	}
	
	result = socket->code(mode);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv mode.\n");
		return FALSE;
	}
	
	result = socket->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: Failed to send/recv eom.\n");
		return FALSE;
	}
	
	return TRUE;
	
}

void store_cred_handler(void *, int i, Stream *s) {

	//	For debugging
//	DebugFP = stderr;
	
	wchar_t pwbuf[MAX_PASSWORD_LENGTH];
	wchar_t userbuf[MAX_PASSWORD_LENGTH];

	char *user = NULL;
	char *pw = NULL;
	int mode;
	int result;
	priv_state priv;
	int errno_result = 0;
	int answer = FAILURE;
	lsa_mgr lsa_man;
	
	s->decode();
	
	result = code_store_cred(s, user, pw, mode);
	
	if( result == FALSE ) {
		dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
		return;
	} else {
		if ( user ) {
			swprintf(userbuf, L"%S", user);
		}
	}

	dprintf( D_FULLDEBUG, "store_cred: Switching to root priv\n");
	priv = set_root_priv();
	
	switch(mode) {
	case ADD_MODE:
		HANDLE usrHnd;
		bool retval;
		char* dom;

		dprintf( D_FULLDEBUG, "Adding %S to credential storage.\n", 
			userbuf );

		usrHnd = NULL;
		retval = false;
	
		// split the domain and the user name for LogonUser
		dom = strchr(user, '@');
		*dom = '\0';
		dom++;
		// now see if we can get a user token from this password
		retval = LogonUser(
			user,						// user name
			dom,						// domain or server - local for now
			pw,							// password
			LOGON32_LOGON_NETWORK,		// NETWORK is fastest. 
			LOGON32_PROVIDER_DEFAULT,	// logon provider
			&usrHnd						// receive tokens handle
		);
		CloseHandle(usrHnd); // don't leak the handle
		if (pw) {
			swprintf(pwbuf, L"%S", pw); // make a wide-char copy first
			ZeroMemory(pw, strlen(pw));
		}
		if ( ! retval ) {
			dprintf(D_FULLDEBUG, "store_cred: Credential not valid\n");
			answer=FAILURE_BAD_PASSWORD; //fail
			break;
		}

		// call lsa_mgr api
		// answer = return code
		if (!lsa_man.add(userbuf, pwbuf)){
			answer = FAILURE;
		} else {
			answer = SUCCESS;
		}
		ZeroMemory(pwbuf, MAX_PASSWORD_LENGTH*sizeof(wchar_t)); 
		ZeroMemory(userbuf, MAX_PASSWORD_LENGTH*sizeof(wchar_t));
		break;
	case DELETE_MODE:
		dprintf( D_FULLDEBUG, "Deleting %S from credential storage.\n", 
			userbuf );
		// call lsa_mgr api
		// answer = return code
		swprintf(userbuf, L"%S", user);
		if (!lsa_man.remove(userbuf)) {
			answer = FAILURE;
		} else {
			answer = SUCCESS;
		}
		break;
	case QUERY_MODE:
		dprintf( D_FULLDEBUG, "Checking for %S in credential storage.\n", 
			userbuf );
		swprintf(userbuf, L"%S", user);
		if (!lsa_man.isStored(userbuf)) {
			answer = FAILURE;
		} else {
			answer = SUCCESS;
		}
		break;
	default:
		dprintf( D_ALWAYS, "store_cred: Unknown access mode (%d).\n", mode );
		answer=0;
		break;
	}

	if (pw) {
		free(pw);
	}
	if (user) {
		free(user);
	}
	
	dprintf(D_FULLDEBUG, "Switching back to old priv state.\n");
	set_priv(priv);
	
	s->encode();
	if( ! s->code(answer) ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send result.\n" );
		return;
	}
	
	if( ! s->eom() ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send end of message.\n");
	}
	return;
}	


int store_cred(char* user, char* pw, int mode)
{
//----For debugging purposes-----
//	DebugFP = stdout;
//	DebugFlags = D_ALL;
//-------------------------------
	
	int result;
	int return_val;
	Sock* sock;
	int cmd = STORE_CRED;
	
	Daemon my_schedd(DT_SCHEDD, NULL, NULL);

	sock = my_schedd.startCommand(cmd, Stream::reli_sock, 0);
	if( !sock ) {
		dprintf(D_ALWAYS, 
			"STORE_CRED: Failed to start command.\n");
		return FALSE;
	}
	
	result = code_store_cred(sock, user, pw, mode);
	
	if( result == FALSE ) {
		dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
        delete sock;
		return FALSE;
	}
	
	sock->decode();
	
	result = sock->code(return_val);
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: failed to recv schedd's answer.\n");
        delete sock;
		return FALSE;
	}
	
	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "store_cred: failed to code eom.\n");
        delete sock;
		return FALSE;
	}
	
	
	switch(mode)
	{
	case ADD_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Schedd says the Addition succeeded!\n");					
		} else {
			dprintf(D_FULLDEBUG, "Schedd says the Addition failed!\n");
		}
		break;
	case DELETE_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Schedd says the Delete succeeded!\n");
		} else {
			dprintf(D_FULLDEBUG, "Schedd says the Delete failed!\n");
		}
		break;
	case QUERY_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Schedd says we have a credential stored!\n");
		} else {
			dprintf(D_FULLDEBUG, "Schedd says the query failed!\n");
		}
		break;
	}
	delete sock;
	return return_val;
}	

// reads at most maxlength chars without echoing to the terminal into buf
bool
read_no_echo(char* buf, int maxlength) {
	int ch, ch_count;

	ch = ch_count = 0;
	fflush(stdout);		
			
	while ( ch_count < maxlength-1 ) {
		ch = _getch();
		if ( ch == '\r' ) {
			break;
		} else if ( ch == '\b') { // backspace
			if ( ch_count > 0 ) { ch_count--; }
			continue;
		} else if ( ch == '\003' ) { // CTRL-C
			return FALSE;
		}
		buf[ch_count++] = (char) ch;
	}
	buf[ch_count] = '\0';
	return TRUE;
}



char*
get_password() {
	int failure;
	char *buf, *buf2;
	
	buf = new char[MAX_PASSWORD_LENGTH];
	buf2 = new char[MAX_PASSWORD_LENGTH];
	failure = 0;
	
	
	if ((! buf) || (! buf2)) { fprintf(stderr, "Out of Memory!\n\n"); exit(1); }
	
	do {
		
		printf("Enter password: ");
		if ( ! read_no_echo(buf, MAX_PASSWORD_LENGTH) ) {
			delete[] buf;
			delete[] buf2;
			return NULL;
		}
		
		printf("\nConfirm password: ");
		if ( ! read_no_echo(buf2, MAX_PASSWORD_LENGTH) ) {
			delete[] buf;
			delete[] buf2;
			return NULL;
		}

		failure = strcmp(buf, buf2);
		
		if ( failure ) {
			fprintf(stderr, "\n\nPasswords don't match!\n\n");
		} 
	} while ( failure );	
	
	ZeroMemory(buf2, MAX_PASSWORD_LENGTH);	
	delete[] buf2;

	return buf;
}

int deleteCredential( char* user ) {
	return store_cred(user, NULL, DELETE_MODE);	
}

int addCredential( char* user, char* pw ) {
	return store_cred(user, pw, ADD_MODE);	
}

int queryCredential( char* user ) {
	return store_cred(user, NULL, QUERY_MODE);	
}


