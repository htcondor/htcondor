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

#include "config.h"
#ifndef HAVE_GETPAGESIZE
 

#if !defined(Solaris)
#include <machine/param.h>
#else
#define NBPG 0x1000   /*couldnt get this val from header files. Rough
				estimate from Suns header files - Raghu */
#endif


/*
** Compatibility routine for systems which don't have this call.
*/
int getpagesize()
{
	return NBPG;
}

#endif