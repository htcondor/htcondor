/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 


#include <stdio.h>
#include <stdlib.h>
#include <varargs.h>
#include <time.h>
#include "condor_sys.h"
#include "exit.h"
#include "debug.h"

#ifdef LINT
/* VARARGS */
_EXCEPT_(foo)
char	*foo;
{ printf( foo ); }
#else LINT

extern int	sys_nerr;
extern char	*sys_errlist[];

extern int	condor_nerr;
extern char	*condor_errlist[];

int		_EXCEPT_Line;
int		_EXCEPT_Errno;
char	*_EXCEPT_File;
int		(*_EXCEPT_Cleanup)();
int		SetSyscalls(int);
void	dprintf ( int flags, char *fmt, ... );

void
_EXCEPT_(va_alist)
va_dcl
{
	va_list pvar;
	char *fmt;
	char buf[ BUFSIZ ];

	(void)SetSyscalls( SYS_LOCAL | SYS_RECORDED );
	va_start(pvar);

	fmt = va_arg(pvar, char *);

#if vax || (i386 && !LINUX && !defined(Solaris)) || bobcat || ibm032
	{
		FILE _strbuf;
		int *argaddr = &va_arg(pvar, int);

		_strbuf._flag = _IOWRT/*+_IOSTRG*/;
		_strbuf._ptr  = buf;
		_strbuf._cnt  = BUFSIZ;
		_doprnt( fmt, argaddr, &_strbuf );
		putc('\0', &_strbuf);
	}
#else vax || (i386 && !LINUX && !defined(Solaris)) || bobcat || ibm032
	vsprintf( buf, fmt, pvar );
#endif vax || (i386 && !LINUXi && !defined(Solaris)) || bobcat || ibm032

	if( _EXCEPT_Errno < 0 ) {
		_EXCEPT_Errno = -_EXCEPT_Errno;
		dprintf( D_ALWAYS, "ERROR \"%s\" at line %d in file %s: %s\n",
					buf, _EXCEPT_Line, _EXCEPT_File,
					(_EXCEPT_Errno<condor_nerr) ? condor_errlist[_EXCEPT_Errno]
												 : "Unknown CONDOR error" );
	} else {
		dprintf( D_ALWAYS, "ERROR \"%s\" at line %d in file %s: %s\n",
						buf, _EXCEPT_Line, _EXCEPT_File,
						(_EXCEPT_Errno<sys_nerr) ? sys_errlist[_EXCEPT_Errno]
												 : "Unknown error" );
	}

	if( _EXCEPT_Cleanup ) {
		(*_EXCEPT_Cleanup)( _EXCEPT_Line, _EXCEPT_Errno );
	}

	va_end(pvar);

	exit( JOB_EXCEPTION );
}
#endif LINT
