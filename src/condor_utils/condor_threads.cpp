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

#include "condor_threads.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "HashTable.h"
#include "dc_service.h"		// for class Service
#include "subsystem_info.h"
#include <queue>

/**********************************************************************/
/**********************************************************************/

/*	
	Implementation of just the pthread functions
	we need on Windows if we are not linked w/ some Win32 pthread library
*/

#if defined(WIN32) && !defined(HAVE_PTHREADS)

#define HAVE_PTHREADS 1

// Some definitions to translate types from pthread to Win32
typedef CRITICAL_SECTION pthread_mutex_t;
typedef DWORD pthread_t;

// Some no-op definitions of pthread types we don't support on Win32.
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
typedef int pthread_key_t;
#define PTHREAD_MUTEX_RECURSIVE_NP 1
#define PTHREAD_MUTEX_RECURSIVE 2

static int
pthread_mutexattr_init(pthread_mutexattr_t *)
{
	// not implemented on Win32
	return 0;	// init always returns 0 according to the man page
}

static int
pthread_mutexattr_settype(pthread_mutexattr_t *, int)
{
	// not implemented on Win32
	return -1;
}

static pthread_t
pthread_self()
{
	return GetCurrentThreadId();
}

static int
pthread_equal(pthread_t t1, pthread_t t2)
{
	return t1 == t2 ? 1 : 0;
}

static int
pthread_detach(pthread_t /* thread */)
{
	return 0;
}

static int 
pthread_mutex_init (pthread_mutex_t *mv, 
                    const pthread_mutexattr_t *)
{
#pragma warning(suppress: 28125)
	InitializeCriticalSection(mv);
	return 0;
}

static int
pthread_mutex_destroy(pthread_mutex_t *mv)
{
	DeleteCriticalSection(mv);
	return 0;
}

static int 
pthread_mutex_lock (pthread_mutex_t *mv)
{
	EnterCriticalSection(mv);
	return 0;
}

static int
pthread_mutex_unlock(pthread_mutex_t *mv)
{
	LeaveCriticalSection(mv);
	return 0;
}

enum 
{
    SIGNAL = 0,
    BROADCAST = 1,
    MAX_EVENTS = 2
};

typedef struct
{
  u_int waiters_count_;
  // Count of the number of waiters.
  
  CRITICAL_SECTION waiters_count_lock_;
  // Serialize access to <waiters_count_>.

  HANDLE events_[MAX_EVENTS];
  // Signal and broadcast event HANDLEs.
} pthread_cond_t;


static int 
pthread_cond_init (pthread_cond_t *cv, 
                   const pthread_condattr_t *)
{
  // Initialize the count to 0.
  cv->waiters_count_ = 0;

  // Initialize critical section
#pragma warning(suppress: 28125)
  InitializeCriticalSection(&(cv->waiters_count_lock_));

  // Create an auto-reset event.
  cv->events_[SIGNAL] = CreateEvent (NULL,  // no security
                                     FALSE, // auto-reset event
                                     FALSE, // non-signaled initially
                                     NULL); // unnamed

  // Create a manual-reset event.
  cv->events_[BROADCAST] = CreateEvent (NULL,  // no security
                                        TRUE,  // manual-reset
                                        FALSE, // non-signaled initially
                                        NULL); // unnamed

  return 0;
}

// The pthread_cond_wait function waits for a condition cv and atomically 
// releases the associated external_mutex that it holds 
// while checking the condition expression.
static int 
pthread_cond_wait (pthread_cond_t *cv,
                   pthread_mutex_t *external_mutex)
{
  // Avoid race conditions.
  EnterCriticalSection (&cv->waiters_count_lock_);
  cv->waiters_count_++;
  LeaveCriticalSection (&cv->waiters_count_lock_);

  // It's ok to release the <external_mutex> here since Win32
  // manual-reset events maintain state when used with
  // <SetEvent>.  This avoids the "lost wakeup" bug...
  LeaveCriticalSection (external_mutex);

  // Wait for either event to become signaled due to <pthread_cond_signal>
  // being called or <pthread_cond_broadcast> being called.
  int result = WaitForMultipleObjects (2, cv->events_, FALSE, INFINITE);

  EnterCriticalSection (&cv->waiters_count_lock_);
  cv->waiters_count_--;
  int last_waiter =
    result == WAIT_OBJECT_0 + BROADCAST 
    && cv->waiters_count_ == 0;
  LeaveCriticalSection (&cv->waiters_count_lock_);

  // Some thread called <pthread_cond_broadcast>.
  if (last_waiter)
    // We're the last waiter to be notified or to stop waiting, so
    // reset the manual event. 
    ResetEvent (cv->events_[BROADCAST]); 

  // Reacquire the <external_mutex>.
  EnterCriticalSection (external_mutex);

  return 0;
}

static int 
pthread_cond_broadcast (pthread_cond_t *cv)
{
  // Avoid race conditions.
  EnterCriticalSection (&cv->waiters_count_lock_);
  int have_waiters = cv->waiters_count_ > 0;
  LeaveCriticalSection (&cv->waiters_count_lock_);

  if (have_waiters)
    SetEvent (cv->events_[BROADCAST]);

  return 0;
}

static int 
pthread_cond_signal (pthread_cond_t *cv)
{
  // Avoid race conditions.
  EnterCriticalSection (&cv->waiters_count_lock_);
  int have_waiters = cv->waiters_count_ > 0;
  LeaveCriticalSection (&cv->waiters_count_lock_);

  if (have_waiters)
    SetEvent (cv->events_[SIGNAL]);

  return 0;
}
#endif  // of pthread routines for Win32

/**********************************************************************/
/**********************************************************************/

#include "threads_implementation.h"

#ifdef WIN32
	int ThreadImplementation::m_CurrentTid = 0;
#endif

static ThreadImplementation* TI = NULL;

/**********************************************************************/


// WorkerThread ctor
WorkerThread::WorkerThread()
{
	name_ = NULL;
	routine_ = NULL;
	arg_ = NULL;
	user_pointer_ = NULL;
	tid_ = 0;
	enable_parallel_flag_ = false;
	parallel_mode_count_ = 0;
	status_ = THREAD_UNBORN;
}

WorkerThread::WorkerThread(const char* name, condor_thread_func_t routine, void* arg)
{
	name_ = NULL;
	routine_ = NULL;
	arg_ = NULL;
	user_pointer_ = NULL;
	tid_ = 0;
	enable_parallel_flag_ = false;
	parallel_mode_count_ = 0;
	status_ = THREAD_UNBORN;


	name_ = name ? strdup(name) : NULL;
	routine_ = routine;
	arg_ = arg;
}


// WorkerThread dtor
WorkerThread::~WorkerThread()
{	
	// note: do NOT delete arg_  !
	if (name_) free(name_);
	if (user_pointer_) delete (Service *)user_pointer_;

	// remove tid from our hash table
	if ( tid_ &&  TI ) {
		TI->remove_tid(tid_);
	}
}

WorkerThreadPtr_t 
WorkerThread::create(const char* name, condor_thread_func_t routine, void* arg )
{
	WorkerThread* newthread_rawptr = new WorkerThread(name,routine,arg);
	ASSERT(newthread_rawptr);
	WorkerThreadPtr_t newthread(newthread_rawptr);
	return newthread;
}

#define THREADSTATUS( x ) case THREAD_##x: return (#x);
const char*
WorkerThread::get_status_string(thread_status_t s)
{
	switch ( s ) {
		THREADSTATUS( UNBORN )
		THREADSTATUS( READY )
		THREADSTATUS( RUNNING )
		THREADSTATUS( WAITING )
		THREADSTATUS( COMPLETED )
	default:
		return "Unknown";
	}
}


/**********************************************************************/

const WorkerThreadPtr_t 
ThreadImplementation::get_main_thread_ptr()
{
	static bool already_been_here = false;
	static WorkerThreadPtr_t main_thread_ptr;

	// create a thread object for the main thread if we have not already
	if ( !main_thread_ptr ) {
		ASSERT ( already_been_here == false );
		WorkerThreadPtr_t main_thread_tmp(new WorkerThread("Main Thread",NULL));
		main_thread_ptr = main_thread_tmp;
		already_been_here = true;
		main_thread_ptr->tid_ = 1;	// main thread always has tid of 1
	}

	return main_thread_ptr;
}

#ifndef HAVE_PTHREADS

bool operator== (const ThreadInfo &lhs, const ThreadInfo &rhs)
{
	return ( &lhs == &rhs ? true : false );
}

#endif

// The rest of the ThreadImplementaion class needs pthreads.
#ifdef HAVE_PTHREADS

#ifndef WIN32
	#include <pthread.h>
#endif

bool operator== (const ThreadInfo &lhs, const ThreadInfo &rhs)
{
	return ( pthread_equal(lhs.get_pthread(),rhs.get_pthread()) ? true : false );
}

void
WorkerThread::set_status(thread_status_t newstatus)
{
	static int previous_ready_tid = 0;
	static int previous_running_tid = 0;
	static char previous_ready_message[200];
	bool same_thread_running = false;

	/* NOTE: IF newstatus == THREAD_RUNNING we assume we already have the 
	 * big lock ! */

	// TODO if ( newstatus == THREAD_RUNNING ) ASSERT have big lock

		// THREAD_COMPLETED is a terminal state; don't allow any changes
	if ( status_ == THREAD_COMPLETED  ) {		
		return;
	}

		// don't log a message unless we are actually changing state
	if ( status_ == newstatus ) {
		return;
	}

	int mytid = get_tid();
	thread_status_t currentstatus = status_;

		// change the state.
	status_ = newstatus;

		// don't bother with all the below code to dprintf a state change
		// if we aren't running with threads enabled.
	if ( !TI ) {
		return;
	}

		/* Print out a thread status change message to the log, BUT surpress
		   printing out anything if the SAME thread is going from 
		   RUNNING->READY->RUNNING.  To do this we save RUNNING->READY messages
		   into a buffer and only print them if we see the tid change.
		   This is common if there is only one thread
		   ready to run and it calls functions that are marked parallel safe.
		*/

		// grab mutex to protect static variables previous_ready_*
	pthread_mutex_lock(&(TI->set_status_lock));

		// put the thread that used to be running into READY state
	if ( previous_running_tid > 0 && newstatus == THREAD_RUNNING && 
		 mytid != previous_running_tid ) 
	{
		WorkerThreadPtr_t context = CondorThreads::get_handle(previous_running_tid);
		if ( context ) {
			if ( context->status_ == THREAD_RUNNING ) {
				context->status_ = THREAD_READY;
				dprintf(D_THREADS,
					"Thread %d (%s) status change from %s to %s\n",previous_running_tid,
					context->get_name(),
					get_status_string(THREAD_RUNNING),get_status_string(THREAD_READY));
			}
		}
	}

	if ( (currentstatus == THREAD_RUNNING && newstatus == THREAD_READY))
	{
		snprintf(previous_ready_message,sizeof(previous_ready_message),
			"Thread %d (%s) status change from %s to %s\n",mytid,get_name(),
			get_status_string(currentstatus),get_status_string(newstatus));
		previous_ready_tid = mytid;
	} 
	else 
	if ( currentstatus == THREAD_READY && newstatus == THREAD_RUNNING ) 
	{
		if ( (mytid != previous_ready_tid)) {
			if ( previous_ready_tid ) {
				dprintf(D_THREADS,"%s\n",previous_ready_message);		
			}
			dprintf(D_THREADS,
				"Thread %d (%s) status change from %s to %s\n",mytid,get_name(),
				get_status_string(currentstatus),get_status_string(newstatus));
		} else {
			same_thread_running = true;
		}
		previous_ready_tid = 0;
	}
	else 
	{
		if ( previous_ready_tid ) {
			dprintf(D_THREADS,"%s\n",previous_ready_message);		
		}
		previous_ready_tid = 0;
		dprintf(D_THREADS,
			"Thread %d (%s) status change from %s to %s\n",mytid,get_name(),
			get_status_string(currentstatus),get_status_string(newstatus));
	}

	if ( newstatus == THREAD_RUNNING ) {
		previous_running_tid = mytid;
	}

	pthread_mutex_unlock(&(TI->set_status_lock));

	if ( newstatus == THREAD_RUNNING && same_thread_running == false )
	{
		// If we are about to schedule a Condor thread, and it is not the same 
		// thread we just ran, invoke the user-supplied callback.
		if ( TI->switch_callback ) {
			(*(TI->switch_callback))(user_pointer_);	
		}
	}
}


size_t
ThreadImplementation::hashFuncThreadInfo(const ThreadInfo & mythread)
{
	// For a nice distribution, break it up into unsigned int 
	// sized chunks and add together.  This is because on Linux and 
	// Win32, it is prolly the size of an unsigned int.  So on these
	// two platforms we will get a very nice distribution -vs- byte adding.
	size_t hash = 0;

	union {
		pthread_t thread;
		unsigned int  ibuf[sizeof(pthread_t)/sizeof(unsigned int)];
		unsigned char cbuf[sizeof(pthread_t)];
	} u;

	u.thread = mythread.get_pthread();

		// add up bytes in unsigned int chunks
	size_t j = 0;
	while (j < sizeof(pthread_t)) {
		hash += u.ibuf[j/sizeof(unsigned int)];
		j += sizeof(unsigned int);
	}
		// add up bytes of anything left over
	while (j < sizeof(pthread_t)) {
		hash += u.cbuf[j];
		++j;
	}

	return hash;
}


int
ThreadImplementation::yield()
{
	// Let someone else run...
	if ( get_handle()->status_ == WorkerThread::THREAD_RUNNING ) {
		get_handle()->set_status( WorkerThread::THREAD_READY );
	}
	mutex_biglock_unlock();

	// and block until we can run again.
	mutex_biglock_lock();
	get_handle()->set_status( WorkerThread::THREAD_RUNNING );

	return 0;
}

/*static*/ void
ThreadImplementation::mutex_biglock_lock()
{
	// This lock protects Condor code.  Only one thread at a time
	// will ever be permitted to have this lock.

	if (! TI ) return;

	// Note: This mutex is recursive, i.e. the kernel will peform
	// referece counting for us so we won't deadlock if the same
	// thread tries to grab the lock again.
	pthread_mutex_lock(&(TI->big_lock));	

}

/*static*/ void
ThreadImplementation::mutex_biglock_unlock()
{
	// This lock protects Condor code.  Only one thread at a time
	// will ever be permitted to have this lock.
	
	if (! TI ) return;

	// Note: This mutex is recursive, i.e. the kernel will peform
	// referece counting for us so we won't deadlock if the same
	// thread tries to grab the lock again.
	pthread_mutex_unlock(&(TI->big_lock));	
}

/*static*/ void
ThreadImplementation::mutex_handle_lock()
{
	// This lock protects the data members in the ThreadImplementation 
	// class, in particular the data members needed by get_handle().
	
	// Note: This mutex is recursive, i.e. the kernel will peform
	// referece counting for us so we won't deadlock if the same
	// thread tries to grab the lock again.
	if ( TI ) 
		pthread_mutex_lock(&(TI->get_handle_lock));	
}

/*static*/ void
ThreadImplementation::mutex_handle_unlock()
{
	// This lock protects the data members in the ThreadImplementation 
	// class, in particular the data members needed by get_handle().
	
	// Note: This mutex is recursive, i.e. the kernel will peform
	// referece counting for us so we won't deadlock if the same
	// thread tries to grab the lock again.
	if ( TI ) 
		pthread_mutex_unlock(&(TI->get_handle_lock));	
}

int
ThreadImplementation::pool_add(condor_thread_func_t routine, void* arg,
		int* pTid, const char* pDescrip)
{
	// no need to grab the big_lock here, we should already have it!
	// ASSERT big_lock ?

	// If we are out of threads, yield here until some are available.
	dprintf(D_THREADS,"Queing work to thread pool - w=%d tbusy=%d tmax=%d\n",
			(int)work_queue.size(),num_threads_busy_,num_threads_);
	while ( num_threads_busy_ >= num_threads_ ) {
		dprintf(D_ALWAYS,"WARNING: thread pool full - w=%d tbusy=%d tmax=%d\n",
			(int)work_queue.size(),num_threads_busy_,num_threads_);
		pthread_cond_wait(&workers_avail_cond,&big_lock);
	}


	WorkerThreadPtr_t newthread = 
		WorkerThread::create(pDescrip ? pDescrip : "Unnamed",routine,arg);

	// Generate a tid (task id) not currently in use.
	mutex_handle_lock();
	do {
		next_tid_++;
		if ( next_tid_ == 1 ) next_tid_++;	// tid 1 is for main thread
		if ( next_tid_ == INT_MAX ) next_tid_ = 2;	// we wrapped!
	} while ( hashTidToWorker.exists(next_tid_) == 0 );	
	int mytid = next_tid_;
	hashTidToWorker.insert(mytid,newthread);
	mutex_handle_unlock();
	
	newthread->tid_ = mytid;
	if (pTid) {
		*pTid = mytid;
	}

	// Queue up the work
	work_queue.push(newthread);

	dprintf(D_THREADS,"Thread %s tid=%d status set to %s\n",
		newthread->get_name(), newthread->get_tid(), 
		newthread->get_status_string(newthread->get_status()));


	// If the queue length is 1, signal to let our workers work is available
	if ( work_queue.size() == 1 ) {
		pthread_cond_broadcast(&work_queue_cond);
	}	

	yield();

	return mytid;
}

const WorkerThreadPtr_t 
ThreadImplementation::get_handle(int tid)
{
	static bool inserted_main_thread = false;
	static WorkerThreadPtr_t zombie = WorkerThread::create("zombie",NULL);

	if ( !TI )  {
		tid = 1;
	}

	if ( tid == 1 ) {		
		return get_main_thread_ptr();
	}

	if ( tid < 0 ) {
		tid = 0;
	}

	WorkerThreadPtr_t worker;

	mutex_handle_lock();

	if ( tid ) {
		TI->hashTidToWorker.lookup(tid,worker);
	} else {
		ThreadInfo ti( pthread_self() );
		TI->hashThreadToWorker.lookup(ti,worker);

		if ( !worker) {
			// not in our table; if this is the first time we are being
			// called, it would be from pool_init, thus we have 
			// been called from the main thread.  if so, insert the 
			// main thread into the hash.
			if ( inserted_main_thread == false ) {
				worker = get_main_thread_ptr();
				TI->hashThreadToWorker.insert(ti,worker);
				inserted_main_thread = true;
			} else {
				// if we have already inserted the main thread, and still
				// we cannot find a thread in our hash, it must be a
				// thread worker being destroyed.  since we have to return
				// some pointer, just return a pointer to a dummy struct.
				worker = zombie;
			}
		}
	}

	mutex_handle_unlock();

	return worker;
}

void
ThreadImplementation::set_switch_callback(condor_thread_switch_callback_t func)
{
	switch_callback = func;
}

int
ThreadImplementation::get_tid()
{
	int return_val = 0;

#ifdef WIN32
	return_val = m_CurrentTid;
#else
	int *saved_tidp = (int *) pthread_getspecific(m_CurrentTidKey);
	if ( saved_tidp ) {
		return_val = *saved_tidp;
	}
#endif

	return return_val;
}

void
ThreadImplementation::setCurrentTid(int tid)
{
#ifdef WIN32
	m_CurrentTid = tid;
#else
	int *saved_tidp = (int *) pthread_getspecific(m_CurrentTidKey);
	if ( !saved_tidp ) {
		saved_tidp = (int * )malloc(sizeof(int));
		ASSERT(saved_tidp);
		pthread_setspecific(m_CurrentTidKey,(void *)saved_tidp);
	}
	*saved_tidp = tid;
#endif
}

ThreadStartFunc_t
ThreadImplementation::threadStart(void *)
{
	WorkerThreadPtr_t item;
	pthread_t pthread_info = pthread_self();
	ThreadInfo ti(pthread_info);

	pthread_detach( pthread_info );

	mutex_biglock_lock();
	
	for (;;) {				
		while ( TI->work_queue.empty() ) {
			// we are out of work; yield this thread until something to do
			pthread_cond_wait(&(TI->work_queue_cond),&(TI->big_lock));
		}
		
		// grab next work item
		item = TI->work_queue.front();
		TI->work_queue.pop();

		// stash our current tid for speedy lookup 
		TI->setCurrentTid( item->get_tid() );

		mutex_handle_lock();
		// map this thread to this work unit 
		if ( TI->hashThreadToWorker.insert(ti,item) < 0 ) {
			// Insert failed; this thread must already be in the
			// hashtable.  This should never happen!
			EXCEPT("Threading data structures inconsistent!");
		}		
		mutex_handle_unlock();

		// update status for this work unit
		item->set_status( WorkerThread::THREAD_RUNNING );

		// update our count of how many threads are busy
		(TI->num_threads_busy_)++;
		ASSERT(TI->num_threads_busy_ <= TI->num_threads_);
		
		// Call the user supplied routine to do the work.
		// note: work performed with big_lock!
		(*(item->get_routine()))(item->get_arg());	
		
		// update our count of how many threads are busy, emitting 
		// a signal if needed to wake up threads blocked on adding work.
		if (TI->num_threads_ == TI->num_threads_busy_ ) {
			// if all workers were busy, emit a signal saying one is now avail
			pthread_cond_broadcast(&(TI->workers_avail_cond));
		}
		(TI->num_threads_busy_)--;


		mutex_handle_lock();
		// this thread is done w/ this work item, unmap it.
		if ( TI->hashThreadToWorker.remove(ti) < 0 ) {
			// Removal failed because we didn't find this thread
			// in our hashtable. This should never happen!
			EXCEPT("Threading data structures inconsistent!");
		}		
		mutex_handle_unlock();


		// update status
		item->set_status( WorkerThread::THREAD_COMPLETED );
	}

	// cast is needed to keep both windows and !windows happy with the return value
	return (ThreadStartFunc_t)NULL;
}

#ifndef WIN32
	// Called by pthreads to deallocate m_CurrentTidKey data
void free_CurrentTidKey(void *tidp)
{
	if ( tidp ) free(tidp);
}
#endif

void
ThreadImplementation::initCurrentTid()
{
#ifndef WIN32
	pthread_key_create(&m_CurrentTidKey, free_CurrentTidKey);
#endif
	setCurrentTid(0);
}

ThreadImplementation::ThreadImplementation()
	: hashThreadToWorker(hashFuncThreadInfo), hashTidToWorker(hashFuncInt)
{
	num_threads_ = 0;
	num_threads_busy_ = 0;
	next_tid_ = 0;
	switch_callback = NULL;
	pthread_mutexattr_t mutex_attrs;
	pthread_mutexattr_init(&mutex_attrs);
#if defined(PTHREAD_MUTEX_RECURSIVE_NP)
	pthread_mutexattr_settype(&mutex_attrs,PTHREAD_MUTEX_RECURSIVE_NP);
#else
	pthread_mutexattr_settype(&mutex_attrs,PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&big_lock,&mutex_attrs);
	pthread_mutex_init(&get_handle_lock,&mutex_attrs);
	pthread_mutex_init(&set_status_lock,&mutex_attrs);
	pthread_cond_init(&work_queue_cond,NULL);
	pthread_cond_init(&workers_avail_cond,NULL);	
	initCurrentTid();
}

ThreadImplementation::~ThreadImplementation()
{
	// It is assumed no threads in the pool are active when 
	// this dtor is invoked.
	pthread_mutex_destroy(&big_lock);
	pthread_mutex_destroy(&get_handle_lock);
	pthread_mutex_destroy(&set_status_lock);
#ifndef WIN32
	pthread_key_delete(m_CurrentTidKey);
#endif
	// TODO pthread_cond_destroy ....
}

int 
ThreadImplementation::pool_init()
{
	int i;
	
		// For now, only allow the COLLECTOR to have a thread pool.
	if ( strcmp(get_mySubSystem()->getName(),"COLLECTOR")==0 ) {
		num_threads_ = param_integer("THREAD_WORKER_POOL_SIZE",0,0);
	} else {
		num_threads_ = 0;
	}

	if ( num_threads_ == 0 ) {
		// dont waste time w/ locks if user doesn't want threads.
		return num_threads_;
	}

	// We need to grab the big lock _before_ we make
	// our thread pool.  We only release the lock when
	// we want to yield to another thread.
	mutex_biglock_lock();

	// initialize static variables for the main thread so that subsequent calls
	// to get_handle() are thread safe.  also, perform a sanity
	// check that pool_init() is only being called from the main
	// thread.
	bool sane =  ( get_main_thread_ptr().get() == get_handle().get() );
	if (!sane) {
		EXCEPT("Thread pool not initialized in the main thread");
	}

	// create the worker threads aggressively right now.
	for (i=0; i < num_threads_; i++ ) {
#ifdef WIN32
		_beginthread(ThreadImplementation::threadStart,0,NULL);
#else
		// TODO --- set thread signal mask
		pthread_t notUsed;
		int result = pthread_create(&notUsed,NULL,ThreadImplementation::threadStart,NULL);
		ASSERT( result == 0 );
#endif
	}

	if ( num_threads_ > 0 ) {
		setCurrentTid(1);
	}

	return num_threads_;
}

int 
ThreadImplementation::start_thread_safe_block()
{
	// we must be very careful about what code we invoke here since
	// this method is supposed to be thread safe.
	
	// note: get_handle() should be thread safe at this point.
	WorkerThreadPtr_t context = get_handle();

	// TODO - perhaps protect parallel flag?
	if (!context->enable_parallel_flag_) {
		// parallel mode disabled; this method should be a no-op
		return 1;
	}

#if 0 // until we fix how this interacts w/ yield
	// TODO --- record global process state here if not yet parallel?

	// note: we should be able to safely  mess with parallel_mode_count_
	// since the end user is not able to twiddle with it (it is private).
	context->parallel_mode_count_ += 1;

	// is ASSERT/EXCEPT thread safe?  dunno, but not much we can do.
	if ( context->parallel_mode_count_ < 1 ) {		
		EXCEPT("Thread %d has thread safe block mismatch",context->tid_);
	}
	
	
	if ( context->parallel_mode_count_ == 1 ) {
		// lord help us.  release the big lock!
		mutex_biglock_unlock();
	}
#endif

	// TODO - once set_status is for certain thread safe, perhaps
	// we should set the status of the thread to IO.
	
	mutex_biglock_unlock();

	return 0;
}

int 
ThreadImplementation::stop_thread_safe_block()
{
	// we must be very careful about what code we invoke here until
	// we have the big_lock.  at this moment, we likely do not!

	// note: get_handle() should be thread safe at this point.
	WorkerThreadPtr_t context = get_handle();

	// TODO - perhaps protect parallel flag?
	if (!context->enable_parallel_flag_) {
		// parallel mode disabled; this method should be a no-op
		return 1;
	}

#if 0 // until we fix how this interacts w/ yield
	// note: we should be able to safely  mess with parallel_mode_count_
	// since the end user is not able to twiddle with it (it is private).
	context->parallel_mode_count_ -= 1;

	// is ASSERT/EXCEPT thread safe?  dunno, but not much we can do.
	if ( context->parallel_mode_count_ < 0 ) {
		EXCEPT("Thread %d has thread safe block mismatch",context->tid_);
	}

	if ( context->parallel_mode_count_ == 0 ) {
		context->set_status( WorkerThread::THREAD_READY );
		// block until we can grab our big lock
		mutex_biglock_lock();
	}
#endif

	mutex_biglock_lock();
		// don't use context anymore, we are now in a different thread!
	get_handle()->set_status( WorkerThread::THREAD_RUNNING );

	return 0;
}

void
ThreadImplementation::remove_tid(int tid)
{
	// don't remove tid 1
	if (tid < 2 ) {
		return;
	}

	mutex_handle_lock();
	hashTidToWorker.remove(tid);
	mutex_handle_unlock();
}

#else	// ifdef HAVE_PTHREADS

/* Stubs for systems that do not have pthreads */

void
WorkerThread::set_status(thread_status_t newstatus)
{
		// THREAD_COMPLETED is a terminal state; don't allow any changes
	if ( status_ == THREAD_COMPLETED  ) {		
		return;
	}

		// change the state.
	status_ = newstatus;
}

size_t
ThreadImplementation::hashFuncThreadInfo(const ThreadInfo & /*mythread*/)
{
	return 1;
}

ThreadImplementation::ThreadImplementation()
	: hashThreadToWorker(hashFuncThreadInfo), hashTidToWorker(hashFuncInt)
{
	return;
}

ThreadImplementation::~ThreadImplementation()
{
	return;
}

int ThreadImplementation::start_thread_safe_block()
{
	return -1;
}

int ThreadImplementation::stop_thread_safe_block()
{
	return -1;
}

int ThreadImplementation::yield() 
{
	return -1;
}

int ThreadImplementation::get_tid()
{
	return -1;
}

void ThreadImplementation::set_switch_callback(condor_thread_switch_callback_t )
{
	return;
}

int ThreadImplementation::pool_init()
{
	return -1;
}

int ThreadImplementation::pool_add(condor_thread_func_t , void *, int *, const char *)
{
	return -1;
}

const WorkerThreadPtr_t ThreadImplementation::get_handle(int /*tid*/)
{
	return get_main_thread_ptr();
}

void ThreadImplementation::remove_tid(int)
{
	return;
}

#endif	// ifdef else HAVE_PTHREADS

/**********************************************************************/

int 
CondorThreads::pool_init()
{
	static bool already_been_here = false;

	// no reconfig support (yet)
	if ( already_been_here ) return -2;
	already_been_here = true;

	TI = new ThreadImplementation;
	int numThreads = TI->pool_init();
	if ( numThreads < 1 ) {
		delete TI;
		TI = NULL;
	}

	return numThreads;
}

int
CondorThreads::pool_size()
{
	int size;

	if (TI) {
		size = TI->pool_size();
	} else {
		size = 0;
	}

	return size;
}

int 
CondorThreads::pool_add(condor_thread_func_t routine, void* arg, int* tid,
				const char* descrip)
{

	// If we are using threads, call implementation.
	// If no threads, then just call the routine right now.

	if ( !TI ) {
		if ( tid )  {
			*tid = 0;
		}
		(*(routine))(arg);	// call routine directly
		return 0;
	}

	return TI->pool_add(routine,arg,tid,descrip);
}

int
CondorThreads::start_thread_safe_block()
{
	if (!TI) return -1;
	return TI->start_thread_safe_block();
}

int
CondorThreads::stop_thread_safe_block()
{
	if (!TI) return -1;
	return TI->stop_thread_safe_block();
}

int 
CondorThreads::yield()
{
	if (!TI) return -1;
	return TI->yield();
}

void 
CondorThreads::set_switch_callback(condor_thread_switch_callback_t func)
{
	if (!TI) return;
	TI->set_switch_callback(func);
}

int 
CondorThreads::get_tid()
{
	if (!TI) return -1;
	return TI->get_tid();
}

const WorkerThreadPtr_t 
CondorThreads::get_handle(int tid)
{
	// we need to give something back even if TI==NULL
	return ThreadImplementation::get_handle(tid);
}

	/* Some simple C wrappers for use by dprintf */
extern "C" {
	int CondorThreads_pool_size() 
	{
		return CondorThreads::pool_size();
	}

	int CondorThreads_gettid()
	{
		return CondorThreads::get_tid();

	}
}	// of extern C
