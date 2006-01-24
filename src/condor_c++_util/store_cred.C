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
		// we'll need a wide-char version of the user name later
		if ( user ) {
			swprintf(userbuf, L"%S", user);
		}
	}

	if (!can_switch_ids()) {
		answer = FAILURE_NOT_SUPPORTED;
	}
	else {
		priv = set_root_priv();
		
		switch(mode) {
		case ADD_MODE:
			bool retval;
			
			dprintf( D_FULLDEBUG, "Adding %S to credential storage.\n", 
				 userbuf );
			
			retval = isValidCredential(user, pw);
			
			if ( ! retval ) {
				answer=FAILURE_BAD_PASSWORD; 
				break; // bail out 
			}
			
			if (pw) {
				swprintf(pwbuf, L"%S", pw); // make a wide-char copy first
				ZeroMemory(pw, strlen(pw));
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
		{
			dprintf( D_FULLDEBUG, "Checking for %S in credential storage.\n", 
				 userbuf );
			swprintf(userbuf, L"%S", user);
			
			char passw[MAX_PASSWORD_LENGTH];
			wchar_t *pw_wc = NULL;
			pw_wc = lsa_man.query(userbuf);
			
			if ( !pw_wc ) {
				answer = FAILURE;
			} else {
				sprintf(passw, "%S", pw_wc);
				ZeroMemory(pw_wc, wcslen(pw_wc));
				delete[] pw_wc;
				
				if ( isValidCredential(user, passw) ) {
					answer = SUCCESS;
				} else {
					answer = FAILURE_BAD_PASSWORD;
				}
				
				ZeroMemory(passw, MAX_PASSWORD_LENGTH);
			}
			break;
		}
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
	}
	
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
	char *buf;
	
	buf = new char[MAX_PASSWORD_LENGTH];
	
	if (! buf) { fprintf(stderr, "Out of Memory!\n\n"); return NULL; }
	
		
	printf("Enter password: ");
	if ( ! read_no_echo(buf, MAX_PASSWORD_LENGTH) ) {
		delete[] buf;
		return NULL;
	}
	
	return buf;
}

// takes user@domain format for user argument
bool
isValidCredential( char *user, char* pw ) {
	// see if we can get a user token from this password
	HANDLE usrHnd;
	char* dom;
	DWORD LogonUserError;
	int retval;

	retval = 0;
	usrHnd = NULL;
	
	// split the domain and the user name for LogonUser
	dom = strchr(user, '@');

	if ( dom ) {
		*dom = '\0';
		dom++;
	}

	retval = LogonUser(
		user,						// user name
		dom,						// domain or server - local for now
		pw,							// password
		LOGON32_LOGON_NETWORK,		// NETWORK is fastest. 
		LOGON32_PROVIDER_DEFAULT,	// logon provider
		&usrHnd						// receive tokens handle
	);
	LogonUserError = GetLastError();
	
	if ( (retval == 0) && ((LogonUserError == ERROR_PRIVILEGE_NOT_HELD ) || 
		 (LogonUserError == ERROR_LOGON_TYPE_NOT_GRANTED )) ) {
		
		dprintf(D_FULLDEBUG, "NETWORK logon failed. Attempting INTERACTIVE\n");

		retval = LogonUser(
			user,						// user name
			dom,						// domain or server - local for now
			pw,							// password
			LOGON32_LOGON_INTERACTIVE,	// INTERACTIVE should be held by everyone.
			LOGON32_PROVIDER_DEFAULT,	// logon provider
			&usrHnd						// receive tokens handle
		);
		LogonUserError = GetLastError();
	}

	if ( retval == 0 ) {
		dprintf(D_ALWAYS, "Failed to log in %s@%s with err=%d\n", user, dom,
				LogonUserError);
		return false;
	} else {
		CloseHandle(usrHnd);
		return true;
	}
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


