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

//----------------------------------------------------------------------
// Author: Hao Wang
// $id:$
//----------------------------------------------------------------------

#include "condor_cred_base.h"
#include "condor_common.h"
#include "condor_config.h"

Condor_Credential_B :: Condor_Credential_B(Credential_t type)
  : type_ (type)
{
}

Condor_Credential_B :: Condor_Credential_B(const Condor_Credential_B& copy)
  : type_ (copy.type_)
{
}

Credential_t Condor_Credential_B :: credential_type() const
{
  return type_;
}

Condor_Credential_B :: ~Condor_Credential_B()
{
}

bool Condor_Credential_B :: Condor_set_credential_env()
{
  // Does not do anything in the base class
  // If you want to set some environment variable
  // please over write this method in your derived class
  return TRUE;
}

char * Condor_Credential_B :: condor_cred_dir()
{
  char * cred_dir = NULL;

  // Check for Condor environment variable 
  cred_dir = param( STR_CONDOR_CRED_DIR );
  
  if (cred_dir == 0) {
    // CONDOR_CREDENTIAL_DIR does not exist,
    // use spool dir
    cred_dir = param ( STR_CONDOR_SPOOL_DIR );
    
    if (cred_dir == 0) {
      // Shouldn't happen! If so, let's use /tmp instead
      cred_dir = strdup(STR_CONDOR_DEFAULT_CRED_DIR);
    }
  }
  
  return cred_dir;
}

