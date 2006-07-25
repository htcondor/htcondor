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

//----------------------------------------------------------------------
// Description:  This file contains definitions for condor credentail
//               management API.
// 
// $id:$
//----------------------------------------------------------------------

#ifndef CONDOR_CREDENTIAL_H
#define CONDOR_CREDENTIAL_H

#include "condor_cred_common.h"
#include "condor_cred_base.h"
#include "reli_sock.h"

class Condor_Cred_Map;          // Private to Condor_Credential_M
// If gcc supports namespace, this can be moved to a different file

class Condor_Credential_M
{
 public: 
  Condor_Credential_M(const char * userid);
  //------------------------------------------
  // PURPOSE: Constructor
  // REQUIRE: userid for which the manager is to be constructed
  //          userid could also be service name
  // RETURNS: NONE
  // NOTE   : A second parameter could be added
  //          to distinguish between user and service
  //------------------------------------------

  Condor_Credential_M(const char   * userid,
                      ReliSock     * sock);
  //------------------------------------------
  // PURPOSE: Constructor
  // REQUIRE: userid, sock
  // RETURNS: Construct a credential (forwarded)
  //          for a remote user
  //------------------------------------------

  ~Condor_Credential_M();
  //------------------------------------------
  // PURPOSE: Destructor
  // REQUIRE: NONE
  // RETURNS: NONE
  //------------------------------------------

  bool forward_credential(ReliSock   * sock,
			  Credential_t cred_type =  CONDOR_CRED_ALL);
  //------------------------------------------
  // PURPOSE: Forward all credentials belong to 
  //          the user
  // REQUIRE: sock      -- A valid socket to forward the credential to
  //          cred_type -- type of credential to be forwarded
  //          NOTE: ReliSock has to provide a interface to access
  //                the host information and service information
  //                i.e. which daemon are we talking to on which machine
  // RETURNS: TRUE -- if success
  //          FALSE -- if failure
  //------------------------------------------

  bool receive_credential(ReliSock   * sock, 
                          Credential_t cred_type =  CONDOR_CRED_ALL);
  //------------------------------------------
  // PURPOSE: Receive a set of credentials from
  //          a remote/local process
  // REQUIRE: sock   -- Socket to be used to receive the credential
  //          NOTE: ReliSock has to provide a interface to access
  //                the host information and service information
  //                i.e. which daemon are we talking to on which machine
  // RETURNS: TRUE -- if success
  //          FALSE -- if failure
  //------------------------------------------

  bool has_valid_credential(Credential_t cred_type) const;
  //------------------------------------------
  // PURPOSE: Check to see if it has any credential for a user
  // REQUIRE: NONE
  // RETURNS: true -- if user has any credential
  //                  on file with the manager
  //          false -- otherwise
  //------------------------------------------

  bool remove_user_cred(Credential_t type = CONDOR_CRED_ALL);
  //------------------------------------------
  // PURPOSE: Remove a user from the system
  // REQUIRE: Credential type or all -- default, probably dangeous
  // RETURNS: TRUE -- if success
  //          FALSE -- if failure
  //------------------------------------------

  bool add_user_cred(Credential_t type = CONDOR_CRED_ALL);
  bool add_user_cred(Condor_Credential_B ** newCred);
  //------------------------------------------
  // PURPOSE: Different from receiving credentials, this call
  //          will actually read/retrieve credentials for a 
  //          user/daemon
  // REQUIRE: credential type, default is all.
  // RETURNS: TRUE -- if success
  //          FALSE -- if failure
  //------------------------------------------

  bool add_service_cred(Credential_t type);
  //------------------------------------------
  // PURPOSE: Similar to add_user_cred. This one is for service
  //          will actually read/retrieve credentials for a 
  //          user/daemon
  // REQUIRE: credential type, default is all.
  // RETURNS: TRUE -- if success
  //          FALSE -- if failure
  //------------------------------------------
 private:
  
  Condor_Credential_B * create_new_cred(Credential_t type);
  //------------------------------------------
  // PURPOSE: Create a corresponding credential
  // REQUIRE: Credential type
  //          ReliSock -- if the credential is created from a socket
  // RETURNS: Condor_Credential_B object
  //------------------------------------------

  bool forward_all(ReliSock  * sock = NULL);
  //------------------------------------------
  // PURPOSE: Create a corresponding credential
  // REQUIRE: Credential type
  //          ReliSock -- if the credential is created from a socket
  // RETURNS: Condor_Credential_B object
  //------------------------------------------

  //------------------------------------------
  // Data
  //------------------------------------------

  char *                       userid_;    // user identity
  Condor_Cred_Map *            credMap_;   // Contains actual credentials
};

#endif










