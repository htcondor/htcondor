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

#include "condor_common.h"
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
    char *tmpOwner = NULL;
	int fail = 0;
    
    if ( mySock_->isClient() ) {
        mySock_->encode();
        if( (tmpOwner = my_username()) ) {
            //send 1 and then username
            retval = 1;
            if (!mySock_->code( retval ) || 
            	!mySock_->code( tmpOwner ))
			{ 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				free(tmpOwner);
				return fail; 
			}
            setRemoteUser( tmpOwner );
            free( tmpOwner );
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
        } else {
            //send 0
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
        //if 1, receive owner and send back ok
        if( retval == 1 ) {
            if (!mySock_->code( tmpOwner ) ||
            	!mySock_->end_of_message())
			{
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					pszFunction, __LINE__);
				if (tmpOwner != NULL)
				{
					free(tmpOwner);
				}
				return fail;
			}
            mySock_->encode();
            if( tmpOwner ) {
                retval = 1;
                setRemoteUser( tmpOwner );
				free(tmpOwner);
            } else {
                retval = 0;
            }
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
