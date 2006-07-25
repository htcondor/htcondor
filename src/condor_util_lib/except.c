/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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
#include "condor_syscall_mode.h"
#include "exit.h"
#include "condor_debug.h"

#ifdef LINT
/* VARARGS */
_EXCEPT_(foo)
char	*foo;
{ printf( foo ); }
#else /* LINT */

int		_EXCEPT_Line;
int		_EXCEPT_Errno;
char	*_EXCEPT_File;
int		(*_EXCEPT_Cleanup)(int,int,char*);
int		SetSyscalls(int);

extern int		_condor_dprintf_works;

void
_EXCEPT_(char *fmt, ...)
{
	va_list pvar;
	char buf[ BUFSIZ ];

	(void)SetSyscalls( SYS_LOCAL | SYS_RECORDED );
	va_start(pvar, fmt);

	vsprintf( buf, fmt, pvar );

	if( _condor_dprintf_works ) {
		dprintf( D_ALWAYS|D_FAILURE, "ERROR \"%s\" at line %d in file %s\n",
				 buf, _EXCEPT_Line, _EXCEPT_File );
	} else {
		fprintf( stderr, "ERROR \"%s\" at line %d in file %s\n",
				 buf, _EXCEPT_Line, _EXCEPT_File );
	}

	if( _EXCEPT_Cleanup ) {
		(*_EXCEPT_Cleanup)( _EXCEPT_Line, _EXCEPT_Errno, buf );
	}

	va_end(pvar);

	exit( JOB_EXCEPTION );
}
#endif /* LINT */
