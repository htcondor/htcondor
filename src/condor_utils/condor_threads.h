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


#ifndef _CONDOR_THREADS_H
#define _CONDOR_THREADS_H

BEGIN_C_DECLS

#ifdef _GNUC_
#define start_thread_safe(a) _mark_thread_safe(1, 1, a, __FUNCTION__, __FILE__, __LINE__)
#define stop_thread_safe(a) _mark_thread_safe(2, 1, a, __FUNCTION__, __FILE__, __LINE__)
#else
#define start_thread_safe(a) _mark_thread_safe(1, 1, a, "unknown", __FILE__, __LINE__)
#define stop_thread_safe(a) _mark_thread_safe(2, 1, a, "unknown", __FILE__, __LINE__)
#endif

typedef int (*mark_thread_func_t)();
void _mark_thread_safe_callback(mark_thread_func_t start_routine, mark_thread_func_t stop_routine);
void _mark_thread_safe(int start_or_stop, int dologging, const char* descrip, 
					   const char* func, const char* file, int line);

	/* couple of simple C wrappers for use by dprintf... */
int CondorThreads_pool_size(void);
int CondorThreads_gettid(void);

END_C_DECLS
#ifdef __cplusplus
#include <memory>

class ThreadImplementation;	// forward declaration
class WorkerThread;			// forward declaration
class EnableParallel;		// forward declaration

#define ScopedEnableParallel(m) std::shared_ptr<EnableParallel> _enableparallel(new EnableParallel( m ))

typedef void (*condor_thread_func_t)(void*);
typedef void (*condor_thread_switch_callback_t)(void* &);
typedef std::shared_ptr<WorkerThread> WorkerThreadPtr_t;


class WorkerThread
{
	friend class ThreadImplementation;
public:
	~WorkerThread();
		
	// if you add a status, update get_status_string()
	enum thread_status_t { 
		THREAD_UNBORN,		// waiting to be assigned a thread for execution
		THREAD_READY,		// ready to run, waiting for its CPU quanta
		THREAD_RUNNING,		// currently running
		THREAD_WAITING,		// waiting for an event signal
		THREAD_COMPLETED	// all done; the start routine returned
	};

	static WorkerThreadPtr_t create(const char* name, condor_thread_func_t routine, void* arg = NULL);
	bool enable_parallel(bool flag) {bool ret=enable_parallel_flag_; enable_parallel_flag_=flag; return ret;}
	thread_status_t get_status() { return status_; }
	static const char* get_status_string(thread_status_t status);
	const char* get_name() const {return name_;}
	condor_thread_func_t get_routine() const {return routine_;}
	void* get_arg() const {return arg_;}  // get args to routine
	int get_tid() const {return tid_;}

	int operator== (const WorkerThread& r) const {return tid_ == r.tid_; }
	int operator< (const WorkerThread& r) const {return tid_ < r.tid_; }

	void *user_pointer_;

private:
	// Ctors are private since only the ThreadImplementation class
	// should create WorkerThread objects.
	WorkerThread();
	WorkerThread(const char* name, condor_thread_func_t routine, void* arg = NULL);	
	void set_status(thread_status_t status);
	condor_thread_func_t routine_;
	void *arg_;	// will NOT be deleted
	char *name_;
	int tid_;
	bool enable_parallel_flag_;
	int parallel_mode_count_;
	thread_status_t status_;	// use set_status(), get_status()
};

class CondorThreads
{
public:
	static int pool_init();
	static int pool_add(condor_thread_func_t routine, void* arg, 
			int* tid=NULL, const char* descrip=NULL);
	static int pool_size();

	static void set_switch_callback(condor_thread_switch_callback_t func);

	static int get_tid();

	static const WorkerThreadPtr_t get_handle(int tid = 0);

	static int yield();

	static int start_thread_safe_block();
	static int stop_thread_safe_block();

	inline static bool enable_parallel(bool flag) {return get_handle()->enable_parallel(flag); }
};

class EnableParallel
{
public:
	EnableParallel(bool flag) { oldflag = CondorThreads::enable_parallel(flag); }
	~EnableParallel() { CondorThreads::enable_parallel(oldflag); }
	int operator< (const EnableParallel& r) { return this < &r; }
	int operator== (const EnableParallel& r) { return this == &r; }
private:
	bool oldflag;

};

#endif // __cplusplus

#endif // _CONDOR_THREADS_H
