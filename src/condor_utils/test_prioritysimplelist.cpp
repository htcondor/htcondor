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


#include "condor_common.h"
#include <stdio.h>
#include "prioritysimplelist.h"


void
PrintList( SimpleList<int> &sl )
{
	sl.Rewind();
	int		item;
	while ( sl.Next( item ) ) {
		printf( "%d ", item );
	}
	printf( "\n" );
}

void
PrintArray( int size, int array[] )
{
	for ( int i = 0; i < size; i++ ) {
		printf( "%d ", array[i] );
	}
	printf( "\n" );
}

// Returns 0 if okay, 1 if error
int
CheckList( SimpleList<int> &sl, int expectedSize, int expectedList[] )
{
	sl.Rewind();

	if ( sl.Number() != expectedSize ) {
		fprintf( stderr, "Error: list has %d items, expected %d\n",
					sl.Number(), expectedSize );
		return 1;
	}

	int		index = 0;
	int		item;
	while ( sl.Next( item ) ) {
		if ( item != expectedList[index] ) {
			fprintf( stderr, "Error: list mismatch at item %d\n", index );
			printf( "Expected list: " );
			PrintArray( expectedSize, expectedList );
			printf( "Actual list:   " );
			PrintList( sl );
			return 1;
		}
		index++;
	}

	return 0;
}

// Returns 0 if okay, 1 if error
int
CheckList( PrioritySimpleList<int> &psl )
{
	int		result = 0;

	psl.Rewind();

	int		prev = -1;
	int		curr;
	while ( psl.Next( curr ) ) {
		if ( prev > curr ) {
			fprintf( stderr, "Error: %d before %d\n", prev, curr );
			result = 1;
		}
		prev = curr;
	}

	return result;
}

// Just test a regular SimpleList.
int test0()
{
	printf( "Testing a plain SimpleList...\n" );

	SimpleList<int>		sl;

	sl.Append( 1 );
	sl.Append( 2 );
	sl.Prepend( 3 );

	int expected[] = { 3, 1, 2 };
	int result = CheckList( sl, sizeof( expected ) / sizeof( int ), expected );

	sl.Rewind();
	int		item;
	sl.Next( item );
	sl.Next( item );
	sl.DeleteCurrent();

	int expected2[] = { 3, 2 };
	result |= CheckList( sl, sizeof( expected2 ) / sizeof( int ), expected2 );

	sl.Append( 4 );
	sl.Prepend( 5 );

	int expected3[] = { 5, 3, 2, 4 };
	result |= CheckList( sl, sizeof( expected3 ) / sizeof( int ), expected3 );

		//
		// Test copy constructor.
		//
	SimpleList<int>		sl2( sl );
	result |= CheckList( sl2, sizeof( expected3 ) / sizeof( int ), expected3 );

	sl2.Prepend( 6 );

	int expected4[] = { 6, 5, 3, 2, 4 };
	result |= CheckList( sl2, sizeof( expected4 ) / sizeof( int ), expected4 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

// Test a list with a single priority -- make sure order is maintained.
int test1()
{
	printf( "Testing a PrioritySimpleList with only one priority...\n" );

	PrioritySimpleList<int>		psl;

	psl.Append( 1, 1 );
	psl.Append( 2, 1 );
	psl.Prepend( 3, 1 );

	int expected[] = { 3, 1, 2 };
	int result = CheckList( psl, sizeof( expected ) / sizeof( int ), expected );

	psl.Rewind();
	int		item;
	psl.Next( item );
	psl.Next( item );
	psl.Next( item );
	psl.DeleteCurrent();

	int expected2[] = { 3, 1 };
	result |= CheckList( psl, sizeof( expected2 ) / sizeof( int ), expected2 );

	psl.Append( 4, 1 );
	psl.Append( 5, 1 );
	psl.Prepend( 6, 1 );

	int expected3[] = { 6, 3, 1, 4, 5 };
	result |= CheckList( psl, sizeof( expected3 ) / sizeof( int ), expected3 );

	psl.Rewind();
	psl.Next( item );
	psl.DeleteCurrent();
	psl.Next( item );
	psl.Next( item );
	psl.DeleteCurrent();
	psl.Append( 7, 1 );
	psl.Prepend( 8, 1 );

	int expected4[] = { 8, 3, 4, 5, 7 };
	result |= CheckList( psl, sizeof( expected4 ) / sizeof( int ), expected4 );

		//
		// Test copy constructor.
		//
	PrioritySimpleList<int>		psl2( psl );
	result |= CheckList( psl2, sizeof( expected4 ) / sizeof( int ), expected4 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

// Test a list with multiple priorities.
int test2()
{
	printf( "Testing a PrioritySimpleList with multiple priorities...\n" );

	PrioritySimpleList<int>		psl;

	psl.Append( 1, 0 ); // 1
	psl.Append( 2, 0 ); // 1, 2
	psl.Append( 3, -1 ); // 3, 1, 2
	psl.Prepend( 4, 0 ); // 3, 4, 1, 2
	psl.Append( 5, -1 ); // 3, 5, 4, 1, 2
	psl.Prepend( 6, 1 ); // 3, 5, 4, 1, 2, 6
	psl.Prepend( 7, 3 ); // 3, 5, 4, 1, 2, 6, 7

	int expected[] = { 3, 5, 4, 1, 2, 6, 7 };
	int result = CheckList( psl, sizeof( expected ) / sizeof( int ), expected );

	psl.Rewind();
	int		item;
	psl.Next( item );
	psl.DeleteCurrent();

	int expected2[] = { 5, 4, 1, 2, 6, 7 };
	result |= CheckList( psl, sizeof( expected2 ) / sizeof( int ), expected2 );

	psl.Append( 8, 0 );
	psl.Prepend( 9, 10 );

	int expected3[] = { 5, 4, 1, 2, 8, 6, 7, 9 };
	result |= CheckList( psl, sizeof( expected3 ) / sizeof( int ), expected3 );

	if ( !psl.Delete( 2 ) ) {
		fprintf( stderr, "Delete failed\n" );
		result = 1;
	}

	psl.Append( 10, 0 );
	psl.Append( 11, -2 );
	psl.Prepend( 12, 11 );

	int expected4[] = { 11, 5, 4, 1, 8, 10, 6, 7, 9, 12 };
	result |= CheckList( psl, sizeof( expected4 ) / sizeof( int ), expected4 );

		//
		// Test copy constructor.
		//
	PrioritySimpleList<int>		psl2( psl );
	result |= CheckList( psl2, sizeof( expected4 ) / sizeof( int ), expected4 );

	psl2.Append( 13, 0 );
	int expected5[] = { 11, 5, 4, 1, 8, 10, 13, 6, 7, 9, 12 };
	result |= CheckList( psl2, sizeof( expected5 ) / sizeof( int ), expected5 );

	psl2.Rewind();
	psl2.Next( item );
	psl2.Next( item );
	psl2.ReplaceCurrent( 13 );

	int expected6[] = { 11, 13, 4, 1, 8, 10, 13, 6, 7, 9, 12 };
	result |= CheckList( psl2, sizeof( expected6 ) / sizeof( int ), expected6 );


	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

// Test a list without explicit priorities.
int test3()
{
	printf( "Testing a PrioritySimpleList without explicit priorities...\n" );

	PrioritySimpleList<int>		psl;

	psl.Prepend( 1 );
	psl.Append( 2 );
	psl.Append( 3 );
	psl.Prepend( 4 );

	int expected[] = { 4, 1, 2, 3 };
	int result = CheckList( psl, sizeof( expected ) / sizeof( int ), expected );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

// Test a large list.
int test4()
{

	printf( "Testing a large PrioritySimpleList...\n" );

	PrioritySimpleList<int>		psl;

	struct timeval	tvStart, tvStop;
	gettimeofday( &tvStart, NULL );

	const int	size = 100000;

		// Insert in increasing order -- this should be fast.
	for ( int i = 0; i < size; i++ ) {
		psl.Prepend( i, i );
	}

	psl.Prepend( 0, 0 );
	psl.Prepend( 1, 1 );
	psl.Append( size/2, size/2 );
	psl.Append( size-1, size-1 );

	gettimeofday( &tvStop, NULL );
	double	start = tvStart.tv_sec + ((double)tvStart.tv_usec / (1000 * 1000));
	double	stop = tvStop.tv_sec + ((double)tvStop.tv_usec / (1000 * 1000));
	double	duration = stop - start;
	printf( "List has %d elements; %f sec\n", psl.Number(), duration );

	int result = CheckList( psl );


	psl.Clear();

	gettimeofday( &tvStart, NULL );

		// Insert in decreasing order -- this will be slower because we
		// have to keep moving the existing entries.
	for ( int i = size-1; i >= 0; i-- ) {
		psl.Append( i, i );
	}

	gettimeofday( &tvStop, NULL );
	start = tvStart.tv_sec + ((double)tvStart.tv_usec / (1000 * 1000));
	stop = tvStop.tv_sec + ((double)tvStop.tv_usec / (1000 * 1000));
	duration = stop - start;
	printf( "List has %d elements; %f sec\n", psl.Number(), duration );

	result |= CheckList( psl );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

// Test a PrioritySimpleList vs. a SimpleList (for speed).
int test5()
{
	int		result = 0;

	printf( "Testing a large SimpleList vs. a PrioritySimpleList...\n" );

	SimpleList<int>		sl;

	struct timeval	tvStart, tvStop;
	gettimeofday( &tvStart, NULL );

	const int	size = 1000000;

		// Insert in increasing order -- this should be fast.
	for ( int i = 0; i < size; i++ ) {
		sl.Append( i );
	}

	gettimeofday( &tvStop, NULL );
	double	start = tvStart.tv_sec + ((double)tvStart.tv_usec / (1000 * 1000));
	double	stop = tvStop.tv_sec + ((double)tvStop.tv_usec / (1000 * 1000));
	double	duration = stop - start;
	printf( "SimpleList has %d elements; %f sec\n", sl.Number(), duration );


	gettimeofday( &tvStart, NULL );

	PrioritySimpleList<int>		psl;
		// Insert in increasing order -- this should be fast.
	for ( int i = 0; i < size; i++ ) {
		psl.Append( i );
	}

	gettimeofday( &tvStop, NULL );
	start = tvStart.tv_sec + ((double)tvStart.tv_usec / (1000 * 1000));
	stop = tvStop.tv_sec + ((double)tvStop.tv_usec / (1000 * 1000));
	duration = stop - start;
	printf( "PrioritySimpleList has %d elements; %f sec\n", sl.Number(),
				duration );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int
main(int argc, char *argv[])
{
	int		result = 0;

	printf( "Testing PrioritySimpleList template\n" );

	result |= test0();
	result |= test1();
	result |= test2();
	result |= test3();
	result |= test4();
	result |= test5();

	if ( result == 0 ) {
		printf( "Test succeeded\n" );
	} else {
		printf( "Test failed\n" );
	}
	return result;
}
