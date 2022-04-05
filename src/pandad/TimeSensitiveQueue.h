#ifndef TIME_SENSITIVE_QUEUE_H
#define TIME_SENSITIVE_QUEUE_H

#include <pthread.h>
#include <queue>
#include <sys/time.h>

//
// This class never blocks on a write.  Instead, if the buffer would overflow,
// the offending write is ignored (thrown away).  However, if the write occurs
// within the grace period, the capacity limit is ignored (and the buffer grows
// to fit).  The grace period is a specified number of seconds after the first
// write to an empty buffer.
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

#define GRACE_HERTZ 10
template< class T > void * timerFunction( void * context );

template< class T >
class TimeSensitiveQueue {
	public:
		TimeSensitiveQueue( unsigned capacity, pthread_mutex_t * mutex );

		bool allowGracePeriod( unsigned seconds );
		bool empty() { return storage.empty(); }
		void enqueue( const T & item );
		T dequeue();

		void drain();
		void resize( unsigned _capacity );

		~TimeSensitiveQueue() {
			pthread_cond_destroy( & nonempty );
			pthread_cond_destroy( & drained );
			pthread_cond_destroy( & graceCond );
		}

	protected:
		void * timerFunction();

		unsigned			capacity;
		pthread_cond_t		drained;
		pthread_cond_t		nonempty;
		pthread_mutex_t *	mutex;
		std::queue< T >		storage;

		pthread_cond_t		graceCond;
		pthread_mutex_t		graceMutex;
		unsigned long		graceCount;
		unsigned long		graceStart;
		unsigned 			gracePeriod;

	public:
		unsigned			queueFullCount;
		unsigned			enqueueCallCount;

	friend void * ::timerFunction< T >( void * context );
};


template< class T >
void * timerFunction( void * context ) {
	return ((TimeSensitiveQueue< T > *)context)->timerFunction();
}

template< class T >
void * TimeSensitiveQueue< T >::timerFunction() {
	pthread_mutex_lock( & graceMutex );
	pthread_cond_signal( & graceCond );
	pthread_mutex_unlock( & graceMutex );

	struct timeval tv;
	while( true ) {
		tv.tv_sec = 0;
		tv.tv_usec = 1000000 / GRACE_HERTZ;
		// For now, just let the count advance faster if we catch a signal.
		select( 0, NULL, NULL, NULL, & tv );
	}

	return NULL;
}

template< class T >
bool TimeSensitiveQueue< T >::allowGracePeriod( unsigned seconds ) {
	int rv = pthread_mutex_init( & graceMutex, NULL );
	if( rv != 0 ) { return false; }
	pthread_mutex_lock( & graceMutex );

	pthread_t graceThread;
	rv = pthread_create( & graceThread, NULL, ::timerFunction< T >, this );
	if( rv != 0 ) { 
		pthread_mutex_unlock( & graceMutex );
		return false;
	}

	rv = pthread_detach( graceThread );
	if( rv != 0 ) { 
		pthread_mutex_unlock( & graceMutex );
		return false;
	}

	// Block until the timer thread has started.
	pthread_cond_wait( & graceCond, & graceMutex );
    pthread_mutex_unlock( & graceMutex );

   	gracePeriod = seconds * GRACE_HERTZ;
	return true;
}

template< class T >
void TimeSensitiveQueue< T >::enqueue( const T & item ) {
	++enqueueCallCount;

	if( gracePeriod && storage.empty() ) {
		graceStart = graceCount;
	}

	if( (gracePeriod && graceCount < graceStart + gracePeriod) || storage.size() < capacity ) {
		storage.push( item );
		pthread_cond_broadcast( & nonempty );
	} else {
		++queueFullCount;
	}
}


template< class T >
TimeSensitiveQueue< T >::TimeSensitiveQueue( unsigned _capacity, pthread_mutex_t * _mutex ) :
  capacity( _capacity ),
  mutex( _mutex ),
  graceCount( 0 ),
  graceStart( -1 ),
  gracePeriod( 0 ),
  queueFullCount( 0 ),
  enqueueCallCount( 0 ) {
	pthread_cond_init( & drained, NULL );
	pthread_cond_init( & nonempty, NULL );
	pthread_cond_init( & graceCond, NULL );
}

template< class T >
T TimeSensitiveQueue< T >::dequeue() {
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
void TimeSensitiveQueue< T >::drain() {
	while( ! storage.empty() ) {
		pthread_cond_wait( & drained, mutex );
	}
} // end drain()

template< class T >
void TimeSensitiveQueue< T >::resize( unsigned _capacity ) {
	capacity = _capacity;
} // end resize()

#endif
