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

	if ( user ) {
		char *tmp=NULL;
		if ( (tmp=strchr(user,'@')) ) {
			*tmp = '\0';		// replace @ sign with a NULL
		}
		if ( stricmp(user,"condor")==0 ) {
				// Not allowed to set the user "condor" password over the network!
			dprintf(D_ALWAYS, 
				"store_cred_handler: "
				"ERROR - attempt to set pool password via network!\n");
			answer = FAILURE_BAD_PASSWORD;
		} else {
			if (tmp) {
				*tmp = '@';		// put back the @ sign we removed above
			}
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


int 
store_cred(const char* user, const char* pw, int mode, Daemon* d) {
//----For debugging purposes-----
//	DebugFP = stdout;
//	DebugFlags = D_ALL;
//-------------------------------
	
	int result;
	int return_val;
	Sock* sock = NULL;
	int cmd = STORE_CRED;

		// If we are root / SYSTEM and we want the local schedd, 
		// then do the work directly to the local registry.
		// If not, then send the request over the wire to a remote credd or schedd.

	if ( is_root() && d == NULL ) {
			// do the work directly onto the local registry
		return_val = store_cred_service(user,pw,mode);
	} else {
			// send out request remotely.

			// If credd_host is defined, use that as a pointer to our credd on
			// the network.  If not defined, try to use the schedd like 
			// we've always done in the past.
		char *credd_host = param("CREDD_HOST");
		if (credd_host) {
				// Setup a socket to a central credd
			MyString credd_sinful;
			credd_sinful = "<" ;
			credd_sinful += credd_host;
			credd_sinful += ">";
			free(credd_host);
			credd_host = NULL;

			dprintf(D_FULLDEBUG,"Storing credential to credd at %s\n",
					credd_sinful.Value());
			Daemon credd(DT_ANY,credd_sinful.Value());
			sock = credd.startCommand(cmd, Stream::reli_sock, 0);
		} else {
				// Setup a socket to a schedd
			if (d == NULL) {
				dprintf(D_FULLDEBUG,"Storing credential to local schedd\n");
				Daemon my_schedd(DT_SCHEDD, NULL, NULL);
				sock = my_schedd.startCommand(cmd, Stream::reli_sock, 0);
			} else {
				dprintf(D_FULLDEBUG,"Storing credential to remote schedd\n");
				sock = d->startCommand(cmd, Stream::reli_sock, 0);
			}
		}
		
		if( !sock ) {
			dprintf(D_ALWAYS, 
				"STORE_CRED: Failed to start command.\n");
			return FALSE;
		}
		
		result = code_store_cred(sock, (char*&)user, (char*&)pw, mode);
		
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
isValidCredential( const char *input_user, const char* input_pw ) {
	// see if we can get a user token from this password
	HANDLE usrHnd = NULL;
	char* dom;
	DWORD LogonUserError;
	BOOL retval;

	retval = 0;
	usrHnd = NULL;

	char * user = strdup(input_user);
	
	// split the domain and the user name for LogonUser
	dom = strchr(user, '@');

	if ( dom ) {
		*dom = '\0';
		dom++;
	}

	// user "condor" is magic and treated as the pool password - it doesn't 
	// have to exist as an actual local user.
	if ( stricmp(user,"condor")==0 ) {
		if (user) free(user);
		return true;
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

int deleteCredential( const char* user ) {
	return store_cred(user, NULL, DELETE_MODE);	
}

int addCredential( const char* user, const char* pw ) {
	return store_cred(user, pw, ADD_MODE);	
}

int queryCredential( const char* user ) {
	return store_cred(user, NULL, QUERY_MODE);	
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

