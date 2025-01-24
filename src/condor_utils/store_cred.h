/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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


#ifndef STORE_CRED_H
#define STORE_CRED_H

#include "condor_common.h"
#include "condor_io.h"

#if !defined(WIN32)
// TODO: move this to an appropriate place
void SecureZeroMemory(void *, size_t);
#endif

// store cred return codes
const int SUCCESS = 1; 					// it worked!
const int FAILURE = 0;					// communication error
const int FAILURE_BAD_PASSWORD = 2;		// bad (wrong) password
const int FAILURE_NO_IMPERSONATE = 3;	// user switching not supported
const int FAILURE_NOT_SECURE = 4;		// channel is insecure
const int FAILURE_NOT_FOUND = 5;		// user not found
// error codes new for 8.9.7
const int SUCCESS_PENDING = 6;			// The credential has not yet been processed by the credmon
const int FAILURE_NOT_ALLOWED = 7;		// authenticated user is not allowed to do the operation
const int FAILURE_BAD_ARGS = 8;			// argument missing or arguments contradict each other
const int FAILURE_PROTOCOL_MISMATCH = 9; // not all of the correct information was sent on the wire, client and server my be mismatched.
const int FAILURE_CREDMON_TIMEOUT = 10;  // The credmon did not process credentials within the timeout period
const int FAILURE_CONFIG_ERROR = 11;    // an operation failed because of a configuration error
const int FAILURE_JSON_PARSE = 12;      // Failure parsing credential as JSON
const int FAILURE_CRED_MISMATCH = 13;   // Credential was found but it did not match requested scopes or audience

// not a return code - reserved for caller's use
const int FAILURE_ABORTED = -1;	

const int STORE_CRED_USER_KRB = 0x20;
const int STORE_CRED_USER_PWD = 0x24;
const int STORE_CRED_USER_OAUTH = 0x28;
const int STORE_CRED_LEGACY = 0x40;
const int STORE_CRED_WAIT_FOR_CREDMON = 0x80; // wait for a credmon to process the users credential(s) before returning
const int STORE_CRED_LEGACY_PWD = STORE_CRED_LEGACY | STORE_CRED_USER_PWD | 0x40;	 // prior to 8.9.7 the CREDD only supports this mode

// store cred modes
const int GENERIC_ADD = 0;
const int GENERIC_DELETE = 1;
const int GENERIC_QUERY = 2;
const int GENERIC_CONFIG = 3;

const char ADD_CREDENTIAL[] = "add";
const char DELETE_CREDENTIAL[] = "delete";
const char QUERY_CREDENTIAL[] = "query";
const char CONFIG_CREDENTIAL[] = "config";

const int ADD_KRB_MODE = STORE_CRED_USER_KRB | GENERIC_ADD;
const int DELETE_KRB_MODE = STORE_CRED_USER_KRB | GENERIC_DELETE;
const int QUERY_KRB_MODE = STORE_CRED_USER_KRB | GENERIC_QUERY;
const int CONFIG_KRB_MODE = STORE_CRED_USER_KRB | GENERIC_CONFIG; 

const int ADD_PWD_MODE = STORE_CRED_USER_PWD | GENERIC_ADD;
const int DELETE_PWD_MODE = STORE_CRED_USER_PWD | GENERIC_DELETE;
const int QUERY_PWD_MODE = STORE_CRED_USER_PWD | GENERIC_QUERY;
const int CONFIG_PWD_MODE = STORE_CRED_USER_PWD | GENERIC_CONFIG; 

const int ADD_OAUTH_MODE = STORE_CRED_USER_OAUTH | GENERIC_ADD;
const int DELETE_OAUTH_MODE = STORE_CRED_USER_OAUTH | GENERIC_DELETE;
const int QUERY_OAUTH_MODE = STORE_CRED_USER_OAUTH | GENERIC_QUERY;
const int CONFIG_OAUTH_MODE = STORE_CRED_USER_OAUTH | GENERIC_CONFIG; 


const int STORE_CRED_FIRST_MODE = STORE_CRED_USER_KRB;
// config mode for debugging
#if defined(WIN32)
const int STORE_CRED_LAST_MODE = STORE_CRED_LEGACY_PWD + GENERIC_CONFIG;
#else
const int STORE_CRED_LAST_MODE = STORE_CRED_LEGACY_PWD + GENERIC_QUERY;
#endif

#define POOL_PASSWORD_USERNAME "condor_pool"

#define MAX_PASSWORD_LENGTH 255

class Daemon;
namespace classad { class ClassAd; }

class Service;
int store_pool_cred_handler(int, Stream *s);

// talk to credd to query, delete or store a credential, use this one for passwords, but NOT kerberose or Oauth 
int do_store_cred_passwd(const char *user, const char* pw, int mode, Daemon *d = NULL, bool force = false);

// talk to credd to query, delete, or store a credential, use this one for password, kerberos, or oauth
// pass return value to store_cred_failed to determine if the return is an error or other information
long long do_store_cred(
	const char *user,                   // IN: full username in form user@domain
	int mode,                           // IN: one of the command/modes above. ADD_KRB_MODE, DELETE_OAUTH_MODE, etc
	const unsigned char * cred,         // IN: binary credential if GENERIC_ADD, may not contain \0 if mode is ADD_PWD_MODE
	int credlen,                        // IN: size of binary credential
	classad::ClassAd &return_ad, // OUT: some modes will return information here
	classad::ClassAd* ad=NULL,	// IN: optional additional information (for future use)
	Daemon *d = NULL);                  // IN: optional daemon to contact, defaults to local schedd

bool store_cred_failed(long long ret, int mode, const char ** errstring=NULL);

int do_check_oauth_creds(const classad::ClassAd* request_ads[], int num_ads, std::string & outputURL, Daemon* d = NULL);

// store a password into a file on unix and into the registry on Windows
int store_cred_password(const char *user, const char *pass, int mode);
// store a binary blob, (kerberos ticket or OAuth token), on successful ADD ccfile will be set to a filename to watch for if a credmon needs to process the blob
long long store_cred_blob(const char *user, int mode, const unsigned char * cred, int credlen, const classad::ClassAd* ad, std::string & ccfile);
// command handler for STORE_CRED in Credd, Master and Schedd
int store_cred_handler(int i, Stream *s);
// command handler for CREDD_GET_PASSWD in Credd and Shadow
int cred_get_password_handler(int i, Stream *s);
// command handler for CREDD_GET_CRED in Shadow
int cred_get_cred_handler(int i, Stream *s);
// check whether credfile has matching scopes and audience to those in requestAd
int cred_matches(const std::string & credfile, const classad::ClassAd * requestAd);

bool read_from_keyboard(char* buf, int maxlength, bool echo = true);
char* get_password(void);	// get password from user w/o echo on the screen
//int addCredential(const char* user, const char* pw, Daemon *d = NULL);
//int deleteCredential(const char* user, const char* pw, Daemon *d = NULL);
//int queryCredential(const char* user, Daemon *d = NULL);  // just tell me if I have one stashed

#if defined(WIN32)
// attempt to login with given username/password to see if Windows considers it valid
bool isValidCredential( const char *user, const char* pw );
#endif

/** Get the stored password from our password stuff in the registry.  
	Result must be deallocated with free() by the caller if not NULL.  
	Will fail on Windows if not LocalSystem when called.
	TODO: support this fully on Linux so a Linux CREDD can service Windows execute nodes
	@return malloced string with the password, or NULL if failure.

	TJ says - DO NOT USE THIS FOR SOME OTHER PURPOSE ON LINUX!
*/
char* getStoredPassword(const char *user, const char *domain);

/**
 * Retrieve the stored credential from the KRB5 credential directory.
 */
unsigned char* getStoredCredential(int mode, const char *username, const char *domain, int & credlen);

/** Get an IDTOKEN signing key from disk. */
bool getTokenSigningKey(const std::string &key_id, std::string &contents, CondorError *err);
/** Check to see if an IDTOKEN signing key exists,
    uses the IssuerKeyNameCache if it is populated, but will not populate that cache */
bool hasTokenSigningKey(const std::string &key_id, CondorError *err);

/** List all the IDTOKEN signing keys. To allow this to be invoked frequently,
 *  it will cache the list internally. */
const std::string & getCachedIssuerKeyNames(CondorError * err);
void clearIssuerKeyNameCache();

bool okay_for_oauth_filename(const std::string & s);

// Given an oauth2 token name (wihtout the optional handle name), determine
// which credmon is responsible for maintaining it.
class CredSorter
{
 public:
	CredSorter() { Init(); }
	void Init();

	enum CredType { OAuth2Type, LocalIssuerType, LocalClientType, VaultType, UnknownType };

	CredType Sort(const std::string& cred_name) const;

 private:
	std::string m_local_names;
	std::string m_client_names;
	std::string m_oauth2_names;
	std::string m_vault_names;
};

#endif // STORE_CRED_H

