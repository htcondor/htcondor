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

#ifndef _CONDOR_EMAIL_H
#define _CONDOR_EMAIL_H

#if defined(__cplusplus)
extern "C" {
#endif

FILE * email_open( const char *email_addr, const char *subject );

FILE * email_admin_open(const char *subject);

FILE * email_developers_open(const char *subject);

void email_close(FILE *mailer);

void email_asciifile_tail( FILE* mailer, const char* filename,
						   int lines );  

void email_corefile_tail( FILE* mailer, const char* subsystem_name );

#if defined(__cplusplus)
}
#endif

/* I, Alain Roy, moved the declaration for this down from where it was
 * above.  It's a bad move to include c++ header files within an
 * extern C.  */
#if defined(__cplusplus)
#include "condor_classad.h"
extern "C"{
FILE * email_user_open( ClassAd* jobAd, const char *subject );
}
#endif

#endif /* _CONDOR_EMAIL_H */
