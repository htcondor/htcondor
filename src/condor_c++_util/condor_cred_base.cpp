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

