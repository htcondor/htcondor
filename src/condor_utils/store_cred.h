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
const int FAILURE_NOT_SUPPORTED = 3;	// user switching not supported
const int FAILURE_NOT_SECURE = 4;		// channel is insecure
const int FAILURE_NOT_FOUND = 5;		// user not found

// not a return code - reserved for caller's use
const int FAILURE_ABORTED = -1;	

// store cred modes
const int ADD_MODE = 100;
const int DELETE_MODE = 101;
const int QUERY_MODE = 102;

const char ADD_CREDENTIAL[] = "add";
const char DELETE_CREDENTIAL[] = "delete";
const char QUERY_CREDENTIAL[] = "query";

// config mode for debugging
#if defined(WIN32)
const int CONFIG_MODE = 103;
const char CONFIG_CREDENTIAL[] = "config";
#endif

#define POOL_PASSWORD_USERNAME "condor_pool"

#define MAX_PASSWORD_LENGTH 255

class Daemon;

void store_pool_cred_handler(void *, int i, Stream *s);
int store_cred(const char *user, const char* pw, int mode, Daemon *d = NULL, bool force = false);
int store_cred_service(const char *user, const char *pw, int mode);
bool read_from_keyboard(char* buf, int maxlength, bool echo = true);
char* get_password(void);	// get password from user w/o echo on the screen
int addCredential(const char* user, const char* pw, Daemon *d = NULL);
int deleteCredential(const char* user, const char* pw, Daemon *d = NULL);
int queryCredential(const char* user, Daemon *d = NULL);  // just tell me if I have one stashed

#if !defined(WIN32)
int write_password_file(const char* path, const char* password);
#endif

#if defined(WIN32)
void store_cred_handler(void *, int i, Stream *s);
bool isValidCredential( const char *user, const char* pw );
#endif

/** Get the stored password from our password staff in the registry.  
	Result must be deallocated with free()
	by the caller if not NULL.  Will fail if not LocalSystem when
	called.
	@return malloced string with the password, or NULL if failure.
*/
char* getStoredCredential(const char *user, const char *domain);

#endif // STORE_CRED_H



	
