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


#include "condor_header_features.h"

#define   LOCK_SH   1<<0
#define   LOCK_EX   1<<1
#define   LOCK_NB   1<<2
#define   LOCK_UN   1<<3

/* Solaris specific -- found at in sys/file.h in OSF1 ..dhaval 6/23 */

#define L_XTND          2       /* relative to end of file */

/* Also need a prototype, otherwise C++ gets confused by
   the definition of a struct flock, and thinks flock() is
   a constructor for a class by the same name.  */

BEGIN_C_DECLS
extern int flock(int fd, int op);
END_C_DECLS
