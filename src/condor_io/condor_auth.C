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
#include "condor_auth.h"
#include "condor_string.h"

static const char root[] = "root";

Condor_Auth_Base :: Condor_Auth_Base(ReliSock * sock, int mode)
    : mode_          ( mode  ),
      authenticated_ ( false ),
      remoteUser_    ( NULL  ),
      remoteDomain_  ( NULL  ),
      remoteHost_    ( NULL  ),
      isDaemon_      ( false ),
      mySock_        ( sock  )
{
    char * username = my_username();
    
    if (mySock_->isClient()) {
        
        //------------------------------------------
        // There are two possible cases:
        // 1. This is a user who is calling authentication
        // 2. This is a daemon who is calling authentication
        // 
        // So, we first find out whether this is a daemon or user
        //------------------------------------------
        
        if ((strncmp(username,
                     STR_DEFAULT_CONDOR_USER,
                     strlen(STR_DEFAULT_CONDOR_USER)) == 0) ||
            (strncmp(username, root, strlen(root)) == 0)) {
            // I am a daemon! This is a daemon-daemon authentication
            isDaemon_ = true;
            
            //fprintf(stderr,"This is a daemon with Condor uid:%d; uid:%d\n",
            //      get_condor_uid(), get_my_uid());
        }
    }
  
    free(username);

    setRemoteHost(inet_ntoa(mySock_->endpoint()->sin_addr));
}

Condor_Auth_Base :: ~Condor_Auth_Base()
{
    if (remoteUser_) {
        free(remoteUser_);
    }

    if (remoteDomain_) {
        free(remoteDomain_);
    }

    if (remoteHost_) {
        free(remoteHost_);
    }
}

int Condor_Auth_Base :: wrap(char *   input, 
                             int      input_len, 
                             char*&   output, 
                             int&     output_len)
{
    // By default, all wrap does is copying the buffer to a newly
    // allocated buffer of the same size as the input. The derived
    // class should override this method if it supports
    // encryption/decryption
    output_len = input_len;
    output = (char *) malloc(output_len);

    memcpy(output, input, output_len);

    return 1;
}
    
int Condor_Auth_Base :: unwrap(char*   input, 
                               int     input_len, 
                               char*&  output, 
                               int&    output_len)
{
    // By default, all unwrap does is copying the buffer to a newly
    // allocated buffer of the same size as the input. The derived
    // class should override this method if it supports
    // encryption/decryption
    output_len = input_len;
    output = (char *) malloc(output_len);

    memcpy(output, input, output_len);

    return 1;
}

const char * Condor_Auth_Base :: getRemoteHost() const
{
    return remoteHost_;
}

const int Condor_Auth_Base :: getMode() const
{
    return mode_;
}
    
const int Condor_Auth_Base :: isAuthenticated() const
{
    return authenticated_;
}

const char * Condor_Auth_Base :: getRemoteUser() const
{
    return remoteUser_;
}

const char * Condor_Auth_Base :: getRemoteDomain() const
{
    return remoteDomain_;
}

    
Condor_Auth_Base& Condor_Auth_Base :: setRemoteDomain(const char * domain)
{
    if (remoteDomain_) {
        free(remoteDomain_);
        remoteDomain_ = NULL;
    }

    if (domain) {
        remoteDomain_ = strdup(domain);
    }

    return *this;
}

Condor_Auth_Base& Condor_Auth_Base :: setRemoteHost(const char * hostAddr)
{
    if (remoteHost_) {
        free(remoteHost_);
        remoteHost_ = NULL;
    }
    
    if (hostAddr) {
        remoteHost_ = strdup(hostAddr);
    }

    return *this;
}

Condor_Auth_Base& Condor_Auth_Base :: setAuthenticated(int authenticated)
{
    authenticated_ = authenticated;
    return *this;
}

Condor_Auth_Base& Condor_Auth_Base :: setRemoteUser( const char *owner ) 
{
    if ( remoteUser_ ) {
        free(remoteUser_);
        remoteUser_ = NULL;
    }
    if ( owner ) {
        remoteUser_ = strdup( owner );   // strdup uses new
    }
    return *this;
}

const bool Condor_Auth_Base :: isDaemon() const
{
    return isDaemon_;
}

Condor_Auth_Base :: Condor_Auth_Base()
{
}


