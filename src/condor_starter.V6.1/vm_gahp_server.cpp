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
#include "condor_classad.h"
#include "condor_config.h"
#include "exit.h"
#include "env.h"
#include "starter.h"
#include "condor_daemon_core.h"
#include "strupr.h"
#include "my_popen.h"
#include "condor_environ.h"
#include "vm_univ_utils.h"
#include "vm_gahp_server.h"
#include "vm_gahp_request.h"
#include "setenv.h"

extern class Starter* Starter;

VMGahpServer::VMGahpServer(const char *vmgahpserver,
                           const char *vmtype,
                           ClassAd* job_ad)
{
	m_is_initialized = false;
	m_is_cleanuped = false;
	m_is_async_mode = false;

	m_vmgahp_pid = -1;
	m_vm_id = 0;
	m_vmgahp_readfd = -1;
	m_vmgahp_writefd = -1;
	m_vmgahp_errorfd = -1;

	m_pollInterval = 5;
	m_poll_tid = -1;
	m_poll_real_soon_tid = -1;
	m_stderr_tid = -1;

	m_next_reqid = 1;
	m_rotated_reqids = false;

	m_vmgahp_error_buffer = "";

	m_vm_type = vmtype;
	m_vmgahp_server = vmgahpserver;
	m_job_ad = job_ad;

	char *gahp_log_file = param("VM_GAHP_LOG");
	if( gahp_log_file ) {
		// logs from vmgahp will be stored in the file defined as "VM_GAHP_LOG".
		// So we don't need to gather logs from vmgahp server.
		m_include_gahp_log = false;
		free(gahp_log_file);
	}else {
		// The log file of Stater will also include logs from vmgahp.
		m_include_gahp_log = true;
	}
#if defined(WIN32)
	// On Windows machine, this option causes deadlock.. Hum..
	// even if this option works well on Linux machine.
	// I guess that is due to Windows Pipes but I don't know the exact reason.
	// Until the problem is solved, this option will be disabled on Windows machine.
	m_include_gahp_log = false;
#endif

	m_send_all_classad = param_boolean("VM_GAHP_SEND_ALL_CLASSAD", true);

	if( m_send_all_classad ) {
		dprintf( D_FULLDEBUG, "Will send the entire job ClassAd to vmgahp\n");
	}else {
		dprintf( D_FULLDEBUG, "Will send the part of job ClassAd to vmgahp\n");
	}
}

VMGahpServer::~VMGahpServer()
{
	cleanup();
}

bool
VMGahpServer::cleanup(void)
{
	bool result = true;

	if( m_is_cleanuped ) {
		return true;
	}
	m_is_cleanuped = true;

	dprintf(D_FULLDEBUG,"Inside VM_GAHP_SERVER::cleanup()\n");

	// Remove all pending requests
	for (auto& [key, req]: m_request_table) {
		if( req ) {
			req->detachVMGahpServer();
		}
	}
	m_request_table.clear();

	// Stop poll timer
	if(m_poll_tid != -1) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(m_poll_tid);
		}
		m_poll_tid = -1;
	}
	if(m_poll_real_soon_tid != -1) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(m_poll_real_soon_tid);
		}
		m_poll_real_soon_tid = -1;
	}

	// Stop stderr timer
	if( m_stderr_tid != -1 ) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(m_stderr_tid);
		}
		m_stderr_tid = -1;
	}

	if( m_is_initialized && (m_vmgahp_pid > 0 )) {
		result = command_quit();
	}

	// print last messages from vmgahp before closing pipes
	printSystemErrorMsg();

	// Close pipes
	if( m_vmgahp_readfd != -1 ) {
		if( daemonCore ) {
			daemonCore->Close_Pipe(m_vmgahp_readfd);
		}
		m_vmgahp_readfd = -1;
	}

	if( m_vmgahp_writefd != -1 ) {
		if( daemonCore ) {
			daemonCore->Close_Pipe(m_vmgahp_writefd);
		}
		m_vmgahp_writefd = -1;
	}

	if( m_vmgahp_errorfd != -1 ) {
		if( daemonCore ) {
			daemonCore->Close_Pipe(m_vmgahp_errorfd);
		}
		m_vmgahp_errorfd = -1;
	}
	m_vmgahp_error_buffer = "";

	// Make sure Virtual machine exits.
	if( m_vm_id > 0 ) {
		killVM();
		m_vm_id = 0;
	}

	m_is_initialized = false;
	m_is_async_mode = false;
	m_vmgahp_pid = -1;
	m_rotated_reqids = false;

	m_commands_supported.clear();
	m_vms_supported.clear();

	dprintf(D_FULLDEBUG,"End of VM_GAHP_SERVER::cleanup\n");
	sleep(1);

	return result;
}

bool
VMGahpServer::startUp(Env *job_env, const char *workingdir, int nice_inc, FamilyInfo *family_info)
{
	//check if we already have spawned a vmgahp server
	if( m_vmgahp_pid > 0 ) {
		//vmgahp is already running
		return true;
	}

	if( !m_job_ad ) {
		start_err_msg = "No JobAd in VMGahpServer::startUp()";
		dprintf(D_ALWAYS,"%s\n", start_err_msg.c_str());
		return false;
	}

	std::string JobName;
	if( m_vmgahp_server.empty() ) {
		start_err_msg = "No path for vmgahp in VMGahpServer::startUp()";
		dprintf(D_ALWAYS,"%s\n", start_err_msg.c_str());
		return false;
	}

	JobName = m_vmgahp_server;

	// Create two pairs of pipes which we will use to
	int stdin_pipefds[2];
	int stdout_pipefds[2];
	int stderr_pipefds[2];

	if(!daemonCore->Create_Pipe(stdin_pipefds, 
				true, // read end registerable
				false, // write end not registerable
				false, // read end blocking
				false // write end blocking
				)) {
		start_err_msg = "unable to create pipe to stdin of VM gahp";
		dprintf(D_ALWAYS,"%s\n", start_err_msg.c_str());
		return false;
	}
	if(!daemonCore->Create_Pipe(stdout_pipefds,
				true,  //read end registerable
				false, // write end not registerable
				false, // read end blocking
				false // write end blocking
				)) {
		// blocking read
		start_err_msg = "unable to create pipe to stdout of VM gahp";
		dprintf(D_ALWAYS,"%s\n", start_err_msg.c_str());
		return false;
	}
	if( m_include_gahp_log ) {
		if(!daemonCore->Create_Pipe(stderr_pipefds,
					true,  // read end registerable
					false, // write end not registerable
					true,  // read end non-blocking
					true  // write end non-blocking
					)) {
			// nonblocking read
			start_err_msg = "unable to create pipe to stderr of VM gahp";
			dprintf(D_ALWAYS,"%s\n", start_err_msg.c_str());
			return false;
		}
	}

	int io_redirect[3];
	io_redirect[0] = stdin_pipefds[0];	//stdin gets read side of in pipe
	io_redirect[1] = stdout_pipefds[1];	//stdout gets write side of out pipe
	if( m_include_gahp_log ) {
		io_redirect[2] = stderr_pipefds[1];	//stderr gets write side of err pipe
	}else {
		int null_fd = safe_open_wrapper_follow(NULL_FILE, O_WRONLY | O_APPEND, 0666);
		if( null_fd < 0 ) {
			start_err_msg = "unable to open null file for stderr of VM gahp";
			dprintf(D_ALWAYS,"Failed to open '%s':%s (errno %d)\n", 
					NULL_FILE, strerror(errno), errno);
			return false;
		}
		io_redirect[2] = null_fd;
	}

	// Set Arguments
	ArgList vmgahp_args;

	vmgahp_args.SetArgV1SyntaxToCurrentPlatform();
	vmgahp_args.AppendArg(m_vmgahp_server.c_str());

	// Add daemonCore options
	vmgahp_args.AppendArg("-f");
	if( m_include_gahp_log ) {
		vmgahp_args.AppendArg("-t");
	}
	vmgahp_args.AppendArg("-M");
	vmgahp_args.AppendArg(std::to_string(VMGAHP_STANDALONE_MODE));

	std::string args_string;
	vmgahp_args.GetArgsStringForDisplay(args_string, 1);
	dprintf( D_ALWAYS, "About to exec %s %s\n", JobName.c_str(),
			args_string.c_str() );

#if !defined(WIN32)
	uid_t vmgahp_user_uid = (uid_t) -1;
	gid_t vmgahp_user_gid = (gid_t) -1;

	if( can_switch_ids() ) {
		// Condor runs as root
		vmgahp_user_uid = get_user_uid();
		vmgahp_user_gid = get_user_gid();
	}
	else {
		// vmgahp may have setuid-root (e.g. vmgahp for Xen)
		vmgahp_user_uid = get_condor_uid();
		vmgahp_user_gid = get_condor_gid();
	}

	// Setup vmgahp user uid/gid
	if( vmgahp_user_uid > 0 ) {
		if( vmgahp_user_gid <= 0 ) {
			vmgahp_user_gid = vmgahp_user_uid;
		}

		std::string tmp_str;
		formatstr(tmp_str, "%d", (int)vmgahp_user_uid);
		job_env->SetEnv("VMGAHP_USER_UID", tmp_str.c_str());
		formatstr(tmp_str, "%d", (int)vmgahp_user_gid);
		job_env->SetEnv("VMGAHP_USER_GID", tmp_str.c_str());
	}
#endif

	job_env->SetEnv("VMGAHP_VMTYPE", m_vm_type.c_str());
	job_env->SetEnv("VMGAHP_WORKING_DIR", workingdir);

	// Grab the full environment back out of the Env object
	if(IsFulldebug(D_FULLDEBUG)) {
		std::string env_str;
		job_env->getDelimitedStringForDisplay( env_str);
		dprintf(D_FULLDEBUG, "Env = %s\n", env_str.c_str());
	}

	priv_state vmgahp_priv = PRIV_ROOT;

	std::string create_process_err_msg;
	OptionalCreateProcessArgs cpArgs(create_process_err_msg);
	m_vmgahp_pid = daemonCore->CreateProcessNew( JobName, vmgahp_args, 
			cpArgs.priv(vmgahp_priv)
				.reaperID(1)
				.wantCommandPort(FALSE).wantUDPCommandPort(FALSE)
				.env(job_env)
			    .cwd(workingdir)
				.familyInfo(family_info)
				.std(io_redirect)
				.niceInc(nice_inc));

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = nullptr;
	if(m_vmgahp_pid == FALSE && errno) create_process_error = strerror(errno);

	// Now that the VMGAHP server is running, close the sides of
	// the pipes we gave away to the server, and stash the ones
	// we want to keep in an object data member.
	daemonCore->Close_Pipe(io_redirect[0]);
	daemonCore->Close_Pipe(io_redirect[1]);
	if( m_include_gahp_log ) {
		daemonCore->Close_Pipe(io_redirect[2]);
	}else {
		close(io_redirect[2]);
	}

	if ( m_vmgahp_pid == FALSE ) {
		m_vmgahp_pid = -1;
		start_err_msg = "Failed to start vm-gahp server";
		dprintf(D_ALWAYS, "%s (%s)\n", start_err_msg.c_str(), 
				m_vmgahp_server.c_str());
		if(create_process_error) {
			std::string err_msg = "Failed to execute '";
			err_msg += m_vmgahp_server;
			err_msg += "'";
			if(!args_string.empty()) {
				err_msg += " with arguments ";
				err_msg += args_string;
			}
			err_msg += ": ";
			err_msg += create_process_error;
			dprintf(D_ALWAYS, "Failed to start vmgahp server (%s)\n", 
					err_msg.c_str());
		}
		return false;
	}

	dprintf(D_ALWAYS, "VMGAHP server pid=%d\n", m_vmgahp_pid);

	m_vmgahp_writefd = stdin_pipefds[1];
	m_vmgahp_readfd = stdout_pipefds[0];
	if( m_include_gahp_log ) {
		m_vmgahp_errorfd = stderr_pipefds[0];
	}

	// Now initialization is done
	m_is_initialized = true;

	// print initial stderr messages from vmgahp
	printSystemErrorMsg();

	// Read the initial greeting from the vm-gahp, which is the version
	if( command_version() == false ) {
		start_err_msg = "Internal vmgahp server error";
		dprintf(D_ALWAYS,"Failed to read vmgahp server version\n");
		printSystemErrorMsg();
		cleanup();
		return false;
	}
	
	dprintf(D_FULLDEBUG,"VMGAHP server version: %s\n", m_vmgahp_version.c_str());

	// Now see what commands this server supports.
	if( command_commands() == false ) {
		start_err_msg = "Internal vmgahp server error";
		dprintf(D_ALWAYS,"Failed to read supported commands from vmgahp server\n");
		printSystemErrorMsg();
		cleanup();
		return false;
	}

	// Now see what virtual machine types this server supports
	if( command_support_vms() == false ) {
		start_err_msg = "Internal vmgahp server error";
		dprintf(D_ALWAYS,"Failed to read supported vm types from vmgahp server\n");
		printSystemErrorMsg();
		cleanup();
		return false;
	}

	int result = -1;
	if( m_include_gahp_log ) {
		result = daemonCore->Register_Pipe(m_vmgahp_errorfd,
				"m_vmgahp_errorfd",
				(PipeHandlercpp)&VMGahpServer::err_pipe_ready_from_pipe,
				"VMGahpServer::err_pipe_ready",this);

		if( result == -1 ) { 
			dprintf(D_ALWAYS,"Failed to register vmgahp stderr pipe\n"); 
			if(m_stderr_tid != -1) {
				daemonCore->Cancel_Timer(m_stderr_tid);
				m_stderr_tid = -1;
			}
			m_stderr_tid = daemonCore->Register_Timer(2, 
					2, (TimerHandlercpp)&VMGahpServer::err_pipe_ready_from_timer, 
					"VMGahpServer::err_pipe_ready",this);
			if( m_stderr_tid == -1 ) {
				start_err_msg = "Internal vmgahp server error";
				dprintf(D_ALWAYS,"Failed to register stderr timer\n");
				printSystemErrorMsg();
				cleanup();
				return false;
			}
		}
	}

	// try to turn on vmgahp async notification mode
	if  ( !command_async_mode_on() ) {
		// not supported, set a poll interval
		m_is_async_mode = false;
		setPollInterval(m_pollInterval);
	} else {
		// command worked... register the pipe and stop polling
		result = daemonCore->Register_Pipe(m_vmgahp_readfd,
				"m_vmgahp_readfd",
				static_cast<PipeHandlercpp>(&VMGahpServer::pipe_ready),
				"VMGahpServer::pipe_ready",this);
		if( result == -1 ) {
			// failed to register the pipe for some reason; fall 
			// back on polling (yuck).
			dprintf(D_ALWAYS,"Failed to register vmgahp Read pipe\n");
			m_is_async_mode = false;
			setPollInterval(m_pollInterval);
		} else {
			// pipe is registered.  stop polling.
			setPollInterval(0);
			m_is_async_mode = true;
		}
	}

	return true;
}

bool
VMGahpServer::command_version(void)
{
	int i,j,result;
	bool ret_val = false;

	if( m_is_initialized == false ) {
		return false;
	}

	if( m_vmgahp_readfd == -1 ) {
		return false;
	}

	char vmgahp_version[360];

	j = sizeof(vmgahp_version);
	i = 0;
	while ( i < j ) {
		result = daemonCore->Read_Pipe(m_vmgahp_readfd, &(vmgahp_version[i]), 1 );
		/* Check return value from read() */
		if( result < 0 ) {		/* Error - try reading again */
			//continue;
			dprintf(D_ALWAYS, "Read_Pipe Error for vm command_version\n");
			return false;
		}
		if( result == 0 ) {	/* End of File */
				// may as well just return false, and let reaper cleanup
			return false;
		}
		if( i==0 && vmgahp_version[0] != '$' ) {
			continue;
		}
		if( vmgahp_version[i] == '\\' ) {
			continue;
		}
		if( vmgahp_version[i] == '\n' ) {
			ret_val = true;
			vmgahp_version[i] = '\0';
			break;
		}
		i++;
	}

	if( ret_val ) {
		m_vmgahp_version = vmgahp_version;
	}

	return ret_val;
}

bool
VMGahpServer::command_commands(void)
{
	static const char* command = "COMMANDS";

	if( m_is_initialized == false ) {
		return false;
	}

	// Before sending command, print the remaining stderr messages from vmgahp
	printSystemErrorMsg();

	if( write_line(command) == false) {
		return false;
	}

	// give some time to gahp server
	sleep(1);

	Gahp_Args result;
	if( read_argv(result) == false ) {
		return false;
	}

	if( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"VMGAHP command '%s' failed\n",command);
		return false;
	}

	m_commands_supported.clear();

	int i;
	for (i = 1; i < result.argc; i++) {
		m_commands_supported.emplace_back(result.argv[i]);
	}

	return true;
} 

bool
VMGahpServer::command_support_vms(void)
{
	static const char* command = "SUPPORT_VMS";

	if( m_is_initialized == false ) {
		return false;
	}

	// Before sending command, print the remaining stderr messages from vmgahp
	printSystemErrorMsg();

	if( write_line(command) == false) {
		return false;
	}

	// give some time to gahp server
	sleep(1);

	Gahp_Args result;
	if( read_argv(result) == false ) {
		return false;
	}

	if( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"VMGAHP command '%s' failed\n",command);
		return false;
	}

	m_vms_supported.clear();

	int i;
	for (i = 1; i < result.argc; i++) {
		m_vms_supported.emplace_back(result.argv[i]);
	}

	return true;
} 

bool
VMGahpServer::command_async_mode_on(void)
{
	static const char* command = "ASYNC_MODE_ON";

	if( m_is_initialized == false ) {
		return false;
	}

	if( isSupportedCommand(command) == FALSE ) {
		return false;
	}

	if(write_line(command) == false) {
		return false;
	}

	// give some time to gahp server
	sleep(1);

	Gahp_Args result;
	if( read_argv(result) == false ) {
		return false;
	}

	if( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"VMGAHP command '%s' failed\n",command);
		return false;
	}

	return true;
}

bool
VMGahpServer::command_quit(void)
{
	static const char* command = "QUIT";

	if( m_is_initialized == false ) {
		return false;
	}

	if(write_line(command) == false) {
		return false;
	}

	// give some time to gahp server
	sleep(1);

	Gahp_Args result;
	if( read_argv(result) == false ) {
		return false;
	}

	if( result.argc == 0 || result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"VMGAHP command '%s' failed\n",command);
		return false;
	}

	return true;
}

void
VMGahpServer::setPollInterval(unsigned int interval)
{
	if(m_poll_tid != -1) {
		daemonCore->Cancel_Timer(m_poll_tid);
		m_poll_tid = -1;
	}
	m_pollInterval = interval;
	if( m_pollInterval > 0 ) {
		m_poll_tid = daemonCore->Register_Timer(m_pollInterval, 
				m_pollInterval, 
				(TimerHandlercpp)&VMGahpServer::poll_from_timer, 
				"VMGahpServer::poll",this); 
	}
}

unsigned int
VMGahpServer::getPollInterval(void) const
{
	return m_pollInterval;
}

int
VMGahpServer::pipe_ready(int)
{
	if( m_is_initialized == false ) {
		return false;
	}

	dprintf(D_FULLDEBUG, "pipe_ready is called\n");

	if( poll() < 0 ) {
		// Pipe error
		daemonCore->Send_Signal(daemonCore->getpid(), DC_SIGHARDKILL);
	}
	return TRUE;
}

int
VMGahpServer::err_pipe_ready(void)
{
	int count = 0;

	if( ( m_is_initialized == false) ||
			( m_vmgahp_errorfd == -1 ) || !daemonCore ) {
		return FALSE;
	}

	char buff[2049];
	buff[0] = '\0';

	while ((count = daemonCore->Read_Pipe(m_vmgahp_errorfd, &buff, 2048))>0) {

		char *prev_line = buff;
		char *newline = buff - 1;
		buff[count]='\0';

			// Search for each newline in the string we just read and
			// print out the text between it and the previous newline 
			// (which may include text stored in m_vmgahp_error_buffer).
			// Any text left at the end of the string is added to
			// m_vmgahp_error_buffer to be printed when the next newline
			// is read.
		while ( (newline = strchr( newline + 1, '\n' ) ) != NULL ) {

			*newline = '\0';
			dprintf( D_ALWAYS, "VMGAHP[%d] (stderr) -> %s%s\n", 
					m_vmgahp_pid, 
					m_vmgahp_error_buffer.c_str(), 
					prev_line );
			prev_line = newline + 1;
			m_vmgahp_error_buffer = "";
		}

		m_vmgahp_error_buffer += prev_line;
	}

	return TRUE;
}

bool
VMGahpServer::write_line(const char *command) const
{
	if( m_is_initialized == false ) {
		return false;
	}

	if( !command || m_vmgahp_writefd == -1 ) {
		return false;
	}
	
	if( daemonCore->Write_Pipe(m_vmgahp_writefd,command,strlen(command)) <= 0 ) {
		dprintf( D_ALWAYS, "VMGAHP write line(%s) Error\n", command);
		return false;
	}

	if( daemonCore->Write_Pipe(m_vmgahp_writefd,"\r\n",2) <= 0) {
		dprintf( D_ALWAYS, "VMGAHP write line(%s) Error\n", "\r\n");
		return false;
	}

	std::string debug;
	formatstr( debug, "'%s'", command );
	dprintf( D_FULLDEBUG, "VMGAHP[%d] <- %s\n", m_vmgahp_pid,
			debug.c_str() );

	return true;
}

bool
VMGahpServer::write_line(const char *command, int req, const char *args) const
{
	if( m_is_initialized == false ) {
		return false;
	}

	if( !command || m_vmgahp_writefd == -1 ) {
		return false;
	}

	std::string buf;
	formatstr(buf, " %d ",req);
	if( daemonCore->Write_Pipe(m_vmgahp_writefd,command,strlen(command)) <= 0 ) {
		dprintf( D_ALWAYS, "VMGAHP write line(%s) Error\n", command);
		return false;
	}
	if( daemonCore->Write_Pipe(m_vmgahp_writefd,buf.c_str(),buf.length()) <= 0 ) {
		dprintf( D_ALWAYS, "VMGAHP write line(%s) Error\n", buf.c_str());
		return false;
	}
	if( args ) {
		if( daemonCore->Write_Pipe(m_vmgahp_writefd,args,strlen(args)) <= 0 ) {
			dprintf( D_ALWAYS, "VMGAHP write line(%s) Error\n", args);
			return false;
		}
	}
	if( daemonCore->Write_Pipe(m_vmgahp_writefd,"\r\n",2) <= 0 ) {
			dprintf( D_ALWAYS, "VMGAHP write line(%s) Error\n", "\r\n");
			return false;
	}

	std::string debug;
	if( args ) {
		formatstr( debug, "'%s%s%s'", command, buf.c_str(), args );
	} else {
		formatstr( debug, "'%s%s'", command, buf.c_str() );
	}
	dprintf( D_FULLDEBUG, "VMGAHP[%d] <- %s\n", m_vmgahp_pid,
			debug.c_str() );

	return true;
}

bool
VMGahpServer::read_argv(Gahp_Args &g_args)
{
	static char* buf = NULL;
	int ibuf = 0;
	int result = 0;
	bool trash_this_line = false;
	bool escape_seen = false;
	static const int buf_size = 1024 * 10;

	if( m_is_initialized == false ) {
		return false;
	}

	if( m_vmgahp_readfd == -1 ) {
		dprintf( D_ALWAYS, "VMGAHP[%d] -> (no pipe)\n", m_vmgahp_pid );
		return false;
	}

	g_args.reset();

	if( buf == NULL ) {
		buf = (char*)malloc(buf_size);
		ASSERT( buf != NULL );
	}

	ibuf = 0;
	for (;;) {
		ASSERT(ibuf < buf_size);
		result = daemonCore->Read_Pipe(m_vmgahp_readfd, &(buf[ibuf]), 1 );

		/* Check return value from read() */
		if( result < 0 ) {
			return false;
		}
		if( result == 0 ) {	/* End of File */
			// clear out all entries
			g_args.reset();
			dprintf( D_ALWAYS, "VMGAHP[%d] -> EOF\n", m_vmgahp_pid );
			return false;
		}

		/* If we just saw an escaping backslash, let this character
		 * through unmolested and without special meaning.
		 */
		if( escape_seen ) {
			ibuf++;
			escape_seen = false;
			continue;
		}

		/* Check if the character read is a backslash. If it is, then it's
		 * escaping the next character.
		 */
		if( buf[ibuf] == '\\' ) {
			escape_seen = true;
			continue;
		}

		/* Unescaped carriage return characters are ignored */
		if( buf[ibuf] == '\r' ) {
			continue;
		}

		/* An unescaped space delimits a parameter to copy into argv */
		if( buf[ibuf] == ' ' ) {
			buf[ibuf] = '\0';
			g_args.add_arg( strdup( buf ) );
			ibuf = 0;
			continue;
		}

		/* If character was a newline, copy into argv and return */
		if( buf[ibuf]=='\n' ) { 
			buf[ibuf] = 0;
			g_args.add_arg( strdup( buf ) );

			trash_this_line = false;

			// Note: A line of unexpected text from the vmgahp server
			//   that triggers a RESULTS command (whether it's the
			//   async-mode 'R' or some extraneous un-prefixed text)
			//   will be printed in the log after the RESULTS line
			//   is logged. This implied reversal of causality isn't
			//   easy to fix, so we leave it as-is.
			static std::string debug;
			debug = "";
			if( g_args.argc > 0 ) {
				debug += "'";
				for ( int i = 0; i < g_args.argc; i++ ) {
					if( i != 0 ) {
						debug += "' '";
					}
					if( g_args.argv[i] ) {
						debug += g_args.argv[i];
					}
				}
				debug += "'";
			}
			dprintf( D_FULLDEBUG, "VMGAHP[%d] -> %s\n", m_vmgahp_pid,
					debug.c_str() );

			// check for a single "R".  This means we should check
			// for results in vmgahp async mode.  
			if( trash_this_line==false && g_args.argc == 1 &&
					g_args.argv[0][0] == 'R' ) {
				poll_real_soon();
				// ignore anything else on this line & read again
				trash_this_line = true;
			}

			if( trash_this_line ) {
				// reset all our buffers and read the next line
				g_args.reset();
				ibuf = 0;
				continue;	// go back to the top of the for loop
			}

			return true;
		}

		/* Character read was just a regular one.. increment index
		 * and loop back up to read the next character */
		ibuf++;

	}	/* end of infinite for loop */

	// We will never reach here
	return false;
}

int
VMGahpServer::new_reqid(void)
{
	int starting_reqid;

	starting_reqid  = m_next_reqid;

	m_next_reqid++;
	while (starting_reqid != m_next_reqid) {
		if( m_next_reqid > 990'000'000 ) {
			m_next_reqid = 1;
			m_rotated_reqids = true;
		}
		// Make certain this reqid is not already in use.
		// Optimization: only need to do the lookup if we have
		// rotated request ids...
		if( (!m_rotated_reqids) || 
				(m_request_table.find(m_next_reqid) == m_request_table.end()) ) {
			// not in use, we are done
			return m_next_reqid;
		}
		m_next_reqid++;
	}

	// If we made it here, we are out of request ids
	dprintf(D_ALWAYS,"VMGAHP Server - out of request ids !!!?!?!?\n");

	return -1;  // just to make C++ not give a warning...
}

void
VMGahpServer::poll_real_soon()
{
	if( m_is_initialized == false ) {
		return;
	}

	// Poll for results asap via a timer, unless a request
	// to poll for results is already pending
	if( m_poll_real_soon_tid == -1) {
		m_poll_real_soon_tid = daemonCore->Register_Timer(0,
				(TimerHandlercpp)&VMGahpServer::poll_now,
				"VMGhapServer::poll from poll_real_soon", this);
	}
}

void
VMGahpServer::poll_now( int /* timerID */ )
{
	m_poll_real_soon_tid = -1;
	(void)poll();
}

int
VMGahpServer::poll()
{
	Gahp_Args* result = NULL;
	int num_results = 0;
	int i, result_reqid;
	VMGahpRequest* entry;
	std::vector<Gahp_Args*> result_lines;

	if( m_is_initialized == false ) {
		return 0;
	}

	// First, send the RESULTS comand to the vmgahp server
	if( write_line("RESULTS") == false) {
		return -1;
	}

	// give some time to gahp server
	sleep(1);
	
	// First line of RESULTS command contains how many subsequent
	// result lines should be read.
	result = new Gahp_Args;
	ASSERT(result);
	if( read_argv(result) == false ){
		delete result;
		return -1;
	}

	if( result->argc < 2 || result->argv[0][0] != 'S' ) {
		// Badness !
		dprintf(D_ALWAYS,"VMGAHP command 'RESULTS' failed\n");
		delete result;
		return -1;
	}
	num_results = strtol(result->argv[1], (char **)NULL, 10);
	if( num_results < 0 ) {
		dprintf(D_ALWAYS,"Invalid number of results('%s') from vmgahp Server\n", 
				result->argv[1]);
		delete result;
		return -1;
	}

	// Now store each result line in an array.
	for (i=0; i < num_results; i++) {
		// Allocate a result buffer if we don't already have one
		if( !result ) {
			result = new Gahp_Args;
			ASSERT(result);
		}
		if( read_argv(result) == false ){
			delete result;
			return -1;
		}
		result_lines.push_back(result);
		result = NULL;
	}

	// At this point, the Results command has compelted.  We needed
	// to store all the results in an array before operating on them,
	// because we need the Results command to complete before we
	// operate on the results.  Why?  Because some of the results
	// require us to make a callback, and the callback may want to 
	// initiate a new Gahp request...

	// Now for each stored request line,
	// lookup the request id in our hash table and stash the result.
	for (i=0; i < num_results; i++) {
		if( result ) { 
			delete result;
			result = NULL;
		}
		result = result_lines[i];

		result_reqid = 0;
		if( result->argc > 0 ) {
			result_reqid = (int)strtol(result->argv[0], (char **)NULL, 10);
		}
		if( result_reqid <= 0 ) {
			// something is very weird; log it and move on...
			dprintf(D_ALWAYS,"VMGAHP - Bad RESULTS line\n");
			continue;
		}

		// Now lookup in our hashtable....
		entry = NULL;
		auto itr = m_request_table.find(result_reqid);
		if (itr != m_request_table.end()) {
			entry = itr->second;
		}
		if( entry ) {
			// found the entry!  stash the result
			entry->setResult(result);
			// set result to NULL so we do not deallocate above
			result = NULL;
			// and set pending status
			entry->setPendingStatus(REQ_DONE);
			// mark pending request completed by setting reqid to 0
			entry->setReqId(0);
			// and reset the user's timer if requested
			entry->resetUserTimer();
		}
		// clear entry from our hashtable so we can reuse the reqid
		m_request_table.erase(result_reqid);

	}	// end of looping through each result line

	if( result ) { 
		delete result;
		result = NULL;
	}
	return num_results;
}

void 
VMGahpServer::cancelPendingRequest(int req_id)
{
	if( m_is_initialized == false ) {
		return;
	}

	if( req_id <= 0 ) {
		return;
	}

	auto itr = m_request_table.find(req_id);

	if(itr != m_request_table.end() && itr->second) {
		itr->second->setPendingStatus(REQ_CANCELLED);
		//Entry was still in the hashtable, which means
		//that this reqid is still with the vmgahp server
		//so keep the entry with this pending_reqid
		//with a NULL data field so we do not reuse this reqid
		itr->second = nullptr;
	}
}

VMGahpRequest *
VMGahpServer::findRequestbyReqId(int req_id)
{
	auto itr = m_request_table.find(req_id);
	if (itr != m_request_table.end()) {
		return itr->second;
	} else {
		return nullptr;
	}
}

bool
VMGahpServer::isPendingRequest(int req_id)
{
	VMGahpRequest *req = NULL;
	req = findRequestbyReqId(req_id);
	
	if(req) {
		return TRUE;
	}

	return FALSE;
}

bool
VMGahpServer::nowPending(const char *command, const char *args, VMGahpRequest *req)
{
	if( m_is_initialized == false ) {
		return false;
	}

	if( !command || !req ) {
		return false;
	}

	int req_id;
	req_id = new_reqid();

	if( req_id < 0 ) {
		return false;
	}

	//Set reqid
	req->setReqId(req_id);

	m_request_table[req_id] = req;
	req->setPendingStatus(REQ_SUBMITTED);

	// Write the command out to the gahp server.
	if(write_line(command,req_id,args) == false) {
		return false;
	}

	// give some time to gahp server
	sleep(1);

	Gahp_Args return_line;
	if( read_argv(return_line) == false ) {
		return false;
	}
	if( return_line.argc == 0 || return_line.argv[0][0] != 'S' ) {
		// Badness !
		dprintf(D_ALWAYS,"Bad %s Request\n",command);
		return false;
	}

	return true;
}

// Result will be stored in poll 
void
VMGahpServer::getPendingResult(int req_id, bool is_blocking)
{
	if( m_is_initialized == false ) {
		return;
	}

	//Give some time to vmGahpServer
	sleep(1);

	VMGahpRequest *req = NULL;
	req = findRequestbyReqId(req_id);

	if( !req ) {
		// this means that the request with this req_id does not exit in m_pending_table
		return;
	}

	if( req->getPendingStatus() != REQ_SUBMITTED ) {
		return;
	}

	if(is_blocking) {
		for(;;) {
			if( poll() < 0 ) {
				// result error
				req->detachVMGahpServer();
				req->setPendingStatus(REQ_RESULT_ERROR);
				return;
			}
			if( req->getPendingStatus() == REQ_DONE) {
				// We got the result, stop blocking
				return;
			}
			if( req->isPendingTimeout()) {
				// We timed out, stop blocking
				dprintf(D_ALWAYS,"VM GAHP command('%s') exceeds timeout(%d seconds)\n", 
						req->getCommand(), req->getTimeout());
				return;
			}
			dprintf(D_FULLDEBUG,"VM GAHP command('%s') is not completed yet, "
					"waiting for 2 seconds\n", req->getCommand());
			sleep(2); // block for one second and then poll again...
		}
	}else {
		// Here we just try to poll one time.
		// Usually it seems to be not necessary if we use async mode.
		// but ...
		if( poll() < 0 ) {
			// result error
			req->detachVMGahpServer();
			req->setPendingStatus(REQ_RESULT_ERROR);
			return;
		}
	}

	return;
}

bool 
VMGahpServer::isSupportedCommand(const char *command) 
{
	if(contains_anycase(m_commands_supported, command)==false) {
		dprintf(D_ALWAYS, "'%s' command is not supported by the gahp-server\n", 
				command);
		return false;
	}
	return true;
}

bool 
VMGahpServer::isSupportedVMType(const char *vmtype)
{
	return contains_anycase(m_vms_supported, vmtype);
}

void 
VMGahpServer::printSystemErrorMsg(void) 
{
	err_pipe_ready();
}

bool
VMGahpServer::publishVMClassAd(const char *workingdir)
{
	static const char* command = "CLASSAD";

	if( !m_job_ad || !workingdir ) {
		return false;
	}

	if( m_is_initialized == false ) {
		return false;
	}

	if( isSupportedCommand(command) == FALSE ) {
		return false;
	}

	// Before sending command, print the remaining stderr messages from vmgahp
	printSystemErrorMsg();

	if(write_line(command) == false) {
		return false;
	}

	// give some time to gahp server
	sleep(1);

	Gahp_Args start_result;
	if( read_argv(start_result) == false ){
		return false;
	}

	if( start_result.argc == 0 || start_result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"VMGAHP command '%s' failed\n",command);
		return false;
	}

	// Send Working directory
	std::string vmAttr;
	formatstr(vmAttr, "VM_WORKING_DIR = \"%s\"", workingdir);

	if ( write_line( vmAttr.c_str() ) == false ) {
		return false;
	}

	// publish job ClassAd to vmgahp server via pipe
	bool can_send_it = false;
	int total_len = 0;

	const char *name;
	ExprTree *expr = NULL;
	for( auto itr = m_job_ad->begin(); itr != m_job_ad->end(); itr++ ) {
		name = itr->first.c_str();
		expr = itr->second;
		can_send_it = false;

		if( !m_send_all_classad ) {
			// Instead of sending entire job ClassAd to vmgahp, 
			// we will send some part of job ClassAd necessary to vmgahp. 
			if( !strncasecmp( name, "JobVM", strlen("JobVM") ) ||
				!strncasecmp( name, "VMPARAM", strlen("VMPARAM") ) ||
				!strncasecmp( name, ATTR_CLUSTER_ID, strlen(ATTR_CLUSTER_ID)) ||
				!strncasecmp( name, ATTR_PROC_ID, strlen(ATTR_PROC_ID)) ||
				!strncasecmp( name, ATTR_USER, strlen(ATTR_USER)) ||
				!strncasecmp( name, ATTR_ORIG_JOB_IWD, 
					strlen(ATTR_ORIG_JOB_IWD)) ||
				!strncasecmp( name, ATTR_JOB_ARGUMENTS1, 
					strlen(ATTR_JOB_ARGUMENTS1)) ||
				!strncasecmp( name, ATTR_JOB_ARGUMENTS2, 
					strlen(ATTR_JOB_ARGUMENTS2)) ||
				!strncasecmp( name, ATTR_TRANSFER_INTERMEDIATE_FILES, 
					strlen(ATTR_TRANSFER_INTERMEDIATE_FILES)) ||
				!strncasecmp( name, ATTR_TRANSFER_INPUT_FILES, 
					strlen(ATTR_TRANSFER_INPUT_FILES)) ) {
				can_send_it = true;
			}
		}else {
			// We will send the entire job ClassAd to vmgahp
			can_send_it = true;
		}

		if( !can_send_it ) {
			continue;
		}

		formatstr( vmAttr, "%s = %s", name, ExprTreeToString( expr ) );

		if ( write_line( vmAttr.c_str() ) == false ) {
			return false;
		}
		total_len += vmAttr.length();
		if( total_len > 2048 ) {
			// Give some time for vmgahp to read this pipe
			sleep(1);
			printSystemErrorMsg();
			total_len = 0;
		}
	}

	static const char *endcommand = "CLASSAD_END";

	if(write_line(endcommand) == false) {
		return false;
	}

	// give some time to vmgahp
	sleep(1);

	Gahp_Args end_result;
	if( read_argv(end_result) == false ) {
		return false;
	}
	if( end_result.argc == 0 || end_result.argv[0][0] != 'S' ) {
		dprintf(D_ALWAYS,"VMGAHP command '%s' failed\n",endcommand);
		return false;
	}

	m_workingdir = workingdir;
	return true;
}

void
VMGahpServer::killVM(void)
{
	if( m_vm_type.empty() || m_vmgahp_server.empty() ) {
		return;
	}

	if( m_workingdir.empty() ) {
		dprintf(D_ALWAYS, "VMGahpServer::killVM() : no workingdir\n");
		return;
	}

	std::string matchstring;
	if( (strcasecmp(m_vm_type.c_str(), CONDOR_VM_UNIVERSE_XEN ) == MATCH) || (strcasecmp(m_vm_type.c_str(), CONDOR_VM_UNIVERSE_KVM ) == MATCH) ) {
		if( create_name_for_VM(m_job_ad, matchstring) == false ) {
			dprintf(D_ALWAYS, "VMGahpServer::killVM() : "
					"cannot make the name of VM\n");
			return;
		}
	}else {
		// Except Xen, we need the path of working directory of Starter 
		// in order to destroy a VM.
		matchstring = m_workingdir;
	}

	if( matchstring.empty() ) {
		dprintf(D_ALWAYS, "VMGahpServer::killVM() : empty matchstring\n");
		return;
	}

	// vmgahp is daemonCore, so we need to add -f -t options of daemonCore.
	// Then, try to execute vmgahp with 
	// vmtype <vmtype> match <string>"
	ArgList systemcmd;
	systemcmd.AppendArg(m_vmgahp_server);
	systemcmd.AppendArg("-f");
	if( m_include_gahp_log ) {
		systemcmd.AppendArg("-t");
	}
	systemcmd.AppendArg("-M");
	systemcmd.AppendArg(std::to_string(VMGAHP_KILL_MODE));
	systemcmd.AppendArg("vmtype");
	systemcmd.AppendArg(m_vm_type);
	systemcmd.AppendArg("match");
	systemcmd.AppendArg(matchstring);

#if !defined(WIN32)
	if( can_switch_ids() ) {
		std::string tmp_str;
		formatstr(tmp_str, "%d", (int)get_condor_uid());
		SetEnv("VMGAHP_USER_UID", tmp_str.c_str());
	}
#endif

	priv_state oldpriv; 
	if( (strcasecmp(m_vm_type.c_str(), CONDOR_VM_UNIVERSE_XEN ) == MATCH) || (strcasecmp(m_vm_type.c_str(), CONDOR_VM_UNIVERSE_KVM ) == MATCH) ) {
		oldpriv = set_root_priv();
	}else {
		oldpriv = set_user_priv();
	}
	int ret = my_system(systemcmd);
	set_priv(oldpriv);

	if( ret == 0 ) {
		dprintf( D_FULLDEBUG, "VMGahpServer::killVM() is called with "
							"'%s'\n", matchstring.c_str());
	}else {
		dprintf( D_FULLDEBUG, "VMGahpServer::killVM() failed!\n");
	}

	return;
}

void 
VMGahpServer::setVMid(int vm_id)
{
	m_vm_id = vm_id;
}
