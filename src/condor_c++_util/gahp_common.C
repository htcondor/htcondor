/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
