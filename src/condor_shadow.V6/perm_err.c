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
#include <varargs.h>
#include <time.h>
#include "condor_sys.h"
#include "exit.h"
#include "debug.h"


extern int	errno;
extern int	sys_nerr;
extern char	*sys_errlist[];

void add_error_string( const char *str );
char * strdup( const char *str );

PERM_ERR(va_alist)
va_dcl
{
	va_list pvar;
	char *fmt;
	char buf[ BUFSIZ ];
	char err_str[ BUFSIZ ];

	(void)SetSyscalls( SYS_LOCAL | SYS_RECORDED );
	va_start(pvar);

	fmt = va_arg(pvar, char *);

	vsprintf( buf, fmt, pvar );

	if( errno ) {
		sprintf( err_str, "ERROR: %s: %s\n",
			buf, (errno<sys_nerr) ? sys_errlist[errno] : "Unknown error" );
	} else {
		sprintf( err_str, "ERROR: %s\n", buf );
	}

	dprintf( D_ALWAYS, err_str );
	add_error_string( err_str );

	va_end(pvar);
}

#define MAX_ERROR 512
static char	*Error[MAX_ERROR];
static int	N_Errors=0;

void
add_error_string( const char *str )
{
	if( N_Errors >= MAX_ERROR ) {
		return;
	}
	Error[ N_Errors++ ] = strdup(str);
}

void
display_errors( FILE *fp )
{
	int		i;

	for( i=0; i<N_Errors; i++ ) {
		if (Error[i] != NULL)
			fprintf( fp, Error[i] );
	}
	if( N_Errors > 0 ) {
		fprintf( fp, "\n" );
	}
}
