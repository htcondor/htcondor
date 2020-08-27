/***************************************************************
 *
 * Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

#include <pthread.h>
#include "condor_common.h"
#include "gahp_common.h"
#include "simplelist.h"
#include "PipeBuffer.h"

#define GAHP_COMMAND_ASYNC_MODE_ON "ASYNC_MODE_ON"
#define GAHP_COMMAND_ASYNC_MODE_OFF "ASYNC_MODE_OFF"
#define GAHP_COMMAND_RESULTS "RESULTS"
#define GAHP_COMMAND_QUIT "QUIT"
#define GAHP_COMMAND_VERSION "VERSION"
#define GAHP_COMMAND_COMMANDS "COMMANDS"

#define GAHP_RESULT_SUCCESS "S"
#define GAHP_RESULT_ERROR "E"
#define GAHP_RESULT_FAILURE "F"

// Forward declaration of Worker, referenced in Request.
class Worker;

class Request {
 public:
	Request(const char* cmd);

	int m_reqid;
	Worker* m_worker;

	std::string m_raw_cmd;
	Gahp_Args m_args;
	std::string m_result;
};

class Worker {
 public:
	Worker(int worker_id);
	~Worker();

	bool removeRequest(int req_id);
	int numOfRequest(void) const { return m_request_list.Number(); }

	int m_id;

	bool m_is_doing;
	bool m_is_waiting;

	pthread_cond_t m_cond;

	SimpleList<Request*> m_request_list;
};

#define MIN_NUMBER_WORKERS 2
#define MAX_NUMBER_WORKERS 50
#define WORKER_MANAGER_TIMER_INTERVAL	15

class IOProcess {
 public:
	IOProcess();
	~IOProcess();

	bool startUp(int stdin_pipe, int min_workers, int max_workers);

	int stdinPipeHandler();

	Worker* createNewWorker(void);
	Worker* findFreeWorker(void);
	Worker* findWorker(int id);
	bool removeWorkerFromWorkerList(int id);

	Request* addNewRequest(const char* cmd);
	void addResult(const char* result);

	int numOfResult(void) { return m_result_list.number(); }
	void startResultIteration(void) { m_result_list.rewind(); }
	char* NextResult(void) { return m_result_list.next(); }
	void deleteCurrentResult(void) { m_result_list.deleteCurrent(); }
	Request* popPendingRequest(void);
	int numOfPendingRequest(void);

	bool m_async_mode;
	bool m_new_results_signaled;
	int m_min_workers;
	int m_max_workers;

	int m_avail_workers_num;

 private:
	void addRequestToWorker(Request* request, Worker* worker);

	int newWorkerId(void);
	int m_next_worker_id;
	int m_rotated_worker_ids;

	PipeBuffer m_stdin_buffer;

	StringList m_result_list; // The list of results ready to be output to IO

	HashTable<int, Worker*> m_workers_list;

	SimpleList<Request*> m_pending_req_list;
};

#endif
