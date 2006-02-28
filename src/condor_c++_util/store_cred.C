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
#include "condor_io.h"
#include "condor_debug.h"
#include "daemon.h"
#include "condor_uid.h"
#include "lsa_mgr.h"
#include "store_cred.h"
#include "condor_config.h"

#ifndef WIN32
	// **** UNIX CODE *****

// TODO: stub functions for now
char* getStoredCredential(const char *username, const char *domain)
{
	return NULL;
}

#else
	// **** WIN32 CODE ****


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

int store_cred_service(const char *user, const char *pw, int mode) 
{

	wchar_t pwbuf[MAX_PASSWORD_LENGTH];
	wchar_t userbuf[MAX_PASSWORD_LENGTH];
	priv_state priv;
	int errno_result = 0;
	int answer = FAILURE;
	lsa_mgr lsa_man;
	wchar_t *pw_wc;
	
	// we'll need a wide-char version of the user name later
	if ( user ) {
		swprintf(userbuf, L"%S", user);
	}

	if (!can_switch_ids()) {
		answer = FAILURE_NOT_SUPPORTED;
	} else {
		priv = set_root_priv();
		
		switch(mode) {
		case ADD_MODE:
			bool retval;

			dprintf( D_FULLDEBUG, "Adding %S to credential storage.\n", 
				userbuf );

			retval = isValidCredential(user, pw);

			if ( ! retval ) {
				dprintf(D_FULLDEBUG, "store_cred: tried to add invalid credential\n");
				answer=FAILURE_BAD_PASSWORD; 
				break; // bail out 
			}

			if (pw) {
				swprintf(pwbuf, L"%S", pw); // make a wide-char copy first
			}

			// call lsa_mgr api
			// answer = return code
			if (!lsa_man.add(userbuf, pwbuf)){
				answer = FAILURE;
			} else {
				answer = SUCCESS;
			}
			SecureZeroMemory(pwbuf, MAX_PASSWORD_LENGTH*sizeof(wchar_t)); 
			break;
		case DELETE_MODE:
			dprintf( D_FULLDEBUG, "Deleting %S from credential storage.\n", 
				userbuf );

			pw_wc = lsa_man.query(userbuf);
			if ( !pw_wc ) {
				answer = FAILURE_NOT_FOUND;
				break;
			}
			else {
				SecureZeroMemory(pw_wc, wcslen(pw_wc));
				delete[] pw_wc;
			}

			if (!isValidCredential(user, pw)) {
				dprintf(D_FULLDEBUG, "store_cred: invalid credential given for delete\n");
				answer = FAILURE_BAD_PASSWORD;
				break;
			}

			// call lsa_mgr api
			// answer = return code
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
				
				char passw[MAX_PASSWORD_LENGTH];
				pw_wc = lsa_man.query(userbuf);
				
				if ( !pw_wc ) {
					answer = FAILURE_NOT_FOUND;
				} else {
					sprintf(passw, "%S", pw_wc);
					SecureZeroMemory(pw_wc, wcslen(pw_wc));
					delete[] pw_wc;
					
					if ( isValidCredential(user, passw) ) {
						answer = SUCCESS;
					} else {
						answer = FAILURE_BAD_PASSWORD;
					}
					
					SecureZeroMemory(passw, MAX_PASSWORD_LENGTH);
				}
				break;
			}
		default:
				dprintf( D_ALWAYS, "store_cred: Unknown access mode (%d).\n", mode );
				answer=0;
				break;
		}
			
		dprintf(D_FULLDEBUG, "Switching back to old priv state.\n");
		set_priv(priv);
	}
	
	return answer;
}	


void store_cred_handler(void *, int i, Stream *s) 
{
	char *user = NULL;
	char *pw = NULL;
	int mode;
	int result;
	int errno_result = 0;
	int answer = FAILURE;
	lsa_mgr lsa_man;
	
	s->decode();
	
	result = code_store_cred(s, user, pw, mode);
	
	if( result == FALSE ) {
		dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
		return;
	} 

	// ensure that the username has an '@' delimteter
	// the only "user" allowed to not contain a '@' is POOL_PASSWORD_USERNAME
	// (we allow queries on POOL_PASSWORD_USERNAME via this handler, but updates
	//  must come in via store_pool_cred_handler)
	if ( user ) {
		if ((mode != QUERY_MODE) && (strchr(user, '@') == NULL)) {
			dprintf(D_ALWAYS, "ERROR: attempt to set pool password via STORE_CRED! (must use STORE_POOL_CRED)\n");
			answer = FAILURE;
		} else {
			answer = store_cred_service(user,pw,mode);
		}
	}
	
	if (pw) {
		SecureZeroMemory(pw, strlen(pw));
		free(pw);
	}
	if (user) {
		free(user);
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

void store_pool_cred_handler(void *, int i, Stream *s)
{
	int result;
	char *pw = NULL;

	s->decode();
	result = s->code(pw);
	if(!result) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to receive password.\n");
		return;
	}
	result = s->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to receive eom.\n");
		return;
	}

	// do the real work
	if (pw) {
		result = store_cred_service(POOL_PASSWORD_USERNAME, pw, ADD_MODE);
		SecureZeroMemory(pw, strlen(pw));
		free(pw);
	}
	else {
		result = store_cred_service(POOL_PASSWORD_USERNAME, pw, DELETE_MODE);
	}

	s->encode();
	if (!s->code(result)) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to send result.\n");
		return;
	}
	if (!s->eom()) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to send end of message.\n");
	}
}

int 
store_cred(const char* user, const char* pw, int mode, Daemon* d, bool force) {
	
	int result;
	int return_val;
	Sock* sock = NULL;

		// If we are root / SYSTEM and we want the local schedd, 
		// then do the work directly to the local registry.
		// If not, then send the request over the wire to a remote credd or schedd.

	if ( is_root() && d == NULL ) {
			// do the work directly onto the local registry
		return_val = store_cred_service(user,pw,mode);
	} else {
			// send out the request remotely.

			// if we're setting the pool password, we need to use STORE_POOL_CRED
			// (which is handled by the master, rather than schedd, and requires
			//  CONFIG authorization)
		int cmd = STORE_CRED;
		if ((mode == ADD_MODE) && (strcmp(user, POOL_PASSWORD_USERNAME) == 0)) {
			cmd = STORE_POOL_CRED;
		}

		if (d == NULL) {
			if (cmd == STORE_POOL_CRED) {
				// need to go to the master for setting the pool password
				dprintf(D_FULLDEBUG, "Storing credential to local master\n");
				Daemon my_master(DT_MASTER);
				sock = my_master.startCommand(cmd, Stream::reli_sock, 0);
			}
			else {
				dprintf(D_FULLDEBUG, "Storing credential to local schedd\n");
				Daemon my_schedd(DT_SCHEDD);
				sock = my_schedd.startCommand(cmd, Stream::reli_sock, 0);
			}
		} else {
			sock = d->startCommand(cmd, Stream::reli_sock, 0);
		}
		
		if( !sock ) {
			dprintf(D_ALWAYS, 
				"STORE_CRED: Failed to start command.\n");
			return FAILURE;
		}

		// for remote updates, verify we have a secure channel,
		// unless "force" is specified
		if (((mode == ADD_MODE) || (mode == DELETE_MODE)) && !force && (d != NULL) &&
			((sock->type() != Stream::reli_sock) || !((ReliSock*)sock)->isAuthenticated() || !sock->get_encryption())) {
			dprintf(D_ALWAYS, "STORE_CRED: blocking attempt to update over insecure channel\n");
			delete sock;
			return FAILURE_NOT_SECURE;
		}
		
		if (cmd == STORE_CRED) {
			result = code_store_cred(sock, (char*&)user, (char*&)pw, mode);
			if( result == FALSE ) {
				dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
				delete sock;
				return FAILURE;
			}
		}
		else {
				// only need to send the password for STORE_POOL_CRED
			if (!sock->code((char*&)pw)) {
				dprintf(D_ALWAYS, "store_cred: failed to send pw for STORE_POOL_CRED\n");
				delete sock;
				return FAILURE;
			}
			if (!sock->end_of_message()) {
				dprintf(D_ALWAYS, "store_cred: failed to receive eom for STORE_POOL_CRED\n");
				delete sock;
				return FAILURE;
			}
		}
		
		sock->decode();
		
		result = sock->code(return_val);
		if( !result ) {
			dprintf(D_ALWAYS, "store_cred: failed to recv answer.\n");
			delete sock;
			return FAILURE;
		}
		
		result = sock->end_of_message();
		if( !result ) {
			dprintf(D_ALWAYS, "store_cred: failed to recv eom.\n");
			delete sock;
			return FAILURE;
		}
	}	// end of case where we send out the request remotely
	
	
	switch(mode)
	{
	case ADD_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Addition succeeded!\n");					
		} else {
			dprintf(D_FULLDEBUG, "Addition failed!\n");
		}
		break;
	case DELETE_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Delete succeeded!\n");
		} else {
			dprintf(D_FULLDEBUG, "Delete failed!\n");
		}
		break;
	case QUERY_MODE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "We have a credential stored!\n");
		} else {
			dprintf(D_FULLDEBUG, "Query failed!\n");
		}
		break;
	}

	if ( sock ) delete sock;

	return return_val;
}	

// reads at most maxlength chars without echoing to the terminal into buf
bool
read_from_keyboard(char* buf, int maxlength, bool echo) {
	int ch, ch_count;

	ch = ch_count = 0;
	fflush(stdout);		
			
	while ( ch_count < maxlength-1 ) {
		ch = echo ? _getche() : _getch();
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
	if ( ! read_from_keyboard(buf, MAX_PASSWORD_LENGTH, false) ) {
		delete[] buf;
		return NULL;
	}
	
	return buf;
}

// takes user@domain format for user argument
bool
isValidCredential( const char *input_user, const char* input_pw ) {
	// see if we can get a user token from this password
	HANDLE usrHnd = NULL;
	char* dom;
	DWORD LogonUserError;
	BOOL retval;

	retval = 0;
	usrHnd = NULL;

	// the POOL_PASSWORD_USERNAME does not correspond to an actual account
	if ( strcmp(input_user,POOL_PASSWORD_USERNAME)==0 ) {
		return true;
	}

	char * user = strdup(input_user);
	
	// split the domain and the user name for LogonUser
	dom = strchr(user, '@');

	if ( dom ) {
		*dom = '\0';
		dom++;
	}

	char * pw = strdup(input_pw);

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

	if (user) free(user);
	if (pw) {
		SecureZeroMemory(pw,strlen(pw));
		free(pw);
	}

	if ( retval == 0 ) {
		dprintf(D_ALWAYS, "Failed to log in %s with err=%d\n", 
				input_user, LogonUserError);
		return false;
	} else {
		dprintf(D_FULLDEBUG, "Succeeded to log in %s\n", input_user);
		CloseHandle(usrHnd);
		return true;
	}
}

int deleteCredential( const char* user, const char* pw, Daemon *d ) {
	return store_cred(user, pw, DELETE_MODE, d);	
}

int addCredential( const char* user, const char* pw, Daemon *d ) {
	return store_cred(user, pw, ADD_MODE, d);	
}

int queryCredential( const char* user, Daemon *d ) {
	return store_cred(user, NULL, QUERY_MODE, d);	
}

char* getStoredCredential(const char *username, const char *domain)
{

		// Hopefully we're
		// running as LocalSystem here, otherwise we can't get at the 
		// password stash.
	lsa_mgr lsaMan;
	char pw[255];
	wchar_t w_fullname[512];
	wchar_t *w_pw;

	if ( !username || !domain ) {
		return NULL;
	}

	if ( _snwprintf(w_fullname, 254, L"%S@%S", username, domain) < 0 ) {
		return NULL;
	}

	// make sure we're SYSTEM when we do this
	w_pw = lsaMan.query(w_fullname);

	if ( ! w_pw ) {
		dprintf(D_ALWAYS, 
			"getStoredCredential(): Could not locate credential for user "
			"'%s@%s'\n", username, domain);
		return NULL;
	}

	if ( _snprintf(pw, 511, "%S", w_pw) < 0 ) {
		return NULL;
	}

	// we don't need the wide char pw anymore, so clean it up
	SecureZeroMemory(w_pw, wcslen(w_pw)*sizeof(wchar_t));
	delete[](w_pw);

	dprintf(D_FULLDEBUG, "Found credential for user '%s'\n", username);
	return strdup(pw);
}

#endif	// of Win32 Code

