#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <queue>

// This class assumes that its storage never runs out of space; thus,
// writers will never block (until the system runs out of memory).  The
// writers, however, are therefore responsible for unlocking mutex so that
// the reader(s) may proceed.
//
// Likewise, readers must acquire the mutex before calling dequeue().
//
// That is, this class mostly assumes the one-thread-at-a-time model.

template< class T >
class Queue {
	public:
		Queue( pthread_mutex_t * _mutex ) : mutex( _mutex ) {
			pthread_cond_init( & nonempty, NULL );
			pthread_cond_init( & drained, NULL );
		}

		void enqueue( const T & item );
		T dequeue();

		void drain();

		~Queue() {
			pthread_cond_destroy( & nonempty );
		}

	protected:
		pthread_cond_t		drained;
		pthread_cond_t		nonempty;
		pthread_mutex_t *	mutex;
		std::queue< T >		storage;
};

template< class T >
void Queue< T >::enqueue( const T & item ) {
	storage.push( item );
	pthread_cond_broadcast( & nonempty );
} // end enqueue()

template< class T >
T Queue< T >::dequeue() {
	// Allowing more than one dequeue() per signal means the consumer
	// won't fall behind when we get more than one item per read().  If
	// the consumer uses too much CPU, the producer [schedd] will block
	// on write().  We could always prefer the producer by unlocking,
	// yielding, and relocking before checking for an item.
	while( storage.empty() ) {
		pthread_cond_broadcast( & drained );
		pthread_cond_wait( & nonempty, mutex );
	}
	T item = storage.front();
	storage.pop();
	return item;
} // end dequeue()

template< class T >
void Queue< T >::drain() {
	while( ! storage.empty() ) {
		pthread_cond_wait( & drained, mutex );
	}
} // end drain()

#endif
