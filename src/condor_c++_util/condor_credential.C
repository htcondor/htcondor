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

//----------------------------------------------------------------------
// Author: Hao Wang
// $id:$
//----------------------------------------------------------------------

#include "condor_credential.h"
#ifdef KERBEROS_AUTHENTICATION
#include "condor_cred_kerberos.h"     // Condor_Kerberos
#endif
#include "HashTable.h"                // Condor HashTable
//----------------------------------------------------------------------
// Condor_Cred_Map
//----------------------------------------------------------------------
typedef HashTable<Credential_t, Condor_Credential_B *> CredentialHash;

//------------------------------------------
// Hash function need by the hash table
//------------------------------------------
int hashFunction(const Credential_t & key, int bucket)
{
  return key%bucket;
}

class Condor_Cred_Map 
{
  friend class Condor_Credential_M;

public: 
  Condor_Cred_Map();
  ~Condor_Cred_Map();
  Condor_Credential_B * find(Credential_t type);
  //------------------------------------------
  // PURPOSE: To see if a credential exist
  // REQUIRE: Credential type
  // RETURNS: NONE
  //------------------------------------------

  Condor_Cred_Map& add(Condor_Credential_B * cred);
  //------------------------------------------
  // PURPOSE: Add credential to the Map
  // REQUIRE: Credential to be added
  // RETURNS: NONE
  //------------------------------------------

  Condor_Cred_Map& remove(Credential_t type);
  //------------------------------------------
  // PURPOSE: Remove a usr credential from the map 
  // REQUIRE: Credential_t
  // RETURNS: NONE
  //------------------------------------------
  
  bool hasValidCredential(Credential_t type);
  //------------------------------------------
  // PURPOSE: To check to see if the map contains a 
  //          particular credential
  // REQUIRE: Credential_t
  // RETURNS: NONE
  //------------------------------------------
private:
  
  CredentialHash         hash_;       // Hash table
};

Condor_Cred_Map::Condor_Cred_Map()
  : hash_(CredentialHash(5, &hashFunction))
{
}

Condor_Cred_Map::~Condor_Cred_Map()
{
}

Condor_Credential_B * Condor_Cred_Map :: find(Credential_t type)
{
  Condor_Credential_B * cred = NULL;
  hash_.lookup(type, cred);

  return cred;
}

Condor_Cred_Map& Condor_Cred_Map :: add(Condor_Credential_B * cred)
{
  remove(cred->credential_type());
  hash_.insert(cred->credential_type(), cred);
  return *this;
}

Condor_Cred_Map& Condor_Cred_Map :: remove(Credential_t type)
{
  hash_.remove(type);
  return *this;
}

bool Condor_Cred_Map :: hasValidCredential(Credential_t type)
{
  bool isValid = FALSE;

  Condor_Credential_B * cred = NULL;
  if ((cred = find(type))) {
    isValid = cred->is_valid();
  }

  return isValid;
}

//----------------------------------------------------------------------
// Condor_Credential_B
//----------------------------------------------------------------------

Condor_Credential_M :: Condor_Credential_M(const char * userid)
    : userid_(NULL),
      credMap_(new Condor_Cred_Map())
{
    userid_ = new char(strlen(userid));
    strncpy(userid_, userid, strlen(userid));
}

Condor_Credential_M :: Condor_Credential_M(const char   * userid,
					   ReliSock     * sock)
  : userid_ (NULL),
    credMap_(new Condor_Cred_Map())

{
}

Condor_Credential_M :: ~Condor_Credential_M()
{
  delete userid_;
  delete credMap_;
}


bool Condor_Credential_M :: forward_credential(ReliSock   * sock,
					       Credential_t cred_type)
{
  bool forwarded = FALSE;

  if (cred_type == CONDOR_CRED_ALL) {
    forwarded = forward_all(sock);
  }
  else {
    Condor_Credential_B * cred = credMap_->find(cred_type);
    if (cred) {
       forwarded = cred->forward_credential(sock);
    }
  }
  return forwarded;
}

bool Condor_Credential_M :: forward_all(ReliSock * sock)
{
  bool             forwarded = TRUE;
  Condor_Credential_B * cred = NULL;

  // Start iterating through the list
  credMap_->hash_.startIterations();
  
  while(credMap_->hash_.iterate(cred)) {
    forwarded = cred->forward_credential(sock) & forwarded;
  }

  return forwarded;
}


bool Condor_Credential_M :: receive_credential(ReliSock   * sock,
                                               Credential_t type)
{
  Condor_Credential_B * cred = NULL;

  if (credMap_->find(type)) {
    credMap_->remove(type);
  }

  cred = create_new_cred(type);
  cred->receive_credential(sock);
  credMap_->add(cred);

  return TRUE; // Add exception catching
}

bool Condor_Credential_M :: has_valid_credential(Credential_t cred_type) const
{
  return credMap_->hasValidCredential(cred_type);
}

bool Condor_Credential_M :: remove_user_cred(Credential_t type)
{
  credMap_->remove(type);
  
  return TRUE;
}

bool Condor_Credential_M :: add_user_cred(Condor_Credential_B ** newCred)
{
    assert(*newCred);
    credMap_->remove((*newCred)->credential_type());
    credMap_->add((*newCred));
    *newCred = NULL;

    return TRUE;
}

bool Condor_Credential_M :: add_user_cred(Credential_t type)
{
  // Should we delete the old one first?
  credMap_->remove(type);
  // Add the new one
  credMap_->add(create_new_cred(type));

  return TRUE;
}

bool Condor_Credential_M :: add_service_cred(Credential_t type)
{
  // right now, same as add_user_cred
  return add_user_cred(type);
}

Condor_Credential_B * Condor_Credential_M :: create_new_cred(Credential_t type)
{
  Condor_Credential_B * newCred = NULL;
  
  switch (type) {
  case CONDOR_CRED_CLAIMTOBE:
      // newCred = new Condor_ClaimToBe();
    break;
  case CONDOR_CRED_FILESYSTEM:
    break;
  case CONDOR_CRED_NTSSPI:
    break;
  case CONDOR_CRED_GSS:
    break;
  case CONDOR_CRED_FILESYSTEM_REMOTE:
    break;
#ifdef KERBEROS_AUTHENTICATION
  case CONDOR_CRED_KERBEROS:
      newCred = new Condor_Kerberos();
    break;
#endif
  default:
    break;
  }
  return newCred;
}







