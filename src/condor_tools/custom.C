/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Litzkow
**
*/ 

#define _POSIX_SOURCE

#include "custom.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>


char * copy( const char *str );
int is_empty( const char *str );
mode_t copy_mode( char *path );

Macro::Macro( const char *n, const char *p, const char *d ) 
{
	name = copy( n );
	prompt = copy( p );
	dflt = copy( d );
	value = 0;
}

void
Macro::init( int (*f)(const char *) )
{
	char	buf[512];

	for(;;) {
		if( dflt ) {
			printf( "%s:\n[%s] ", prompt, dflt );	
		} else {
			printf( "%s:", prompt );	
		}
		fgets( buf, sizeof(buf), stdin );
		if( is_empty(buf) ) {
			value = copy( dflt );
		} else {
			value = copy( strtok(buf," \t\n") );
		}

		if( !f ) {
			break;
		}

		if( f(value) || confirm("Use anyway? ") ) {
			break;
		}
	}
	printf( "\n" );
}

void
Macro::init( int truth )
{
	if( truth ) {
		value = copy( "TRUE" );
	} else {
		value =copy( "FALSE" );
	}
}

char *
Macro::gen( int is_shell_cmd )
{
	static char answer[ 512 ];

	if( is_shell_cmd ) {
		sprintf( answer, "set %s = %s", name, value );
	} else {
		sprintf( answer, "%s = %s", name, value );
	}
	return answer;
}

/*
  Ask the user about something (prompt), and then make him say "yes",
  or "no".  Return true if the answer is "yes", and false otherwise.
*/
int
confirm( const char *prompt )
{
	char	buf[512];
	char	*answer;

	printf( prompt );

	for(;;) {
		fgets( buf, sizeof(buf), stdin );
		answer = strtok( buf, " " );
		if( *answer == 'y' || *answer == 'Y' ) {
			return TRUE;
		}
		if( *answer == 'n' || *answer == 'N' ) {
			return FALSE;
		}
		printf( "Please answer \"yes\" or \"no\": " );
	}
}


/*
  Allocate memory and make a copy of the given string.  Returns NULL
  if the given string is NULL.
*/
char *
copy( const char *str )
{
	char	*answer;

	if( str ) {
		answer = new char[ strlen(str) + 1 ];
		strcpy( answer, str );
	} else {
		answer = 0;
	}
	return answer;
}

/*
  Return true if the given string is nothing but white space, and false
  otherwise.
*/
int
is_empty( const char *str )
{
	const char *ptr;

	for( ptr=str; *ptr; ptr++ ) {
		if( !isspace(*ptr) ) {
			return FALSE;
		}
	}
	return TRUE;
}

ConfigFile::ConfigFile( const char *d )
{
	dir = new char[ strlen(d) + 1 ];
	strcpy( dir, d );
	src = NULL;
	dst = NULL;
}

void
ConfigFile::begin( const char *name )
{
	char src_name[_POSIX_PATH_MAX];
	char dst_name[_POSIX_PATH_MAX];
	int	fd;
	mode_t creat_mode;

	sprintf( src_name, "%s/%s.generic", dir, name );
	sprintf( dst_name, "%s/%s", dir, name );

	if( (src=fopen(src_name,"r")) == NULL ) {
		perror( src_name );
		exit( 1 );
	}

	creat_mode = copy_mode( src_name );
	if( (fd=open(dst_name,O_WRONLY|O_TRUNC|O_CREAT,creat_mode)) < 0 ) {
		perror( dst_name );
		exit( 1 );
	}
	if( (dst=fdopen(fd,"w")) == NULL ) {
		perror( dst_name );
		exit( 1 );
	}

	printf( "Customizing \"%s\"...", dst_name );
	fflush( stdout );

}

void
ConfigFile::end()
{
	char	buf[512];

	fprintf( dst, "\n" );
	while( fgets(buf,sizeof(buf),src) ) {
		fputs( buf, dst );
	}

	fclose( src );
	src = 0;
	fclose( dst );
	dst = 0;
	printf( " Done\n" );
}

void
ConfigFile::add_macro( const char *def )
{
	fprintf( dst, "%s\n", def );
}

/*
  Return the 3 sets of permissions (user,group,other) for a file as a
  mode_t - suitable for creating another file with the same permissions.
*/
mode_t
copy_mode( char *path )
{
	struct stat buf;
	mode_t	answer = 0;

	if( stat(path,&buf) < 0 ) {
		perror( path );
		exit( 1 );
	}

	answer = buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	return answer;
}
