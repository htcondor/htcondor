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












