/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 
#include "condor_common.h"
#include "condor_syscall_mode.h"
#include "exit.h"
#include "debug.h"

void add_error_string( const char *str );
char * strdup( const char *str );

PERM_ERR(char* fmt, ...)
{
	va_list pvar;
	char buf[ BUFSIZ ];
	char err_str[ BUFSIZ ];

	(void)SetSyscalls( SYS_LOCAL | SYS_RECORDED );
	va_start(pvar, fmt);

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
