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
#include "condor_daemon_core.h" // For Stream decl
#include "gahp_common.h"
#include "PipeBuffer.h"

#define GAHP_COMMAND_JOB_SUBMIT "CONDOR_JOB_SUBMIT"
#define GAHP_COMMAND_JOB_REMOVE "CONDOR_JOB_REMOVE"
#define GAHP_COMMAND_JOB_COMPLETE "CONDOR_JOB_COMPLETE"
#define GAHP_COMMAND_JOB_STATUS_CONSTRAINED "CONDOR_JOB_STATUS_CONSTRAINED"
#define GAHP_COMMAND_JOB_UPDATE_CONSTRAINED "CONDOR_JOB_UPDATE_CONSTRAINED"
#define GAHP_COMMAND_JOB_UPDATE "CONDOR_JOB_UPDATE" // Update by job id
#define GAHP_COMMAND_JOB_HOLD "CONDOR_JOB_HOLD"
#define GAHP_COMMAND_JOB_RELEASE "CONDOR_JOB_RELEASE"
#define GAHP_COMMAND_JOB_STAGE_IN "CONDOR_JOB_STAGE_IN"
#define GAHP_COMMAND_JOB_STAGE_OUT "CONDOR_JOB_STAGE_OUT"
#define GAHP_COMMAND_JOB_REFRESH_PROXY "CONDOR_JOB_REFRESH_PROXY"
#define GAHP_COMMAND_JOB_UPDATE_LEASE "CONDOR_JOB_UPDATE_LEASE"

#define GAHP_COMMAND_ASYNC_MODE_ON "ASYNC_MODE_ON"
#define GAHP_COMMAND_ASYNC_MODE_OFF "ASYNC_MODE_OFF"
#define GAHP_COMMAND_RESULTS "RESULTS"
#define GAHP_COMMAND_QUIT "QUIT"
#define GAHP_COMMAND_VERSION "VERSION"
#define GAHP_COMMAND_COMMANDS "COMMANDS"
#define GAHP_COMMAND_INITIALIZE_FROM_FILE "INITIALIZE_FROM_FILE"
#define GAHP_COMMAND_UPDATE_TOKEN_FROM_FILE "UPDATE_TOKEN"
#define GAHP_COMMAND_REFRESH_PROXY_FROM_FILE "REFRESH_PROXY_FROM_FILE"

#define GAHP_RESULT_SUCCESS "S"
#define GAHP_RESULT_ERROR "E"
#define GAHP_RESULT_FAILURE "F"


struct inter_thread_io_t {
  int request_pipe[2];
  int result_pipe[2];

};

int stdin_pipe_handler(int);
int result_pipe_handler(int);



void gahp_output_return (const char ** , const int );
void gahp_output_return_success();
void gahp_output_return_error();


int parse_gahp_command (const char *, Gahp_Args *);
int verify_gahp_command(char ** argv, int argc);
int verify_request_id(const char *);
int verify_schedd_name (const char *);
int verify_job_id (const char *);
int verify_class_ad (const char *);
int verify_constraint (const char * s);
int verify_number (const char*);
int verify_number_args (const int, const int);

void flush_request (int worker_id, const char * request);
void flush_pending_requests(int tid);

int worker_thread_reaper(int pid, int exit_status);

class Worker : public Service {
 public:
	void Init (int _id) { id = _id; }

	int result_handler (int) {
		return result_pipe_handler(id);
	}
				   
	PipeBuffer request_buffer;
	PipeBuffer result_buffer;

	int result_pipe[2];
	int request_pipe[2];

    int flush_request_tid;
	int pid;

	int id;
};

#endif
