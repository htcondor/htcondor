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

 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

extern char	**environ;

const int MATCH = 0;	// for strcmp()

class StrVector {
public:
	StrVector( int max );
	void Init( int max );		// hack to fix constructor error
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

void StrVector::Init( int max )
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

#include "x_fake_ckpt.h"

int
main( int argc, char *argv[] )
{
	int		i;

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
	ckpt_and_exit();

	check_vector( environ, SaveEnv, "Environment" );
	check_vector( argv, SaveArgs, "Argument" );
	printf( "\n" );

	fflush( stdout );
	ckpt_and_exit();

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
