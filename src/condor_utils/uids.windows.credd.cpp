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
#include "condor_debug.h"
#include "condor_uid.h"
#include "condor_config.h"
#include "condor_environ.h"
#include "condor_distribution.h"
#include "my_username.h"
#include "daemon.h"
#include "store_cred.h"

bool get_password_from_credd (
	const char * credd_host,
	const char  username[],
	const char  domain[],
	char * pw,
	int    cb) // sizeof pw buffer (in bytes)
{
	bool got_password = false;

	dprintf(D_FULLDEBUG, "trying to fetch password from credd: %s\n", credd_host);
	Daemon credd(DT_CREDD);
	Sock * credd_sock = credd.startCommand(CREDD_GET_PASSWD,Stream::reli_sock,10);
	if ( credd_sock ) {
		credd_sock->set_crypto_mode(true);
		credd_sock->put((char*)username);	// send user
		credd_sock->put((char*)domain);		// send domain
		credd_sock->end_of_message();
		credd_sock->decode();
		pw[0] = '\0';
		char *my_buffer = pw;
		if ( credd_sock->get(my_buffer,cb) && pw[0] ) {
			got_password = true;
		} else {
			dprintf(D_FULLDEBUG,
					"credd (%s) did not have info for %s@%s\n",
					credd_host, username,domain);
		}
		delete credd_sock;
		credd_sock = NULL;
	} else {
		dprintf(D_FULLDEBUG,"Failed to contact credd %s: %s\n",
			credd_host,credd.error() ? credd.error() : "");
	}
    return got_password;
}

bool 
cache_credd_locally (
	const char  username[],
	const char  domain[],
	const char * pw)
{
	bool fAdded = false;
	std::string my_full_name;
	formatstr(my_full_name, "%s@%s",username,domain);
	if ( do_store_cred(my_full_name.c_str(),pw,ADD_PWD_MODE) == SUCCESS ) {
		dprintf(D_FULLDEBUG,
			"init_user_ids: "
			"Successfully stashed credential in registry for user %s\n",
			my_full_name.c_str());
        fAdded = true;
	} else {
		dprintf(D_FULLDEBUG,
			"init_user_ids: "
			"Failed to stash credential in registry for user %s\n",
			my_full_name.c_str());
	}
    return fAdded;
}
