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

#ifndef CONDOR_VMGAHP_COMMON_H
#define CONDOR_VMGAHP_COMMON_H

#include "condor_common.h"
#include "condor_debug.h"
#include "vm_univ_utils.h"
#include "vmgahp.h"

#define CONDOR_VMGAHP_VERSION	"0.0.1"

#define ROOT_UID	0

#define vmprintf dprintf

extern int vmgahp_stdout_pipe;
extern int vmgahp_stderr_pipe;
extern PBuffer vmgahp_stderr_buffer;
extern int vmgahp_stderr_tid;
#ifndef vmprintf
extern int oriDebugFlags;
#endif
extern int vmgahp_mode;

bool parse_vmgahp_command(const char* raw, std::vector<std::string>& args);

bool verify_vm_type(const char *vmtype);
bool check_vm_read_access_file(const char *file, bool is_root = false);
bool check_vm_write_access_file(const char *file, bool is_root = false);
bool check_vm_execute_file(const char *file, bool is_root = false);

bool write_local_settings_from_file(FILE* out_fp,
                                    const char* param_name,
                                    const char* start_mark = NULL,
                                    const char* end_mark = NULL);

bool verify_digit_arg(const char *s);
bool verify_number_args(const int is, const int should_be);

#ifndef vmprintf
void vmprintf( int flags, const char *fmt, ... ) CHECK_PRINTF_FORMAT(2,3);
#endif
void write_to_daemoncore_pipe(int pipefd, const char* str, int len);
void write_to_daemoncore_pipe(const char* fmt, ... ) CHECK_PRINTF_FORMAT(1,2);
void write_stderr_to_pipe(int tid);
int systemCommand( ArgList &args, priv_state priv,
		std::vector<std::string> *cmd_out = NULL,
		std::vector<std::string> * cmd_in = NULL,
		std::vector<std::string> * cmd_err = NULL,
		bool merge_stderr_with_stdout = true);
std::string makeErrorMessage(const char* err_string);

void initialize_uids(void);
uid_t get_caller_uid(void);
gid_t get_caller_gid(void);
uid_t get_job_user_uid(void);
gid_t get_job_user_gid(void);
const char* get_caller_name(void);
const char* get_job_user_name(void);
bool canSwitchUid(void);

#endif /* CONDOR_VMGAHP_COMMON_H */
