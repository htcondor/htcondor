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
#include "gahp_common.h"
#include "vm_univ_utils.h"
#include "vmgahp.h"

#define CONDOR_VMGAHP_VERSION	"0.0.1"

#define ROOT_UID	0

extern int vmgahp_stdout_pipe;
extern int vmgahp_stderr_pipe;
extern PBuffer vmgahp_stderr_buffer;
extern int vmgahp_stderr_tid;
extern int oriDebugFlags;
extern int vmgahp_mode;

bool parse_vmgahp_command(const char* raw, Gahp_Args& args);

bool verify_vm_type(const char *vmtype);
bool check_vm_read_access_file(const char *file, bool is_root = false);
bool check_vm_write_access_file(const char *file, bool is_root = false);
bool check_vm_execute_file(const char *file, bool is_root = false);

char *vmgahp_param(const char* name);
int vmgahp_param_integer( const char* name, int default_value, int min_value = INT_MIN, int max_value = INT_MAX );
bool vmgahp_param_boolean( const char* name, const bool default_value );

int read_vmgahp_configfile( const char *config );
bool check_vmgahp_config_permission(const char* configfile, const char* vmtype);
bool write_specific_vm_params_to_file(const char *prefix, FILE *file);
bool write_forced_vm_params_to_file(FILE *file, const char* startmark, const char* endmark);

MyString parse_result_string( const char *result_string, int field_num);
bool verify_digit_arg(const char *s);
bool verify_number_args(const int is, const int should_be);
bool validate_vmgahp_result_string(const char *result_string);

void vmprintf( int flags, const char *fmt, ... ) CHECK_PRINTF_FORMAT(2,3);
void write_to_daemoncore_pipe(int pipefd, const char* str, int len);
void write_to_daemoncore_pipe(const char* fmt, ... ) CHECK_PRINTF_FORMAT(1,2);
int write_stderr_to_pipe();
int systemCommand(ArgList &args, bool is_root);
MyString makeErrorMessage(const char* err_string);

void initialize_uids(void);
uid_t get_caller_uid(void);
gid_t get_caller_gid(void);
uid_t get_job_user_uid(void);
gid_t get_job_user_gid(void);
bool isSetUidRoot(void);
bool needChown(void);
const char* get_caller_name(void);
const char* get_job_user_name(void);
bool canSwitchUid(void);

#define vmgahp_set_priv(s) \
   	_vmgahp_set_priv(s, __FILE__, __LINE__, 1)

#define vmgahp_set_condor_priv() vmgahp_set_priv(PRIV_CONDOR)
#define vmgahp_set_user_priv() vmgahp_set_priv(PRIV_USER)
#define vmgahp_set_user_priv_final() vmgahp_set_priv(PRIV_USER_FINAL)
#define vmgahp_set_root_priv() vmgahp_set_priv(PRIV_ROOT)

priv_state _vmgahp_set_priv(priv_state s, char file[], int line, int dologging);

#endif /* CONDOR_VMGAHP_COMMON_H */
