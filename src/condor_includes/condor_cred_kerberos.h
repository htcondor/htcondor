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









