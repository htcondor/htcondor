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

#ifndef STORE_CRED_H
#define STORE_CRED_H

#ifdef WIN32
#include "condor_common.h"
#include "condor_io.h"


const int ADD_MODE = 0;
const int DELETE_MODE = 1;

const char ADD_CREDENTIAL[] = "add";
const char DELETE_CREDENTIAL[] = "delete";
const char CONFIG_CREDENTIAL[] = "config";

#define MAX_PASSWORD_LENGTH 255

void store_cred_handler(void *, int i, Stream *s);
int  store_cred(char *user, char* pw, int mode );

bool read_no_echo(char* buf, int maxlength);
char* get_password(void);
int addCredential(char* user, char* pw);
int deleteCredential(char* user); // not checking password before removal yet

#endif // WIN32
#endif // STORE_CRED_H



	
