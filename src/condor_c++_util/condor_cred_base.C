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

