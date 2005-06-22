/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef IO_LOOP_H
#define IO_LOOP_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h" // For Stream decl

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

#define GAHP_COMMAND_ASYNC_MODE_ON "ASYNC_MODE_ON"
#define GAHP_COMMAND_ASYNC_MODE_OFF "ASYNC_MODE_OFF"
#define GAHP_COMMAND_RESULTS "RESULTS"
#define GAHP_COMMAND_QUIT "QUIT"
#define GAHP_COMMAND_VERSION "VERSION"
#define GAHP_COMMAND_COMMANDS "COMMANDS"
#define GAHP_COMMAND_INITIALIZE_FROM_FILE "INITIALIZE_FROM_FILE"
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


int parse_gahp_command (const char *, char ***, int *);
int verify_gahp_command(char ** argv, int argc);
int verify_request_id(const char *);
int verify_schedd_name (const char *);
int verify_job_id (const char *);
int verify_class_ad (const char *);
int verify_constraint (const char * s);
int verify_number_args (const int, const int);

void queue_request (const char * request);
int flush_next_request();

int worker_thread_reaper (Service*, int pid, int exit_status);


#endif
