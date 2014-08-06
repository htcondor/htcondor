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

#ifndef __GCEGAHP_COMMON_H__
#define __GCEGAHP_COMMON_H__

#include "gahp_common.h"
#include <string>
#include "string_list.h"
#include "gceCommands.h"

#define GCE_COMMAND_SUCCESS_OUTPUT	"NULL"
//#define GCE_COMMAND_ERROR_OUTPUT	"1"

#define GCE_HTTP_PROXY   "GCE_HTTP_PROXY"

typedef bool (*ioCheckfn)(char **argv, int argc);
typedef bool (*workerfn)(char **argv, int argc, std::string &output_string);

class GceGahpCommand
{
	public:
		GceGahpCommand(const char*, ioCheckfn, workerfn);
		std::string command;
		ioCheckfn iocheckfunction;
		workerfn workerfunction;
};

void
registerGceGahpCommand(const char* command, ioCheckfn iofunc, workerfn workerfunc);
int numofGceCommands(void);
int allGceCommands(StringList &output);
bool executeIOCheckFunc(const char* cmd, char **argv, int argc);
bool executeWorkerFunc(const char* cmd, char **argv, int argc, std::string &output_string);

int parse_gahp_command (const char* raw, Gahp_Args* args);

bool check_read_access_file(const char *file);
bool check_create_file(const char *file, mode_t mode);

int get_int (const char *, int *);
int get_ulong (const char *, unsigned long *);

int verify_number (const char*);
int verify_request_id(const char *);
int verify_string_name(const char *);
int verify_number_args (const int, const int);
int verify_min_number_args (const int, const int);

bool check_access_and_secret_key_file(const char* accesskeyfile, const char* secretkeyfile, std::string &err_msg);

std::string create_failure_result( int req_id, const char *err_msg, const char* err_code = NULL);
std::string create_success_result( int req_id, StringList *result_list);

void set_gce_proxy_server(const char* url);
bool get_gce_proxy_server(const char* &host_name, int& port, const char* &user_name, const char* &passwd );

#endif
