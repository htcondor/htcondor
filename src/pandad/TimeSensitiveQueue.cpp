#include <string>
#include <sstream>
#include <iostream>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "TimeSensitiveQueue.h"

//
// Run three times with '1', '3', and '5' as the arguments.
//

#define QUEUE_COUNT 100
int main( int argc, char ** argv ) {
	int gracePeriod = 0;
	if( argc >= 2 ) {
		gracePeriod = atoi( argv[1] );
		if( gracePeriod < 0 ) {
			fprintf( stderr, "Grace periods must be positive.\n" );
			return 1;
		}
	}

	pthread_mutex_t oneTrueMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock( & oneTrueMutex );

	int matchedCount = 0;
	TimeSensitiveQueue< int > queue( 10, & oneTrueMutex );
	if( ! queue.allowGracePeriod( gracePeriod ) ) {
		fprintf( stderr, "Unable to allow grace period %d...\n", gracePeriod );
		return 1;
	}

	for( int i = 0; i < QUEUE_COUNT; ++i ) {
		queue.enqueue( i );
		if( i == 0 ) { sleep( 2 ); }
		if( i == 49 ) { sleep( 2 ); }
	}

	for( int i = 0; i < QUEUE_COUNT - queue.queueFullCount; ++i ) {
		int j = queue.dequeue();
		if( j == i ) { ++matchedCount; }
		else {
			fprintf( stderr, "FAILURE (i = %d != j = %d)\n", i, j );
			return 1;
		}
	}

	switch( gracePeriod ) {
		// Consider a more-gruelling test for non-graceful mode?
		case 0:
			if( matchedCount != 10 ) {
				fprintf( stderr, "FAILURE (incorrect number of items in queue.\n" );
				return 1;
			}
		break;

		case 1:
			if( matchedCount != 10 ) {
				fprintf( stderr, "FAILURE (incorrect number of items in queue.\n" );
				return 1;
			}
		break;

		case 3:
			if( matchedCount != 50 ) {
				fprintf( stderr, "FAILURE (incorrect number of items in queue.\n" );
				return 1;
			}
		break;

		case 5:
			if( matchedCount != 100 ) {
				fprintf( stderr, "FAILURE (incorrect number of items in queue.\n" );
				return 1;
			}
		break;

		default:
			std::cout << "queueFullCount: " << queue.queueFullCount << " ";
			std::cout << "enqueueCallCount: " << queue.enqueueCallCount << "\n";
		break;
	}
	fprintf( stdout, "Initial sanity testing looks OK...\n" );


	//
	// Fill the queue, wait longer than its grace period.  Verify that
	// the new entry doesn't happen.  Then empty the queue, partially fill
	// it (past normal capacity), sleep past its grace period, and try to
	// fill it some more.  Verify that only the partial fill enters the queue.
	//
	TimeSensitiveQueue< int > secondQ( 10, & oneTrueMutex );
	if( ! secondQ.allowGracePeriod( gracePeriod ) ) {
		fprintf( stderr, "Unable to allow grace period %d...\n", gracePeriod );
		return 1;
	}
	for( int i = 0; i < QUEUE_COUNT; ++i ) {
		secondQ.enqueue( i );
	}
	assert( secondQ.queueFullCount == 0 );
	sleep( gracePeriod + 1 );

	secondQ.enqueue( -1 );
	if( secondQ.queueFullCount != 1 ) {
		fprintf( stderr, "FAILURE (accepted insert should not have been).\n" );
		return 1;
	}

	for( int i = 0; i < QUEUE_COUNT; ++i ) {
		secondQ.dequeue();
	}
	if( ! secondQ.empty() ) {
		fprintf( stderr, "FAILURE (queue not empty after dequeueing all).\n" );
		return 1;
	}

	for( int i = 0; i < QUEUE_COUNT; ++i ) {
		secondQ.enqueue( i );
		if( i == 49 ) { sleep( gracePeriod + 1 ); }
	}
	int itemsInSecondQ = 0;
	while( ! secondQ.empty() ) { ++itemsInSecondQ; secondQ.dequeue(); }
	if( itemsInSecondQ != 50 ) {
		fprintf( stderr, "FAILURE (queue had wrong number of items in it after grace period reset).\n" );
		return 1;
	}

	fprintf( stdout, "Grace period reset tested OK.\n" );

	return 0;
}
