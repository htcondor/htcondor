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
