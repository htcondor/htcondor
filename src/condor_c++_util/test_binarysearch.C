/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include <stdio.h>
#include "binary_search.h"

template class BinarySearch<int>;
template class BinarySearch<float>;

int		debug = 0;
const int maxPrint = 10;

void
PrintArray( int array[], int length )
{
	printf( "{ " );
	if ( length <= maxPrint ) {
		for ( int i = 0; i < length; i++ ) {
			printf( "%d ", array[i] );
		}
	} else {
		printf( "...%d elements... ", length );
	}
	printf( "}" );
}

void
PrintArray( float array[], int length )
{
	printf( "{ " );
	if ( length <= maxPrint ) {
		for ( int i = 0; i < length; i++ ) {
			printf( "%f ", array[i] );
		}
	} else {
		printf( "...%d elements... ", length );
	}
	printf( "}" );
}

int
TestSearch( int array[], int length, int key, bool expectedFound,
			int expectedIndex )
{
	int		result = 0;

	int		index;
	bool	found = BinarySearch<int>::Search( array, length, key, index );

	if ( debug >= 1 ) {
		printf( "Search for %d in ", key );
		PrintArray( array, length );
		printf( " returned %d, %d\n", found, index );
	}

	bool	match = found && (array[index] == key);
	if ( !match && (found != expectedFound || index != expectedIndex) ) {
		fprintf( stderr, "Error: search for %d returned %d, %d; "
					"should have returned %d, %d\n", key, found, index,
					expectedFound, expectedIndex );
		printf( "  Array is: " );
		PrintArray( array, length );
		printf( "\n" );
		result = 1;
	}

	return result;
}

int
TestSearch( float array[], int length, float key, bool expectedFound,
			int expectedIndex )
{
	int		result = 0;

	int		index;
	bool	found = BinarySearch<float>::Search( array, length, key, index );

	if ( debug >= 1 ) {
		printf( "Search for %f in ", key );
		PrintArray( array, length );
		printf( " returned %d, %d\n", found, index );
	}

	if ( found != expectedFound || index != expectedIndex ) {
		fprintf( stderr, "Error: search for %f returned %d, %d; "
					"should have returned %d, %d\n", key, found, index,
					expectedFound, expectedIndex );
		printf( "  Array is: " );
		PrintArray( array, length );
		printf( "\n" );
		result = 1;
	}

	return result;
}

int test0()
{
	printf( "Testing an integer binary search...\n" );
	int		result = 0;

	int		array[] = { 1, 2, 3 };

	result |= TestSearch( array, sizeof(array)/sizeof(int), 0, false, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 1, true, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 2, true, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 3, true, 2 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 4, false, 3 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test1()
{
	printf( "Testing an integer binary search...\n" );
	int		result = 0;

	int		array[] = { 1, 3, 4, 7 };

	result |= TestSearch( array, sizeof(array)/sizeof(int), 0, false, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 1, true, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 2, false, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 3, true, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 4, true, 2 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 5, false, 3 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 6, false, 3 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 7, true, 3 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 8, false, 4 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test2()
{
	printf( "Testing an integer binary search with duplicates...\n" );
	int		result = 0;

	int		array[] = { 1, 3, 3, 3, 7, 7 };

	result |= TestSearch( array, sizeof(array)/sizeof(int), 0, false, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 1, true, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 2, false, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 3, true, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 4, false, 4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 5, false, 4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 6, false, 4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 7, true, 4 );
	result |= TestSearch( array, sizeof(array)/sizeof(int), 8, false, 6 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test3()
{
	printf( "Testing a float binary search...\n" );
	int		result = 0;

	float	array[] = { -2349.67, -1005.39, 0.0, 55.72, 55.74, 100.9 };

	result |= TestSearch( array, sizeof(array)/sizeof(float), -3333.0, false, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), -2349.67, true, 0 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), -2000.0, false, 1 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), -1000.0, false, 2);
	result |= TestSearch( array, sizeof(array)/sizeof(float), 0.0, true, 2 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 50.0, false, 3 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 55.73, false, 4 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 55.75, false, 5 );
	result |= TestSearch( array, sizeof(array)/sizeof(float), 101.0, false, 6 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test4()
{
	printf( "Testing a float binary search with an empty array...\n" );
	int		result = 0;

	float	array[] = {};

	result |= TestSearch( array, sizeof(array)/sizeof(float), -3333.0, false, 0 );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int test5()
{
	printf( "Testing an integer binary search with a large array...\n" );
	int		result = 0;

	const int	size = 10000000;
	int		*array = new int[size];

	for ( int i = 0; i < size; i++ ) {
		array[i] = i;
	}

	result |= TestSearch( array, size, -1, false, 0 );
	result |= TestSearch( array, size, 0, true, 0 );
	result |= TestSearch( array, size, size/2, true, size/2 );
	result |= TestSearch( array, size, size-1, true, size-1 );
	result |= TestSearch( array, size, size, false, size );

	printf( "...%s\n", result == 0 ? "OK" : "Failed" );
	return result;
}

int
main(int argc, char *argv[])
{
	int		result = 0;

	printf( "Testing BinarySearch template\n" );

	if ( argc > 1 && !strcmp( argv[1], "-d" ) ) {
		debug = 1;
	}

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
