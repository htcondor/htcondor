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


#if !defined(SKIP_AUTHENTICATION) && !defined(WIN32)
#include <stdlib.h>
#include <munge.h>
#include "condor_auth_munge.h"
#include "condor_string.h"
#include "condor_environ.h"
#include "CondorError.h"
#include "condor_mkstemp.h"
#include "ipv6_hostname.h"

Condor_Auth_MUNGE :: Condor_Auth_MUNGE(ReliSock * sock)
    : Condor_Auth_Base    ( sock, CAUTH_MUNGE )
{
}

Condor_Auth_MUNGE :: ~Condor_Auth_MUNGE()
{
}


int Condor_Auth_MUNGE::authenticate(const char * /* remoteHost */, CondorError* errstack, bool /* non_blocking */)
{
	int client_result = -1;
	int server_result = -1;
	int fail = -1 == 0;
	char *munge_token = NULL;
	munge_err_t err;

	if ( mySock_->isClient() ) {

		// Until session caching supports clients that present
		// different identities to the same service at different
		// times, we want condor daemons to always authenticate
		// as condor priv, rather than as the current euid.
		// For tools and daemons not started as root, this
		// is a no-op.
		priv_state saved_priv = set_condor_priv();

		err = munge_encode (&munge_token, NULL, NULL, 0);
      		if ( err != EMUNGE_SUCCESS ) {
          		dprintf(D_SECURITY, "AUTHENTICATE_MUNGE: %s\n", munge_strerror (err));
			return fail;
      		}

		// send over result as a success/failure indicator (-1 == failure)
		mySock_->encode();
		client_result = 0;
		if (!mySock_->code( client_result ) || !mySock_->code( munge_token ) || !mySock_->end_of_message()) {
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
				free( munge_token );
			set_priv(saved_priv);
			return fail; 
		}

		free(munge_token);

		// now let the server verify
		mySock_->decode();
		if (!mySock_->code( server_result ) || !mySock_->end_of_message()) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			set_priv(saved_priv);
			return fail; 
		}

		set_priv(saved_priv);

		dprintf( D_SECURITY, "AUTHENTICATE_MUNGE:  status: %d\n",
				(server_result == 0) );

		// this function returns TRUE on success, FALSE on failure,
		// which is just the opposite of server_result.
		return( server_result == 0 );

	} else {
		uid_t uid;
		gid_t gid;

		// server code
		setRemoteUser( NULL );

		mySock_->decode();
		if (!mySock_->code( client_result ) || !mySock_->code( munge_token ) || !mySock_->end_of_message() || client_result != 0 ) {
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			return fail;
		}

		err = munge_decode (munge_token, NULL, NULL, NULL, &uid, &gid);

		if (err != EMUNGE_SUCCESS) {
			mySock_->encode();
			server_result = -1;
		} else {
			char *tmpOwner = my_username( uid );
			if (!tmpOwner) {
				// this could happen if, for instance,
				// getpwuid() failed.
				server_result = -1;
				errstack->pushf("MUNGE", 1006,
						"Unable to lookup uid %i", uid);
			} else {
				server_result = 0;	// 0 means success here. sigh.
				setRemoteUser( tmpOwner );
				setAuthenticatedName( tmpOwner );
				free( tmpOwner );
				setRemoteDomain( getLocalDomain() );
			}
		}

		mySock_->encode();
		if (!mySock_->code( server_result ) || !mySock_->end_of_message()) {
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n",
				__FUNCTION__, __LINE__);
			return fail;
		}
		return server_result == 0;
	}
}

int Condor_Auth_MUNGE :: isValid() const
{
    return TRUE;
}

#endif
