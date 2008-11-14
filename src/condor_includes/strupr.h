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


#ifndef _CONDOR_STRUPR_H
#define _CONDOR_STRUPR_H

#include "condor_header_features.h"

BEGIN_C_DECLS

#ifndef WIN32	/* on Win32, these come out of libc */
char* strupr( char * );
char* strlwr( char * );
#endif

END_C_DECLS

#endif /* _CONDOR_STRUPR_H */
