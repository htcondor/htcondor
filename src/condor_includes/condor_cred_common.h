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
