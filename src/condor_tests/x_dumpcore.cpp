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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#pragma warning(disable: 6011) // deref of null pointer warning.
#endif

int main( int , char ** /*argv*/ )
{
	char *null = NULL;
	char *space;
#ifndef WIN32
	int dumpnow = atoi("0");
#endif

	space = (char *) malloc(600000);
	memset(space, '\0', 600000);

	/* try a couple of different ways to dump core. */
#ifndef WIN32
	dumpnow = 7 / dumpnow;
#endif
	*null = space[100]; // just to use space

	return 0;
}
