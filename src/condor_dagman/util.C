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

#include "condor_common.h"   /* for <ctype.h>, <assert.h> */
#include "condor_debug.h"
#include "debug.h"
#include "util.h"

extern "C" {

//------------------------------------------------------------------------
int util_getline(FILE *fp, char *line, int max) {
  int c, i = 0;
    
  ASSERT( EOF  == -1 );
  ASSERT( fp   != NULL );
  ASSERT( line != NULL );
  ASSERT( max  > 0 );      /* Need at least 1 slot for '\0' character */

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


}
