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
	"Failure parsing credential as JSON", // FAILURE_JSON_PARSE
	"Credential was found but it did not match requested scopes or audience", // FAILURE_CRED_MISMATCH
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

class IssuerKeyNameCache
{
private:
	std::string m_name_list;
	time_t m_last_refresh;

public:
	IssuerKeyNameCache() : m_last_refresh(0) {}

	void Clear() {
		m_name_list.clear();
		m_last_refresh = 0;
	}

	const std::string & Peek(time_t * last_ref=NULL) {
		if (last_ref) *last_ref = m_last_refresh;
		return m_name_list;
	}

	const std::string & NameList(CondorError *err);
};

IssuerKeyNameCache g_issuer_name_cache;

}

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



#ifndef WIN32
	// **** UNIX CODE *****

void SecureZeroMemory(void *p, size_t n)
{
	// TODO: make this Secure
	memset(p, 0, n);
}

#endif

int
cred_matches(const std::string & credfile, const ClassAd * requestAd)
{
	// read the file back to make sure the scopes & audience match
	// the request
	size_t clen = 0;
	void *credp = NULL;
	if (!read_secure_file(credfile.c_str(), &credp, &clen, true, SECURE_FILE_VERIFY_ACCESS)) {
		// read_secure_file already logged the failure if it failed
		return FAILURE_JSON_PARSE;
	}
	// unfortunately the buffer is not null terminated so need to make a copy
	std::string credbuf;
	credbuf.assign((char *) credp, clen);
	free(credp);
	classad::ClassAdJsonParser jsonp;
	ClassAd credad;
	if (!jsonp.ParseClassAd(credbuf.c_str(), credad)) {
		dprintf(D_ALWAYS, "Error, could not parse cred from %s as JSON\n", credfile.c_str());
		return FAILURE_JSON_PARSE;
	}

	std::string scopes;
	std::string audience;
	if (requestAd) {
		requestAd->LookupString("Scopes", scopes);
		requestAd->LookupString("Audience", audience);
	}

	std::string oldscopes;
	std::string oldaudience;
	credad.LookupString("scopes", oldscopes);
	credad.LookupString("audience", oldaudience);
	if ((scopes != oldscopes) || (audience != oldaudience)) {
		return FAILURE_CRED_MISMATCH;
	}

	return SUCCESS;
}

long long
PWD_STORE_CRED(const char *username, const unsigned char * rawbuf, const int rawlen, int mode, std::string & ccfile)
{
	dprintf(D_ALWAYS, "PWD store cred user %s len %i mode %i\n", username, rawlen, mode);

	ccfile.clear();

	long long rc;
	std::string pw;
	if ((mode & MODE_MASK) == GENERIC_ADD) {
		pw.assign((const char *)rawbuf, rawlen);
		// check for null characters in password, we don't support those
		if (pw.length() != strlen(pw.c_str())) {
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

// this checks for a specific set of valid characters - ones that could be used
// in both an OAuth service name and a valid file name.  perhaps this is too
// restrictive, but rather than enumerate all bad characters and potential miss
// some, let's just declare what we are okay with.
//
// we restrict service names to alphanumeric plus the following:
//     - + = _ .
//
// everything else is disallowed.

bool
okay_for_oauth_filename(const std::string & s) {
	for (char c: s) {
		if (
			!isalpha(c) &&
			!isdigit(c) &&
			!(c == '-') &&
			!(c == '+') &&
			!(c == '=') &&
			!(c == '_') &&
			!(c == '.') ) {

			// NOT any of the above... bad character.
			dprintf(D_SECURITY | D_VERBOSE, "ERROR: encountered bad char '%c' in string \"%s\"\n", c, s.c_str());
			return false;
		}
	}
	return true;
}

long long
OAUTH_STORE_CRED(const char *username, const unsigned char *cred, const int credlen, int mode, const ClassAd * ad, ClassAd & return_ad, std::string & ccfile)
{
	// store an OAuth token, this is presumed to be a refresh token (*.top file) unless the classad argument
	// indicates that it is not a refresh token, in which case it is stored as a *.use file
	//

/*
	ad arg schema is
		"service"  : <SERVICE> name
		"handle"   : optional <handle> name appended to service name to create filenames
		"scopes"   : optional comma-separated <scopes> list to include in JSON storage or query
		"audience" : optional <audience> to include in JSON storage or query
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
	if (!okay_for_oauth_filename(username)) {
		// include a backtrace here because the username is NOT user-provided, it comes
		// from the authenticted socket.  the caller of OAUTH_STORE_CRED should have
		// verified it is in the correct form, but out of caution we check it again.
		// this could arguably be an ASSERT() but we'd rather not crash the CredD.
		dprintf(D_ALWAYS | D_BACKTRACE, "OAUTH store cred ERROR - Illegal char in username\n");
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
	std::string user_cred_path;
	dircat(cred_dir, username, user_cred_path);

	// lookup and validate service name
	std::string service;
	if (ad && ad->LookupString("Service", service)) {
		// this is user input so validate
		if (!okay_for_oauth_filename(service)) {
			dprintf(D_ALWAYS, "OAUTH store cred ERROR - Illegal char in Service name.\n");
			return FAILURE_BAD_ARGS;
		}
	}

	// lookup and validate handle name
	std::string handle;
	if (ad && ad->LookupString("Handle", handle)) {
		// this is user input so validate
		if (!okay_for_oauth_filename(handle)) {
			dprintf(D_ALWAYS, "OAUTH store cred ERROR - Illegal char in Handle name.\n");
			return FAILURE_BAD_ARGS;
		}
	}

	CredSorter sorter;
	CredSorter::CredType cred_type = sorter.Sort(service);

	// if there is a service name and a handle name, concat the two together.
	// this does mean we don't actually support querying service and/or handle
	// separately.  until such time as these tokens live in some semi-structured
	// data store, perhaps with the scopes and audience, this will have to do.
	if (!service.empty() && !handle.empty()) {
		service += "_";
		service += handle;
	}

	// If this is a query and the service name is empty, report the
	//   timestamp on every last .top and .use file found, and return
	//   whether any .top file is missing its corresponding .use file.
	// If this is a query and the service name is given, verify that
	//   any scopes or audience given match and if so report the
	//   timestamp on the .top and .use files.
	if ((mode & MODE_MASK) == GENERIC_QUERY) {
		if (service.empty()) {
			Directory creddir(cred_dir, PRIV_ROOT);
			if (creddir.Find_Named_Entry(username)) {
				Directory dir(user_cred_path.c_str(), PRIV_ROOT);
				const char * fn;
				std::set<std::string> top_files;
				std::set<std::string> use_files;
				while ((fn = dir.Next())) {
					if (ends_with(fn, ".top")) {
						top_files.emplace(fn, strlen(fn)-4);
					} else if (ends_with(fn, ".use")) {
						use_files.emplace(fn, strlen(fn)-4);
					} else {
						continue;
					}
					return_ad.Assign(fn, dir.GetModifyTime());
				}
				// TODO: add code to wait for all pending creds?
				int rc = SUCCESS;
				for (const auto& top_name: top_files) {
					if (use_files.count(top_name) == 0) {
						rc = SUCCESS_PENDING;
						break;
					}
				}
				if (!top_files.empty() || !use_files.empty()) {
					ccfile.clear();
					return rc;
				}
			}
			ccfile.clear();
			return FAILURE_NOT_FOUND;
		} else {
			// does the .top file exist?
			dircat(user_cred_path.c_str(), service.c_str(), ".top", ccfile);
			struct stat cred_stat_buf;
			if (stat(ccfile.c_str(), &cred_stat_buf) == 0) {
				std::string attr("Top"); attr += service; attr += "Time";
				return_ad.Assign(attr, cred_stat_buf.st_mtime);

				// read the file back to make sure the scopes & audience match
				int rc = cred_matches(ccfile, ad);
				ccfile.clear();
				if (rc != SUCCESS) {
					return rc;
				}
			} else if (cred_type != CredSorter::UnknownType) {
				ccfile.clear();
				return FAILURE_NOT_FOUND;
			}

			// if there's a .use file, get its mod
			//  time as the service attribute
			// Service names without a known credmon type are assumed to be
			// stored by the user. They don't have a .top file and should
			// always have a .use file.
			dircat(user_cred_path.c_str(), service.c_str(), ".use", ccfile);
			if (stat(ccfile.c_str(), &cred_stat_buf) >= 0) {
				ccfile.clear();
				return_ad.Assign(service, cred_stat_buf.st_mtime);
				return SUCCESS;
			} else if (cred_type == CredSorter::UnknownType) {
				ccfile.clear();
				return FAILURE_NOT_FOUND;
			} else {
				return SUCCESS_PENDING;
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
		dprintf(D_ERROR, "Name of service credential to add not given\n");
		return FAILURE_BAD_ARGS;
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

	size_t clen = credlen;

	if (cred_type == CredSorter::UnknownType) {
		dircat(user_cred_path.c_str(), service.c_str(), ".use", ccfile);
	} else {

		// append filename for tokens file
		dircat(user_cred_path.c_str(), service.c_str(), ".top", ccfile);

		std::string scopes;
		std::string audience;
		if (ad) {
			ad->LookupString("Scopes", scopes);
			ad->LookupString("Audience", audience);
		}
		std::string jsoncred;
		if ((scopes != "") || (audience != "")) {
			// Add scopes and/or audience into the JSON-formatted credentials
			classad::ClassAdJsonParser jsonp;
			ClassAd credad;
			if (!jsonp.ParseClassAd((const char *) cred, credad)) {
				dprintf(D_ALWAYS, "Error, could not parse cred for %s as JSON\n", ccfile.c_str());
				return FAILURE_JSON_PARSE;
			}
			if (scopes != "") credad.Assign("scopes", scopes);
			if (audience != "") credad.Assign("audience", audience);
			sPrintAdAsJson(jsoncred, credad);
			jsoncred += "\n";
			cred = (const unsigned char *) jsoncred.c_str();
			clen = jsoncred.length();
		}
	}

	// create/overwrite the credential file
	dprintf(D_ALWAYS, "Writing OAuth user cred data to %s\n", ccfile.c_str());
	if ( ! replace_secure_file(ccfile.c_str(), ".tmp", cred, clen, true)) {
		// replace_secure_file already logged the failure if it failed.
		ccfile.clear();
		return FAILURE;
	}

	// return the name of the file to wait for
	dircat(user_cred_path.c_str(), service.c_str(), ".use", ccfile);

	// credential succesfully stored
	return SUCCESS;
}


// this is a helper function massages the info enough that we can call the oauth store cred
long long
LOCAL_STORE_CRED(const char *username, const char *servicename, std::string &ccfile) {

	// put the service name in an ad so the oauth code can extract it
	ClassAd ad, ret;
	ad.Assign("Service", servicename);

	// username is passed through
	//
	// doesn't matter what the contents of the cred are, just that it exists,
	// so we just store the username in there as the cred contents.
	//
	// mode is ADD (not a QUERY or DELETE command).
	int mode = ADD_OAUTH_MODE;
	//
	// ad is constructed above.
	//
	// return value is not used because this is not a query.
	//
	// ccfile reference just passed through.

	return OAUTH_STORE_CRED(username, (unsigned const char*)username, strlen(username), mode, &ad, ret, ccfile);
}


// handle ADD, DELETE, & QUERY for kerberos credential.
// if command is ADD, and a credential is stored
//   but the caller should wait for a .cc file before proceeding,
//   the ccfile argument will be returned
long long
KRB_STORE_CRED(const char *username, const unsigned char *cred, const int credlen, int mode, ClassAd & return_ad, std::string & ccfile, bool &detected_local_cred)
{
	dprintf(D_ALWAYS, "Krb store cred user %s len %i mode %i\n", username, credlen, mode);

	// default to this being a real krb store cred
	detected_local_cred = false;

	// special case: if the credential starts with the magic string
	// "LOCAL:", we are not actually going to store anything in the krb directory,
	// but instead simply touch a file in the OAuth directory.  detect this right away
	// and call that function instead.
	if (cred && credlen > 6 && MATCH == strncmp((const char*)cred, "LOCAL:", 6)) {
		std::string servicename((const char*)&cred[6], credlen-6);

		if ((mode & MODE_MASK) != GENERIC_ADD) {
			dprintf(D_ALWAYS, "LOCAL_STORE_CRED does not support QUERY or DELETE modes, aborting the command.");
			return FAILURE;
		}

		// capture return value
		long long rv = LOCAL_STORE_CRED(username, servicename.c_str(), ccfile);

		// report status
		dprintf(D_SECURITY, "KRB_STORE_CRED: detected magic value with username \"%s\" and service name \"%s\", rv == %lli.\n", username, servicename.c_str(), rv);

		// only update the return param if we succeeded
		if (rv == SUCCESS) {
			detected_local_cred = true;
		}

		// pass through return from LOCAL store.
		return rv;
	}

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

	std::string credfile;
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
	std::string filename;
	formatstr(filename,"%s%c%s.cred", cred_dir.ptr(), DIR_DELIM_CHAR, user);
	dprintf(D_ALWAYS, "CREDS: reading data from %s\n", filename.c_str());

	// read the file (fourth argument "true" means as_root)
	unsigned char *buf = 0;
	if (read_secure_file(filename.c_str(), (void**)(&buf), &len, true)) {
		return buf;
	}

	return NULL;
}


long long store_cred_blob(const char *user, int mode, const unsigned char *blob, int bloblen, const ClassAd * ad, std::string &ccfile)
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
			bool junk = false; // API requires this output parameter, we don't look at it.
			return KRB_STORE_CRED(username.c_str(), blob, bloblen, krb_mode, return_ad, ccfile, junk);
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
	std::string credfile;
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
				((Sock*)s)->peer_addr().to_sinful().c_str());
		return TRUE;
	}

	ReliSock* sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
				"WARNING - authentication failed for password fetch attempt from %s\n",
				sock->peer_addr().to_sinful().c_str());
		goto bail_out;
	}

	// Enable encryption if available. If it's not available, the next
	// call will fail and we'll abort the connection.
	sock->set_crypto_mode(true);

	if ( !sock->get_encryption() ) {
		dprintf(D_ALWAYS,
			"WARNING - password fetch attempt without encryption from %s\n",
				sock->peer_addr().to_sinful().c_str());
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
	client_ipaddr = strdup(sock->peer_addr().to_sinful().c_str());

	// we do not want to send out the pool password through a command handler
	if(strcmp(user, POOL_PASSWORD_USERNAME) == 0) {
		dprintf(D_ALWAYS,
			"Refusing to fetch password for %s@%s requested by %s@%s at %s\n",
			user,domain,
			client_user,client_domain,client_ipaddr);
		goto bail_out;
	}

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
			((Sock*)s)->peer_addr().to_sinful().c_str());
		return TRUE;
	}

	ReliSock* sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
			"WARNING - authentication failed for credential fetch attempt from %s\n",
			sock->peer_addr().to_sinful().c_str());
		goto bail_out;
	}

	// Enable encryption if available. If it's not available, the next
	// call will fail and we'll abort the connection.
	sock->set_crypto_mode(true);

	if ( !sock->get_encryption() ) {
		dprintf(D_ALWAYS,
			"WARNING - credential fetch attempt without encryption from %s\n",
			sock->peer_addr().to_sinful().c_str());
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
	client_ipaddr = strdup(sock->peer_addr().to_sinful().c_str());

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
void store_cred_handler_continue(int tid);

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
	std::string ccfile;   // credmon completion file to watch for before returning
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
				((Sock*)s)->peer_addr().to_sinful().c_str());
		return FALSE;
	}

	ReliSock *sock = (ReliSock*)s;

	// Ensure authentication happened and succeeded and enable encryption
	// Daemons should register this command with force_authentication = true
	if ( !sock->isAuthenticated() ) {
		dprintf(D_ALWAYS,
			"WARNING - authentication failed for credential store attempt from %s\n",
			sock->peer_addr().to_sinful().c_str());
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

	return_ad.InsertAttr("fully_qualified_user", sock->getFullyQualifiedUser());

	// if no user was sent, this instructs us to take the authenticated
	// user from the socket.
	if (fulluser.empty()) {
		// fulluser needs to have the @domain attached to pass checks
		// below, even though at the moment it ultimately gets stripped
		// off.
		fulluser = sock->getFullyQualifiedUser();
		dprintf(D_SECURITY | D_VERBOSE, "store_cred: Storing cred for authenticated user \"%s\"\n", fulluser.c_str());
	}

	if ( ! fulluser.empty()) {
			// ensure that the username has an '@' delimteter
		size_t ix_at = fulluser.find('@');
		if (ix_at == std::string::npos || ix_at == 0) {
			dprintf(D_ALWAYS, "store_cred_handler: user \"%s\" not in user@domain format\n", fulluser.c_str());
			answer = FAILURE_BAD_ARGS;
		}
		else {
			username = fulluser.substr(0, ix_at);
				// We don't allow one user to set another user's credential
				//   (except for users explicitly allowed to)
				// TODO: We deliberately ignore the user domains. Isn't
				//   that a security issue?
				// we don't allow updates to the pool password through this interface
			std::vector<std::string> auth_users;
			param_and_insert_unique_items("CRED_SUPER_USERS", auth_users);
			auth_users.emplace_back(username);
			const char *sock_owner = sock->getOwner();
			if ( sock_owner == NULL ||
#if defined(WIN32)
			     !contains_anycase_withwildcard(auth_users, sock_owner)
#else
			     !contains_withwildcard(auth_users, sock_owner)
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
					bool is_local = false; // to find out if the KRB was a "LOCAL"
					answer = KRB_STORE_CRED(username.c_str(), (unsigned char*)cred.ptr(), credlen, krb_mode, return_ad, ccfile, is_local);
					// if this was a "locally issued" cred, switch cred type to OAUTH from here
					// on out so we SIGHUP the correct (OAuth) credmon and not the krb one.
					if(is_local) {
						// remove cred type from mode and change it to OAuth
						mode &= (~CRED_TYPE_MASK);
						mode |= STORE_CRED_USER_OAUTH;
						dprintf(D_SECURITY | D_FULLDEBUG, "STORE_CRED: modifed mode to STORE_CRED_USER_OAUTH.  new mode: %i\n", mode);
					}
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

		struct stat ccfile_stat;
		priv_state priv = set_root_priv();
		int rc = stat(ccfile.c_str(), &ccfile_stat);
		set_priv(priv);

		if (rc == 0) {
			// If the completion file already exists, just return success
			// with the file mtime.
			answer = ccfile_stat.st_mtime;
			dprintf(D_ALWAYS, "Completion file %s exists. mtime=%lld\n", ccfile.c_str(), (long long)answer);
		} else if (wake_the_credmon(mode) && request_credmon_wait) {
			StoreCredState* retry_state = new StoreCredState();
			retry_state->ccfile = strdup(ccfile.c_str());
			retry_state->retries = param_integer("CREDD_POLLING_TIMEOUT", 20);
			retry_state->s = new ReliSock(*((ReliSock*)s));
			retry_state->return_ad = return_ad;

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


void store_cred_handler_continue(int /* tid */)
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
		dprintf(D_ALWAYS, "Completion file %s exists. mtime=%lld\n", dptr->ccfile, (long long)answer);
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
	std::string username = POOL_PASSWORD_USERNAME "@";

	if (s->type() != Stream::reli_sock) {
		dprintf(D_ALWAYS, "ERROR: pool password set attempt via UDP\n");
		return CLOSE_STREAM;
	}

	// if we're the CREDD_HOST, make sure any password setting is done locally
	// (since knowing what the pool password is on the CREDD_HOST means being
	//  able to fetch users' passwords)
	char *credd_host = param("CREDD_HOST");
	if (credd_host) {

		std::string my_fqdn_str = get_local_fqdn();
		std::string my_hostname_str = get_local_hostname();
		// TODO: Arbitrarily picking IPv4
		std::string my_ip_str = get_local_ipaddr(CP_IPV4).to_ip_string();

		// figure out if we're on the CREDD_HOST
		bool on_credd_host = (strcasecmp(my_fqdn_str.c_str(), credd_host) == MATCH);
		on_credd_host = on_credd_host || (strcasecmp(my_hostname_str.c_str(), credd_host) == MATCH);
		on_credd_host = on_credd_host || (strcmp(my_ip_str.c_str(), credd_host) == MATCH);

		if (on_credd_host) {
				// we're the CREDD_HOST; make sure the source address matches ours
			const char *addr = ((ReliSock*)s)->peer_ip_str();
			if (!addr || strcmp(my_ip_str.c_str(), addr)) {
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
		result = store_cred_password(username.c_str(), pw, GENERIC_ADD);
		SecureZeroMemory(pw, strlen(pw));
	}
	else {
		result = store_cred_password(username.c_str(), NULL, GENERIC_DELETE);
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


// OBSOLETE!!! For LEGACY password modes only!!.
// use do_store_cred() for working with Krb or OAuth creds
int 
do_store_cred_passwd(const char* user, const char* pw, int mode, Daemon* d, bool force) {
	
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
	dprintf ( D_ALWAYS,  "STORE_CRED: (old) In mode %d '%s', user is \"%s\"\n", mode, mode_name[mode & MODE_MASK], user);
	
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
		if (username_is_pool_password(user, &domain_pos) && (mode & MODE_MASK) != GENERIC_QUERY) {
			cmd = STORE_POOL_CRED;
			user += domain_pos + 1;	// we only need to send the domain name for STORE_POOL_CRED
		}
		if (domain_pos <= 0) {
			dprintf(D_ALWAYS, "store_cred: user \"%s\" not in user@domain format\n", user);
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
// as of 8.9.7, this is the preferred method
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
	std::string daemonid; // for error messages

	// to help future debugging, print out the mode we are in
	dprintf ( D_ALWAYS,  "STORE_CRED: In mode %d '%s', user is \"%s\"\n", mode, mode_name[mode & MODE_MASK], user);

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
		std::string ccfile;	// we don't care about a completion file, but we have to pass this in anyway
		if (mode >= STORE_CRED_LEGACY_PWD && mode <= STORE_CRED_LAST_MODE) {
			std::string pw;
			if (cred) pw.assign((const char *)cred, credlen);
			return_val = store_cred_password(user, pw.c_str(), mode);
		} else {
			return_val = store_cred_blob(user, mode, cred, credlen, ad, ccfile);
		}
	} else {
		// send out the request remotely.

		// first see if we're operating on the pool password
		// if we are just use do_store_cred_passwd(), which handles passwords
		int domain_pos = -1;
		if (username_is_pool_password(user, &domain_pos)) {
			int cred_type = (mode & ~MODE_MASK);
			if ((cred_type != STORE_CRED_LEGACY_PWD) && (cred_type != STORE_CRED_USER_PWD)) {
				return FAILURE_BAD_ARGS;
			}
			std::string pw;
			if (cred) pw.assign((const char *)cred, credlen);
			return do_store_cred_passwd(user, pw.c_str(), mode, d);
		}
		if ((domain_pos <= 0) && user[0]) {
			dprintf(D_ALWAYS, "store_cred: FAILED. user \"%s\" not in user@domain format\n", user);
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

		std::string pw;
		if (mode & STORE_CRED_LEGACY) {
			if (cred) pw.assign((const char *)cred, credlen);
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

int do_check_oauth_creds (
	const classad::ClassAd* request_ads[],
	int num_ads,
	std::string & outputURL,
	Daemon* d /* = NULL*/)
{
	ReliSock * sock;
	CondorError err;
	std::string daemonid;

	outputURL.clear();
	if (num_ads < 0) {
		return -1;
	}
	if (num_ads == 0) {
		return 0;
	}

	if (! d) {
		Daemon my_credd(DT_CREDD);
		if (my_credd.locate()) {
			sock = (ReliSock*)my_credd.startCommand(CREDD_CHECK_CREDS, Stream::reli_sock, 20, &err);
			if (! sock) { daemonid = my_credd.idStr(); }
		} else {
			dprintf(D_ALWAYS, "could not find local CredD\n");
			return -2;
		}
	} else if (d->locate()) {
		sock = (ReliSock*)d->startCommand(CREDD_CHECK_CREDS, Stream::reli_sock, 20, &err);
		if (! sock) { daemonid = d->idStr(); }
	} else {
		daemonid = d->idStr();
		dprintf(D_ALWAYS, "could not locate %s\n", daemonid.c_str());
		return -2;
	}

	if (! sock) {
		dprintf(D_ALWAYS, "startCommand(CREDD_CHECK_CREDS) failed to %s\n", daemonid.c_str());
		return -3;
	}

	bool sent = false;
	sock->encode();
	if ( ! sock->put(num_ads)) {
		sent = false;
	} else {
		sent = true;
		for (int ii = 0; ii < num_ads; ++ii) {
			// to insure backward compability, there are 3 fields that *must* be set to empty strings
			// if they are missing or undefined. 8.9.9 and later will handle missing fields correctly
			// but 8.9.* < 8.9.9 will leak values from one attribute to the other 
			// if Handle, Scope, or Audience are not present or are set to undefined.
			// which can happen if the python-bindings or 8.9.8 submit is making the call
			classad::ClassAd ad(*request_ads[ii]);
			static const char * const fix_fields[] = { "Handle", "Scopes", "Audience" };
			for (int ii = 0; ii < (int)(sizeof(fix_fields) / sizeof(fix_fields[0])); ++ii) {
				const char * attr = fix_fields[ii];
				classad::Value val;
				val.SetUndefinedValue();
				if (! ad.EvaluateAttr(attr, val) || val.IsUndefinedValue()) {
					ad.Assign(attr, "");
				}
			}
			if (! putClassAd(sock, ad)) {
				sent = false;
				break;
			}
		}
	}
	// did we send the query payload? then send EOM also, of that fails the other end probably closed the socket
	if (sent) {
		if ( ! sock->end_of_message()) {
			sent = false;
		}
	}

	// if we sent a the request, no wait for a response
	// and then close the socket.
	if (sent) {
		sock->decode();
		if ( ! sock->get(outputURL) || ! sock->end_of_message()) {
			sent = false;
		}
	}
	sock->close();
	delete sock;

	// report any failures
	if ( ! sent) {
		dprintf(D_ALWAYS, "Failed to query OAuth from the CredD\n");
		return -4;
	}

	// return 0 or positive values for success
	// an empty URL indicates that the creds are already stored
	// a non-empty URL must be visited to create the requested creds
	return (int)outputURL.size();
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
		if ( ch == end_char || ch == EOF ) {
			break;
		} else if ( ch == '\b') { // backspace
			if ( ch_count > 0 ) { ch_count--; }
			continue;
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


const std::string & IssuerKeyNameCache::NameList(CondorError * err)
{
	// First, check to see if our cache is still usable; if so, return it
	time_t now = time(NULL);
	if ((now - m_last_refresh) < param_integer("SEC_TOKEN_POOL_SIGNING_DIR_REFRESH_TIME",0)) {
		return m_name_list;
	}
	m_last_refresh = now;

	std::string poolkeypath;
	param(poolkeypath, "SEC_TOKEN_POOL_SIGNING_KEY_FILE");

	// we also get all other signing keys from the token key directory if there is one
	// it is not an error to have no directory, nor is it an error to have no exclusion list
	Regex excludeFilesRegex;
	auto_free_ptr dirpath(param("SEC_PASSWORD_DIRECTORY"));
	if (dirpath) {
		auto_free_ptr excludeRegex(param("LOCAL_CONFIG_DIR_EXCLUDE_REGEXP"));
		if (excludeRegex) {
			int _errcode;
			int _erroffset;
			if (!excludeFilesRegex.compile(excludeRegex.ptr(), &_errcode, &_erroffset)) {
				if (err) err->pushf("TOKEN", 1, "LOCAL_CONFIG_DIR_EXCLUDE_REGEXP "
					"config parameter is not a valid "
					"regular expression.  Value: %s,  Error Code: %d",
					excludeRegex.ptr(), _errcode);
				return m_name_list;
			}
			if (!excludeFilesRegex.isInitialized()) {
				if (err) err->push("TOKEN", 1, "Failed to initialize exclude files regex.");
				return m_name_list;
			}
		}
	}

	// If we can, try reading out the passwords as root.
	TemporaryPrivSentry sentry(PRIV_ROOT);

	// check access on the pool key, and scan the key directory
	std::set<std::string> keys;

	size_t keyslen = 0;
	if ( ! poolkeypath.empty()) { // this is currently the pool key
		if (0 == access(poolkeypath.c_str(), R_OK)) {
			keys.insert("POOL");
			keyslen += 4;
		}
	}

	// do we have a directory? 
	if (dirpath) {
		Directory dir(dirpath);
		if (!dir.Rewind()) {
			if (err) {
				err->pushf("CRED", 1, "Cannot open %s: %s (errno=%d)",
					dirpath.ptr(), strerror(errno), errno);
			}
		} else {

			const char *file;
			while ((file = dir.Next())) {
				if (dir.IsDirectory()) {
					continue;
				}
				if (!excludeFilesRegex.isInitialized() || !excludeFilesRegex.match(file)) {
					if (0 == access(dir.GetFullPath(), R_OK)) {
						keys.insert(file);
						keyslen += strlen(file);
					}
				} else {
					dprintf(D_FULLDEBUG | D_SECURITY, "Skipping TOKEN key file "
						"based on LOCAL_CONFIG_DIR_EXCLUDE_REGEXP: "
						"'%s'\n", dir.GetFullPath());
				}
			}
		}
	}

	// now turn the temporary set of keys into a comma separated string

	m_name_list.clear();
	if ( ! keys.empty()) {
		m_name_list.reserve(keyslen + 1 + 2 * keys.size());
		for (auto it = keys.begin(); it != keys.end(); ++it) {
			if (! m_name_list.empty()) m_name_list += ", ";
			m_name_list += *it;
		}
	}

	return m_name_list;
}

// helper function for getTokenSigningKeyPath and hasTokenSigningKey
// takes a key_id and returns the path to the file that should contain the signing key
// will set the CondorError if the configuration is missing knobs necessary to determine the path
// will optionally set is_legacy_pool_pass if the signing key should be loaded using backward compat hacks for pool passwords
bool getTokenSigningKeyPath(const std::string &key_id, std::string &fullpath, CondorError *err, bool * is_pool_pass)
{
	bool is_pool = false;
	if (key_id.empty() || (key_id == "POOL")) {
		param(fullpath, "SEC_TOKEN_POOL_SIGNING_KEY_FILE");
		if (fullpath.empty()) {
			if (err) err->push("TOKEN", 1, "No master pool token key setup in SEC_TOKEN_POOL_SIGNING_KEY_FILE");
			return false;
		}
		// 
		is_pool = true;

	} else {

		// we get all other signing keys from the token key directory
		auto_free_ptr dirpath(param("SEC_PASSWORD_DIRECTORY"));
		if ( ! dirpath) {
			if (err) err->push("TOKEN", 1, "SEC_PASSWORD_DIRECTORY is undefined");
			return false;
		}
		dircat(dirpath, key_id.c_str(), fullpath);
	}

	if (is_pool_pass) *is_pool_pass = is_pool;
	return true;
}

// returns true if the token signing key file for this key id exists and is readable by root
// will set CondorError and return false if not.
bool hasTokenSigningKey(const std::string &key_id, CondorError *err) {

	// do a quick check in the issuer name cache, but don't rebuild it
	const auto &keys = g_issuer_name_cache.Peek();
	for (const auto& item: StringTokenIterator(keys)) {
		if (item == key_id) {
			return true;
		}
	}

	std::string fullpath;
	if ( ! getTokenSigningKeyPath(key_id, fullpath, err, nullptr)) {
		return false;
	}
	// check to see if the file exits and is readable
	TemporaryPrivSentry sentry(PRIV_ROOT);
	if (0 == access(fullpath.c_str(), R_OK)) {
		return true;
	}
	return false;
}

bool getTokenSigningKey(const std::string &key_id, std::string &contents, CondorError *err) {

	// If using the POOL password as the signing key may have to truncate at first null for backward compat
	bool truncate_at_first_null = false;
	// for backward compatibility, if using the pool password we may want to double the key
	bool double_the_key = false; 

	// We get the "pool" signing key for this pool from a specific knob
	std::string fullpath;
	bool is_pool = false;
	if ( ! getTokenSigningKeyPath(key_id, fullpath, err, &is_pool)) {
		return false;
	}
	// so that tokens issued against a non-trucated POOL signing key pre 8.9.12 still work
	// we will always double the pool key
	double_the_key = is_pool;
	if (is_pool) {
			// secret knob to tell us to process the token signing key the way we would pre 9.0
			// Set this to true if the key has imbedded nulls and had been used to sign tokens with
			// an earlier version of Condor, and for some reason you don't want to just truncate the
			// file itself
		truncate_at_first_null = param_boolean("SEC_TOKEN_POOL_SIGNING_KEY_IS_PASSWORD", false);
	}

	dprintf(D_SECURITY, "getTokenSigningKey(): for id=%s, pool=%d v84mode=%d reading %s\n",
		key_id.c_str(), is_pool, truncate_at_first_null, fullpath.c_str());

	char* buf = nullptr;
	size_t len = 0;
	const bool as_root = true; // TODO: don't require root if we can't do priv switching
	if ( ! read_secure_file(fullpath.c_str(), (void**)&buf, &len, as_root) || ! buf) {
		if (err) err->pushf("TOKEN", 1, "Failed to read file %s securely.", fullpath.c_str());
		dprintf(D_ALWAYS, "getTokenSigningKey(): read_secure_file(%s) failed!\n", fullpath.c_str());
		return false;
	}
	size_t file_len = len;
	if (truncate_at_first_null) {
		// buffer now contains the binary contents from the file.
		// due to the way 8.4.X and earlier version wrote the file,
		// there will be trailing NULL characters, although they are
		// ignored in 8.4.X by the code that reads them.  As such, for
		// us to agree on the password, we also need to discard
		// everything after the first NULL.  we do this by simply
		// resetting the len.  there is a function "strnlen" but it's a
		// GNU extension so we just do the raw scan here:
		size_t newlen = 0;
		while(newlen < len && buf[newlen]) {
			newlen++;
		}
		len = newlen;
	}

	// copy back the result, undoing the trivial scramble
	// and doubling the key if needed
	std::vector<char> pw;
	if (double_the_key) {
		pw.resize(len*2+1);
		simple_scramble(reinterpret_cast<char*>(pw.data()), buf, (int)len);
		if (truncate_at_first_null) {
			// we want to be sure that post-scramble we still have no imbedded nulls
			// so we force null after the end of the buffer and then get the length again. 
			pw[len] = 0;
			len = strlen(pw.data());
		}
		memcpy(&pw[len], &pw[0], len);
		if (len < file_len) {
			dprintf(D_ALWAYS, "WARNING: pool signing key truncated from %d to %d bytes because of internal NUL characters\n",
				(int)file_len, (int)len);
		}
		len *= 2;
	} else {
		pw.resize(len);
		simple_scramble(reinterpret_cast<char*>(pw.data()), buf, (int)len);
	}
	free(buf);

	contents.assign(pw.data(), len);
	return true;
}

const std::string & getCachedIssuerKeyNames(CondorError * err)
{
	return g_issuer_name_cache.NameList(err);
}

void clearIssuerKeyNameCache()
{
	g_issuer_name_cache.Clear();
}

void CredSorter::Init()
{
	if (!param(m_local_names, "LOCAL_CREDMON_PROVIDER_NAMES")) {
		if (!param(m_local_names, "LOCAL_CREDMON_PROVIDER_NAME", "scitokens")) {
			m_client_names.clear();
		}
	}
	if (!param(m_client_names, "CLIENT_CREDMON_PROVIDER_NAMES")) {
		m_client_names.clear();
	}
	// For oauth2 and vault, an empty value means they claim all tokens
	// not explicitly claimed by another type.
	if (!param(m_oauth2_names, "OAUTH2_CREDMON_PROVIDER_NAMES") || m_oauth2_names == "*") {
		m_oauth2_names.clear();
	}
	// If neither VAULT_CREDMON_PROVIDER_NAMES nor SEC_CREDENTIAL_STORER
	// is set, then assume there are no Vault tokens.
	m_vault_names.clear();
	m_vault_enabled = false;
	if (param(m_vault_names, "VAULT_CREDMON_PROVIDER_NAMES")) {
		m_vault_enabled = true;
		if (m_vault_names == "*") {
			m_vault_names.clear();
		}
	}
	std::string storer;
	if (param(storer, "SEC_CREDENTIAL_STORER")) {
		m_vault_enabled = true;
	}
}

CredSorter::CredType CredSorter::Sort(const std::string& cred_name) const
{
	std::string param_name;
	std::string param_val;
	for (const auto& str: StringTokenIterator(m_local_names)) {
		if (cred_name == str) {
			return LocalIssuerType;
		}
	}
	for (const auto& str: StringTokenIterator(m_client_names)) {
		if (cred_name == str) {
			return LocalClientType;
		}
	}
	for (const auto& str: StringTokenIterator(m_oauth2_names)) {
		if (cred_name == str) {
			return OAuth2Type;
		}
	}
	for (const auto& str: StringTokenIterator(m_vault_names)) {
		if (cred_name == str) {
			return VaultType;
		}
	}
	formatstr(param_name, "%s_CLIENT_ID", cred_name.c_str());
	bool client_id_defined = param(param_val, param_name.c_str());
	if (m_oauth2_names.empty() && client_id_defined) {
		return OAuth2Type;
	}
	if (m_vault_enabled && m_vault_names.empty() && !client_id_defined) {
		return VaultType;
	}
	return UnknownType;
}
