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
