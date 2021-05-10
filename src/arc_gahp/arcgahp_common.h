/***************************************************************
 *
 * Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

#ifndef __ARCGAHP_COMMON_H__
#define __ARCGAHP_COMMON_H__

#include "gahp_common.h"
#include <string>
#include "string_list.h"



#define ARC_COMMAND_SUCCESS_OUTPUT	"NULL"
//#define ARC_COMMAND_ERROR_OUTPUT	"1"

#define ARC_HTTP_PROXY   "ARC_HTTP_PROXY"

class GahpRequest;

typedef bool (*ioCheckfn)(char **argv, int argc);
typedef bool (*workerfn)(GahpRequest *gahp_request);

class Worker;

class GahpRequest {
 public:
	GahpRequest(const char* cmd, const std::string &proxy_file);

	int m_reqid;
	Worker* m_worker;

	std::string m_proxy_file;
	std::string m_raw_cmd;
	Gahp_Args m_args;
	std::string m_result;
};

class ArcGahpCommand
{
	public:
		ArcGahpCommand(const char*, ioCheckfn, workerfn);
		std::string command;
		ioCheckfn iocheckfunction;
		workerfn workerfunction;
};

void
registerArcGahpCommand(const char* command, ioCheckfn iofunc, workerfn workerfunc);
int numofArcCommands(void);
int allArcCommands(StringList &output);
bool executeIOCheckFunc(const char* cmd, char **argv, int argc);
bool executeWorkerFunc(const char* cmd, GahpRequest *gahp_request);

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

//std::string create_result_string(int req_id, const std::string &result_code, const std::string &result_msg);
std::string create_result_string(int req_id, const std::string &result_code, const std::string &result_msg, const std::vector<std::string> &args = std::vector<std::string>());

std::string create_failure_result( int req_id, const char *err_msg, const char* err_code = NULL);
std::string create_success_result( int req_id, StringList *result_list);

void set_arc_proxy_server(const char* url);
bool get_arc_proxy_server(const char* &host_name, int& port, const char* &user_name, const char* &passwd );

#endif
