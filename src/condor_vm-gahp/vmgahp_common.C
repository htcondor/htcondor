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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_config.h"
#include "condor_string.h"
#include "string_list.h"
#include "condor_attributes.h"
#include "condor_classad.h"
#include "condor_daemon_core.h"
#include "MyString.h"
#include "gahp_common.h"
#include "my_popen.h"
#include "vmgahp_common.h"
#include "vm_request.h"
#include "vmgahp.h"
#include "vmgahp_error_codes.h"
#include "condor_vm_universe_types.h"
#include "../condor_privsep/condor_privsep.h"

MyString caller_name;
MyString job_user_name;

uid_t caller_uid = ROOT_UID;
gid_t caller_gid = ROOT_UID;
uid_t job_user_uid = ROOT_UID;
uid_t job_user_gid = ROOT_UID;

const char *support_vms_list[] = {
#if defined(LINUX)
CONDOR_VM_UNIVERSE_XEN,
#endif
#if defined(LINUX) || defined(WIN32)
CONDOR_VM_UNIVERSE_VMWARE,
#endif
NULL
};

// parse raw string into args
bool parse_vmgahp_command(const char* raw, Gahp_Args& args) 
{
	if (!raw) {
		vmprintf(D_ALWAYS,"ERROR parse_vmgahp_command: empty command\n");
		return false;
	}

	args.reset();

	int beginning = 0;

	int len=strlen(raw);

	char * buff = (char*)malloc(len+1);
	int buff_len = 0;

	for (int i = 0; i<len; i++) {

		if ( raw[i] == '\\' ) {
			i++; 			//skip this char
			if (i<(len-1)) {
				buff[buff_len++] = raw[i];
			}
			continue;
		}

		/* Check if charcater read was whitespace */
		if ( raw[i]==' ' || raw[i]=='\t' || raw[i]=='\r' || raw[i] == '\n') {

			/* Handle Transparency: we would only see these chars
			if they WEREN'T escaped, so treat them as arg separators
			*/
			buff[buff_len++] = '\0';
			args.add_arg( strdup(buff) );
			buff_len = 0;	// re-set temporary buffer

			beginning = i+1; // next char will be one after whitespace
		} else {
			// It's just a regular character, save it
			buff[buff_len++] = raw[i];
		}
	}

	/* Copy the last portion */
	buff[buff_len++] = '\0';
	args.add_arg(strdup(buff) );

	free (buff);
	return true;
}

// Check whether the given vmtype is in supported vmtype list
bool 
verify_vm_type(const char *vmtype)
{
	int i=0;
	while(support_vms_list[i] != NULL) {
		if(strcasecmp(vmtype, support_vms_list[i]) == 0 ) {
			return true;
		}
		i++;
	}
	vmprintf(D_ALWAYS, "Not supported VM TYPE(%s)\n", vmtype);
	return false;
}


bool
check_vm_read_access_file(const char *file, bool is_root /*false*/)
{
	if( (file == NULL) || (file[0] == '\0') ) {
		return false;
	}

	priv_state priv = PRIV_UNKNOWN;
	if( is_root ) {
		priv = set_root_priv();
	}
	int ret = access(file, R_OK);
	if( is_root ) {
		set_priv(priv);
	}

	if( ret < 0 ) {
		vmprintf(D_ALWAYS, "File(%s) can't be read\n", file);
		return false;
	}
	return true;
}

bool
check_vm_write_access_file(const char *file, bool is_root /*false*/)
{
	if( (file == NULL) || (file[0] == '\0') ) {
		return false;
	}

	priv_state priv = PRIV_UNKNOWN;
	if( is_root ) {
		priv = set_root_priv();
	}
	int ret = access(file, W_OK);
	if( is_root ) {
		set_priv(priv);
	}

	if( ret < 0 ) {
		vmprintf(D_ALWAYS, "File(%s) can't be modified\n", file);
		return false;
	}
	return true;
}

bool
check_vm_execute_file(const char *file, bool is_root /*false*/)
{
	if( (file == NULL) || (file[0] == '\0') ) {
		return false;
	}

	priv_state priv = PRIV_UNKNOWN;
	if( is_root ) {
		priv = set_root_priv();
	}
	int ret = access(file, X_OK);
	if( is_root ) {
		set_priv(priv);
	}

	if( ret < 0 ) {
		vmprintf(D_ALWAYS, "File(%s) can't be executed\n", file);
		return false;
	}
	return true;
}

bool
write_local_settings_from_file(FILE* out_fp,
                               const char* param_name,
                               const char* start_mark,
                               const char* end_mark)
{
	char* tmp = param(param_name);
	if (tmp == NULL) {
		return true;
	}
	MyString local_settings_file = tmp;
	free(tmp);
	if (start_mark != NULL) {
		if (fprintf(out_fp, "%s\n", start_mark) < 0) {
			vmprintf(D_ALWAYS,
			         "fprintf error writing start marker: %s\n",
			         strerror(errno));
			return false;
		}
	}
	FILE* in_fp = safe_fopen_wrapper(local_settings_file.Value(), "r");
	if (in_fp == NULL) {
		vmprintf(D_ALWAYS,
		         "fopen error on %s: %s\n",
		         local_settings_file.Value(),
		         strerror(errno));
		return false;
	}
	MyString line;
	while (line.readLine(in_fp)) {
		if (fputs(line.Value(), out_fp) == EOF) {
			vmprintf(D_ALWAYS,
			         "fputs error copying local settings: %s\n",
			         strerror(errno));
			fclose(in_fp);
			return false;
		}
	}
	fclose(in_fp);
	if (end_mark != NULL) {
		if (fprintf(out_fp, "%s\n", end_mark) == EOF) {
			vmprintf(D_ALWAYS,
			         "fputs error writing end marker: %s\n",
			         strerror(errno));
			return false;
		}
	}
	return true;
}

// extract the n-th field string from string result.
// field_num starts from 1.
// For example, if result_string is "10 0 internal_error",
// field_num = 1 will return "10";
// field_num = 2 will return "0";
// field_num = 3 will return "internal_error"
MyString parse_result_string( const char *result_string, int field_num)
{
	StringList result_list(result_string, " ");
	if( result_list.isEmpty() ) {
		return "";
	}

	if( field_num > result_list.number() ) {
		return "";
	}

	char *arg = NULL;
	int field = 0;
	result_list.rewind();
	while( (arg = result_list.next()) != NULL ) {
		field++;
		if( field == field_num ) {
			return arg;
		}
	}
	return "";
}

bool verify_digit_arg(const char *s)
{
	if( !s ) {
		return false;
	}
	unsigned int i = 0;
	for(i=0;i<strlen(s);i++) {
		if(!isdigit(s[i])) {
			vmprintf(D_ALWAYS, "Arg(%s) is not digit\n", s);
			return false;
		}
	}
	return true;
}

bool verify_number_args(const int is, const int should_be) 
{
	if( is != should_be) {
		vmprintf(D_ALWAYS, "Wrong # of args %d, should be %d\n", is, should_be);
		return false;
	}
	return true;
}

// Validate a result string
bool validate_vmgahp_result_string(const char *result_string)
{
	StringList result_list(result_string, " ");
	if( result_list.isEmpty() ) {
		return false;
	}

	// Format: <req_id> 0 <result1> ..
	// Format: <req_id> 1 <result1> ..

	if(result_list.number() < 3 ) {
		return false;
	}

	char *arg = NULL;
	result_list.rewind();

	// first arg must be digit
	arg = result_list.next();
	if( !arg || !verify_digit_arg(arg)) {
		vmprintf(D_ALWAYS, "First arg in result must be digit: %s\n", result_string);
		return false;
	}

	// second arg must be either 0 or 1
	arg = result_list.next();
	if( !arg || ( strcmp(arg, "0") && strcmp(arg, "1") ) ) {
		vmprintf(D_ALWAYS, "Second arg in result must be either 0 or 1: %s\n", result_string);
		return false;
	}

	return true;
}

void
write_to_daemoncore_pipe(int pipefd, const char* str, int len)
{
	if( pipefd == -1 || !str || len <= 0 || !daemonCore ) {
		return;
	}

	// Now flush:
	daemonCore->Write_Pipe( pipefd, str, len);
}

void
write_to_daemoncore_pipe(const char* fmt, ... )
{
	if( vmgahp_stdout_pipe == -1 ) {
		return;
	}

	MyString output;
	va_list args;
	va_start(args, fmt);
	output.vsprintf(fmt, args);
	write_to_daemoncore_pipe(vmgahp_stdout_pipe, 
			output.Value(), output.Length());
	va_end(args);
}

int
write_stderr_to_pipe()
{
	if( vmgahp_stderr_pipe == -1 ) {
		return TRUE;
	}

	vmgahp_stderr_buffer.Write();

	if( vmgahp_stderr_buffer.IsError() ) { 
		if( vmgahp_stderr_tid != -1 ) {
			daemonCore->Cancel_Timer(vmgahp_stderr_tid);
			vmgahp_stderr_tid = -1;
			vmgahp_stderr_pipe = -1;
		}
	} 
	return TRUE;
}

void vmprintf( int flags, const char *fmt, ... ) 
{
	int saved_flags = 0;
	static pid_t mypid = 0;

	if( !mypid ) {
		mypid = daemonCore->getpid();
	}

	if( !fmt ) {
		return;
	}

	if( !(flags & oriDebugFlags) ) {
		return;
	}

	saved_flags = oriDebugFlags;	/* Limit recursive calls */
	oriDebugFlags = 0;

	MyString output;
	va_list args;
	va_start(args, fmt);
	output.vsprintf(fmt, args);
	va_end(args);
	if( output.IsEmpty() ) {
		oriDebugFlags = saved_flags;
		return;
	}

	if( Termlog ) {
		if( (vmgahp_mode == VMGAHP_TEST_MODE) ||
				(vmgahp_mode == VMGAHP_KILL_MODE) ) {
			fprintf(stderr, "VMGAHP[%d]: %s", (int)mypid, output.Value());
			oriDebugFlags = saved_flags;
			return;
		}

		if( (vmgahp_stderr_tid != -1 ) &&
				(vmgahp_stderr_pipe != -1 )) {
			vmgahp_stderr_buffer.Write(output.Value());
			daemonCore->Reset_Timer(vmgahp_stderr_tid, 0, 2);
		}
	}else {
		dprintf(flags, "VMGAHP[%d]: %s", (int)mypid, output.Value());
	}
	oriDebugFlags = saved_flags;
}


void 
initialize_uids(void)
{
#if defined(WIN32)
#include "my_username.h"

	char *name = NULL;
	char *domain = NULL;

	name = my_username();
	domain = my_domainname();

	caller_name = name;
	job_user_name = name;

	if ( !init_user_ids(name, domain ) ) {
		// shouldn't happen - we always can get our own token
		vmprintf(D_ALWAYS, "Could not initialize user_priv with our own token!\n");
	}

	vmprintf(D_ALWAYS, "Initialize Uids: caller=%s@%s, job user=%s@%s\n", 
			caller_name.Value(), domain, job_user_name.Value(), domain);

	if( name ) {
		free(name);
	}
	if( domain ) {
		free(domain);
	}
	return;
#else
	// init_user_ids was called in main_pre_dc_init()
	vmprintf(D_ALWAYS, "Initial UID/GUID=%d/%d, EUID/EGUID=%d/%d, "
			"Condor UID/GID=%d,%d\n", (int)getuid(), (int)getuid(), 
			(int)geteuid(), (int)getegid(), 
			(int)get_condor_uid(), (int)get_condor_gid());

	vmprintf(D_ALWAYS, "Initialize Uids: caller=%s, job user=%s\n", 
			caller_name.Value(), job_user_name.Value());
	
	return;
#endif
}

uid_t 
get_caller_uid(void)
{
	return caller_uid;
}

gid_t 
get_caller_gid(void)
{
	return caller_gid;
}

uid_t 
get_job_user_uid(void)
{
	return job_user_uid;
}

gid_t 
get_job_user_gid(void)
{
	return job_user_gid;
}

const char* 
get_caller_name(void)
{
	return caller_name.Value();
}

const char* 
get_job_user_name(void)
{
	return job_user_name.Value();
}

bool canSwitchUid(void)
{
	return can_switch_ids();
}

int systemCommand( ArgList &args, bool is_root, StringList *cmd_out )
{
	int result = 0;
	FILE *fp = NULL;
	MyString line;
	char buff[1024];

	priv_state prev = PRIV_UNKNOWN;
	if( is_root ) {
		prev = set_root_priv();
	}else {
		prev = set_user_priv();
	}
#if !defined(WIN32)
	if ( privsep_enabled() && (job_user_uid != get_condor_uid())) {
		fp = privsep_popen(args, "r", 0, job_user_uid);
	}
	else {
		fp = my_popen( args, "r", 0 );
	}
#else
	fp = my_popen( args, "r", 0 );
#endif
	if ( fp == NULL ) {
		MyString args_string;
		args.GetArgsStringForDisplay( &args_string, 0 );
		vmprintf( D_ALWAYS, "Failed to execute command: %s\n",
				  args_string.Value() );
		return -1;
	}
	set_priv( prev );

	while ( cmd_out && fgets( buff, sizeof(buff), fp ) != NULL ) {
		line += buff;
		if ( line.chomp() ) {
			cmd_out->append( line.Value() );
			line = "";
		}
	}

	result = my_pclose( fp );

	if( result != 0 ) {
		MyString args_string;
		args.GetArgsStringForDisplay(&args_string,0);
		vmprintf(D_ALWAYS,
		         "Command returned non-zero: %s\n",
		         args_string.Value());
	}
	return result;
}

MyString
makeErrorMessage(const char* err_string)
{
	MyString buffer;

	if( err_string ) {
		for( int i = 0; err_string[i] != '\0'; i++ ) {
			switch( err_string[i] ) {
				case ' ':
				case '\\':
				case '\r':
				case '\n':
					buffer += '\\';
				default:
					buffer += err_string[i];
			}
		}
	}

	return buffer;
}
