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



#ifndef CONDOR_KERBEROS_H
#define CONDRO_KERBEROS_H

#include "condor_cred_common.h"
#include "condor_cred_base.h"        // Base class

extern "C" {
#include "krb5.h"
}

//class KerberosHandle;
//----------------------------------------------------------------------
// class Condor_Kerberos
//----------------------------------------------------------------------
class Condor_Kerberos : 
public Condor_Credential_B {

 public:
  Condor_Kerberos();
  Condor_Kerberos(const Condor_Kerberos& copy);
  //------------------------------------------
  // PURPOSE: Create a kerberos object for local
  //          user
  // REQUIRE: NONE
  // RETURNS: NONE
  //------------------------------------------

  Condor_Kerberos(ReliSock * sock);
  //------------------------------------------
  // PURPOSE: Create a kerberos object for remote
  //          user
  // REQUIRE: valid socket
  // RETURNS: NONE
  //------------------------------------------
  ~Condor_Kerberos();
  //------------------------------------------
  // Destructor
  //------------------------------------------

  bool is_valid() const;
  //------------------------------------------
  // PURPOSE: To check to see if a credential
  //          is valid or not
  // REQUIRE: NONE
  // RETURNS: true  -- if credential is valid
  //          false -- otherwise
  //------------------------------------------

  bool forward_credential(ReliSock * sock);
  //------------------------------------------
  // PURPOSE: Each authentication system knows
  //          how to forward its type of 
  //          credentials.
  // REQUIRE: ReliSock
  // RETURNS: TRUE -- success; FALSE -- failure
  //------------------------------------------

  bool receive_credential(ReliSock * sock);
  //------------------------------------------
  // PURPOSE: Each authentication system knows
  //          how to received its own type of
  //          credential. This is a pure virtual method
  // REQUIRE: ReliSock
  // RETURNS: NONE
  //------------------------------------------

  //Condor_Credential_B& IPC_transfer_credential(int pid);
  // This probably will not work
  //------------------------------------------
  // PURPOSE: gss_api supports interprocess security context
  //          transfer
  // REQUIRE: receiver's pid
  // RETURNS: NONE
  //------------------------------------------
  // NOTE: After interprocess security transfer, the 
  //       security context becomes valid (since the 
  //       other process controls it).
  //------------------------------------------

  //Condor_Credential_B& IPC_receive_credential(int pid);
  // This probably will not work
  //------------------------------------------
  // PURPOSE: gss_api supports interprocess security context
  //          transfer
  // REQUIRE: sender's pid
  // RETURNS: NONE
  //------------------------------------------
 private:
  
  bool store_credential(krb5_creds * cred);
  //------------------------------------------
  // PURPOSE: Store the credential cache somewhere
  //          on the disk, regardless whether it was
  //          forwarded or retrieved locally
  // REQUIRE: NONE
  // RETURNS: NONE
  //------------------------------------------

  void condor_krb5_error(int retval, char * message);
  //------------------------------------------
  // PURPOSE: Log error messages
  // REQUIRE: retval and text message
  // RETURNS: None
  //------------------------------------------

  //------------------------------------------
  // Data
  //------------------------------------------
  //KerberosHandle * handleMap_;     // Map of the handles
  bool              forwarded_;      // Whether this is forwared or not
  krb5_context      context_;        // security context
  char *            ccname_;         // cache name
  //krb5_principal    principal_;      // Principal owner of the cache
};

#endif









