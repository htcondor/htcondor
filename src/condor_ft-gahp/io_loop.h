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
#include "file_transfer.h"

#define GAHP_COMMAND_DOWNLOAD_SANDBOX "DOWNLOAD_SANDBOX"
#define GAHP_COMMAND_UPLOAD_SANDBOX "UPLOAD_SANDBOX"
#define GAHP_COMMAND_DESTROY_SANDBOX "DESTROY_SANDBOX"
#define GAHP_COMMAND_DOWNLOAD_PROXY "DOWNLOAD_PROXY"
#define GAHP_COMMAND_GET_SANDBOX_PATH "GET_SANDBOX_PATH"
#define GAHP_COMMAND_CREATE_CONDOR_SECURITY_SESSION "CREATE_CONDOR_SECURITY_SESSION"
#define GAHP_COMMAND_CONDOR_VERSION "CONDOR_VERSION"

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

// the struct we store per-sandbox
struct SandboxEnt {
	int pid;
	std::string sandbox_id;
	std::string request_id;
	int error_pipe;
//	bool is_download;
//	FileTransfer *ft;
};

// our map of <sandbox_id> to <sandbox_struct>
//typedef std::unordered_map<std::string, struct SandboxEnt> SandboxMap;
typedef std::unordered_map<int, struct SandboxEnt> SandboxMap;
SandboxMap sandbox_map;


int stdin_pipe_handler(int);
void handle_results( std::string line );


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

int ftgahp_reaper(FileTransfer *);

void enqueue_result (const std::string &req_id, const char ** results, const int argc);
void enqueue_result (int req_id, const char ** results, const int argc);

void define_sandbox_path(std::string sid, std::string &path);

void define_path(std::string sid, std::string &path);
bool create_sandbox_dir (std::string sid, std::string &iwd);
bool download_proxy(const std::string &sid, const ClassAd &ad, std::string &err);
bool destroy_sandbox(std::string sid, std::string &err);

int do_command_download_sandbox(void *arg, Stream*);
int do_command_upload_sandbox(void *arg, Stream*);
int do_command_download_proxy(void *arg, Stream*);
int do_command_destroy_sandbox(void *arg, Stream*);

int download_sandbox_reaper(int, int);
int upload_sandbox_reaper(int, int);
int download_proxy_reaper(int, int);
int destroy_sandbox_reaper(int, int);

#endif
