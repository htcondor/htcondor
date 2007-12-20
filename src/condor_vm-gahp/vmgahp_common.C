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
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "MyString.h"
#include "gahp_common.h"
#include "my_popen.h"
#include "vmgahp_common.h"
#include "vm_request.h"
#include "vmgahp.h"
#include "vmgahp_error_codes.h"
#include "condor_vm_universe_types.h"

#define VMCONFIGVARSIZE	128
static BUCKET *VMConfigVars[VMCONFIGVARSIZE];
static StringList allConfigParams;
static StringList forcedConfigParams;

bool SetUid = false;
bool needchown = false;
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
		priv = vmgahp_set_root_priv();
	}
	int ret = access(file, R_OK);
	if( is_root ) {
		vmgahp_set_priv(priv);
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
		priv = vmgahp_set_root_priv();
	}
	int ret = access(file, W_OK);
	if( is_root ) {
		vmgahp_set_priv(priv);
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
		priv = vmgahp_set_root_priv();
	}
	int ret = access(file, X_OK);
	if( is_root ) {
		vmgahp_set_priv(priv);
	}

	if( ret < 0 ) {
		vmprintf(D_ALWAYS, "File(%s) can't be executed\n", file);
		return false;
	}
	return true;
}

char *
vmgahp_param(const char* name)
{
	if( !name ) {
		return NULL;
	}

	char *pval = lookup_macro(name, VMConfigVars, VMCONFIGVARSIZE);

	if( !pval ) {
		// It means the value isn't in vmgahp config file
		return NULL;
	}

	pval = expand_macro(pval, VMConfigVars, VMCONFIGVARSIZE);

	if( !pval ) {
		vmprintf( D_ALWAYS, "\nERROR: Failed to expand macros in: %s\n", name );
		DC_Exit(1);
	}

	return pval;
}

int
vmgahp_param_integer( const char* name, int default_value, int min_value, int max_value )
{
	int result;
	int long_result;
	char *string = NULL;
	char *endptr = NULL;

	string = vmgahp_param( name );
	if( !string ) {
		vmprintf( D_FULLDEBUG, "%s is undefined, using default value of %d\n",
				 name, default_value );
		return default_value;
	}

	long_result = (int)strtol(string,&endptr,10);
	result = long_result;

	if( endptr && (endptr != string)) {
		while( isspace(*endptr) ) {
			endptr++;
		}
	}
	bool valid = ( endptr && endptr != string && *endptr == '\0');

	if( !valid ) {
		vmprintf(D_ALWAYS, "%s in the condor configuration is not "
				"an integer (%s).  Please set it to an integer in the "
				"range %d to %d (default %d).\n",
		        name, string, min_value, max_value, default_value );
	}
	else if( (long)result != long_result ) {
		vmprintf(D_ALWAYS, "%s in the condor configuration is "
				"out of bounds for an integer (%s).  Please set it to "
				"an integer in the range %d to %d (default %d).\n",
		        name, string, min_value, max_value, default_value );
	}
	else if( result < min_value ) {
		vmprintf(D_ALWAYS, "%s in the condor configuration is too low (%s)."
		        " Please set it to an integer in the range %d to %d"
		        " (default %d).\n",
		        name, string, min_value, max_value, default_value );
	}
	else if( result > max_value ) {
		vmprintf(D_ALWAYS, "%s in the condor configuration is too high (%s)."
		        " Please set it to an integer in the range %d to %d"
		        " (default %d).\n",
		        name, string, min_value, max_value, default_value );
	}
	free( string );
	return result;
}

bool
vmgahp_param_boolean( const char* name, const bool default_value )
{
	bool result;
	char *string = NULL;

	string = vmgahp_param( name );
	if( !string ) {
		vmprintf( D_FULLDEBUG, "%s is undefined, using default value of %s\n",
				 name, default_value ? "True" : "False" );
		return default_value;
	}

	switch ( string[0] ) {
		case 'T':
		case 't':
		case '1':
			result = true;
			break;
		case 'F':
		case 'f':
		case '0':
			result = false;
			break;
		default:
		    vmprintf( D_ALWAYS, "WARNING: %s not a boolean (\"%s\"), using "
					 "default value of %s\n", name, string,
					 default_value ? "True" : "False" );
			result = default_value;
			break;
	}

	free( string );
	return result;
}

bool
write_specific_vm_params_to_file(const char *prefix, FILE *file) 
{
	if( !file ) {
		return false;
	}

	int prefix_length = 0;
	if( prefix ) {
		prefix_length = strlen(prefix);
	}

	char *tmp = NULL;
	allConfigParams.rewind();
	while( (tmp = allConfigParams.next()) ) {
		if( !prefix_length ) {
			if( fprintf(file, "%s\n", tmp) < 0 ) {
				return false;
			}
			continue;
		}
		if( (int)strlen(tmp) <= prefix_length ) {
			continue;
		}

		if( !strncasecmp(tmp, prefix, prefix_length) ) {
			MyString tmp_name;
			MyString tmp_value;
			parse_param_string(tmp, tmp_name, tmp_value, false);

			tmp_name.strlwr();

			const char* name = tmp_name.Value();
			name += prefix_length; 

			if( fprintf(file, "%s=%s\n", name, tmp_value.Value()) < 0 ) {
				return false;
			}
		}
	}
	return true;
}

bool
write_forced_vm_params_to_file(FILE *file, const char* startmark, const char* endmark)
{
	if( !file ) {
		return false;
	}

	if( forcedConfigParams.isEmpty() ) {
		return true;
	}

	if( startmark ) {
		if( fprintf(file, "%s\n", startmark) < 0 ) {
			return false;
		}
	}

	char *tmp = NULL;
	forcedConfigParams.rewind();
	while( (tmp = forcedConfigParams.next()) ) {
		if( fprintf(file, "%s\n", tmp) < 0 ) {
			return false;
		}
	}

	if( endmark ) {
		if( fprintf(file, "%s\n", endmark) < 0 ) {
			return false;
		}
	}

	return true;
}

int 
read_vmgahp_configfile( const char *config )
{
	FILE *fp = NULL;

	if((fp=safe_fopen_wrapper(config, "r")) == NULL ) {
		vmprintf(D_ALWAYS, "\nERROR: Failed to open vmgahp config file (%s)\n",
				strerror(errno));
		DC_Exit(1);
	}

#define isop(c)     ((c) == '=')

	char *ptr = NULL, *name = NULL, *value = NULL;
	int LineNo = 0;
	MyString tmp_value;
	for(;;) {
		name = getline(fp);
		LineNo++;
		
		if( name == NULL ) {
			break;
		}

		/* Skip over comments */
		if( *name == '#' || blankline(name) ) {
			continue;
		}

		/* * check whether this parameter should be forced into 
		 * a vm config file */
		if( *name == '+' ) {
			name++;
			forcedConfigParams.append(name);
			continue;
		}else {
			allConfigParams.append(name);
		}

		/* Separate out the parameter name */
		ptr = name;
		while( *ptr ) {
			if( isspace(*ptr) || isop(*ptr) ) {
				break;
			} else {
				ptr++;
			}
		}

		if( !*ptr ) {
			(void)fclose( fp );
			return( -1 );
		}

		if( isop(*ptr) ) {
			*ptr++ = '\0';
		} else {
			*ptr++ = '\0';
			while( *ptr && !isop(*ptr) ) {
				ptr++;
			}

			if( !*ptr ) {
				(void)fclose( fp );
				return( -1 );
			}
			ptr += 1;
		}

		/* Skip to next non-space character */
		while( *ptr && isspace(*ptr) ) {
			ptr++;
		}

		value = ptr;
		if( value[0] == '\0' ) {
			continue;
		}
		tmp_value = value;
		tmp_value.trim();
		if( tmp_value.IsEmpty() ) {
			continue;
		}

		if( (name != NULL) && (name[0] != '\0') ) {
			char *name_str = NULL;
			MyString finalname;

			// First, try to use macro in vmgahp config
			/* Expand references to other parameters */
			name = expand_macro( name, VMConfigVars, VMCONFIGVARSIZE );
			if( name == NULL ) {
				(void)fclose( fp );
				vmprintf( D_ALWAYS, "\nERROR: Failed to expand macros in vmgahp "
						"config: %s\n", name );
				return( -1 );
			}

			// Second, try to use macro in Condor config
			name_str = macro_expand( name );
			if( name_str == NULL ) {
				(void)fclose( fp );
				vmprintf( D_ALWAYS, "\nERROR: Failed to expand macros in Condor "
						"config: %s\n", name );
				free(name);
				return( -1 );
			}
			free(name);

			finalname = name_str;
			free(name_str);

			// First check if this parameter is already in Condor config
			char *exist = param(finalname.GetCStr());
			bool is_in_condorconfig = false;
			if( exist && (exist[0] != '\0') ) {
				is_in_condorconfig = true;
			}
			if( exist ) {
				free(exist);
			}

			if( is_in_condorconfig ) {
				// There exists a parameter with the same name 
				// in Condor configurations
				// We will not overwrite it. 
				// So just add this parameter to vmgahp configurations.
				insert(finalname.GetCStr(), tmp_value.GetCStr(), VMConfigVars, VMCONFIGVARSIZE);
			}else {
				MyString realvalue;
				// This parameter is not in Condor config file
				// So first, we will add this parameter to Condor configurations
				config_insert(finalname.GetCStr(), tmp_value.GetCStr());

				// Try to get this param again from Condor configurations
				exist = param(finalname.GetCStr());
				if( !exist ) {
					// Impossible
					(void)fclose( fp );
					vmprintf( D_ALWAYS, "\nERROR: Failed to get %s in Condor config\n",
							finalname.Value() );
					return( -1 );
				}
				realvalue = exist;
				free(exist);

				insert(finalname.GetCStr(), realvalue.GetCStr(), 
						VMConfigVars, VMCONFIGVARSIZE);
	
				// Here, we reset value for this parameter to "" in Condor config
				config_insert(finalname.GetCStr(), "");
			}
		}
	}

	fclose(fp);
	return 0;
}

bool check_vmgahp_config_permission(const char* configfile, const char* vmtype)
{
	if( !configfile || !vmtype ) {
		return false;
	}

#if !defined(WIN32)
	// check permission of config file
	struct stat cbuf;
	if( stat(configfile, &cbuf ) < 0 ) {
		vmprintf(D_ALWAYS, "\nERROR: Failed to access vmgahp config file(%s:%s)\n", 
				configfile, strerror(errno));
		return false;
	}

	// Other writable bit
	if( cbuf.st_mode & S_IWOTH ) {
		vmprintf(D_ALWAYS, "\nFile Permission Error: "
				"other writable bit is not allowed for \"%s\" "
				"due to security issues\n", configfile);
		return false;
	}

	if( strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == 0 ) {
		if( cbuf.st_uid != ROOT_UID	) {
			vmprintf(D_ALWAYS, "\nFile Permission Error: " 
					"owner of \"%s\" must be root, "
					"because vmgahp has root privilege.\n", configfile);
			return false;
		}
	}else {
		if( isSetUidRoot() ) {
			if( cbuf.st_uid != ROOT_UID	) {
				vmprintf(D_ALWAYS, "\nFile Permission Error: " 
						"owner of \"%s\" must be root, "
						"because vmgahp has setuid-root\n", configfile);
				return false;
			}
		}

		// Other readable bit
		if( !(cbuf.st_mode & S_IROTH) ) {
			vmprintf(D_ALWAYS, "\nFile Permission Error: "
					"\"%s\" must be readable by anybody, because vmgahp program "
					"will be executed with user permission.\n", configfile);
			return false;
		}
	}
#endif

	// Can read vmgahp config file?
	if( check_vm_read_access_file(configfile) == false ) {
		return false;
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
		if( vmgahp_mode == VMGAHP_WORKER_MODE ) {
			dprintf(flags, "Worker[%d]: %s", (int)mypid, output.Value());
		}else {
			dprintf(flags, "VMGAHP[%d]: %s", (int)mypid, output.Value());
		}
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

bool isSetUidRoot(void)
{
	return SetUid;
}

bool needChown(void)
{
	return needchown;
}

bool canSwitchUid(void)
{
	return can_switch_ids();
}

static char* get_priv_state_name(priv_state s)
{
	switch(s) {
		case PRIV_UNKNOWN:
			return "PRIV_UNKNOWN";
			break;
		case PRIV_ROOT:
			return "PRIV_ROOT";
			break;
		case PRIV_CONDOR: 
			return "PRIV_CONDOR";
			break;
		case PRIV_USER:
			return "PRIV_USER";
			break;
		case PRIV_USER_FINAL:
			return "PRIV_USER_FINAL";
			break;
		case PRIV_FILE_OWNER:
			return "PRIV_FILE_OWNER";
			break;
		default:
			return "UNKNOWN_PRIV";
			break;
	}
	return "";
}

static void 
_priv_debug_log(priv_state prev, priv_state new_priv, char file[], int line)
{
#if !defined(WIN32)
	if( prev == new_priv ) {
		return;
	}

	vmprintf(D_PRIV, "%s --> %s at %s:%d "
			"(uid/gid=%d/%d,euid/egid=%d/%d)\n", 
			get_priv_state_name(prev), get_priv_state_name(new_priv),
			file, line, (int)getuid(), (int)getgid(), 
			(int)geteuid(), (int)getegid());
#endif
}


priv_state 
_vmgahp_set_priv(priv_state s, char file[], int line, int dologging)
{
	// We use daemonCore uid switch.
	priv_state PrevPrivState = get_priv();
	_set_priv(s, file, line, dologging);
	priv_state CurrentPrivState = get_priv();

	if(dologging) {
		_priv_debug_log(PrevPrivState, CurrentPrivState, file, line);
	}

	return PrevPrivState;
}

int
systemCommand(ArgList &args, bool is_root)
{
	int result = 0;

	priv_state prev = PRIV_UNKNOWN;
	if( is_root ) {
		prev = vmgahp_set_root_priv();
	}else {
		prev = vmgahp_set_user_priv();
	}
	result = my_system(args);
	vmgahp_set_priv(prev);

	if( result != 0 ) {
		MyString args_string;
		args.GetArgsStringForDisplay(&args_string,0);
		vmprintf(D_ALWAYS, "Failed to execute my_system: %s\n", args_string.Value());
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
