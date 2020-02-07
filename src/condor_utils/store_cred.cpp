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
#include "condor_daemon_core.h"
#include "daemon.h"
#include "condor_uid.h"
#include "lsa_mgr.h"
#include "store_cred.h"
#include "condor_config.h"
#include "ipv6_hostname.h"
#include "credmon_interface.h"
#include "secure_file.h"
#include "condor_base64.h"
#include "zkm_base64.h"
#include "my_popen.h"
#include "directory.h"

#include <chrono>
#include <algorithm>
#include <iterator>
#include <unordered_set>

static int code_store_cred(Stream *socket, char* &user, char* &pw, int &mode);

namespace {

class NamedCredentialCache
{
private:
	std::vector<std::string> m_creds;
	std::chrono::steady_clock::time_point m_last_refresh;

public:
	NamedCredentialCache() {}

	void Refresh() {
		m_creds.clear();
		m_last_refresh = std::chrono::steady_clock::time_point();
	}

	bool List(std::vector<std::string> &creds, CondorError *err);
};

NamedCredentialCache g_cred_cache;

char *
read_password_from_filename(const char *filename, CondorError *err)
{
	char  *buffer = nullptr;
	size_t len;
	bool rc = read_secure_file(filename, (void**)(&buffer), &len, true);
	if(rc) {
		// buffer now contains the binary contents from the file.
		// due to the way 8.4.X and earlier version wrote the file,
		// there will be trailing NULL characters, although they are
		// ignored in 8.4.X by the code that reads them.  As such, for
		// us to agree on the password, we also need to discard
		// everything after the first NULL.  we do this by simply
		// resetting the len.  there is a function "strnlen" but it's a
		// GNU extension so we just do the raw scan here:
		size_t newlen = 0;
		while(newlen < len && buffer[newlen]) {
			newlen++;
		}
		len = newlen;

		// undo the trivial scramble
		char *pw = (char *)malloc(len + 1);
		simple_scramble(pw, buffer, len);
		pw[len] = '\0';
		free(buffer);
		return pw;
	}

	if (err) err->pushf("CRED", 1, "Failed to read file %s securely.", filename);
	dprintf(D_ALWAYS, "read_password_from_filename(): read_secure_file(%s) failed!\n", filename);
	return nullptr;
}

}


#ifndef WIN32
	// **** UNIX CODE *****

void SecureZeroMemory(void *p, size_t n)
{
	// TODO: make this Secure
	memset(p, 0, n);
}


int
OAUTH_STORE_CRED(const char *user, const char *pw, const int len, int mode, int &cred_modified)
{
	// store a SciToken produced by the credential producer.
	//
	// this is a stop-gap measure to transition from the "old" (cern/desy)
	// framework into the "new" (scitokens) framework.  once the modules
	// exist and are configurable, the token should be stored using that
	// method, not via the credential producer.

	dprintf(D_ALWAYS, "OAUTH store cred user %s len %i mode %i\n", user, len, mode);

	// only set to true if it actually happens
	cred_modified = false;

	auto_free_ptr cred_dir( param("SEC_CREDENTIAL_DIRECTORY") );
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got STORE_CRED but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return FAILURE;
	}

	// get username
	char username[256];
	const char *at = strchr(user, '@');
	strncpy(username, user, (at-user));
	username[at-user] = 0;

	// remove mark on update for "mark and sweep"
	credmon_clear_mark(username);

	// create dir for user's creds
	MyString user_cred_dir;
	user_cred_dir.formatstr("%s%c%s", cred_dir.ptr(), DIR_DELIM_CHAR, username);
	mkdir(user_cred_dir.Value(), 0700);

	// create filenames
	char tmpfilename[PATH_MAX];
	char filename[PATH_MAX];
	sprintf(tmpfilename, "%s%cscitokens.top.tmp", user_cred_dir.Value(), DIR_DELIM_CHAR);
	sprintf(filename, "%s%cscitokens.top", user_cred_dir.Value(), DIR_DELIM_CHAR);
	dprintf(D_ALWAYS, "Writing user cred data to %s\n", tmpfilename);

	// contents of pw are base64 encoded.  decode now just before they go
	// into the file.
	int rawlen = -1;
	unsigned char* rawbuf = NULL;
	zkm_base64_decode(pw, &rawbuf, &rawlen);

	if (rawlen <= 0) {
		dprintf(D_ALWAYS, "Failed to decode credential!\n");
		free(rawbuf);
		return false;
	}

	// create user cred dir
	priv_state priv = set_root_priv();
	mkdir(user_cred_dir.Value(), 0700);
	set_priv(priv);

	// write temp file
	int rc = write_secure_file(tmpfilename, rawbuf, rawlen, true);

	// caller of condor_base64_decode is responsible for freeing buffer
	free(rawbuf);

	if (rc != SUCCESS) {
		dprintf(D_ALWAYS, "Failed to write secure temp file %s\n", tmpfilename);
		return FAILURE;
	}

	// now move into place
	dprintf(D_ALWAYS, "Renaming %s to %s\n", tmpfilename, filename);
	priv = set_root_priv();
	rc = rename(tmpfilename, filename);
	set_priv(priv);

	if (rc == -1) {
		dprintf(D_ALWAYS, "Failed to rename %s to %s\n", tmpfilename, filename);

		// should we rm tmpfilename ?
		return FAILURE;
	}

	// credential succesfully stored
	cred_modified = true;
	return SUCCESS;
}

int
UNIX_STORE_CRED(const char *user, const char *pw, const int len, int mode, int &cred_modified)
{
	dprintf(D_ALWAYS, "Unix store cred user %s len %i mode %i\n", user, len, mode);

	// only set to true if it actually happens
	cred_modified = false;

	auto_free_ptr cred_dir( param("SEC_CREDENTIAL_DIRECTORY") );
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got STORE_CRED but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return FAILURE;
	}

	// get username
	char username[256];
	const char *at = strchr(user, '@');
	strncpy(username, user, (at-user));
	username[at-user] = 0;

	// remove mark on update for "mark and sweep"
	credmon_clear_mark(username);

	// check to see if .cc already exists
	char ccfilename[PATH_MAX];
	sprintf(ccfilename, "%s%c%s.cc", cred_dir.ptr(), DIR_DELIM_CHAR, username);
	struct stat cred_stat_buf;
	int rc = stat(ccfilename, &cred_stat_buf);

	// if the credential already exists, we should update it if
	// it's more than X seconds old.  if X is zero, we always
	// update it.  if X is negative, we never update it.
	int fresh_time = param_integer("SEC_CREDENTIAL_REFRESH_INTERVAL", -1);

	// if it already exists and we don't update, call it a "success".
	if (rc==0 && fresh_time < 0) {
		dprintf(D_FULLDEBUG, "CREDMON: credentials for user %s already exist in %s, and interval is %i\n",
			username, ccfilename, fresh_time );

		return SUCCESS;
	}

	// return success if the credential exists and has been recently
	// updated.  note that if fresh_time is zero, we'll never return
	// success here, meaning we will always update the credential.
	time_t now = time(NULL);
	if ((rc==0) && (now - cred_stat_buf.st_mtime < fresh_time)) {
		// was updated in the last X seconds, so call it a "success".
		dprintf(D_FULLDEBUG, "CREDMON: credentials for user %s already exist in %s, and interval is %i\n",
			username, ccfilename, fresh_time );

		return SUCCESS;
	}

	// create filenames
	char tmpfilename[PATH_MAX];
	char filename[PATH_MAX];
	sprintf(tmpfilename, "%s%c%s.cred.tmp", cred_dir.ptr(), DIR_DELIM_CHAR, username);
	sprintf(filename, "%s%c%s.cred", cred_dir.ptr(), DIR_DELIM_CHAR, username);
	dprintf(D_ALWAYS, "Writing credential data to %s\n", tmpfilename);

	// contents of pw are base64 encoded.  decode now just before they go
	// into the file.
	int rawlen = -1;
	unsigned char* rawbuf = NULL;
	zkm_base64_decode(pw, &rawbuf, &rawlen);

	if (rawlen <= 0) {
		dprintf(D_ALWAYS, "Failed to decode credential!\n");
		free(rawbuf);
		return false;
	}

	// write temp file
	rc = write_secure_file(tmpfilename, rawbuf, rawlen, true);

	// caller of condor_base64_decode is responsible for freeing buffer
	free(rawbuf);

	if (rc != SUCCESS) {
		dprintf(D_ALWAYS, "Failed to write secure temp file %s\n", tmpfilename);
		return FAILURE;
	}

	// now move into place
	dprintf(D_ALWAYS, "Renaming %s to %s\n", tmpfilename, filename);
	priv_state priv = set_root_priv();
	rc = rename(tmpfilename, filename);
	set_priv(priv);

	if (rc == -1) {
		dprintf(D_ALWAYS, "Failed to rename %s to %s\n", tmpfilename, filename);

		// should we rm tmpfilename ?
		return FAILURE;
	}

	// credential succesfully stored
	cred_modified = true;
	return SUCCESS;
}


char*
UNIX_GET_CRED(const char *user, const char *domain)
{
	dprintf(D_ALWAYS, "Unix get cred user %s domain %s\n", user, domain);

	auto_free_ptr cred_dir( param("SEC_CREDENTIAL_DIRECTORY") );
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got GET_CRED but SEC_CREDENTIAL_DIRECTORY not defined!\n");
		return NULL;
	}

	// create filenames
	MyString filename;
	filename.formatstr("%s%c%s.cred", cred_dir.ptr(), DIR_DELIM_CHAR, user);
	dprintf(D_ALWAYS, "CREDS: reading data from %s\n", filename.c_str());

	// read the file (fourth argument "true" means as_root)
	unsigned char *buf = 0;
	size_t len = 0;
	bool rc = read_secure_file(filename.c_str(), (void**)(&buf), &len, true);

	if(rc) {
		// immediately convert to base64
		char* textpw = condor_base64_encode(buf, len);
		free(buf);
		return textpw;
	}

	return NULL;
}


char* getStoredCredential(const char *username, const char *domain)
{
	// TODO: add support for multiple domains

	if ( !username || !domain ) {
		return NULL;
	}

	if (strcmp(username, POOL_PASSWORD_USERNAME) != 0) {
		dprintf(D_ALWAYS, "GOT UNIX GET CRED\n");
		return UNIX_GET_CRED(username, domain);
	} 

	// See if the security manager has overridden the pool password.
	const std::string &secman_pass = SecMan::getPoolPassword();
	if (secman_pass.size()) {
		return strdup(secman_pass.c_str());
	}

	// EVERYTHING BELOW HERE IS FOR POOL PASSWORD ONLY

	auto_free_ptr filename( param("SEC_PASSWORD_FILE") );
	if (!filename) {
		dprintf(D_ALWAYS,
		        "error fetching pool password; "
		            "SEC_PASSWORD_FILE not defined\n");
		return NULL;
	}

	return read_password_from_filename(filename.ptr(), nullptr);
}

int store_cred_service(const char *user, const char *cred, const size_t credlen, int mode, int &cred_modified)
{
	const char *at = strchr(user, '@');
	if ((at == NULL) || (at == user)) {
		dprintf(D_ALWAYS, "store_cred: malformed user name\n");
		return FAILURE;
	}
	if (( (size_t)(at - user) != strlen(POOL_PASSWORD_USERNAME)) ||
	    (memcmp(user, POOL_PASSWORD_USERNAME, at - user) != 0))
	{
		// See if we are operating in "new" or "old" mode and dispatch accordingly
		if (param_boolean("CREDD_OAUTH_MODE", false)) {
			dprintf(D_ALWAYS, "GOT OAUTH STORE CRED\n");
			return OAUTH_STORE_CRED(user, cred, credlen, mode, cred_modified);
		}

		dprintf(D_ALWAYS, "GOT UNIX STORE CRED\n");
		return UNIX_STORE_CRED(user, cred, credlen, mode, cred_modified);
	}

	//
	// THIS CODE BELOW ALL DEALS EXCLUSIVELY WITH POOL PASSWORD
	//

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
		size_t cred_sz = strlen(cred);
		if (!cred_sz) {
			dprintf(D_ALWAYS,
			        "store_cred_service: empty password not allowed\n");
			break;
		}
		if (cred_sz > MAX_PASSWORD_LENGTH) {
			dprintf(D_ALWAYS, "store_cred_service: password too large\n");
			break;
		}
		priv_state priv = set_root_priv();
		answer = write_password_file(filename, cred);
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

	// if we got here we were dealing with pool password.  a return value of
	// SUCCESS means we operated on the credential, so it was modified.
	cred_modified = (answer == SUCCESS);

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

int store_cred_service(const char *user, const char *pw, const size_t, int mode, int &cred_modified)
{

	wchar_t pwbuf[MAX_PASSWORD_LENGTH];
	wchar_t userbuf[MAX_PASSWORD_LENGTH];
	priv_state priv;
	int answer = FAILURE;
	lsa_mgr lsa_man;
	wchar_t *pw_wc;
	
	// we'll need a wide-char version of the user name later
	if ( user ) {
		swprintf_s(userbuf, COUNTOF(userbuf), L"%S", user);
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
				swprintf_s(pwbuf, COUNTOF(pwbuf), L"%S", pw); // make a wide-char copy first
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
	
	// if we got here we were dealing with pool password.  a return value of
	// SUCCESS means we operated on the credential, so it was modified.
	cred_modified = (answer == SUCCESS);

	return answer;
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


int
get_cred_handler(void *, int /*i*/, Stream *s)
{
	char *client_user = NULL;
	char *client_domain = NULL;
	char *client_ipaddr = NULL;
	int result;
	char * user = NULL;
	char * domain = NULL;
	char * password = NULL;

	/* Check our connection.  We must be very picky since we are talking
	   about sending out passwords.  We want to make certain
	     a) the Stream is a ReliSock (tcp)
		 b) it is authenticated (and thus authorized by daemoncore)
		 c) it is encrypted
	*/

	if ( s->type() != Stream::reli_sock ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt via UDP from %s\n",
				((Sock*)s)->peer_addr().to_sinful().Value());
		return TRUE;
	}

	ReliSock* sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
				"WARNING - authentication failed for password fetch attempt from %s\n",
				sock->peer_addr().to_sinful().Value());
		goto bail_out;
	}

	// Enable encryption if available. If it's not available, the next
	// call will fail and we'll abort the connection.
	sock->set_crypto_mode(true);

	if ( !sock->get_encryption() ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt without encryption from %s\n",
				sock->peer_addr().to_sinful().Value());
		goto bail_out;
	}

		// Get the username and domain from the wire

	//dprintf (D_ALWAYS, "First potential block in get_cred_handler, DC==%i\n", daemonCore != NULL);

	sock->decode();

	result = sock->code(user);
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to recv user.\n");
		goto bail_out;
	}

	result = sock->code(domain);
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to recv domain.\n");
		goto bail_out;
	}

	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to recv eom.\n");
		goto bail_out;
	}

	client_user = strdup(sock->getOwner());
	client_domain = strdup(sock->getDomain());
	client_ipaddr = strdup(sock->peer_addr().to_sinful().Value());

		// Now fetch the password from the secure store --
		// If not LocalSystem, this step will fail.
	password = getStoredCredential(user,domain);
	if (!password) {
		dprintf(D_ALWAYS,
			"Failed to fetch password for %s@%s requested by %s@%s at %s\n",
			user,domain,
			client_user,client_domain,client_ipaddr);
		goto bail_out;
	}

		// Got the password, send it
	sock->encode();
	result = sock->code(password);
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to send password.\n");
		goto bail_out;
	}

	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "get_passwd_handler: Failed to send eom.\n");
		goto bail_out;
	}

		// Now that we sent the password, immediately zero it out from ram
	SecureZeroMemory(password,strlen(password));

	dprintf(D_ALWAYS,
			"Fetched user %s@%s password requested by %s@%s at %s\n",
			user,domain,client_user,client_domain,client_ipaddr);

bail_out:
	if (client_user) free(client_user);
	if (client_domain) free(client_domain);
	if (client_ipaddr) free(client_ipaddr);
	if (user) free(user);
	if (domain) free(domain);
	if (password) free(password);
	return TRUE;
}


// forward declare the non-blocking continuation function.
void store_cred_handler_continue();

// declare a simple data structure for holding the info needed
// across non-blocking retries
struct StoreCredState {
	char *user;
	int  retries;
	Stream *s;
};


/* NOW WORKS ON BOTH WINDOWS AND UNIX */
int store_cred_handler(void *, int /*i*/, Stream *s)
{
	char *user = NULL;
	char *pw = NULL;
	int mode = STORE_CRED_FIRST_MODE - 1;
	int result;
	int answer = FAILURE;
	int cred_modified = false;

	//dprintf (D_ALWAYS, "First potential block in store_cred_handler, DC==%i\n", daemonCore != NULL);

	if ( s->type() != Stream::reli_sock ) {
		dprintf(D_ALWAYS,
			"WARNING - credential store attempt via UDP from %s\n",
				((Sock*)s)->peer_addr().to_sinful().Value());
		return FALSE;
	}

	ReliSock *sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded and enable encryption
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
			"WARNING - authentication failed for credential store attempt from %s\n",
			sock->peer_addr().to_sinful().Value());
		return FALSE;
	}

	s->set_crypto_mode( true );

	s->decode();

	result = code_store_cred(s, user, pw, mode);

	if( result == FALSE ) {
		dprintf(D_ALWAYS, "store_cred: code_store_cred failed.\n");
		goto cleanup_and_exit;
	}

	if (mode < STORE_CRED_FIRST_MODE || mode > STORE_CRED_LAST_MODE) {
		dprintf(D_ALWAYS, "store_cred: %d is not a valid mode\n", mode);
		answer = FAILURE;
		goto cleanup_and_exit;
	}

	if ( user ) {
			// ensure that the username has an '@' delimteter
		char const *tmp = strchr(user, '@');
		if ((tmp == NULL) || (tmp == user)) {
			dprintf(D_ALWAYS, "store_cred_handler: user not in user@domain format\n");
			answer = FAILURE;
		}
		else {
				// We don't allow one user to set another user's credential
				//   (except for users explicitly allowed to)
				// TODO: We deliberately ignore the user domains. Isn't
				//   that a security issue?
				// we don't allow updates to the pool password through this interface
			std::string param_val;
			param(param_val, "CRED_SUPER_USERS");
			StringList auth_users( param_val.c_str() );
			auth_users.append(std::string(user).substr(0, tmp-user).c_str());
			const char *sock_owner = sock->getOwner();
			if ( sock_owner == NULL ||
#if defined(WIN32)
			     !auth_users.contains_anycase_withwildcard( sock_owner )
#else
			     !auth_users.contains_withwildcard( sock_owner )
#endif
			   )
			{
				dprintf( D_ALWAYS, "WARNING: store_cred() for user %s attempted by user %s, rejecting\n", user, sock_owner ? sock_owner : "<unknown>" );
				answer = FAILURE;

			} else if ((mode != QUERY_MODE) &&
			    (tmp - user == strlen(POOL_PASSWORD_USERNAME)) &&
			    (memcmp(user, POOL_PASSWORD_USERNAME, tmp - user) == 0))
			{
				dprintf(D_ALWAYS, "ERROR: attempt to set pool password via STORE_CRED! (must use STORE_POOL_CRED)\n");
				answer = FAILURE;
			} else {
				size_t pwlen = 0;
				if(pw) {
					pwlen = strlen(pw)+1;
				}
				answer = store_cred_service(user,pw,pwlen,mode,cred_modified);
			}
		}
	}

	// no credmon on windows
#ifdef WIN32
#else

	// see if we're in "new" mode.  call a hook to translate refresh into access
	if(param_boolean("CREDD_OAUTH_MODE", false)) {
		char* cthook = param("SEC_CREDD_TOKEN_HOOK");
		if (!cthook) {
			dprintf(D_ALWAYS, "CREDS: no SEC_CREDD_TOKEN_HOOK... skipping\n");
		} else {
			MyString credd_token_hook = cthook;
			free(cthook);

			// run it with the full path to "top" cred as an argument
			char* scd = param("SEC_CREDENTIAL_DIRECTORY");
			if (!scd) {
				dprintf(D_ALWAYS, "CREDS: no SEC_CREDENTIAL_DIRECTORY\n");
				return false;
			}
			MyString cred_filename;
			cred_filename.formatstr("%s/%s/%s", scd, sock->getOwner(), "scitokens.top");
			free(scd);

			ArgList hook_args;
			hook_args.AppendArg(credd_token_hook.Value());
			hook_args.AppendArg(cred_filename.Value());

			dprintf(D_ALWAYS, "CREDS: invoking %s %s as root\n", credd_token_hook.Value(), cred_filename.Value());
			priv_state priv = set_root_priv();

			// need to use Create_Process and make this non-blocking
			int rc = my_system(hook_args);

			set_priv(priv);
			if (rc) {
				// fail
				dprintf(D_ALWAYS, "CREDS: invoking %s %s failed with %i.\n", credd_token_hook.Value(), cred_filename.Value(), rc);
				return FALSE;
			}

			// success
			dprintf(D_ALWAYS, "CREDS: success converting %s\n", cred_filename.Value());
		}
	}

	// we only need to signal CREDMON if the file was just written.  if it
	// already existed, just leave it be, and don't signal the CREDMON.
	if(answer == SUCCESS && cred_modified) {
		// good so far, but the real answer is determined by our ability
		// to signal the credmon and have the .cc file appear.  we don't
		// want this to block so we go back to daemoncore and set a timer
		// for 0 seconds.  the timer will reset itself as needed.
		answer = credmon_poll_setup(user, false, true);
		if (answer == SUCCESS) {
			StoreCredState* retry_state = (StoreCredState*)malloc(sizeof(StoreCredState));
			retry_state->user = strdup(user);
			retry_state->retries = param_integer("CREDD_POLLING_TIMEOUT", 20);
			retry_state->s = new ReliSock(*((ReliSock*)s));

			dprintf( D_FULLDEBUG, "NBSTORECRED: retry_state: %lx, dptr->user: %s, dptr->retries: %i, dptr->s %lx\n",
				(unsigned long)retry_state, retry_state->user, retry_state->retries, (unsigned long)(retry_state->s));
			daemonCore->Register_Timer(0, store_cred_handler_continue, "Poll for existence of .cc file");
			daemonCore->Register_DataPtr(retry_state);
		}
	} else {
		dprintf(D_SECURITY | D_FULLDEBUG, "NBSTORECRED: not signaling credmon.  (answer==%i, cred_modified==%i)\n", answer, cred_modified);
	}
#endif // WIN32

cleanup_and_exit:
	if (pw) {
		SecureZeroMemory(pw, strlen(pw));
		free(pw);
	}
	if (user) {
		free(user);
	}

#ifndef WIN32  // no credmon on windows
	// if we modified the credential, then we also attempted to register a
	// timer to poll for the cred file.  if that happened succesfully, return
	// now so we can poll in a non-blocking way.
	//
	// if we either had a failure, or had success but didn't modify the file,
	// then there's nothing to poll for and we should not return and should
	// finish the wire protocol below.
	//
	if(answer == SUCCESS && cred_modified) {
		return TRUE;
	}
#endif // WIN32

	s->encode();
	if( ! s->code(answer) ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send result.\n" );
		return FALSE;
	}

	if( ! s->end_of_message() ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send end of message.\n");
	}

	return (answer == SUCCESS);
}


#ifdef WIN32
#else
void store_cred_handler_continue()
{
	// can only be called when daemonCore is non-null since otherwise
	// there's no data
	if(!daemonCore) return;

	StoreCredState *dptr = (StoreCredState*)daemonCore->GetDataPtr();

	dprintf( D_FULLDEBUG, "NBSTORECRED: dptr: %lx, dptr->user: %s, dptr->retries: %i, dptr->s: %lx\n", (unsigned long)dptr, dptr->user, dptr->retries, (unsigned long)(dptr->s));
	int answer = credmon_poll_continue(dptr->user, dptr->retries);
	dprintf( D_FULLDEBUG, "NBSTORECRED: answer: %i\n", answer);

	if (answer == FAILURE) {
		if (dptr->retries > 0) {
			// re-register timer with one less retry
			dprintf( D_FULLDEBUG, "NBSTORECRED: re-registering timer and dptr\n");
			dptr->retries--;
			daemonCore->Register_Timer(1, store_cred_handler_continue, "Poll for existence of .cc file");
			daemonCore->Register_DataPtr(dptr);
			return;
		}
	}

	// regardless of SUCCESS or FAILURE, if we got here we need to finish
	// the wire protocol for STORE_CRED

	dprintf( D_FULLDEBUG, "NBSTORECRED: finishing wire protocol on stream %lx\n", (unsigned long)(dptr->s));
	dptr->s->encode();
	if( ! dptr->s->code(answer) ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send result.\n" );
	} else if( ! dptr->s->end_of_message() ) {
		dprintf( D_ALWAYS,
			"store_cred: Failed to send end of message.\n");
	}

	dprintf( D_FULLDEBUG, "NBSTORECRED: freeing %lx\n", (unsigned long)dptr);

	// we copied the stream and strdup'ed the user, so do a deep free of dptr
	delete (dptr->s);
	free(dptr->user);
	free(dptr);

	dprintf( D_FULLDEBUG, "NBSTORECRED: done!\n");
	return;
}
#endif


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

int store_pool_cred_handler(class Service *, int, Stream *s)
{
	int result;
	char *pw = NULL;
	char *domain = NULL;
	MyString username = POOL_PASSWORD_USERNAME "@";

	if (s->type() != Stream::reli_sock) {
		dprintf(D_ALWAYS, "ERROR: pool password set attempt via UDP\n");
		return CLOSE_STREAM;
	}

	// if we're the CREDD_HOST, make sure any password setting is done locally
	// (since knowing what the pool password is on the CREDD_HOST means being
	//  able to fetch users' passwords)
	char *credd_host = param("CREDD_HOST");
	if (credd_host) {

		MyString my_fqdn_str = get_local_fqdn();
		MyString my_hostname_str = get_local_hostname();
		// TODO: Arbitrarily picking IPv4
		MyString my_ip_str = get_local_ipaddr(CP_IPV4).to_ip_string();

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
				return CLOSE_STREAM;
			}
		}
		free(credd_host);
	}

	//dprintf (D_ALWAYS, "First potential block in store_pool_cred_handler, DC==%i\n", daemonCore != NULL);

	// we don't actually care if the cred was modified in this
	// situation, but the below function signature requires it
	int cred_modified = false;

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
	if (pw && *pw) {
		size_t pwlen = strlen(pw)+1;
		result = store_cred_service(username.Value(), pw, pwlen, ADD_MODE, cred_modified);
		SecureZeroMemory(pw, strlen(pw));
	}
	else {
		result = store_cred_service(username.Value(), NULL, 0, DELETE_MODE, cred_modified);
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

	return CLOSE_STREAM;
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
		size_t pwlen = 0;
		if(pw) {
			pwlen=strlen(pw)+1;
		}
		// we don't actually care if the cred was modified in this
		// situation, but the below function signature requires it
		int cred_modified = false;

		return_val = store_cred_service(user,pw,pwlen,mode,cred_modified);
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
		// For STORE_CRED, enable encryption.
		if ( cmd == STORE_CRED ) {
			sock->set_crypto_mode( true );
		}

		if (!force && (d != NULL) &&
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
			if (!sock->put(user) ||
				!sock->put(pw) ||
				!sock->end_of_message()) {
				dprintf(D_ALWAYS, "store_cred: failed to send STORE_POOL_CRED message\n");
				delete sock;
				return FAILURE;
			}
		}
		
		//dprintf (D_ALWAYS, "First potential block in store_cred, DC==%i\n", daemonCore != NULL);

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
	cch = MIN(cch, maxlength-1);
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
	
	buf = (char *)malloc(MAX_PASSWORD_LENGTH + 1);
	
	if (! buf) { fprintf(stderr, "Out of Memory!\n\n"); return NULL; }
	
		
	printf("Enter password: ");
	if ( ! read_from_keyboard(buf, MAX_PASSWORD_LENGTH + 1, false) ) {
		free(buf);
		return NULL;
	}
	
	return buf;
}


bool
NamedCredentialCache::List(std::vector<std::string> &creds, CondorError *err)
{
	// First, check to see if our cache is still usable; if so, make a copy.
	auto current = std::chrono::steady_clock::now();
	if (std::chrono::duration_cast<std::chrono::seconds>(current - m_last_refresh).count() < 10) {
		std::copy(m_creds.begin(), m_creds.end(), std::back_inserter(creds));
		return true;
	}

	// Next, iterate through the passwords directory and cache the names
	// Note we reuse the exclude regexp from the configuration subsys.

	std::string dirpath;
	if (!param(dirpath, "SEC_PASSWORD_DIRECTORY")) {
		if (err) err->push("CRED", 1, "SEC_PASSWORD_DIRECTORY is undefined");
		return false;
	}

	const char* _errstr;
	int _erroffset;
	std::string excludeRegex;
		// We simply fail invalid regex as the config subsys should have EXCEPT'd
		// in this case.
	if (!param(excludeRegex, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP")) {
		if (err) err->push("CRED", 1, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP is unset");
		return false;
	}
	Regex excludeFilesRegex;
	if (!excludeFilesRegex.compile(excludeRegex, &_errstr, &_erroffset)) {
		if (err) err->pushf("CRED", 1, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
			"config parameter is not a valid "
			"regular expression.  Value: %s,  Error: %s",
			excludeRegex.c_str(), _errstr ? _errstr : "");
		return false;
	}
	if(!excludeFilesRegex.isInitialized() ) {
		if (err) err->push("CRED", 1, "Failed to initialize exclude files regex.");
		return false;
	}

		// If we can, try reading out the passwords as root.
	TemporaryPrivSentry sentry(get_priv_state() == PRIV_UNKNOWN ? PRIV_UNKNOWN : PRIV_ROOT);

	m_creds.clear();
	std::unordered_set<std::string> tmp_creds;

	std::string pool_password;
	if (param(pool_password, "SEC_PASSWORD_FILE")) {
		if (0 == access(pool_password.c_str(), R_OK)) {
			tmp_creds.insert("POOL");
		}
	}

	Directory dir(dirpath.c_str());
	if (!dir.Rewind()) {
		if (err) {
			err->pushf("CRED", 1, "Cannot open %s: %s (errno=%d)",
				dirpath.c_str(), strerror(errno), errno);
		}
		if (!tmp_creds.empty()) {
			std::copy(tmp_creds.begin(), tmp_creds.end(), std::back_inserter(m_creds));
			std::copy(m_creds.begin(), m_creds.end(), std::back_inserter(creds));
		} else {
			return false;
		}
	}

	const char *file;
	while( (file = dir.Next()) ) {
		if (dir.IsDirectory()) {
			continue;
		}
		if(!excludeFilesRegex.match(file)) {
			if (0 == access(dir.GetFullPath(), R_OK)) {
				tmp_creds.insert(file);
			}
		} else {
			dprintf(D_FULLDEBUG|D_SECURITY, "Ignoring password file "
				"based on LOCAL_CONFIG_DIR_EXCLUDE_REGEXP: "
				"'%s'\n", dir.GetFullPath());
		}
	}

	std::copy(tmp_creds.begin(), tmp_creds.end(), std::back_inserter(m_creds));
	std::sort(m_creds.begin(), m_creds.end());
	std::copy(m_creds.begin(), m_creds.end(), std::back_inserter(creds));

	return true;
}


bool getNamedCredential(const std::string &cred, std::string &contents, CondorError *err) {
	std::string dirpath;
	if (!param(dirpath, "SEC_PASSWORD_DIRECTORY")) {
		if (err) err->push("CRED", 1, "SEC_PASSWORD_DIRECTORY is undefined");
		return false;
	}
	std::string fullpath = dirpath + DIR_DELIM_CHAR + cred;
	std::unique_ptr<char> password(read_password_from_filename(fullpath.c_str(), err));

	if (!password.get()) {
		return false;
	}
	contents = std::string(password.get());
	return true;
}


bool
listNamedCredentials(std::vector<std::string> &creds, CondorError *err) {
	return g_cred_cache.List(creds, err);
}

void refreshNamedCredentials() {
	g_cred_cache.Refresh();
}
