/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"

#if !defined(SKIP_AUTHENTICATION)

#include "condor_auth_claim.h"

Condor_Auth_Claim :: Condor_Auth_Claim(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_CLAIMTOBE)
{
}

Condor_Auth_Claim :: ~Condor_Auth_Claim()
{
}

int Condor_Auth_Claim :: authenticate(const char * remoteHost)
{
#ifdef WIN32
    const char * __FUNCTION__ = "Condor_Auth_Claim :: authenticate";
#endif 

	int retval = 0;
    char *tmpOwner = NULL;
	int fail = 0;
    
    if ( mySock_->isClient() ) {
        mySock_->encode();
        if ( tmpOwner = my_username() ) {
            //send 1 and then username
            retval = 1;
            if (!mySock_->code( retval ) || 
            	!mySock_->code( tmpOwner ))
			{ 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					__FUNCTION__, __LINE__);
				free(tmpOwner);
				return fail; 
			}
            setRemoteUser( tmpOwner );
            free( tmpOwner );
            if (!mySock_->end_of_message()) { 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					__FUNCTION__, __LINE__);
				return fail;
			}
            mySock_->decode();
            if (!mySock_->code( retval )) {
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					__FUNCTION__, __LINE__);
				return fail; 
			}
        } else {
            //send 0
            if (!mySock_->code( retval )) { 
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					__FUNCTION__, __LINE__);
				return fail; 
			}
        }
    } else { //server side
        mySock_->decode();
        if (!mySock_->code( retval )) { 
			dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
				__FUNCTION__, __LINE__);
			return fail; 
		}
        //if 1, receive owner and send back ok
        if( retval == 1 ) {
            if (!mySock_->code( tmpOwner ) ||
            	!mySock_->end_of_message())
			{
				dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
					__FUNCTION__, __LINE__);
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
					__FUNCTION__, __LINE__);
				return fail;
			}
        }
    }
    
    if (!mySock_->end_of_message()) { 
		dprintf(D_SECURITY, "Protocol failure at %s, %d!\n", 
			__FUNCTION__, __LINE__);
		return fail; 
	}
    return retval;
}

int Condor_Auth_Claim :: isValid() const
{
    return TRUE;
}

#endif
