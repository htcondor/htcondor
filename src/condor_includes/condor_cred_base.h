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

#ifndef CONDOR_CRED_BASE_H
#define CONDOR_CRED_BASE_H

#include "condor_cred_common.h"
#include "reli_sock.h"

class Condor_Credential_B
{
 public:
  Condor_Credential_B(Credential_t type);
  //------------------------------------------
  // PURPOSE: Constructor
  // REQUIRE: type -- type of authentication, Kerberos, X509, AFS, etc
  // RETURNS: NONE
  //------------------------------------------

  Condor_Credential_B(const Condor_Credential_B& copy);
  //------------------------------------------
  // PURPOSE: virtual Copy Constructor
  // REQUIRE: copy -- the original
  // RETURNS: NONE
  //------------------------------------------

  virtual ~Condor_Credential_B();
  //------------------------------------------
  // PURPOSE: Pure virtual destructor
  // REQUIRE: NONE
  // RETURNS: NONE
  //------------------------------------------

  Credential_t credential_type() const;
  //------------------------------------------
  // PURPOSE: Returns the type of credential
  // REQUIRE: NONE
  // RETURNS: a valid credential type
  //------------------------------------------

  virtual bool is_valid() const = 0;
  //------------------------------------------
  // PURPOSE: To check to see if a credential
  //          is valid or not
  // REQUIRE: NONE
  // RETURNS: true  -- if credential is valid
  //          false -- otherwise
  //------------------------------------------

  virtual bool forward_credential(ReliSock * sock) = 0;
  //------------------------------------------
  // PURPOSE: Each authentication system knows
  //          how to forward its type of 
  //          credentials. This is a pure virtusl method
  // REQUIRE: ReliSock
  // RETURNS: NONE
  //------------------------------------------

  virtual bool receive_credential(ReliSock * sock) = 0;
  //------------------------------------------
  // PURPOSE: Each authentication system knows
  //          how to received its own type of
  //          credential. This is a pure virtual method
  // REQUIRE: ReliSock
  // RETURNS: NONE
  //------------------------------------------

  virtual bool Condor_set_credential_env();
  //------------------------------------------
  // PURPOSE: Set up necessary environment variables
  // REQUIRE: None
  // RETURNS: NONE
  // NOTICE:  This is not a pure virtual method, it's implementation
  //          specific. If your derived class needs to set up something
  //          then overwrite it.
  //------------------------------------------

  //virtual Condor_Credential_B& IPC_transfer_credential(int pid);
  //------------------------------------------
  // PURPOSE: Some authentication method such as Kerberos
  //          supports interprocess credential transfer. This method is
  //          provided to support this feature
  // REQUIRE: pid ? -- receiver's pid
  // RETURNS: NONE
  //------------------------------------------

  //virtual Condor_Credential_B& IPC_receive_credential(int pid);
  //------------------------------------------
  // PURPOSE: Some authentication method such as Kerberos
  //          supports interprocess credential transfer. This method is
  //          provided to support this feature
  // REQUIRE: pid ? -- sender's pid
  // RETURNS: NONE
  //------------------------------------------

 protected:
  char * condor_cred_dir();
  //------------------------------------------
  // PURPOSE: Get the default credential directory
  // REQUIRE: NONE
  // RETURNS: char * points to the default credential dir
  //          DO NOT FREE it!
  // NOTE: I will change it to return new char * if required
  //------------------------------------------

  //void *      cred_data_;            // Private data
  //------------------------------------------
  // cred_data_ is local to each type of 
  // authentication system. Should be used
  // by derived classes to store their 
  // own data. This data probably should be
  // placed in derived class instead.
  //------------------------------------------
 private:

  Condor_Credential_B();
  //------------------------------------------
  // PURPOSE: Private constructor
  // REQUIRE: None
  // RETUNRS: NONE
  //------------------------------------------

  //------------------------------------------
  // Data
  //------------------------------------------

  Credential_t               type_;         // Credential type
};

#endif












