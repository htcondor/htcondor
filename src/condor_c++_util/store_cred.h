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

#ifndef STORE_CRED_H
#define STORE_CRED_H

#ifdef WIN32
#include "condor_common.h"
#include "condor_io.h"


// store cred return codes
const int SUCCESS = 1; 				// it worked!
const int FAILURE = 0;				// communication error
const int FAILURE_BAD_PASSWORD = 2;		// bad (wrong) password
const int FAILURE_NOT_SUPPORTED = 3;		// user switching not supported
						// (not running as SYSTEM)

// store cred modes
const int ADD_MODE = 100;
const int DELETE_MODE = 101;
const int QUERY_MODE = 102;
const int CONFIG_MODE = 103;

const char ADD_CREDENTIAL[] = "add";
const char DELETE_CREDENTIAL[] = "delete";
const char QUERY_CREDENTIAL[] = "query";
const char CONFIG_CREDENTIAL[] = "config";

#define MAX_PASSWORD_LENGTH 255

void store_cred_handler(void *, int i, Stream *s);
int store_cred(const char *user, const char* pw, int mode, Daemon *d = NULL );
int store_cred_service(const char *user, const char *pw, int mode);

bool read_no_echo(char* buf, int maxlength);
char* get_password(void);	// get password from user w/o echo on the screen
bool isValidCredential( const char *user, const char* pw );
int addCredential(const char* user, const char* pw);
int deleteCredential(const char* user); // not checking password before removal yet
int queryCredential(const char* user);  // just tell me if I have one stashed

#endif // WIN32

/** Get the stored password from our password staff in the registry.  
	Result must be deallocated with free()
	by the caller if not NULL.  Will fail if not LocalSystem when
	called.
	@return malloced string with the password, or NULL if failure.
*/
char* getStoredCredential(const char *user, const char *domain);

#endif // STORE_CRED_H



	
