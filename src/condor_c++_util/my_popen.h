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


#ifndef __MY_POPEN__
#define __MY_POPEN__

BEGIN_C_DECLS

FILE *my_popenv( char *const argv[],
                 const char * mode,
                 int want_stderr );
int my_pclose( FILE *fp );

int my_systemv( char *const argv[] );

int my_spawnl( const char* cmd, ... );
int my_spawnv( const char* cmd, char *const argv[] );

#if defined(WIN32)
// on Windows, expose the ability to use a raw command line
FILE *my_popen( const char *cmd, const char *mode, int want_stderr );
int my_system( const char *cmd );
#endif

END_C_DECLS

#if defined(__cplusplus)

// ArgList versions only available from C++
#include "condor_arglist.h"
FILE *my_popen( ArgList &args,
                const char * mode,
                int want_stderr );
int my_system( ArgList &args );

// PrivSep version
#if !defined(WIN32)
FILE *privsep_popen( ArgList &args,
                     const char * mode,
                     int want_stderr,
                     uid_t uid );
#endif

#endif

#endif
