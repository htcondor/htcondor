/* 
** Copyright 1991 by the Condor Design Team
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
** Author:  Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>

#if !defined(MATCH)
#define MATCH 0		/* for strcmp() and friends */
#endif

char	*getenv(), *index();

#if defined(__STDC__)
	char *strdup(char *);
#else
	char *strdup();
#endif

#ifdef STANDALONE
main( argc, argv )
int		argc;
char	*argv[];
{
	while( *(++argv) ) {
		add_to_path( *argv );
	}
	printf( "New path = %s\n", getenv("PATH") );
}
#endif

/*
** Add the given name to our PATH environment variable.
*/
add_to_path( name )
char	*name;
{
	char	*orig_path;
	char	*next;
	char	*ptr;
	static char	buf[1024];

		/* Get original PATH */
	orig_path = strdup( getenv("PATH") );

		/* Look to see if "name" is already in there */
	ptr = orig_path;
	for(;;) {
		if( strncmp(name,ptr,strlen(name)) == MATCH ) {		/* Already there */
			free( orig_path );
			return;
		}
		if( next = index(ptr,':') ) {
			ptr = next + 1;
		} else {
			break;
		}
	}

#if defined(AIX31) || defined(AIX32) || defined(IRIX331) || defined(SUNOS41) || defined(CMOS) || defined(HPUX9) || defined(IRIX53) || defined(Solaris)
		/* Build up the new PATH */
	strcpy( buf, "PATH=" );
	strcat( buf, orig_path );
	strcat( buf, ":" );
	strcat( buf, name );

		/* Install it */
	putenv( buf );
#else
		/* Build up the new PATH */
	strcpy( buf, orig_path );
	strcat( buf, ":" );
	strcat( buf, name );

		/* Install it */
	setenv( "PATH", buf, 1 );
#endif

	free( orig_path );
}

#if 0	/* Eliminated */
/*
** Return TRUE if the pattern is a substring of or equal to the
** given string, otherwise return FALSE.
*/
substr( pattern, string )
char	*pattern;
char	*string;
{
	while( *pattern ) {
		if( *pattern != *string ) {
			return 0;
		}
		pattern += 1;
		string += 1;
	}
	return 1;
}
#endif

#ifdef STANDALONE
char	*
strdup( str )
char	*str;
{
	char	*buf, *malloc();

	buf = malloc( strlen(str) + 1 );
	strcpy( buf, str );
	return buf;
}
#endif

