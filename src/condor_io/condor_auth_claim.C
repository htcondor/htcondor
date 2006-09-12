/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_config.h"
#include "CondorError.h"

#if !defined(SKIP_AUTHENTICATION)

#include "condor_auth_claim.h"

Condor_Auth_Claim :: Condor_Auth_Claim(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_CLAIMTOBE)
{
}

Condor_Auth_Claim :: ~Condor_Auth_Claim()
{
}

int Condor_Auth_Claim :: authenticate(const char * remoteHost, CondorError* errstack)
{

	const char * pszFunction = "Condor_Auth_Claim :: authenticate";

	int retval = 0;
	int fail = 0;

	if ( mySock_->isClient() ) {

		MyString myUser;
		bool error_getting_name = false;

		// get our user name in condor priv
		// (which is what we want to use for daemons. for
		//  tools and daemons not started as root, this will
		//  just give us the username that we were invoked
		//  as, which is also what we want)
		priv_state priv = set_condor_priv();
		char* tmpOwner = my_username();
		set_priv(priv);
		if ( !tmpOwner ) {
			//send 0
			if (!mySock_->code( retval )) { 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				return fail; 
			}
			error_getting_name = true;
		}
		else {
			myUser = tmpOwner;
			free(tmpOwner);

			// check SEC_CLAIMTOBE_INCLUDE_DOMAIN. this knob exists (and defaults
			// to false) to provide backwards compatibility. it will be removed
			// completely in the development (6.9) series
			if (param_boolean("SEC_CLAIMTOBE_INCLUDE_DOMAIN", false)) {
				char* tmpDomain = param("UID_DOMAIN");
				if ( !tmpDomain ) {
					//send 0
					if (!mySock_->code( retval )) { 
						dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
							pszFunction, __LINE__);
						return fail; 
					}
					error_getting_name = true;
				}
				else {
					myUser += "@";
					myUser += tmpDomain;
					free(tmpDomain);
				}
			}
		}

		if (!error_getting_name) {

			//send 1 and then our username
			mySock_->encode();
			retval = 1;
			char* tmpUser = strdup(myUser.Value());
			ASSERT(tmpUser);
			if (!mySock_->code( retval ) || 
			    !mySock_->code( tmpUser ))
			{
				free(tmpUser);
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				return fail; 
			}
			//setRemoteUser(tmpUser); // <-- IS THIS NEEDED????
			free(tmpUser);
			if (!mySock_->end_of_message()) { 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				return fail;
			}
			mySock_->decode();
			if (!mySock_->code( retval )) {
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				return fail; 
			}
		}

	} else { //server side

		mySock_->decode();
		if (!mySock_->code( retval )) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
				pszFunction, __LINE__);
			return fail; 
		}

		//if 1, receive user and send back ok
		if( retval == 1 ) {

			char* tmpUser = NULL;
			if (!mySock_->code( tmpUser ) ||
			    !mySock_->end_of_message())
			{
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				if (tmpUser != NULL)
				{
					free(tmpUser);
				}
				return fail;
			}

			if( tmpUser ) {

				MyString myUser = tmpUser;

				// check SEC_CLAIMTOBE_INCLUDE_DOMAIN. this knob exists (and defaults
				// to false) to provide backwards compatibility. it will be removed
				// completely in the development (6.9) series
				if (param_boolean("SEC_CLAIMTOBE_INCLUDE_DOMAIN", false)) {
					// look for an '@' char in the name we received.
					// if present (newer clients), set the domain using
					// the given component. if not present (older clients),
					// use UID_DOMAIN from our config
					char* tmpDomain = NULL;
					char* at = strchr(tmpUser, '@');
					if ( at ) {
						*at = '\0';
						if (*(at + 1) != '\0') {
							tmpDomain = strdup(at + 1);
						}
					}
					if (!tmpDomain) {
						tmpDomain = param("UID_DOMAIN");
					}
					ASSERT(tmpDomain);
					setRemoteDomain(tmpDomain);
					myUser.sprintf("%s@%s", tmpUser, tmpDomain);
					free(tmpDomain);
				}
				setRemoteUser(tmpUser);
				setAuthenticatedName(myUser.Value());
				free(tmpUser);
				retval = 1;
			} else {
				// tmpUser is NULL; failure
				retval = 0;
			}

			mySock_->encode();
			if (!mySock_->code( retval )) { 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				return fail;
			}
		}
	}
    
	if (!mySock_->end_of_message()) { 
		dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
			pszFunction, __LINE__);
		return fail; 
	}

	return retval;
}

int Condor_Auth_Claim :: isValid() const
{
    return TRUE;
}

#endif
