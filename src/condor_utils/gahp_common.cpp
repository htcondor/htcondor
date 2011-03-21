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

#include "condor_common.h"
#include "gahp_common.h"


Gahp_Args::Gahp_Args()
{
	argv = NULL;
	argc = 0;
	argv_size = 0;
}

Gahp_Args::~Gahp_Args()
{
	reset();
}

/* Restore the object to its fresh, clean state. This means that argv is
 * completely freed and argc and argv_size are set to zero.
 */
void
Gahp_Args::reset()
{
	if ( argv == NULL ) {
		return;
	}

	for ( int i = 0; i < argc; i++ ) {
		free( argv[i] );
		argv[i] = NULL;
	}

	free( argv );
	argv = NULL;
	argv_size = 0;
	argc = 0;
}

/* Add an argument to the end of the args array. argv is extended
 * automatically if required. The string passed in becomes the property
 * of the Gahp_Args object, which will deallocate it with free(). Thus,
 * you would typically give add_arg() a strdup()ed string.
 */
void
Gahp_Args::add_arg( char *new_arg )
{
	if ( new_arg == NULL ) {
		return;
	}
	if ( argc >= argv_size ) {
		argv_size += 60;
		argv = (char **)realloc( argv, argv_size * sizeof(char *) );
	}
	argv[argc] = new_arg;
	argc++;
}
