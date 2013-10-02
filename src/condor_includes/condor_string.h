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

#ifndef _STRING
#define _STRING

#include "condor_header_features.h"
#include "util_lib_proto.h"

BEGIN_C_DECLS

DLL_IMPORT_MAGIC char* strupr( char *str );
DLL_IMPORT_MAGIC char* strlwr( char *str );
char * getline ( FILE *fp );

char * chomp( char *buffer );

END_C_DECLS


#ifdef __cplusplus
/* like strdup() but uses new[] */
char *strnewp( const char * );

#include "condor_arglist.h"
#endif

#include "basename.h"

#endif /* _STRING */
