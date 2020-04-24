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

const int MODE_MASK = 3;
const int CRED_TYPE_MASK = 0x2C;

static const char *mode_name[] = {
	ADD_CREDENTIAL,
	DELETE_CREDENTIAL,
	QUERY_CREDENTIAL,
	CONFIG_CREDENTIAL,
};

static const char *err_strings[] = {
	"Operation failed",       // FAILURE
	"Operation succeeded",    // SUCCESS
	"A credential is stored, but it is invalid", // FAILURE_BAD_PASSWORD
	"Operation failed because the target daemon is not running as SYSTEM", // FAILURE_NO_IMPERSONATE
	"Operation aborted because communiction was not secure", // FAILURE_NOT_SECURE
	"No credential is stored", // FAILURE_NOT_FOUND
	"A credential is stored, but it has not yet been processed", // SUCCESS_PENDING
	"Operation failed because it is not allowed", // FAILURE_NOT_ALLOWED
	"Operation aborted due to missing or bad arguments", // FAILURE_BAD_ARGS
	"Arguments are missing, client and server my be mismatched", // FAILURE_PROTOCOL_MISMATCH
	"The credmon did not process credentials within the timeout period", // FAILURE_CREDMON_TIMEOUT
	"Operation failed because of a configuration error", // FAILURE_CONFIG_ERROR
};

// check to see if store_cred return value is a failure, and return a string for the failure code if so
bool store_cred_failed(long long ret, int mode, const char ** errstring /*=NULL*/)
{
	if ((mode & MODE_MASK) != GENERIC_DELETE) {
		if (ret > 100) { // return value may be a time_t for query
			return false;
		}
	}
	if (ret == SUCCESS || ret == SUCCESS_PENDING) {
		return false;
	}
	if (errstring && ret >= 0 && ret < (int)COUNTOF(err_strings)) {
		*errstring = err_strings[ret];
	}
	return true;
}


// check username, which may or may not end in @domain to see if the user name
// is the magic username that indicates pool password
static bool username_is_pool_password(const char *user, int * domain_pos = NULL)
{
	const int pool_name_len = sizeof(POOL_PASSWORD_USERNAME) - 1;
	const char *at = strchr(user, '@');
	int len;
	if (at) {
		len = (int)(at - user);
		if (domain_pos) { *domain_pos = len; }
	} else {
		len = (int)strlen(user);
		if (domain_pos) { *domain_pos = -1; }
	}
	return (len == pool_name_len) && (MATCH == memcmp(user, POOL_PASSWORD_USERNAME, len));
}


// returns true if there is a credmon and we managed to kick it
static bool wake_the_credmon(int mode)
{
	int cred_type = mode & CRED_TYPE_MASK;
	if (cred_type == STORE_CRED_USER_KRB) {
		return credmon_kick(credmon_type_KRB);
	} else if (cred_type == STORE_CRED_USER_OAUTH) {
		return credmon_kick(credmon_type_OAUTH);
	}
	return false;
}


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
		simple_scramble(pw, buffer, (int)len);
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

#endif

long long
PWD_STORE_CRED(const char *username, const unsigned char * rawbuf, const int rawlen, int mode, MyString & ccfile)
{
	dprintf(D_ALWAYS, "PWD store cred user %s len %i mode %i\n", username, rawlen, mode);

	ccfile.clear();

	int rc;
	MyString pw;
	if ((mode & MODE_MASK) == GENERIC_ADD) {
		pw.set((const char *)rawbuf, rawlen);
		// check for null characters in password, we don't support those
		if (pw.length() != (int)strlen(pw.c_str())) {
			dprintf(D_ALWAYS, "Failed to add password for user %s, password contained NULL characters\n", username);
			return FAILURE_BAD_PASSWORD;
		}
		rc = store_cred_password(username, pw.c_str(), mode);
		if (rc == SUCCESS) {
			// on success we return the current time
			rc = time(NULL);
		}
	} else {
		rc = store_cred_password(username, NULL, mode);
		if (rc == SUCCESS && (mode & MODE_MASK) == GENERIC_QUERY) {
			// on success we return the current time
			rc = time(NULL);
		}
	}

	return rc;
}

long long
OAUTH_STORE_CRED(const char *username, const unsigned char *cred, const int credlen, int mode, const ClassAd * ad, ClassAd & return_ad, MyString & ccfile)
{
	// store an OAuth token, this is presumed to be a refresh token (*.top file) unless the classad argument
	// indicates that it is not a refresh token, in which case it is stored as a *.use file
	//

/*
	ad arg schema is
		"service" : <SERVICE> name
		"handle"  : optional <handle> name appended to service name to create filenames
		any of the .meta JSON keywords below

	<service>_<handle>.meta JSON file is
		{
		"use_refresh_token": true,  # if false, other fields are not needed
		"client_secret": user provided or credd generated from <SERVICE>_CLIENT_SECRET_FILE,
		"client_id": user provided or credd generated from <SERVICE>_CLIENT_ID,
		"token_url": user provided or credd generated from <SERVICE>_TOKEN_URL,
		}
*/


	dprintf(D_ALWAYS, "OAUTH store cred user %s len %i mode %i\n", username, credlen, mode);
	if (strchr(username, '@')) {
		dprintf(D_ALWAYS | D_BACKTRACE, "OAUTH store cred ERROR - username has a @, it should be bare\n");
		return FAILURE_BAD_ARGS;
	}

	// set to full pathname of a file to watch for if the caller needs to wait for a credmon
	ccfile.clear();

	auto_free_ptr cred_dir( param("SEC_CREDENTIAL_DIRECTORY_OAUTH") );
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got STORE_CRED_USER_OAUTH but SEC_CREDENTIAL_DIRECTORY_OAUTH not defined!\n");
		return FAILURE_CONFIG_ERROR;
	}

	// remove mark on update or query for "mark and sweep"
	// if mode is QUERY or DELETE we do it because of interest
	// if mode is DELETE we do it because, well... delete.
	credmon_clear_mark(cred_dir, username);

	// the user's creds go into a directory
	MyString user_cred_path;
	dircat(cred_dir, username, user_cred_path);

	std::string service;
	if (ad && ad->LookupString("Service", service)) {
		// TODO: PRAGMA_REMIND("convert service name to filename")
	}

	// if this is a query, just return the timestamp on the .use file
	if ((mode & MODE_MASK) == GENERIC_QUERY) {
		if (service.empty()) {
			Directory creddir(cred_dir, PRIV_ROOT);
			if (creddir.Find_Named_Entry(username)) {
				Directory dir(user_cred_path.c_str(), PRIV_ROOT);
				const char * fn;
				int num_top_files = 0;
				int num_use_files = 0;
				while ((fn = dir.Next())) {
					if (ends_with(fn, ".top")) {
						++num_top_files;
					} else if (ends_with(fn, ".use")) {
						++num_use_files;
					} else {
						continue;
					}
					return_ad.Assign(fn, dir.GetModifyTime());
				}
				// TODO: add code to wait for all pending creds?
				if (num_top_files > 0) {
					ccfile.clear();
					return (num_top_files > num_use_files) ? SUCCESS_PENDING : SUCCESS;
				}
			}
			ccfile.clear();
			return FAILURE_NOT_FOUND;
		} else {
			// does the .use file exist already?
			dircat(user_cred_path.c_str(), service.c_str(), ".use", ccfile);
			struct stat cred_stat_buf;
			int rc = stat(ccfile.c_str(), &cred_stat_buf);
			bool got_ccfile = rc == 0;

			if (got_ccfile) { // if it exists, return it's timestamp
				ccfile.clear();
				return_ad.Assign(service, cred_stat_buf.st_mtime);
				return cred_stat_buf.st_mtime;
			} else {
				// if no use file, check for a .top file.  if that exists return its timestamp
				dircat(user_cred_path.c_str(), service.c_str(), ".top", ccfile);
				if (stat(ccfile.c_str(), &cred_stat_buf) >= 0) {
					std::string attr("Top"); attr += service; attr += "Time";
					return_ad.Assign(attr, cred_stat_buf.st_mtime);
					return SUCCESS_PENDING;
				} else {
					ccfile.clear();
					return FAILURE_NOT_FOUND;
				}
			}
		}
	}

	if ((mode & MODE_MASK) == GENERIC_DELETE) {
		if (service.empty()) {
			Directory creddir(cred_dir, PRIV_ROOT);
			if (creddir.Find_Named_Entry(username)) {
				dprintf(D_ALWAYS, "Deleting OAuth dir for user %s\n", username);
				if (!creddir.Remove_Current_File()) {
					dprintf(D_ALWAYS, "Could not remove %s\n", user_cred_path.c_str());
					return FAILURE_NOT_ALLOWED;
				}
			}
		} else {
			dprintf(D_ALWAYS, "Deleting OAuth files for service %s for user %s\n", service.c_str(), username);
			dircat(user_cred_path.c_str(), service.c_str(), ".top", ccfile);

			priv_state priv = set_root_priv();
			unlink(ccfile.c_str());
			dircat(user_cred_path.c_str(), service.c_str(), ".use", ccfile);
			unlink(ccfile.c_str());
			set_priv(priv);

			ccfile.clear();
		}
		return SUCCESS;
	}

	if (service.empty()) {
		service = "scitokens";
	}

	// create dir for user's creds, note that for OAUTH we *don't* create this as ROOT
	// oauth cred dir should be chmod 2770, chown root:condor (drwxrws--- root condor)
	if (mkdir(user_cred_path.c_str(), 0700) < 0) {
		int err = errno;
		if (err != EEXIST) {
			dprintf(D_ALWAYS, "Error %d, attempting to create OAuth cred subdir %s", err, user_cred_path.c_str());
			if (err == EACCES || err == EPERM || err == ENOENT || err == ENOTDIR) {
				// for one of several reasons, the parent directory does not allow the creation of subdirs.
				return FAILURE_CONFIG_ERROR;
			}
		}
	}

	// append filename for tokens file
	dircat(user_cred_path.c_str(), service.c_str(), ".top", ccfile);

	// create/overwrite the credential file
	dprintf(D_ALWAYS, "Writing OAuth user cred data to %s\n", ccfile.c_str());
	if ( ! replace_secure_file(ccfile.c_str(), ".tmp", cred, credlen, true)) {
		// replace_secure_file already logged the failure if it failed.
		ccfile.clear();
		return FAILURE;
	}

	// return the name of the file to wait for
	dircat(user_cred_path.c_str(), service.c_str(), ".use", ccfile);

	// credential succesfully stored
	return SUCCESS;
}

// handle ADD, DELETE, & QUERY for kerberos credential.
// if command is ADD, and a credential is stored
//   but the caller should wait for a .cc file before proceeding,
//   the ccfile argument will be returned
long long
KRB_STORE_CRED(const char *username, const unsigned char *cred, const int credlen, int mode, ClassAd & return_ad, MyString & ccfile)
{
	dprintf(D_ALWAYS, "Krb store cred user %s len %i mode %i\n", username, credlen, mode);

	// make sure that the cc filename is cleared
	ccfile.clear();

	auto_free_ptr cred_dir( param("SEC_CREDENTIAL_DIRECTORY_KRB") );
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got STORE_CRED but SEC_CREDENTIAL_DIRECTORY_KRB not defined!\n");
		return FAILURE_CONFIG_ERROR;
	}

	// remove mark on update for "mark and sweep"
	// if mode is QUERY or DELETE we do it because of interest
	// if mode is DELETE we do it because, well... delete.
	credmon_clear_mark(cred_dir, username);

	// check to see if .cc already exists
	dircat(cred_dir, username, ".cc", ccfile);
	struct stat cred_stat_buf;
	int rc = stat(ccfile.c_str(), &cred_stat_buf);
	bool got_ccfile = rc == 0;

	// if the credential already exists, we should update it if
	// it's more than X seconds old.  if X is zero, we always
	// update it.  if X is negative, we never update it.
	int fresh_time = param_integer("SEC_CREDENTIAL_REFRESH_INTERVAL", -1);

	// if it already exists and we don't update, call it a "success".
	if (got_ccfile && fresh_time < 0) {
		dprintf(D_FULLDEBUG, "CREDMON: credentials for user %s already exist in %s, and interval is %i\n",
			username, ccfile.c_str(), fresh_time );

		if ((mode & MODE_MASK) == GENERIC_ADD) {
			ccfile.clear(); // clear this so that the caller knows not to wait for it.
			return cred_stat_buf.st_mtime;
		}
	}

	// return success if the credential exists and has been recently
	// updated.  note that if fresh_time is zero, we'll never return
	// success here, meaning we will always update the credential.
	time_t now = time(NULL);
	if (got_ccfile && (now - cred_stat_buf.st_mtime < fresh_time)) {
		// was updated in the last X seconds, so call it a "success".
		dprintf(D_FULLDEBUG, "CREDMON: credentials for user %s already exist in %s, and interval is %i\n",
			username, ccfile.c_str(), fresh_time );

		if ((mode & MODE_MASK) == GENERIC_ADD) {
			ccfile.clear(); // clear this so that the caller knows not to wait for it.
			return cred_stat_buf.st_mtime;
		}
	}

	// if this is a query, just return the timestamp on the .cc file
	if (((mode & MODE_MASK) == GENERIC_QUERY) && got_ccfile) {
		ccfile.clear(); // clear this so that the caller knows not to wait for it.
		return cred_stat_buf.st_mtime;
	}

	MyString credfile;
	dircat(cred_dir, username, ".cred", credfile);
	const char *filename = credfile.c_str();

	if ((mode & MODE_MASK) == GENERIC_QUERY) {
		if (stat(credfile.c_str(), &cred_stat_buf) >= 0) {
			return_ad.Assign("CredTime", cred_stat_buf.st_mtime);
			return SUCCESS_PENDING;
		} else {
			ccfile.clear(); // clear this so that the caller knows not to wait for it.
			return FAILURE_NOT_FOUND;
		}
	}


	// if this is a delete operation, delete .cred and .cc files
	if ((mode & MODE_MASK) == GENERIC_DELETE) {
		priv_state priv = set_root_priv();
		if (got_ccfile) {
			rc = unlink(ccfile.c_str());
		}
		rc = unlink(filename);
		set_priv(priv);
		ccfile.clear();
		return SUCCESS;
	}

	// mode is GENERIC_ADD
	// we write to a temp file, then rename it over the original

	dprintf(D_ALWAYS, "Writing credential data to %s\n", filename);

	if ( ! replace_secure_file(filename, "tmp", cred, credlen, true)) {
		// replace_secure_file already logged the failure if it failed.
		return FAILURE;
	}

	// credential succesfully stored
	return SUCCESS;
}


unsigned char*
UNIX_GET_CRED(const char *user, const char *domain, size_t & len)
{
	dprintf(D_ALWAYS, "Unix get cred user %s domain %s\n", user, domain);
	len = 0;

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
	if (read_secure_file(filename.c_str(), (void**)(&buf), &len, true)) {
		return buf;
	}

	return NULL;
}


long long store_cred_blob(const char *user, int mode, const unsigned char *blob, int bloblen, const ClassAd * ad, MyString &ccfile)
{
	int domain_pos = -1;
	if (username_is_pool_password(user, &domain_pos)) {
		return FAILURE_BAD_ARGS;
	}
	if (domain_pos < 1) { // no @, or no username before the @
		dprintf(D_ALWAYS, "store_cred: malformed user name\n");
		return FAILURE_BAD_ARGS;
	}

	// are we operating in backward compatibility, where the mode arg is one of the ones for storing passwords?
	// in that case, we need to look at a knob to know what kind of blob we are working with
	if (mode >= STORE_CRED_LEGACY_PWD && mode <= STORE_CRED_LAST_MODE) {
		return FAILURE;
	} else {
		int cred_type = mode & CRED_TYPE_MASK;
		std::string username(user, domain_pos);
		if (cred_type == STORE_CRED_USER_PWD) {
			dprintf(D_ALWAYS, "GOT PWD STORE CRED mode=%d\n", mode);
			int pass_mode = (mode & MODE_MASK) | STORE_CRED_USER_PWD;
			return PWD_STORE_CRED(username.c_str(), blob, bloblen, pass_mode, ccfile);
		} else if (cred_type == STORE_CRED_USER_OAUTH) {
			dprintf(D_ALWAYS, "GOT OAUTH STORE CRED mode=%d\n", mode);
			int oauth_mode = (mode & MODE_MASK) | STORE_CRED_USER_OAUTH;
			ClassAd return_ad;
			return OAUTH_STORE_CRED(username.c_str(), blob, bloblen, oauth_mode, ad, return_ad, ccfile);
		} else if (cred_type == STORE_CRED_USER_KRB) {
			dprintf(D_ALWAYS, "GOT KRB STORE CRED mode=%d\n", mode);
			int krb_mode = (mode & MODE_MASK) | STORE_CRED_USER_KRB;
			ClassAd return_ad;
			return KRB_STORE_CRED(username.c_str(), blob, bloblen, krb_mode, return_ad, ccfile);
		} else {
			return FAILURE;
		}
	}

	return FAILURE_BAD_ARGS;
}

unsigned char* getStoredCredential(int mode, const char *username, const char *domain, int & credlen)
{
	// TODO: add support for multiple domains
	credlen = 0;
	if ( !username || !domain ) {
		return NULL;
	}

	// Support only kerberos creds for now
	if ((mode & CRED_TYPE_MASK) != STORE_CRED_USER_KRB)
		return NULL;

	if (MATCH == strcmp(username, POOL_PASSWORD_USERNAME)) {
		return NULL;
	}

	auto_free_ptr cred_dir( param("SEC_CREDENTIAL_DIRECTORY_KRB") );
	if(!cred_dir) {
		dprintf(D_ALWAYS, "ERROR: got GET_CRED but SEC_CREDENTIAL_DIRECTORY_KRB is not defined!\n");
		return NULL;
	}

	// create filenames
	MyString credfile;
	const char * filename = dircat(cred_dir, username, ".cred", credfile);

	dprintf(D_ALWAYS, "CREDS: reading data from %s\n", filename);

	// read the file (fourth argument "true" means as_root)
	unsigned char *buf = 0;
	size_t len = 0;
	if (read_secure_file(filename, (void**)(&buf), &len, true)) {
		credlen = (int)len;
		return buf;
	}

	dprintf(D_ALWAYS, "CREDS: failed to read securely from %s\n", filename);
	return NULL;
}


#ifndef WIN32

char* getStoredPassword(const char *username, const char *domain)
{
	// TODO: add support for multiple domains

	if ( !username || !domain ) {
		return NULL;
	}

	if (MATCH != strcmp(username, POOL_PASSWORD_USERNAME)) {
		dprintf(D_ALWAYS, "GOT UNIX GET CRED\n");
		size_t len = 0;
#if 1
		return (char*)UNIX_GET_CRED(username, domain, len);
#else
		unsigned char * buf = UNIX_GET_CRED(username, domain, len);
		if (buf) {
			// immediately convert to base64.  TJ: this seems wrong to me??
			char* textpw = condor_base64_encode(buf, len);
			free(buf);
			return textpw;
		}
		return NULL;
#endif
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

int store_cred_password(const char *user, const char *cred, int mode)
{
	int domain_pos = -1;
	if ( ! username_is_pool_password(user, &domain_pos)) {
		dprintf(D_ALWAYS, "store_cred: store_cred_password used with non-pool username. this is only valid on Windows\n");
		return FAILURE;
	}
	if (domain_pos < 1) { // no @, or no username before the @
		dprintf(D_ALWAYS, "store_cred: malformed user name\n");
		return FAILURE;
	}

	//
	// THIS CODE BELOW ALL DEALS EXCLUSIVELY WITH POOL PASSWORD
	//

	auto_free_ptr filename;
	if ((mode&MODE_MASK) != GENERIC_QUERY) {
		filename.set(param("SEC_PASSWORD_FILE"));
		if ( ! filename) {
			dprintf(D_ALWAYS, "store_cred: SEC_PASSWORD_FILE not defined\n");
			return FAILURE;
		}
	}

	int answer;
	switch (mode & MODE_MASK) {
	case GENERIC_ADD: {
		answer = FAILURE;
		size_t cred_sz = strlen(cred);
		if (!cred_sz) {
			dprintf(D_ALWAYS, "store_cred_password: empty password not allowed\n");
			break;
		}
		if (cred_sz > MAX_PASSWORD_LENGTH) {
			dprintf(D_ALWAYS, "store_cred_password: password too large\n");
			break;
		}
		priv_state priv = set_root_priv();
		answer = write_password_file(filename, cred);
		set_priv(priv);
		break;
	}
	case GENERIC_DELETE: {
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
	case GENERIC_QUERY: {
		char *password = getStoredPassword(POOL_PASSWORD_USERNAME, NULL);
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
		dprintf(D_ALWAYS, "store_cred_password: unknown mode: %d\n", mode);
		answer = FAILURE;
	}

	return answer;
}


#else
	// **** WIN32 CODE ****

#include <conio.h>

//extern "C" FILE *DebugFP;
//extern "C" int DebugFlags;

char* getStoredPassword(const char *username, const char *domain)
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
			"getStoredPassword(): Could not locate credential for user "
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

int store_cred_password(const char *user, const char *pw, int mode)
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
		answer = FAILURE_NO_IMPERSONATE;
	} else {
		priv = set_root_priv();
		
		switch(mode & MODE_MASK) {
		case GENERIC_ADD:
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

		case GENERIC_DELETE:
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

		case GENERIC_QUERY:
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
cred_get_password_handler(int /*i*/, Stream *s)
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

	//dprintf (D_ALWAYS, "First potential block in get_password_handler, DC==%i\n", daemonCore != NULL);

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
	password = getStoredPassword(user,domain);
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

int
cred_get_cred_handler(int /*i*/, Stream *s)
{
	char *client_user = NULL;
	char *client_domain = NULL;
	char *client_ipaddr = NULL;
	int result;
	int mode = 0;
	char * user = NULL;
	char * domain = NULL;
	unsigned char * cred = NULL;
	int credlen = 0;

	/* Check our connection.  We must be very picky since we are talking
	about sending out passwords.  We want to make certain
	a) the Stream is a ReliSock (tcp)
	b) it is authenticated (and thus authorized by daemoncore)
	c) it is encrypted
	*/

	if ( s->type() != Stream::reli_sock ) {
		dprintf(D_ALWAYS,
			"WARNING - credential fetch attempt via UDP from %s\n",
			((Sock*)s)->peer_addr().to_sinful().Value());
		return TRUE;
	}

	ReliSock* sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
			"WARNING - authentication failed for credential fetch attempt from %s\n",
			sock->peer_addr().to_sinful().Value());
		goto bail_out;
	}

	// Enable encryption if available. If it's not available, the next
	// call will fail and we'll abort the connection.
	sock->set_crypto_mode(true);

	if ( !sock->get_encryption() ) {
		dprintf(D_ALWAYS,
			"WARNING - credential fetch attempt without encryption from %s\n",
			sock->peer_addr().to_sinful().Value());
		goto bail_out;
	}

	// Get the username and domain from the wire

	//dprintf (D_ALWAYS, "First potential block in get_password_handler, DC==%i\n", daemonCore != NULL);

	sock->decode();

	result = sock->code(user);
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to recv user.\n");
		goto bail_out;
	}

	result = sock->code(domain);
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to recv domain.\n");
		goto bail_out;
	}

	result = sock->code(mode);
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to recv mode.\n");
		goto bail_out;
	}

	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to recv eom.\n");
		goto bail_out;
	}

	client_user = strdup(sock->getOwner());
	client_domain = strdup(sock->getDomain());
	client_ipaddr = strdup(sock->peer_addr().to_sinful().Value());

	// Now fetch the password from the secure store --
	// If not LocalSystem, this step will fail.
	cred = getStoredCredential(mode, user, domain, credlen);
	if (!cred) {
		dprintf(D_ALWAYS,
			"Failed to fetch cred mode %d for %s@%s requested by %s@%s at %s\n",
			mode,user,domain,
			client_user,client_domain,client_ipaddr);
		goto bail_out;
	}

	// Got the credential, send it
	sock->encode();
	result = sock->code(credlen);
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to send credential size.\n");
		goto bail_out;
	}

	result = sock->code_bytes(cred, credlen);
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to send credential size.\n");
		goto bail_out;
	}

	result = sock->end_of_message();
	if( !result ) {
		dprintf(D_ALWAYS, "get_cred_handler: Failed to send eom.\n");
		goto bail_out;
	}

	// Now that we sent the password, immediately zero it out from ram
	SecureZeroMemory(cred,credlen);

	dprintf(D_ALWAYS,
		"Fetched user %s@%s credential requested by %s@%s at %s\n",
		user,domain,client_user,client_domain,client_ipaddr);

bail_out:
	if (client_user) free(client_user);
	if (client_domain) free(client_domain);
	if (client_ipaddr) free(client_ipaddr);
	if (user) free(user);
	if (domain) free(domain);
	if (cred) free(cred);
	return TRUE;
}


// forward declare the non-blocking continuation function.
void store_cred_handler_continue();

// declare a simple data structure for holding the info needed
// across non-blocking retries
class StoreCredState {
public:
	StoreCredState() :ccfile(NULL), retries(0), s(NULL) {};
	~StoreCredState() {
		delete s;
		s = NULL;
		if (ccfile) free(ccfile);
		ccfile = NULL;
	};

	ClassAd return_ad;
	char *ccfile;
	int  retries;
	Stream *s;
};


/* WORKS ON BOTH WINDOWS AND UNIX */
int store_cred_handler(int /*i*/, Stream *s)
{
	std::string fulluser; // full username including domain
	std::string username; // just the username part before the @ of the above
	std::string pass;	  // password, if the password is a string and mode is LEGACY
	MyString ccfile;   // credmon completion file to watch for before returning
	ClassAd     ad;
	ClassAd     return_ad;
	auto_free_ptr cred;
	int mode = 0;
	int credlen = 0;
#ifdef WIN32
	int64_t answer = FAILURE;
#else
	long answer = FAILURE;
#endif
	int cred_type = 0;
	bool deferred_reply = false; // set to true when we want to defer replying to this command
	bool request_credmon_wait = false; // mode on the wire requested to wait for the credmon
	const char * errinfo = NULL;

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

	bool got_message = true;
	if (! s->get(fulluser) ||
		! s->get(pass) ||
		! s->get(mode)) {
		dprintf(D_ALWAYS, "store_cred: did not receive user,pw,mode.\n");
		got_message = false;
	} else if (!(mode & STORE_CRED_LEGACY)) {
		if ( ! sock->get(credlen)) {
			got_message = false;
		} else if (credlen) {
			if (credlen > 0x1000000 * 100) {
				dprintf(D_ALWAYS, "store_cred: ERROR cred too large (%d). possible protocol mismatch\n", credlen);
				got_message = false;
			} else {
				cred.set((char*)malloc(credlen));
				if (! sock->get_bytes(cred.ptr(), credlen)) {
					got_message = false;
				}
			}
		}
		if (got_message && ! getClassAd(sock, ad)) {
			got_message = false;
		}
	}

	if ( ! got_message || !sock->end_of_message()) {
		dprintf(D_ALWAYS, "store_cred: did not recieve a valid command\n");
		answer = FAILURE_PROTOCOL_MISMATCH;
		goto cleanup_and_exit;
	}

	// strip the CREDMON_WAIT flag out of the mode
	if (mode & STORE_CRED_WAIT_FOR_CREDMON) {
		request_credmon_wait = true;
		mode &= ~STORE_CRED_WAIT_FOR_CREDMON;
	}

	if (mode < STORE_CRED_FIRST_MODE || mode > STORE_CRED_LAST_MODE) {
		dprintf(D_ALWAYS, "store_cred: %d is not a valid mode\n", mode);
		answer = FAILURE_BAD_ARGS;
		goto cleanup_and_exit;
	}

	if ( ! fulluser.empty()) {
			// ensure that the username has an '@' delimteter
		size_t ix_at = fulluser.find('@');
		if (ix_at == std::string::npos || ix_at == 0) {
			dprintf(D_ALWAYS, "store_cred_handler: user not in user@domain format\n");
			answer = FAILURE_BAD_ARGS;
		}
		else {
			username = fulluser.substr(0, ix_at);
				// We don't allow one user to set another user's credential
				//   (except for users explicitly allowed to)
				// TODO: We deliberately ignore the user domains. Isn't
				//   that a security issue?
				// we don't allow updates to the pool password through this interface
			StringList auth_users;
			param_and_insert_unique_items("CRED_SUPER_USERS", auth_users);
			auth_users.insert(username.c_str());
			const char *sock_owner = sock->getOwner();
			if ( sock_owner == NULL ||
#if defined(WIN32)
			     !auth_users.contains_anycase_withwildcard( sock_owner )
#else
			     !auth_users.contains_withwildcard( sock_owner )
#endif
			   )
			{
				dprintf( D_ALWAYS, "WARNING: store_cred() for user %s attempted by user %s, rejecting\n", fulluser.c_str(), sock_owner ? sock_owner : "<unknown>" );
				answer = FAILURE_NOT_ALLOWED;

			} else if (((mode&MODE_MASK) != GENERIC_QUERY) && username_is_pool_password(fulluser.c_str())) {
				dprintf(D_ALWAYS, "ERROR: attempt to set pool password via STORE_CRED! (must use STORE_POOL_CRED)\n");
				answer = FAILURE_NOT_ALLOWED;
			} else if ((mode & ~(MODE_MASK | STORE_CRED_LEGACY)) == STORE_CRED_USER_PWD) {
				answer = store_cred_password(fulluser.c_str(), pass.c_str(), mode);
			} else {
				// mode is either Krb or OAuth
				cred_type = mode & CRED_TYPE_MASK;

				if ((mode & STORE_CRED_LEGACY) && ! pass.empty()) {
					// the credential was passed as a base64 encoded string in the pass field.
					int rawlen = -1;
					unsigned char* rawbuf = NULL;
					zkm_base64_decode(pass.c_str(), &rawbuf, &rawlen);
					cred.set((char*)rawbuf);  // make cred responsible for freeing this
					if (rawlen <= 0) {
						dprintf(D_ALWAYS, "Failed to decode credential!\n");
						answer = FAILURE;
						goto cleanup_and_exit;
					}
					credlen = rawlen;

					// fixup the cred type when there is a legacy mode credential by looking that the
					// deprecated CREDD_OAUTH_MODE knob.
					cred_type = STORE_CRED_USER_KRB;
					if (param_boolean("CREDD_OAUTH_MODE", false)) {
						cred_type = STORE_CRED_USER_OAUTH;
					}
				}

				// dispatch to the correct handler
				if (cred_type == STORE_CRED_USER_KRB) {
					dprintf(D_ALWAYS, "GOT KRB STORE CRED mode=%d\n", mode);
					int krb_mode = (mode & MODE_MASK) | STORE_CRED_USER_KRB;
					answer = KRB_STORE_CRED(username.c_str(), (unsigned char*)cred.ptr(), credlen, krb_mode, return_ad, ccfile);
				} else if (cred_type == STORE_CRED_USER_OAUTH) {
					dprintf(D_ALWAYS, "GOT OAUTH STORE CRED mode=%d\n", mode);
					int oauth_mode = (mode & MODE_MASK) | STORE_CRED_USER_OAUTH;
					answer = OAUTH_STORE_CRED(username.c_str(), (unsigned char*)cred.ptr(), credlen, oauth_mode, &ad, return_ad, ccfile);
				} else {
					dprintf(D_ALWAYS, "unknown credential type %d\n", cred_type);
					answer = FAILURE_BAD_ARGS;
				}
			}
		}
	}


	// we only need to signal CREDMON if the file was just written.  if it
	// already existed, just leave it be, and don't signal the CREDMON.
	// if we have the name of a file to watch for, then we deferr the reply
	// we setup a timer to handle the reply for us once the file exists.
	if (store_cred_failed(answer, mode, &errinfo)) {
		dprintf(D_SECURITY | D_FULLDEBUG, "NBSTORECRED: not signaling credmon. result=%lld, ccfile=%s\n",
			(long long)answer, ccfile.empty() ? "<null>" : ccfile.c_str());
	} else if ( ! ccfile.empty()) {
		// good so far, but the real answer is determined by our ability
		// to signal the credmon and have the completion file appear.  we don't
		// want this to block so we go back to daemoncore and set a timer
		// for 0 seconds.  the timer will reset itself as needed.
		if (wake_the_credmon(mode) && request_credmon_wait) {
			StoreCredState* retry_state = new StoreCredState();
			retry_state->ccfile = strdup(ccfile.c_str());
			retry_state->retries = param_integer("CREDD_POLLING_TIMEOUT", 20);
			retry_state->s = new ReliSock(*((ReliSock*)s));

			dprintf( D_FULLDEBUG, "store_cred: setting timer to poll for completion file: %s, retries : %i, sock: %p\n",
				retry_state->ccfile, retry_state->retries, retry_state->s);
			daemonCore->Register_Timer(0, store_cred_handler_continue, "Poll for existence of .cc file");
			daemonCore->Register_DataPtr(retry_state);
			deferred_reply = true;
		} else {
			// If we got back a success, but didn't wake the credmon, or were told not to wait
			// change the answer to 'pending'
			if (answer == SUCCESS) {
				answer = SUCCESS_PENDING;
			}
		}
	}

cleanup_and_exit:
	if (cred) {
		SecureZeroMemory(cred.ptr(), credlen);
	}

	// if we modified the credential, then we also attempted to register a
	// timer to poll for the cred file.  if that happened succesfully, return
	// now so we can poll in a non-blocking way.
	//
	// if we either had a failure, or had success but didn't modify the file,
	// then there's nothing to poll for and we should not return and should
	// finish the wire protocol below.
	//
	if ( ! deferred_reply) {
		s->encode();
		if (! s->put(answer)) {
			dprintf(D_ALWAYS, "store_cred: Failed to send result.\n");
			return FAILURE;
		}
		if (!(mode & STORE_CRED_LEGACY)) {
			putClassAd(s, return_ad);
		}
		if (! s->end_of_message()) {
			dprintf(D_ALWAYS, "store_cred: Failed to send end of message.\n");
		}
	}

	return ! store_cred_failed(answer, mode);
}


void store_cred_handler_continue()
{
	// can only be called when daemonCore is non-null since otherwise
	// there's no data
	if(!daemonCore) return;

	StoreCredState *dptr = (StoreCredState*)daemonCore->GetDataPtr();

	dprintf( D_FULLDEBUG, "Checking for completion file: %s, retries: %i, sock: %p\n", dptr->ccfile, dptr->retries, dptr->s);

	// stat the file as root
	struct stat ccfile_stat;
	priv_state priv = set_root_priv();
	int rc = stat(dptr->ccfile, &ccfile_stat);
	set_priv(priv);
#ifdef WIN32  // CEDAR put on *nix thinks int64_t is ambiguous, so we use long instead (long on Win32 is not 64bits)
	int64_t answer;
#else
	long answer;
#endif
	if (rc < 0) {
		if (dptr->retries > 0) {
			// re-register timer with one less retry
			dprintf( D_FULLDEBUG, "Re-registering completion timer and dptr\n");
			dptr->retries--;
			daemonCore->Register_Timer(1, store_cred_handler_continue, "Poll for existence of .cc file");
			daemonCore->Register_DataPtr(dptr);
			return;
		}
		answer = FAILURE_CREDMON_TIMEOUT;
	} else {
		answer = ccfile_stat.st_mtime;
		dprintf(D_ALWAYS, "Completion file %s exists. mtime=%lld", dptr->ccfile, (long long)answer);
	}

	// regardless of SUCCESS or FAILURE, if we got here we need to finish
	// the wire protocol for STORE_CRED

	dptr->s->encode();
	if (! dptr->s->put(answer) || ! putClassAd(dptr->s, dptr->return_ad)) {
		dprintf(D_ALWAYS, "store_cred: Failed to send result.\n");
	} else if( ! dptr->s->end_of_message() ) {
		dprintf( D_ALWAYS, "store_cred: Failed to send end of message.\n");
	}

	// we copied the stream and strdup'ed the user, so do a deep free of dptr
	delete dptr;
	return;
}


/* obsolete wire form for STORE_CRED
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
*/


int store_pool_cred_handler(int, Stream *s)
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
		result = store_cred_password(username.Value(), pw, GENERIC_ADD);
		SecureZeroMemory(pw, strlen(pw));
	}
	else {
		result = store_cred_password(username.Value(), NULL, GENERIC_DELETE);
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


// OBSOLETE!!! do_store_cred for LEGACY password modes only!!. 
// use the other do_store_cred for working with Krb or OAuth creds
int 
do_store_cred(const char* user, const char* pw, int mode, Daemon* d, bool force) {
	
	int result;
	int return_val;
	Sock* sock = NULL;

	// this function only works with password and LEGACY password
	int cred_type = mode & CRED_TYPE_MASK;
	if (cred_type != STORE_CRED_USER_PWD && cred_type != STORE_CRED_LEGACY_PWD) {
		dprintf ( D_ALWAYS | D_BACKTRACE,  "STORE_CRED: Unsupported mode %d\n",  mode);
		return FAILURE_BAD_ARGS;
	}

	// to help future debugging, print out the mode we are in
	dprintf ( D_ALWAYS,  "STORE_CRED: In mode '%s'\n",  mode_name[mode & MODE_MASK]);
	
		// If we are root / SYSTEM and we want a local daemon, 
		// then do the work directly to the local registry.
		// If not, then send the request over the wire to a remote credd or schedd.

	if ( is_root() && d == NULL ) {
		return_val = store_cred_password(user,pw,mode);
	} else {
			// send out the request remotely.

			// first see if we're operating on the pool password
		int cmd = STORE_CRED;
		int domain_pos = -1;
		if (username_is_pool_password(user, &domain_pos)) {
			cmd = STORE_POOL_CRED;
			user += domain_pos + 1;	// we only need to send the domain name for STORE_POOL_CRED
		}
		if (domain_pos <= 0) {
			dprintf(D_ALWAYS, "store_cred: user not in user@domain format\n");
			return FAILURE_BAD_ARGS;
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
			dprintf(D_FULLDEBUG, "Starting a command on %s\n", d->idStr());
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
			int legacy_mode = STORE_CRED_LEGACY_PWD | (mode & MODE_MASK); // make sure legacy bit is set because we plan to use the legacy wire form.
			if (! sock->put(user) ||
				! sock->put(pw) ||
				! sock->put(legacy_mode) ||
				! sock->end_of_message()) {
				dprintf(D_ALWAYS, "store_cred: failed to send STORE_CRED (legacy) message\n");
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

		result = sock->get(return_val);
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
	
	
	switch(mode & MODE_MASK)
	{
	case GENERIC_ADD:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Addition succeeded!\n");
		} else {
			dprintf(D_FULLDEBUG, "Addition failed!\n");
		}
		break;
	case GENERIC_DELETE:
		if( return_val == SUCCESS ) {
			dprintf(D_FULLDEBUG, "Delete succeeded!\n");
		} else {
			dprintf(D_FULLDEBUG, "Delete failed!\n");
		}
		break;
	case GENERIC_QUERY:
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

// This implentation of do_store_cred handles password, krb, or oauth credential types
// as of 8.9.6, this is the preferred method
// 
long long
do_store_cred (
	const char *user,
	int mode,
	const unsigned char * cred, int credlen,
	ClassAd & return_ad, // extra return information for some command modes
	ClassAd* ad /*= NULL*/,
	Daemon *d /*= NULL*/)
{
	const char * err = NULL;
#ifdef WIN32 // long is not 64 bits on Windows, and inta64_t results in compile errors on some platforms.
	int64_t return_val;
#else
	long return_val;
#endif
	Sock* sock = NULL;
	MyString daemonid; // for error messages

	// to help future debugging, print out the mode we are in
	dprintf ( D_ALWAYS,  "STORE_CRED: In mode %d '%s'\n", mode, mode_name[mode & MODE_MASK]);

	// if a legacy mode is requested, no ClassAd argument can be sent
	if (mode & STORE_CRED_LEGACY) {
		if (ad && (ad->size() > 0)) {
			dprintf(D_ALWAYS, "STORE_CRED: ERROR ClassAd argument cannot be used with legacy mode %d\n", mode);
			return FAILURE_BAD_ARGS;
		}
	}

	if (credlen && ! cred) {
		return FAILURE;
	}
	if ((mode & MODE_MASK) == GENERIC_ADD) {
		if ( ! cred) {
			return FAILURE;
		}
	}

	// If we are root / SYSTEM and we want a local daemon, 
	// then do the work directly to the local registry.
	// If not, then send the request over the wire to a remote credd or schedd.

	if ( is_root() && d == NULL ) {
		MyString ccfile;	// we don't care about a completion file, but we have to pass this in anyway
		if (mode >= STORE_CRED_LEGACY_PWD && mode <= STORE_CRED_LAST_MODE) {
			return_val = store_cred_password(user, (const char *)cred, mode);
		} else {
			return_val = store_cred_blob(user, mode, cred, credlen, ad, ccfile);
		}
	} else {
		// send out the request remotely.

		// first see if we're operating on the pool password
		// if we are just use the version of do_store_cred that handles passwords
		int domain_pos = -1;
		if (username_is_pool_password(user, &domain_pos)) {
			int cred_type = (mode & ~MODE_MASK);
			if ((cred_type != STORE_CRED_LEGACY_PWD) && (cred_type != STORE_CRED_USER_PWD)) {
				return FAILURE_BAD_ARGS;
			}
			MyString pw;
			if (cred) pw.set((const char *)cred, credlen);
			return do_store_cred(user, pw.c_str(), mode, d);
		}
		if (domain_pos <= 0) {
			dprintf(D_ALWAYS, "store_cred: user not in user@domain format\n");
			return FAILURE;
		}

		if (d == NULL) {
			dprintf(D_FULLDEBUG, "Storing credential to local schedd\n");
			Daemon my_schedd(DT_SCHEDD);
			sock = my_schedd.startCommand(STORE_CRED, Stream::reli_sock, 0);
			if (! sock) { daemonid = my_schedd.idStr(); }
		} else {
			dprintf(D_FULLDEBUG, "Starting a command on a REMOTE schedd or credd\n");
			sock = d->startCommand(STORE_CRED, Stream::reli_sock, 0);
			if (! sock) { daemonid = d->idStr(); }
		}

		if( !sock ) {
			dprintf(D_ALWAYS, 
				"STORE_CRED: Failed to start STORE_CRED command. Unable to contact %s\n", daemonid.c_str());
			return FAILURE;
		}

		// for remote updates (which send the password), verify we have a secure channel,
		sock->set_crypto_mode( true );

		if ((d != NULL) &&
			((sock->type() != Stream::reli_sock) || !((ReliSock*)sock)->triedAuthentication() || !sock->get_encryption())) {
			dprintf(D_ALWAYS, "STORE_CRED: blocking attempt to update over insecure channel\n");
			delete sock;
			return FAILURE_NOT_SECURE;
		}

		MyString pw;
		if (mode & STORE_CRED_LEGACY) {
			if (cred) pw.set((const char *)cred, credlen);
		} else {
			// TODO: password field could be used for other data, perhaps cred subtype?
		}
		bool sent = true;
		// wire protocol is string, string, int.  then if the mode does NOT have the LEGACY bit set : credlen, credbytes, classad
		if ( ! sock->put(user) ||
			 ! sock->put(pw) || 
			 ! sock->put(mode)) {
			dprintf(D_ALWAYS, "store_cred: Failed to send command payload\n");
			sent = false;
		} else if (!(mode & STORE_CRED_LEGACY)) {
			if ( ! sock->put(credlen)) {
				sent = false;
			} else if (credlen && ! sock->put_bytes(cred, credlen)) {
				sent = false;
			} else if (ad && ! putClassAd(sock, *ad)) {
				sent = false;
			} else if (!ad) { // if no classad supplied, put a dummy ad to satisfy the wire protocol
				ClassAd tmp; tmp.Clear();
				if ( ! putClassAd(sock, tmp)) {
					sent = false;
				}
			}
		}
		if (sent) {
			if ( ! sock->end_of_message()) {
				dprintf(D_ALWAYS, "store_cred: Failed to send EOM.\n");
				sent = false;
			}
		}
		if ( ! sent) {
			dprintf(D_ALWAYS, "store_cred: sending of command mode=%d failed.\n", mode);
			delete sock;
			return FAILURE;
		}

		//dprintf (D_ALWAYS, "First potential block in store_cred, DC==%i\n", daemonCore != NULL);

		// now the reply. For LEGACY, the reply is an integer
		// for non LEGACY the reply is a long long and a ClassAd
		sock->decode();

		err = NULL;
		if ( !sock->get(return_val)) {
			err = "failed to recieve and answer";
			return_val = FAILURE;
		}
		else if (!(mode & STORE_CRED_LEGACY)) {
			if ( ! getClassAd(sock, return_ad)) {
				err = "possibly protocol mismatch - remote store_cred did not return a classad";
				return_val = FAILURE_PROTOCOL_MISMATCH;
			}
		}
		if (!err && !sock->end_of_message()) {
			err = "possibly protocol mismatch - end_of_message failed";
			return_val = FAILURE_PROTOCOL_MISMATCH;
		}

		if (err) {
			dprintf(D_ALWAYS, "store_cred: mode=%d %s\n", mode, err);
			delete sock;
			return return_val;
		}
	}	// end of case where we send out the request remotely

	switch(mode & MODE_MASK)
	{
	case GENERIC_ADD:
		if (store_cred_failed(return_val, mode, &err)) {
			dprintf(D_FULLDEBUG, "Addition failed! err=%d %s\n", (int)return_val, err?err:"");
		} else {
			dprintf(D_FULLDEBUG, "Addition succeeded!\n");
		}
		break;
	case GENERIC_DELETE:
		if (store_cred_failed(return_val, mode, &err)) {
			dprintf(D_FULLDEBUG, "Delete failed! err=%d %s\n", (int)return_val, err?err:"");
		} else {
			dprintf(D_FULLDEBUG, "Delete succeeded!\n");
		}
		break;
	case GENERIC_QUERY:
		if (!store_cred_failed(return_val, mode, &err)) {
			dprintf(D_FULLDEBUG, "We have a credential stored!\n");
		} else if (return_val == FAILURE_NO_IMPERSONATE) {
			dprintf(D_FULLDEBUG, "Running in single-user mode, credential not needed\n");
		} else {
			dprintf(D_FULLDEBUG, "Query failed! err=%d %s\n", (int)return_val, err?err:"");
		}
		break;
	}


	if ( sock ) delete sock;
	return return_val;
}


/*
int deleteCredential( const char* user, const char* pw, Daemon *d ) {
	return do_store_cred(user, pw, DELETE_MODE, d);	
}

int addCredential( const char* user, const char* pw, Daemon *d ) {
	return do_store_cred(user, pw, ADD_MODE, d);	
}

int queryCredential( const char* user, Daemon *d ) {
	return do_store_cred(user, NULL, QUERY_MODE, d);	
}
*/

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
