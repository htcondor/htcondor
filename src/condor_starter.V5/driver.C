/* 
** Copyright 1993 by Miron Livny, and Mike Litzkow
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

#include <stdio.h>
#include "environ.h"

main( int argc, char *argv[] )
{
	char	buf[1024];
	char	**vec;
	char	*answer;
	Environ	foo;
	Environ bar;
	FILE	*fp;

	if( argc != 2 ) {
		printf( "Usage: %s filename\n" );
		return 1;
	}

	if( (fp=fopen(argv[1],"r")) == NULL ) {
		perror( argv[1] );
		return 1;
	}

	foo.add_string( "FOO=bar" );
	foo.add_string( "BAR=glarch" );
	foo.display();

	while( fgets(buf,sizeof(buf),fp) ) {
		bar.add_string( buf );
	}
	vec = bar.get_vector();
	for( ; *vec; vec++ ) {
		printf( "%s\n", *vec );
	}

	while( gets(buf) ) {
		answer = bar.getenv(buf);
		printf( "Variable \"%s\" = \"%s\"\n", buf, answer ? answer : "(NULL)" );
	}
	return 0;
}
