/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
* CONDOR Copyright Notice
*
* See LICENSE.TXT for additional notices and disclaimers.
*
* Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
* University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
* No use of the CONDOR Software Program Source Code is authorized 
* without the express consent of the CONDOR Team.  For more information 
* contact: CONDOR Team, Attention: Professor Miron Livny, 
* 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
* (608) 262-0856 or miron@cs.wisc.edu.
*
* U.S. Government Rights Restrictions: Use, duplication, or disclosure 
* by the U.S. Government is subject to restrictions as set forth in 
* subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
* Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
* (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
* 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
* Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
* WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/


#include "condor_common.h"
#include "get_daemon_addr.h"
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
	
	wchar_t pwbuf[255];
	wchar_t userbuf[255];

	char *user = NULL;
	char *pw = NULL;
	int mode;
	int result;
	priv_state priv;
	int errno_result = 0;
	int answer = FALSE;
	lsa_mgr lsa_man;
	
	s->decode();
	
	result = code_store_cred(s, user, pw, mode);
	
	if( result == FALSE )
	{
		dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
		if( user ) {
			free( user );
		}
		if ( pw ) {
			free ( pw );
		}
		return;
	}

	dprintf( D_FULLDEBUG, "store_cred: Switching to root priv\n");
	
	priv = set_root_priv();
	
	switch(mode) {
	case ADD_MODE:
		dprintf( D_FULLDEBUG, "Adding %s to credential storage.\n", 
			user );
		// call lsa_mgr api
		// answer = return code
		swprintf(userbuf, L"%S", user);
		swprintf(pwbuf, L"%S", pw);
		answer = lsa_man.add(userbuf, pwbuf);
		ZeroMemory(pwbuf, 255*sizeof(wchar_t)); 
		ZeroMemory(userbuf, 255*sizeof(wchar_t));
		break;
	case DELETE_MODE:
		dprintf( D_FULLDEBUG, "Deleting %s from credential storage.\n", 
			user );
		// call lsa_mgr api
		// answer = return code
		swprintf(userbuf, L"%S", user);
		answer = lsa_man.remove(userbuf);
		ZeroMemory(userbuf, 255*sizeof(wchar_t));
		break;
	default:
		dprintf( D_ALWAYS, "store_cred: Unknown access mode.\n" );
		if( user ) {
			free( user );
		}
		if ( pw ) {
			free ( pw );
		}
		return;
	}
	
	if( user ) {
		free( user );
	}
	if ( pw ) {
		free ( pw );
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
		if( return_val ) {
			dprintf(D_FULLDEBUG, "Schedd says the Addition succeeded!\n");					
		} else {
			dprintf(D_FULLDEBUG, "Schedd says the Addition failed!\n");
		}
		break;
	case DELETE_MODE:
		if( return_val ) {
			dprintf(D_FULLDEBUG, "Schedd says the Delete succeeded!\n");
		} else {
			dprintf(D_FULLDEBUG, "Schedd says the Delete failed!\n");
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
		
		printf("\nRe-enter password: ");
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


