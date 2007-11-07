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
