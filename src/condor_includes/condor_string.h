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
#ifndef _STRING
#define _STRING

#include "condor_header_features.h"
#include "util_lib_proto.h"

BEGIN_C_DECLS

#if defined(__STDC__) || (__cplusplus)
int mkargv ( int *argc, char *argv[], char *line );
void lower_case ( char *str );
char * getline ( FILE *fp );
char * ltrunc ( register char *str );
#else
int mkargv();
void lower_case();
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
