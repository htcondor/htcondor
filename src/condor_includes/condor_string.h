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
#ifndef _STRING
#define _STRING

#include "condor_header_features.h"
#include "util_lib_proto.h"

BEGIN_C_DECLS

#if defined(__STDC__) || (__cplusplus)
int mkargv ( int *argc, char *argv[], char *line );
char* strupr( char *str );
char* strlwr( char *str );
char * getline ( FILE *fp );
char * ltrunc ( register char *str );
#else
int mkargv();
char* strupr();
char* strlwr();
char * getline();
char * ltrunc ();
#endif

char * chomp( char *buffer );

END_C_DECLS


#ifdef __cplusplus
/* like strdup() but uses new[] */
char *strnewp( const char * );
#endif

#include "basename.h"

#endif /* _STRING */
