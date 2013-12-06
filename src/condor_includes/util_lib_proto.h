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


#ifndef __UTIL_LIB_PROTO_H
#define __UTIL_LIB_PROTO_H

#include "condor_config.h"
#include "condor_getmnt.h"
#include "condor_types.h"
#include "condor_header_features.h"

#if defined(__cplusplus)
extern "C" {
#endif

int blankline ( const char *str );

char * getline ( FILE *fp );

char* getExecPath( void );

int rotate_file(const char *old_filename, const char *new_filename);

int rotate_file_dprintf(const char *old_filename, const char *new_filename, int calledByDprintf);

/// If new_filename exists, overwrite it.
int copy_file(const char *old_filename, const char *new_filename);

int hardlink_or_copy_file(const char *old_filename, const char *new_filename);

void detach ( void );
int do_connect ( const char *host, const char *service, u_short port );
int udp_connect ( char *host, u_short port );
void dprintf ( int flags, const char* fmt, ... ) CHECK_PRINTF_FORMAT(2,3);
void _EXCEPT_ ( const char *fmt, ... ) CHECK_PRINTF_FORMAT(1,2);
void condor_except_should_dump_core( int flag );
int getdtablesize ( void );

int mkargv ( int *argc, char *argv[], char *line );
char * format_time ( float fp_secs );
void update_rusage( register struct rusage *ru1, register struct rusage *ru2 );

#if defined(__cplusplus)
}		/* End of extern "C" declaration */
#endif

#endif
