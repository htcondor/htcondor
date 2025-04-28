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


#include "condor_common.h"
#include "condor_config.h"
#include "CondorError.h"

#include "condor_auth_claim.h"

Condor_Auth_Claim :: Condor_Auth_Claim(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_CLAIMTOBE)
{
}

Condor_Auth_Claim :: ~Condor_Auth_Claim()
{
}

int Condor_Auth_Claim :: authenticate(const char * /* remoteHost */, CondorError* /* errstack */, bool /*non_blocking*/)
{

	const char * pszFunction = "Condor_Auth_Claim :: authenticate";

	int retval = 0;
	int fail = 0;

	if ( mySock_->isClient() ) {

		std::string myUser;
		bool error_getting_name = false;

		// get our user name in condor priv
		// (which is what we want to use for daemons. for
		//  tools and daemons not started as root, this will
		//  just give us the username that we were invoked
		//  as, which is also what we want)
		priv_state priv = set_condor_priv();
		char* tmpOwner = NULL; 
		char* tmpSwitchUser = param("SEC_CLAIMTOBE_USER");
		if(tmpSwitchUser) {
			tmpOwner = tmpSwitchUser;
			dprintf(D_ALWAYS, "SEC_CLAIMTOBE_USER to %s!\n", tmpSwitchUser);
		} else {
			tmpOwner = my_username();
		}
		// remove temptation to use this variable to see if we were
		// specifying the claim to be user
		tmpSwitchUser = NULL;
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

			// check SEC_CLAIMTOBE_INCLUDE_DOMAIN.
			if (param_boolean("SEC_CLAIMTOBE_INCLUDE_DOMAIN", true)) {
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
					myUser += '@';
					myUser += tmpDomain;
					free(tmpDomain);
				}
			}
		}

		if (!error_getting_name) {

			//send 1 and then our username
			mySock_->encode();
			retval = 1;
			if (!mySock_->code( retval ) || 
			    !mySock_->code( myUser ))
			{
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				return fail; 
			}
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

			std::string myUser = tmpUser;

			// check SEC_CLAIMTOBE_INCLUDE_DOMAIN.
			if (param_boolean("SEC_CLAIMTOBE_INCLUDE_DOMAIN", true)) {
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
				formatstr(myUser, "%s@%s", tmpUser, tmpDomain);
				free(tmpDomain);
			}
			setRemoteUser(tmpUser);
			setAuthenticatedName(myUser.c_str());
			free(tmpUser);
			retval = 1;

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
