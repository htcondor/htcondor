/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "debug.h"
#include "util.h"

//------------------------------------------------------------------------
int util_getline(FILE *fp, char *line, int max) {
  int c, i = 0;
    
  assert (EOF  == -1);
  assert (fp   != NULL);
  assert (line != NULL);
  assert (max  > 0);      /* Need at least 1 slot for '\0' character */

  for (c = getc(fp) ; c != '\n' && c != EOF ; c = getc(fp)) {
    if ( !(i == 0 && isspace(c)) && (i+1 < max)) line[i++] = c;
  }
  line[i] = '\0';
  return (i==0 && c==EOF) ? EOF : i;
}

//-----------------------------------------------------------------------------
int util_popen (const char * cmd) {
    FILE *fp;
    debug_printf( DEBUG_VERBOSE, "Running: %s\n", cmd);
    fp = popen (cmd, "r");
    int r;
    if (fp == NULL || (r = pclose(fp)) != 0) {
		debug_printf( DEBUG_QUIET, "WARNING: failure: %s\n", cmd );
		if( fp != NULL ) {
			debug_printf ( DEBUG_QUIET, "\t(pclose() returned %d)\n", r );
		}
    }
    return r;
}
