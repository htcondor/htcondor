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


#ifndef __CLASSAD_FEATURES_H_
#define __CLASSAD_FEATURES_H_

#define COLLECTIONS
/*#define EXPERIMENTAL*/

#ifndef CLASSAD_DISTRIBUTION
#define CLASSAD_DEPRECATED
#endif

/*#define ENABLE_SHARED_LIBRARY_FUNCTIONS*/

/* Select how we wish to do regular expressions. 
   USE_POSIX_REGEX will use the regcomp/regexec functions
   USE_PCRE will use the PCRE library
*/ 
#define USE_POSIX_REGEX 
/* #define USE_PCRE */

#endif 
