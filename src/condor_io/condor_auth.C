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
#include "condor_auth.h"
#include "condor_string.h"

static const char root[] = "root";

Condor_Auth_Base :: Condor_Auth_Base(ReliSock * sock, int mode) : 
	mySock_        ( sock  ),
	authenticated_ ( false ),
	mode_          ( mode  ),
	isDaemon_      ( false ),
	remoteUser_    ( NULL  ),
	remoteDomain_  ( NULL  ),
	remoteHost_    ( NULL  ),
	localDomain_   ( NULL  ),
	fqu_           ( NULL  )
{
    //if (mySock_->isClient()) {
        
        //------------------------------------------
        // There are two possible cases:
        // 1. This is a user who is calling authentication
        // 2. This is a daemon who is calling authentication
        // 
        // So, we first find out whether this is a daemon or user
        //------------------------------------------
        
        //if ((strncmp(username,
        //            STR_DEFAULT_CONDOR_USER,
        //             strlen(STR_DEFAULT_CONDOR_USER)) == 0) ||
        //    (strncmp(username, root, strlen(root)) == 0)) {
            // I am a daemon! This is a daemon-daemon authentication
        //    isDaemon_ = true;
            
        //dprintf(D_ALWAYS,"This is a daemon with Condor uid:%d; my uid:%d, user uid :%d , 
        // with username %s\n", get_condor_uid(), get_my_uid(), get_user_uid(), username);
        //}
        if (get_my_uid() == 0) {
            isDaemon_ = true;
        }
        //}

		// this will *always* succeed
	localDomain_ = param( "UID_DOMAIN" );

    setRemoteHost(inet_ntoa(mySock_->endpoint()->sin_addr));
    // This is done for protocols such as fs, anonymous. Kerberos should
    // override this with the ip address from Kerbeos
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

    if (localDomain_) {
        free(localDomain_);
    }
    
    if (fqu_) {
        free(fqu_);
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

const char * Condor_Auth_Base :: getRemoteFQU()
{
    if (fqu_ == NULL) {
        int len = 0, userlen = 0, domlen = 0;
        if (remoteUser_) {
            userlen = strlen(remoteUser_);
            len += userlen;
        }
        if (remoteDomain_) {
            domlen  = strlen(remoteDomain_);
            len += domlen;
        }
        if ((len > 0) && remoteUser_) {
            fqu_ = (char *) malloc(len + 2);
            memset(fqu_, 0, len + 2);
            memcpy(fqu_, remoteUser_, userlen);
            if (remoteDomain_) {
                fqu_[userlen] = '@';
                memcpy(fqu_+userlen+1, remoteDomain_, domlen);
                fqu_[len+1] = 0;
            }
        }
    }

    return fqu_;
}

const char * Condor_Auth_Base :: getLocalDomain() const
{
    return localDomain_;
}
    
Condor_Auth_Base& Condor_Auth_Base :: setRemoteDomain(const char * domain)
{
    if (remoteDomain_) {
        free(remoteDomain_);
        remoteDomain_ = NULL;
    }

    if (domain) {
        // need to do some conversion here to use lower case domain
        // name only. This is because later on we hash on the user@domain
        remoteDomain_ = strdup(domain);
        char * at = remoteDomain_;
        while (*at != '\0') {
            *at = tolower((int) *at);
            at++;
        }
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

int Condor_Auth_Base :: endTime() const
{
    return -1;
}

Condor_Auth_Base :: Condor_Auth_Base()
{
}


