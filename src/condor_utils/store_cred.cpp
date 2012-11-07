/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_io.h"
#include "condor_debug.h"
#include "daemon.h"
#include "condor_uid.h"
#include "lsa_mgr.h"
#include "store_cred.h"
#include "condor_config.h"
#include "ipv6_hostname.h"

static int code_store_cred(Stream *socket, char* &user, char* &pw, int &mode);

#ifndef WIN32
	// **** UNIX CODE *****

void SecureZeroMemory(void *p, size_t n)
{
	// TODO: make this Secure
	memset(p, 0, n);
}

// trivial scramble on password file to prevent accidental viewing
// NOTE: its up to the caller to ensure scrambled has enough room
void simple_scramble(char* scrambled,  const char* orig, int len)
{
	const unsigned char deadbeef[] = {0xDE, 0xAD, 0xBE, 0xEF};

	for (int i = 0; i < len; i++) {
		scrambled[i] = orig[i] ^ deadbeef[i % sizeof(deadbeef)];
	}
}

// writes a pool password file using the given password
// returns SUCCESS or FAILURE
//
int write_password_file(const char* path, const char* password)
{
		int fd = safe_open_wrapper_follow(path,
		                           O_WRONLY | O_CREAT | O_TRUNC,
		                           0600);
		if (fd == -1) {
			dprintf(D_ALWAYS,
			        "store_cred_service: open failed on %s: %s (%d)\n",
			        path,
			        strerror(errno),
					errno);
			return FAILURE;
		}
		FILE *fp = fdopen(fd, "w");
		if (fp == NULL) {
			dprintf(D_ALWAYS,
			        "store_cred_service: fdopen failed: %s (%d)\n",
			        strerror(errno),
			        errno);
			return FAILURE;
		}
		size_t password_len = strlen(password);
		char scrambled_password[MAX_PASSWORD_LENGTH + 1];
		memset(scrambled_password, 0, MAX_PASSWORD_LENGTH + 1);
		simple_scramble(scrambled_password, password, password_len);
		size_t sz = fwrite(scrambled_password, 1, MAX_PASSWORD_LENGTH + 1, fp);
		fclose(fp);
		if (sz != MAX_PASSWORD_LENGTH + 1) {
			dprintf(D_ALWAYS,
			        "store_cred_service: "
			            "error writing to password file: %s (%d)\n",
					strerror(errno),
			        errno);
			return FAILURE;
		}
		return SUCCESS;
}

char* getStoredCredential(const char *username, const char *domain)
{
	// TODO: add support for multiple domains

	if ( !username || !domain ) {
		return NULL;
	}

	if (strcmp(username, POOL_PASSWORD_USERNAME) != 0) {
		dprintf(D_ALWAYS,
		        "getStoredCredential: "
		            "only pool password is supported on UNIX\n");
		return NULL;
	} 

	char *filename = param("SEC_PASSWORD_FILE");
	if (filename == NULL) {
		dprintf(D_ALWAYS,
		        "error fetching pool password; "
		            "SEC_PASSWORD_FILE not defined\n");
		return NULL;
	}

	// open the pool password file with root priv
	priv_state priv = set_root_priv();
	FILE* fp = safe_fopen_wrapper_follow(filename, "r");
	set_priv(priv);
	if (fp == NULL) {
		dprintf(D_FULLDEBUG,
		        "error opening SEC_PASSWORD_FILE (%s), %s (errno: %d)\n",
		        filename,
		        strerror(errno),
		        errno);
		free(filename);
		return NULL;
	}

	// make sure the file owner matches our real uid
	struct stat st;
	if (fstat(fileno(fp), &st) == -1) {
		dprintf(D_ALWAYS,
		        "fstat failed on SEC_PASSWORD_FILE (%s), %s (errno: %d)\n",
		        filename,
		        strerror(errno),
		        errno);
		fclose(fp);
		free(filename);
		return NULL;
	}
	free(filename);
	if (st.st_uid != get_my_uid()) {
		dprintf(D_ALWAYS,
		        "error: SEC_PASSWORD_FILE must be owned "
		            "by Condor's real uid\n");
		fclose(fp);
		return NULL;
	}

	char scrambled_pw[MAX_PASSWORD_LENGTH + 1];
	size_t sz = fread(scrambled_pw, 1, MAX_PASSWORD_LENGTH, fp);
	fclose(fp);

	if (sz == 0) {
		dprintf(D_ALWAYS, "error reading pool password (file may be empty)\n");
		return NULL;
	}
	scrambled_pw[sz] = '\0';  // ensure the last char is nil

	// undo the trivial scramble
	int len = strlen(scrambled_pw);
	char *pw = (char *)malloc(len + 1);
	simple_scramble(pw, scrambled_pw, len);
	pw[len] = '\0';

	return pw;
}

int store_cred_service(const char *user, const char *pw, int mode)
{
	const char *at = strchr(user, '@');
	if ((at == NULL) || (at == user)) {
		dprintf(D_ALWAYS, "store_cred: malformed user name\n");
		return FAILURE;
	}
	if (( (size_t)(at - user) != strlen(POOL_PASSWORD_USERNAME)) ||
	    (memcmp(user, POOL_PASSWORD_USERNAME, at - user) != 0))
	{
		dprintf(D_ALWAYS, "store_cred: only pool password is supported on UNIX\n");
		return FAILURE;
	}

	char *filename;
	if (mode != QUERY_MODE) {
		filename = param("SEC_PASSWORD_FILE");
		if (filename == NULL) {
			dprintf(D_ALWAYS, "store_cred: SEC_PASSWORD_FILE not defined\n");
			return FAILURE;
		}
	}

	int answer;
	switch (mode) {
	case ADD_MODE: {
		answer = FAILURE;
		size_t pw_sz = strlen(pw);
		if (!pw_sz) {
			dprintf(D_ALWAYS,
			        "store_cred_service: empty password not allowed\n");
			break;
		}
		if (pw_sz > MAX_PASSWORD_LENGTH) {
			dprintf(D_ALWAYS, "store_cred_service: password too large\n");
			break;
		}
		priv_state priv = set_root_priv();
		answer = write_password_file(filename, pw);
		set_priv(priv);
		break;
	}
	case DELETE_MODE: {
		priv_state priv = set_root_priv();
		int err = unlink(filename);
		set_priv(priv);
		if (!err) {
			answer = SUCCESS;
		}
		else {
			answer = FAILURE_NOT_FOUND;
		}
		break;
	}
	case QUERY_MODE: {
		char *password = getStoredCredential(POOL_PASSWORD_USERNAME, NULL);
		if (password) {
			answer = SUCCESS;
			SecureZeroMemory(password, MAX_PASSWORD_LENGTH);
			free(password);
		}
		else {
			answer = FAILURE_NOT_FOUND;
		}
		break;
	}
	default:
		dprintf(D_ALWAYS, "store_cred_service: unknown mode: %d\n", mode);
		answer = FAILURE;
	}

	// clean up after ourselves
	if (mode != QUERY_MODE) {
		free(filename);
	}

	return answer;
}

#else
	// **** WIN32 CODE ****

#include <conio.h>

//extern "C" FILE *DebugFP;
//extern "C" int DebugFlags;

char* getStoredCredential(const char *username, const char *domain)
{
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
	priv_state priv = set_root_priv();
	w_pw = lsaMan.query(w_fullname);
	set_priv(priv);

	if ( ! w_pw ) {
		dprintf(D_ALWAYS, 
			"getStoredCredential(): Could not locate credential for user "
			"'%s@%s'\n", username, domain);
		return NULL;
	}

	if ( _snprintf(pw, sizeof(pw), "%S", w_pw) < 0 ) {
		return NULL;
	}

	// we don't need the wide char pw anymore, so clean it up
	SecureZeroMemory(w_pw, wcslen(w_pw)*sizeof(wchar_t));
	delete[](w_pw);

	dprintf(D_FULLDEBUG, "Found credential for user '%s@%s'\n",
		username, domain );
	return strdup(pw);
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

	if ( user ) {
			// ensure that the username has an '@' delimteter
		char const *tmp = strchr(user, '@');
		if ((tmp == NULL) || (tmp == user)) {
			dprintf(D_ALWAYS, "store_cred_handler: user not in user@domain format\n");
			answer = FAILURE;
		}
		else {
				// we don't allow updates to the pool password through this interface
			if ((mode != QUERY_MODE) &&
			    (tmp - user == strlen(POOL_PASSWORD_USERNAME)) &&
			    (memcmp(user, POOL_PASSWORD_USERNAME, tmp - user) == 0))
			{
				dprintf(D_ALWAYS, "ERROR: attempt to set pool password via STORE_CRED! (must use STORE_POOL_CRED)\n");
				answer = FAILURE;
			} else {
				answer = store_cred_service(user,pw,mode);
			}
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
	
	if( ! s->end_of_message() ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send end of message.\n");
	}	

	return;
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

	// the POOL_PASSWORD_USERNAME account is not a real account
	if (strcmp(user, POOL_PASSWORD_USERNAME) == 0) {
		free(user);
		return true;
	}

	char * pw = strdup(input_pw);
	bool wantSkipNetworkLogon = param_boolean("SKIP_WINDOWS_LOGON_NETWORK", false);
	if (!wantSkipNetworkLogon) {
	  retval = LogonUser(
		user,						// user name
		dom,						// domain or server - local for now
		pw,							// password
		LOGON32_LOGON_NETWORK,		// NETWORK is fastest. 
		LOGON32_PROVIDER_DEFAULT,	// logon provider
		&usrHnd						// receive tokens handle
	  );
	  LogonUserError = GetLastError();
	}
	if ( 0 == retval ) {
		if (!wantSkipNetworkLogon) {
		  dprintf(D_FULLDEBUG, "NETWORK logon failed. Attempting INTERACTIVE\n");
		} else {
		  dprintf(D_FULLDEBUG, "NETWORK logon disabled. Trying INTERACTIVE only!\n");
		}
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

#endif // WIN32

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

void store_pool_cred_handler(void *, int  /*i*/, Stream *s)
{
	int result;
	char *pw = NULL;
	char *domain = NULL;
	MyString username = POOL_PASSWORD_USERNAME "@";

	if (s->type() != Stream::reli_sock) {
		dprintf(D_ALWAYS, "ERROR: pool password set attempt via UDP\n");
		return;
	}

	// if we're the CREDD_HOST, make sure any password setting is done locally
	// (since knowing what the pool password is on the CREDD_HOST means being
	//  able to fetch users' passwords)
	char *credd_host = param("CREDD_HOST");
	if (credd_host) {

		MyString my_fqdn_str = get_local_fqdn();
		MyString my_hostname_str = get_local_hostname();
		MyString my_ip_str = get_local_ipaddr().to_ip_string();

		// figure out if we're on the CREDD_HOST
		bool on_credd_host = (strcasecmp(my_fqdn_str.Value(), credd_host) == MATCH);
		on_credd_host = on_credd_host || (strcasecmp(my_hostname_str.Value(), credd_host) == MATCH);
		on_credd_host = on_credd_host || (strcmp(my_ip_str.Value(), credd_host) == MATCH);

		if (on_credd_host) {
				// we're the CREDD_HOST; make sure the source address matches ours
			const char *addr = ((ReliSock*)s)->peer_ip_str();
			if (!addr || strcmp(my_ip_str.Value(), addr)) {
				dprintf(D_ALWAYS, "ERROR: attempt to set pool password remotely\n");
				free(credd_host);
				return;
			}
		}
		free(credd_host);
	}

	s->decode();
	if (!s->code(domain) || !s->code(pw) || !s->end_of_message()) {
		dprintf(D_ALWAYS, "store_pool_cred: failed to receive all parameters\n");
		goto spch_cleanup;
	}
	if (domain == NULL) {
		dprintf(D_ALWAYS, "store_pool_cred_handler: domain is NULL\n");
		goto spch_cleanup;
	}

	// construct the full pool username
	username += domain;	

	// do the real work
	if (pw) {
		result = store_cred_service(username.Value(), pw, ADD_MODE);
		SecureZeroMemory(pw, strlen(pw));
	}
	else {
		result = store_cred_service(username.Value(), NULL, DELETE_MODE);
	}

	s->encode();
	if (!s->code(result)) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to send result.\n");
		goto spch_cleanup;
	}
	if (!s->end_of_message()) {
		dprintf(D_ALWAYS, "store_pool_cred: Failed to send end of message.\n");
	}

spch_cleanup:
	if (pw) free(pw);
	if (domain) free(domain);
}

int 
store_cred(const char* user, const char* pw, int mode, Daemon* d, bool force) {
	
	int result;
	int return_val;
	Sock* sock = NULL;

		// to help future debugging, print out the mode we are in
	static const int mode_offset = 100;
	static const char *mode_name[] = {
		ADD_CREDENTIAL,
		DELETE_CREDENTIAL,
		QUERY_CREDENTIAL
#ifdef WIN32
		, CONFIG_CREDENTIAL
#endif
	};	
	dprintf ( D_ALWAYS, 
		"STORE_CRED: In mode '%s'\n", 
		mode_name[mode - mode_offset] );
	
		// If we are root / SYSTEM and we want a local daemon, 
		// then do the work directly to the local registry.
		// If not, then send the request over the wire to a remote credd or schedd.

	if ( is_root() && d == NULL ) {
			// do the work directly onto the local registry
		return_val = store_cred_service(user,pw,mode);
	} else {
			// send out the request remotely.

			// first see if we're operating on the pool password
		int cmd = STORE_CRED;
		char const *tmp = strchr(user, '@');
		if (tmp == NULL || tmp == user || *(tmp + 1) == '\0') {
			dprintf(D_ALWAYS, "store_cred: user not in user@domain format\n");
			return FAILURE;
		}
		if (((mode == ADD_MODE) || (mode == DELETE_MODE)) &&
		    ( (size_t)(tmp - user) == strlen(POOL_PASSWORD_USERNAME)) &&
		    (memcmp(POOL_PASSWORD_USERNAME, user, tmp - user) == 0))
		{
			cmd = STORE_POOL_CRED;
			user = tmp + 1;	// we only need to send the domain name for STORE_POOL_CRED
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
			dprintf(D_FULLDEBUG, "Starting a command on a REMOTE schedd\n");
			sock = d->startCommand(cmd, Stream::reli_sock, 0);
		}
		
		if( !sock ) {
			dprintf(D_ALWAYS, 
				"STORE_CRED: Failed to start command.\n");
			dprintf(D_ALWAYS, 
				"STORE_CRED: Unable to contact the REMOTE schedd.\n");
			return FAILURE;
		}

		// for remote updates (which send the password), verify we have a secure channel,
		// unless "force" is specified
		if (((mode == ADD_MODE) || (mode == DELETE_MODE)) && !force && (d != NULL) &&
			((sock->type() != Stream::reli_sock) || !((ReliSock*)sock)->triedAuthentication() || !sock->get_encryption())) {
			dprintf(D_ALWAYS, "STORE_CRED: blocking attempt to update over insecure channel\n");
			delete sock;
			return FAILURE_NOT_SECURE;
		}
		
		if (cmd == STORE_CRED) {
			result = code_store_cred(sock, const_cast<char*&>(user),
				const_cast<char*&>(pw), mode);
			if( result == FALSE ) {
				dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
				delete sock;
				return FAILURE;
			}
		}
		else {
				// only need to send the domain and password for STORE_POOL_CRED
			if (!sock->code(const_cast<char*&>(user)) ||
				!sock->code(const_cast<char*&>(pw)) ||
				!sock->end_of_message()) {
				dprintf(D_ALWAYS, "store_cred: failed to send STORE_POOL_CRED message\n");
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

int deleteCredential( const char* user, const char* pw, Daemon *d ) {
	return store_cred(user, pw, DELETE_MODE, d);	
}

int addCredential( const char* user, const char* pw, Daemon *d ) {
	return store_cred(user, pw, ADD_MODE, d);	
}

int queryCredential( const char* user, Daemon *d ) {
	return store_cred(user, NULL, QUERY_MODE, d);	
}

#if !defined(WIN32)

// helper routines for UNIX keyboard input
#include <termios.h>

static struct termios stored_settings;

static void echo_off(void)
{
	struct termios new_settings;
	tcgetattr(0, &stored_settings);
	memcpy(&new_settings, &stored_settings, sizeof(struct termios));
	new_settings.c_lflag &= (~ECHO);
	tcsetattr(0, TCSANOW, &new_settings);
	return;
}

static void echo_on(void)
{
    tcsetattr(0,TCSANOW,&stored_settings);
    return;
}

#endif

// reads at most maxlength chars without echoing to the terminal into buf
bool
read_from_keyboard(char* buf, int maxlength, bool echo) {
#if !defined(WIN32)
	int ch, ch_count;

	ch = ch_count = 0;
	fflush(stdout);

	const char end_char = '\n';
	if (!echo) echo_off();
			
	while ( ch_count < maxlength-1 ) {
		ch = getchar();
		if ( ch == end_char ) {
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

	if (!echo) echo_on();
#else
	/*
	The Windows method for getting keyboard input is very different due to
	issues encountered by British users trying to use the pound character in their
	passwords.  _getch did not accept any input from using the alt+#### method of
	inputting characters nor the pound symbol when using a British keyboard layout.
	The solution was to explicitly use ReadConsoleW, the unicode version of ReadConsole,
	to take in the password and down convert it into ascii.
	See Ticket #1639
	*/
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

	/*
	There is a difference between the code page the console is reporting and
	the code page that needs to be used for the ascii conversion.  Below code
	acts to provide additional debug information should the need arise.
	*/
	//UINT cPage = GetConsoleCP();
	//printf("Console CP: %d\n", cPage);

	//Preserve the previous console mode and switch back once input is complete.
	DWORD oldMode;
	GetConsoleMode(hStdin, &oldMode);
	//Default entry method is to not echo back what is entered.
	DWORD newMode = ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
	if(echo)
	{
		newMode |= ENABLE_ECHO_INPUT;
	}

	SetConsoleMode(hStdin, newMode);

	int cch;
	//maxlength is passed in even though it is a fairly constant value, so need to dynamically allocate.
	wchar_t *wbuffer = new wchar_t[maxlength];
	if(!wbuffer)
	{
		return FALSE;
	}
	ReadConsoleW(hStdin, wbuffer, maxlength, (DWORD*)&cch, NULL);
	SetConsoleMode(hStdin, oldMode);
	//Zero terminate the input.
	cch = min(cch, maxlength-1);
	wbuffer[cch] = '\0';

	--cch;
	//Strip out the newline and return that ReadConsoleW appends and zero terminate again.
	while (cch >= 0)
	{
		if(wbuffer[cch] == '\r' || wbuffer[cch] == '\n')
			wbuffer[cch] = '\0';
		else
			break;
		--cch;
	}

	//Down convert the input into ASCII.
	int converted = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wbuffer, -1, buf, maxlength, NULL, NULL);

	delete[] wbuffer;
#endif

	return TRUE;
}

char*
get_password() {
	char *buf;
	
	buf = new char[MAX_PASSWORD_LENGTH + 1];
	
	if (! buf) { fprintf(stderr, "Out of Memory!\n\n"); return NULL; }
	
		
	printf("Enter password: ");
	if ( ! read_from_keyboard(buf, MAX_PASSWORD_LENGTH + 1, false) ) {
		delete[] buf;
		return NULL;
	}
	
	return buf;
}
