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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern char	**environ;

const int MATCH = 0;	// for strcmp()

class StrVector {
public:
	StrVector( int max );
	void Init( int max );			// hack to fix constructor error
	void Add( const char *str );
	char *Get( int idx );
private:
	int		max_size;
	int		cur_size;
	char	**data;
};

StrVector::StrVector( int max )
{
	max_size = max;
	data = new char *[ max ];
	assert( data );
	cur_size = 0;
}


void
StrVector::Init( int max )
{
	max_size = max;
	data = new char *[ max ];
	assert( data );
	cur_size = 0;
}


void
StrVector::Add( const char *str )
{
	assert( cur_size < max_size );

	data[ cur_size ] = new char[ strlen(str) + 1 ];
	assert( data[cur_size] );
	strcpy( data[cur_size], str );
	cur_size += 1;
}

char *
StrVector::Get( int idx )
{
	if( idx >= cur_size ) {
		return NULL;
	} else {
		return data[ idx ];
	}
}

StrVector SaveEnv( 256 );
StrVector SaveArgs( 256 );

void check_vector( char	**vec, StrVector &saved, const char *name );
extern "C" void ckpt();

main( int argc, char *argv[] )
{
	int		i;
	char	*str;

	SaveEnv.Init( 256 );
	SaveArgs.Init( 256 );

	for( i=0; environ[i]; i++ ) {
		SaveEnv.Add( environ[i] );
		printf( "environ[%d] = \"%s\"\n", i, environ[i] );
	}

	for( i=0; i<argc; i++ ) {
		SaveArgs.Add( argv[i] );
		printf( "argv[%d] = \"%s\"\n", i, argv[i] );
	}
	printf( "\n" );

	check_vector( environ, SaveEnv, "Environment" );
	check_vector( argv, SaveArgs, "Argument" );
	printf( "\n" );

	fflush( stdout );
	ckpt();

	check_vector( environ, SaveEnv, "Environment" );
	check_vector( argv, SaveArgs, "Argument" );
	printf( "\n" );

	fflush( stdout );
	ckpt();

	check_vector( environ, SaveEnv, "Environment" );
	check_vector( argv, SaveArgs, "Argument" );
	printf( "\n" );

	printf( "Normal End Of Job\n");

	exit( 0 );
	return 0;
}

void
check_vector( char	**vec, StrVector &saved, const char *name )
{
	int		i;
	char	*s1, *s2;

	for( i=0; vec[i]; i++ ) {
		s1 = saved.Get(i);
		s2 = vec[i];
		assert( strcmp(s1,s2) == MATCH );
	}
	assert( saved.Get(i) == NULL );

	printf( "%s vector OK\n", name );
}
