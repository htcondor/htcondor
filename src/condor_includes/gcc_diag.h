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

#ifndef GCC_DIAG_H

#ifdef __GNUC__
# if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#  define GCC_DIAG_STR(S) #S
#  define GCC_DIAG_JOINSTR(X,Y) GCC_DIAG_STR(X ## Y)
#  define GCC_DIAG_DO_PRAGMA(X) _Pragma (#X)
#  define GCC_DIAG_PRAGMA(X) GCC_DIAG_DO_PRAGMA(GCC diagnostic X)
#  if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#    define GCC_DIAG_OFF(X) GCC_DIAG_PRAGMA(push) \
        GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,X))
#    define GCC_DIAG_ON(X) GCC_DIAG_PRAGMA(pop)
#  else
#    define GCC_DIAG_OFF(X) GCC_DIAG_PRAGMA(ignored GCC_DIAG_JOINSTR(-W,X))
#    define GCC_DIAG_ON(X)  GCC_DIAG_PRAGMA(warning GCC_DIAG_JOINSTR(-W,X))
#  endif
# else
#  pragma GCC system_header // this turns off all warnings
#  define GCC_DIAG_OFF(X)
#  define GCC_DIAG_ON(X)
# endif
#else
# define GCC_DIAG_OFF(X)
# define GCC_DIAG_ON(X)
#endif

#define GCC_DIAG_H
#endif
