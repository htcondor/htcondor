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


/*
   THIS FILE SHOULD ONLY BE INCLUDED INTO condor_thread.C
   DO NOT INCLUDE THIS HEADER FILE INTO ANY OTHER MODULE!
*/

#ifndef _THREADS_IMPLEMENTATION_H
#define _THREADS_IMPLEMENTATION_H

#ifdef WIN32
	typedef void ThreadStartFunc_t;
#else
	typedef void* ThreadStartFunc_t;
	#ifdef HAVE_PTHREADS
		#include <pthread.h>
	#else
		typedef long unsigned int pthread_t;
	#endif
	//#ifndef HAVE_PTHREADS
	//	pthread_t pthread_self() { return (pthread_t) 1; }
	//#endif
#endif
		

// Thin wrapper around pthread_t that defines an operator== for use by
// our hashtable template.
class ThreadInfo {
public:
	ThreadInfo() { pt_ = pthread_self(); }
	ThreadInfo(pthread_t pt) { pt_ = pt; }	
	pthread_t get_pthread() const { return pt_; }
	friend bool operator<(const ThreadInfo &lhs, const ThreadInfo &rhs) {
		return lhs.pt_ < rhs.pt_;
	}
private:
	pthread_t pt_;
};


/** Class to implement the CondorThreads public interface.  
	We it this way so that the condor_threads.h header file does not have
	any implementation code so we only have to pull in pthreads.h into this
	file instead of into everything
*/
class ThreadImplementation
{
public:
	ThreadImplementation();
	~ThreadImplementation();

	/******* IMPLEMENT CondorThreads INTERFACE *********/
	int start_thread_safe_block();
	int stop_thread_safe_block();
	int yield();
	int pool_init(int num_threads);
	int pool_add(condor_thread_func_t routine, void* arg, int* tid=NULL,
				 const char* descrip=NULL);
	static const WorkerThreadPtr_t get_handle(ThreadInfo tid = 0);
	int pool_size() { return num_threads_; }
	int get_tid();
	void set_switch_callback(condor_thread_switch_callback_t func);

	/****** HELPER METHODS *********/
	static ThreadStartFunc_t threadStart(void *);
	static size_t hashFuncThreadInfo(const ThreadInfo & mythread);
	static const WorkerThreadPtr_t get_main_thread_ptr();
	void remove_tid(int tid);
	static void mutex_biglock_lock();
	static void mutex_biglock_unlock();
	static void mutex_handle_lock();
	static void mutex_handle_unlock();
	void setCurrentTid(int tid);
	void initCurrentTid();

	/****** DATA MEMBERS *******/
	pthread_mutex_t big_lock;	// big lock protecting condor code
	pthread_mutex_t get_handle_lock;	// lock protecting method get_handle()
	pthread_mutex_t set_status_lock;	// lock protecting set_status() shared data
	std::map<ThreadInfo,WorkerThreadPtr_t> hashThreadToWorker;
	std::map<int,WorkerThreadPtr_t> hashTidToWorker;
	condor_thread_switch_callback_t switch_callback;
	pthread_key_t m_CurrentTidKey;
#ifdef WIN32
	static THREAD_LOCAL_STORAGE int m_CurrentTid;
#endif

		// Members dealing with our work pool
	int num_threads_ , num_threads_busy_;	
	pthread_cond_t workers_avail_cond;  // signalled when workers are available
	pthread_cond_t work_queue_cond;	    // signalled when work queue is not empty
	std::queue<WorkerThreadPtr_t> work_queue;
	int next_tid_;
};


#endif // _THREADS_IMPLEMENTATION_H
