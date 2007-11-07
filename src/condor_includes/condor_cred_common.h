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
// Description:  Some of the commonly used defines are placed in this
//               file. This is mostly for credential related stuff.
//
// Author:       Hao Wang
// 
// $id:$
//----------------------------------------------------------------------

#ifndef CONDOR_CRED_COMMON_H
#define CONDOR_CRED_COMMON_H

enum Credential_t { 
  CONDOR_CRED_ALL=1,          // Default
  CONDOR_CRED_CLAIMTOBE=2,
  CONDOR_CRED_FILESYSTEM=4, 
  CONDOR_CRED_NTSSPI=8,
  CONDOR_CRED_GSS=16,
  CONDOR_CRED_FILESYSTEM_REMOTE=32,
  CONDOR_CRED_KERBEROS=64
  //, 32, 64, etc.
};

//------------------------------------------
// Commonly defined Strings
//------------------------------------------
const char STR_CONDOR_CRED_DIR[]         = "CONDOR_CREDENTIAL_DIR";
const char STR_CONDOR_DEFAULT_CRED_DIR[] = "/tmp";
const char STR_CONDOR_SPOOL_DIR[]        = "CONDOR_SPOOL";

//Kerberos related 
#define CONDOR_KERBEROS_FORWARD_TGT        10000
#define CONDOR_KERBEROS_RECEIVE_TGT        10001
#define CONDOR_KERBEROS_TGT_FAIL           10002
#define CONDOR_KRB_DIR_LENGTH              256

const char STR_CONDOR_KERBEROS_PREFIX[]  = "condor_krb5_tgt_%s_%d";
const char STR_CONDOR_KERBEROS_DEF_ERR[] = "Failed to get default kerberos credentail";

#endif
