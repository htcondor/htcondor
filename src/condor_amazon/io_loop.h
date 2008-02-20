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

#ifndef IO_LOOP_H
#define IO_LOOP_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "gahp_common.h"
#include "simplelist.h"
#include "PipeBuffer.h"
#include "request.h"

#define GAHP_COMMAND_ASYNC_MODE_ON "ASYNC_MODE_ON"
#define GAHP_COMMAND_ASYNC_MODE_OFF "ASYNC_MODE_OFF"
#define GAHP_COMMAND_RESULTS "RESULTS"
#define GAHP_COMMAND_QUIT "QUIT"
#define GAHP_COMMAND_VERSION "VERSION"
#define GAHP_COMMAND_COMMANDS "COMMANDS"

#define GAHP_RESULT_SUCCESS "S"
#define GAHP_RESULT_ERROR "E"
#define GAHP_RESULT_FAILURE "F"

class Worker {
 public:
	 Worker() {
		 m_pid = 0;
		 m_can_use = true;
		 m_need_kill = false;
	 }

	int resultHandler();
	bool removeRequest(int req_id);
	bool canUse(void) { return (!m_need_kill && m_can_use); }
	int numOfRequest(void) { return m_request_list.Number(); }
				   
	PipeBuffer m_request_buffer;
	PipeBuffer m_result_buffer;
	
	int m_pid;
	bool m_can_use;
	bool m_need_kill;
	SimpleList<Request*> m_request_list;
};

#define MIN_NUMBER_WORKERS 2
#define WORKER_MANAGER_TIMER_INTERVAL	15

class IOProcess : public Service {
 public:
	IOProcess();
	~IOProcess();
	 
	bool startUp(int stdin_pipe, const char* worker_prog, int min_workers, int max_workers);

	int stdinPipeHandler();
	int workerThreadReaper(int pid, int exit_status);
	void flushRequest(Request* request);
	int flushPendingRequests();

	Worker* createNewWorker(void);
	int numOfWorkers(void) { return m_workers_list.getNumElements(); }
	int numOfRealWorkers(void);

	Worker* findFreeWorker(void);
	Worker* findWorkerWithFewestRequest(void);
	Worker* findWorker(int pid);
	Worker* findWorkerwithFewestRequest(void);

	int workerManager();
	void resetWorkerManagerTimer(void);
	void killWorker(int pid, bool graceful);
	void removeAllRequestsFromWorker(Worker *worker);

	Request* addNewRequest(const char* cmd);	
	Request* findRequest(int req_id);
	bool removeRequest(int req_id);

	void addResult(const char* result);
	int numOfResult(void) { return m_result_list.number(); }
	void startResultIteration(void) { m_result_list.rewind(); }
	char* NextResult(void) { return m_result_list.next(); }
	void deleteCurrentResult(void) { m_result_list.deleteCurrent(); }

	bool is_shutdowning;
	int m_async_mode;
	int m_new_results_signaled;
	int m_min_workers;
	int m_max_workers;

 private: 
	void killWorker(Worker *worker, bool graceful);

	int m_flush_request_tid;
	int m_worker_manager_tid;
	int m_reaper_id;
	MyString m_worker_exec_name;
	PipeBuffer m_stdin_buffer;

	StringList m_result_list; // The list of results ready to be output to IO
	HashTable<int, Worker*> m_workers_list;
	HashTable<int, Request*> m_pending_req_list;
};

#endif
