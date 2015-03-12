//
// Consider reimplementing with a ring buffer.
//

#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <queue>

//
// This class never blocks on a write.  Instead, if the buffer would overflow,
// the offending write is ignored (thrown away).
//
// The writer using this class must release the supplied mutex for the
// reader(s) to make progress.  Likewise, the reader(s) must acquire the mutex
// before calling dequeue().
//
// That is, this class mostly assumes the one-thread-at-a-time model.
//
// The diagnostic variable queueFullCount asssumes only one writer (it could
// otherwise lose updates).
//

template< class T >
class Queue {
	public:
		Queue( unsigned _capacity, pthread_mutex_t * _mutex ) : capacity( _capacity ), mutex( _mutex ), queueFullCount( 0 ), enqueueCallCount( 0 ) {
			pthread_cond_init( & nonempty, NULL );
			pthread_cond_init( & drained, NULL );
		}

		bool empty() { return storage.empty(); }
		void enqueue( const T & item );
		T dequeue();

		void drain();
		void resize( unsigned _capacity );

		~Queue() {
			pthread_cond_destroy( & nonempty );
		}

	protected:
		unsigned			capacity;
		pthread_cond_t		drained;
		pthread_cond_t		nonempty;
		pthread_mutex_t *	mutex;
		std::queue< T >		storage;

	public:
		unsigned			queueFullCount;
		unsigned			enqueueCallCount;
};

template< class T >
void Queue< T >::enqueue( const T & item ) {
	++enqueueCallCount;
	if( storage.size() < capacity ) {
		storage.push( item );
		pthread_cond_broadcast( & nonempty );
	} else {
		++queueFullCount;
	}
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

template< class T >
void Queue< T >::resize( unsigned _capacity ) {
	capacity = _capacity;
} // end resize()

#endif
