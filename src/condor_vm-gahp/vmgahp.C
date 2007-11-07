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
#include "env.h"
#include "condor_environ.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "condor_string.h"
#include "MyString.h"
#include "condor_attributes.h"
#include "condor_vm_universe_types.h"
#include "vmgahp_common.h"
#include "vmgahp.h"
#include "vm_type.h"
#include "vmware_type.h"
#include "xen_type.h"
#include "vmgahp_error_codes.h"

#define QUIT_FAST_TIME				30		// 30 seconds

extern const char* vmgahp_version;
extern const char* support_vms_list[];

static const char *commands_list[] = {
VMGAHP_COMMAND_ASYNC_MODE_ON,
VMGAHP_COMMAND_ASYNC_MODE_OFF,
VMGAHP_COMMAND_VM_START,
VMGAHP_COMMAND_VM_STOP,
VMGAHP_COMMAND_VM_SUSPEND,
VMGAHP_COMMAND_VM_SOFT_SUSPEND,
VMGAHP_COMMAND_VM_RESUME,
VMGAHP_COMMAND_VM_CHECKPOINT,
VMGAHP_COMMAND_VM_STATUS,
VMGAHP_COMMAND_VM_GETPID,
VMGAHP_COMMAND_QUIT,
VMGAHP_COMMAND_VERSION,
VMGAHP_COMMAND_COMMANDS,
VMGAHP_COMMAND_SUPPORT_VMS,
VMGAHP_COMMAND_RESULTS,
VMGAHP_COMMAND_CLASSAD,
VMGAHP_COMMAND_CLASSAD_END,
NULL
};

#if defined(WIN32)
int forwarding_pipe = -1;
unsigned __stdcall pipe_forward_thread(void *)
{
	const int FORWARD_BUFFER_SIZE = 4096;
	char buf[FORWARD_BUFFER_SIZE];
	
	// just copy everything from stdin to the forwarding pipe
	while (true) {

		// read from stdin
		int bytes = read(0, buf, FORWARD_BUFFER_SIZE);
		if (bytes == -1) {
			dprintf(D_ALWAYS, "pipe_forward_thread: error reading from stdin\n");
			daemonCore->Close_Pipe(forwarding_pipe);
			return 1;
		}

		// close forwarding pipe and exit on EOF
		if (bytes == 0) {
			daemonCore->Close_Pipe(forwarding_pipe);
			return 0;
		}

		// write to the forwarding pipe
		char* ptr = buf;
		while (bytes) {
			int bytes_written = daemonCore->Write_Pipe(forwarding_pipe, ptr, bytes);
			if (bytes_written == -1) {
				dprintf(D_ALWAYS, "pipe_forward_thread: error writing to forwarding pipe\n");
				daemonCore->Close_Pipe(forwarding_pipe);
				return 1;
			}
			ptr += bytes_written;
			bytes -= bytes_written;
		}
	}

}
#endif

VMGahp::VMGahp(VMGahpConfig* config, const char* iwd)
	: m_pending_req_table(20, &hashFuncInt)
{
	m_stdout_pipe = -1;
	m_stderr_pipe = -1;
	m_async_mode = true;
	m_new_results_signaled = false;
	m_jobAd = NULL;
	m_inClassAd = false;
	m_gahp_config = config;
	m_workingdir = iwd;

	m_max_vm_id = 0;

	m_flush_request_tid = -1;
	m_need_output_for_quit = false;
}

VMGahp::~VMGahp()
{
	cleanUp();
	if( m_gahp_config ) {
		delete m_gahp_config;
	}
	if( m_jobAd ) {
		delete m_jobAd;
	}
}

void
VMGahp::startWorker()
{
	if( vmgahp_mode != VMGAHP_IO_MODE ) {
		return;
	}

	// Find the location of this vmgahp server from condor config
	MyString vmgahp_server;
	char *vmgahpserver = param("VM_GAHP_SERVER");
	if( !vmgahpserver || (access(vmgahpserver,X_OK) < 0 ) ) { 
		vmprintf(D_ALWAYS, "vmgahp server cannot be found/exectued\n");
		DC_Exit(1);
	}
	vmgahp_server = vmgahpserver;
	free(vmgahpserver);

	// vm gahp configuration file
	MyString vmgahp_config;
	vmgahp_config = m_gahp_config->m_configfile;
	
	// vm_type
	MyString vm_type;
	vm_type = m_gahp_config->m_vm_type;

	// To avoid deadlock, the pipe for writing request to worker 
	// is non-blocking mode.
	if (!daemonCore->Create_Pipe (m_worker.m_stdin_pipefds,
				true,	// read end registerable
				false,	// write end not registerable
				false,	// read end blocking
				true	// write end non-blocking
				) ) {
		vmprintf(D_ALWAYS, "unable to create a pipe to stdin of VMGahp worker\n");
		DC_Exit(1);
	}
	if (!daemonCore->Create_Pipe (m_worker.m_stdout_pipefds,
				true,	// read end registerable
				false,	// write end not registerable
				false,	// read end blocking
				false	// write end blocking
				) ) {
		vmprintf(D_ALWAYS, "unable to create a pipe to stdout of VMGahp worker\n");
		DC_Exit(1);
	}

	// The pipe for debug messages or errors from worker 
	// is always non-blocking mode
	if (!daemonCore->Create_Pipe (m_worker.m_stderr_pipefds,
				true,	// read end registerable
				false,	// write end not registerable
				true,	// read end non-blocking
				true	// write end non-blocking
				) ) {
		vmprintf(D_ALWAYS, "unable to create a pipe to stderr of VMGahp worker\n");
		DC_Exit(1);
	}

	m_worker.m_request_buffer.setPipeEnd(m_worker.m_stdin_pipefds[1]);
	m_worker.m_result_buffer.setPipeEnd(m_worker.m_stdout_pipefds[0]);
	m_worker.m_stderr_buffer.setPipeEnd(m_worker.m_stderr_pipefds[0]);

	(void)daemonCore->Register_Pipe (m_worker.m_result_buffer.getPipeEnd(),
			"Worker result pipe",
			(PipeHandlercpp)&VMGahp::workerResultHandler,
			"VMGahp::workerResultHandler",
			this);

	(void)daemonCore->Register_Pipe (m_worker.m_stderr_buffer.getPipeEnd(),
			"Worker stderr pipe",
			(PipeHandlercpp)&VMGahp::workerStderrHandler,
			"VMGahp::workerStderrHandler",
			this);

	// Register the reaper for the child process
	int reaper_id =
		daemonCore->Register_Reaper(
				"workerReaper",
				(ReaperHandlercpp)&VMGahp::workerReaper,
				"workerReaper",
				this);

	m_flush_request_tid = 
		daemonCore->Register_Timer(1,
				1,
				(TimerHandlercpp)&VMGahp::flushPendingRequests,
				"VMGahp::flushPendingRequests",
				this);
	if( m_flush_request_tid == -1 ) {
		vmprintf( D_ALWAYS, "cannot register timer for request to worker\n");
		DC_Exit(1);
	}

	Env job_env;

#if !defined(WIN32)
	// set CONDOR_IDS environment
	const char *envName = EnvGetName(ENV_UG_IDS);

	if( envName ) {
		MyString env_string;
		env_string.sprintf("%d.%d", (int)get_condor_uid(), (int)get_condor_gid());
		job_env.SetEnv(envName, env_string.Value());
	}

	if( get_job_user_uid() > 0 ) {
		MyString tmp_str;
		tmp_str.sprintf("%d", (int)get_job_user_uid());
		job_env.SetEnv("VMGAHP_USER_UID", tmp_str.Value());

		if( get_job_user_gid() > 0 ) {
			tmp_str.sprintf("%d", (int)get_job_user_gid());
			job_env.SetEnv("VMGAHP_USER_GID", tmp_str.Value());
		}
	}
#endif

	// Set vmgahp env
	job_env.SetEnv("VMGAHP_CONFIG", vmgahp_config.Value());
	job_env.SetEnv("VMGAHP_VMTYPE", vm_type.Value());
	job_env.SetEnv("VMGAHP_WORKING_DIR", m_workingdir.Value());

	// Grab the full environment back out of the Env object
	if(DebugFlags & D_FULLDEBUG) {
		MyString env_str;
		job_env.getDelimitedStringForDisplay(&env_str);
		vmprintf(D_FULLDEBUG, "Worker Env = %s\n", env_str.Value());
	}

	int io_redirect[3];
	io_redirect[0] = m_worker.m_stdin_pipefds[0];
	io_redirect[1] = m_worker.m_stdout_pipefds[1];
	io_redirect[2] = m_worker.m_stderr_pipefds[1];

	ArgList args;
	args.AppendArg(vmgahp_server.Value());
	args.AppendArg("-f");
	args.AppendArg("-t");
	args.AppendArg("-M");
	args.AppendArg(VMGAHP_WORKER_MODE);

	MyString args_string;
	args.GetArgsStringForDisplay(&args_string);

	vmprintf( D_ALWAYS, "Starting worker : %s\n",args_string.Value());

	priv_state vmgahp_priv = PRIV_UNKNOWN;
#if !defined(WIN32)
	if( get_caller_uid() == ROOT_UID ) {
		vmgahp_priv = PRIV_ROOT;
	}
#endif

	m_worker.m_pid = daemonCore->Create_Process(
			vmgahp_server.Value(),	//Name of executable
			args,	//Args
			vmgahp_priv, 	//Priv state
			reaper_id, 		//id for our registered reaper
			FALSE, 		//do not want a command port
			&job_env, 	//env
			NULL,		//cwd
			NULL,		 //family_info
			NULL, 		//network sockets to inherit
			io_redirect	//redirect stdin/out/err
			);

	//NOTE: Create_Process() saves the errno for us if it is an
	//"interesting" error.
	char const *create_process_error = NULL;
	if(m_worker.m_pid == FALSE && errno) { 
		create_process_error = strerror(errno);
	}

	daemonCore->Close_Pipe(io_redirect[0]);
	daemonCore->Close_Pipe(io_redirect[1]);
	daemonCore->Close_Pipe(io_redirect[2]);

	if( m_worker.m_pid == FALSE ) {
		m_worker.m_pid = 0;
		vmprintf(D_ALWAYS, "Failed to start worker : %s\n",create_process_error);
		DC_Exit(1);
	}

	vmprintf(D_ALWAYS, "Worker pid=%d\n", m_worker.m_pid);
}

void
VMGahp::startUp()
{
	if( (vmgahp_mode == VMGAHP_TEST_MODE) ||
		(vmgahp_mode == VMGAHP_KILL_MODE)) {
		return;
	}

	// Setting pipes from Starter
	int stdin_pipe = -1; 

#if defined(WIN32)
	// if our parent is not DaemonCore, then our assumption that
	// the pipe we were passed in via stdin is overlapped-mode
	// is probably wrong. we therefore create a new pipe with the
	// read end overlapped and start a "forwarding thread"
	char* tmp;
	if ((tmp = daemonCore->InfoCommandSinfulString(daemonCore->getppid())) == NULL) {

		vmprintf(D_ALWAYS, "parent is not DaemonCore: "
				"creating a forwarding thread\n");

		int pipe_ends[2];
		if (daemonCore->Create_Pipe(pipe_ends, true) == FALSE) {
			vmprintf(D_ALWAYS,"failed to create forwarding pipe");
			DC_Exit(1);
		}
		forwarding_pipe = pipe_ends[1];
		HANDLE thread_handle = (HANDLE)_beginthreadex(NULL,                   // default security
		                                              0,                      // default stack size
		                                              pipe_forward_thread,    // start function
		                                              NULL,                   // arg: write end of pipe
		                                              0,                      // not suspended
													  NULL);                  // don't care about the ID
		if (thread_handle == NULL) {
			vmprintf(D_ALWAYS,"failed to create forwarding thread");
			DC_Exit(1);
		}
		CloseHandle(thread_handle);
		stdin_pipe = pipe_ends[0];
	}
#endif

	if( stdin_pipe == -1 ) {
		stdin_pipe = daemonCore->Inherit_Pipe(fileno(stdin),
				false,		//read pipe
				true,		//registerable
				false);		//blocking
	}

	if( stdin_pipe == -1 ) {
			vmprintf(D_ALWAYS,"Can't get stdin pipe");
			DC_Exit(1);
	}

	m_request_buffer.setPipeEnd(stdin_pipe);
	(void)daemonCore->Register_Pipe(m_request_buffer.getPipeEnd(),
			"stdin_pipe",
			(PipeHandlercpp)&VMGahp::waitForCommand,
			"VMGahp::waitForCommand", this);
}

void
VMGahp::cleanUp()
{
	static bool is_called = false; 

	if( is_called ) {
		return;
	}
	is_called = true;

	// delete reqs
	int currentkey = 0;
	VMRequest *req = NULL;
	m_pending_req_table.startIterations();
	while( m_pending_req_table.iterate(currentkey, req) != 0 ) {
		if( req ) {
			delete req;
		}
	}
	m_pending_req_table.clear();

	m_result_list.clearAll();

	// delete VMs
	VMType *vm = NULL;
	m_vm_list.Rewind();
	while( m_vm_list.Next(vm) ) {
		m_vm_list.DeleteCurrent();
		delete vm;
	}

	// Cancel registered timers
	if( m_flush_request_tid != -1 ) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(m_flush_request_tid);
		}
		m_flush_request_tid = -1;
	}
	if( vmgahp_stderr_tid != -1 ) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(vmgahp_stderr_tid);
		}
		vmgahp_stderr_tid = -1;
	}

	m_request_buffer.setPipeEnd(-1);
	vmgahp_stderr_pipe = -1;
	vmgahp_stderr_buffer.setPipeEnd(-1);

	if( m_worker.m_pid > 0 ) {
		// close all pipes for worker
		if( daemonCore ) {
			int pipe_end = m_worker.m_request_buffer.getPipeEnd();
			if( pipe_end > 0 ) {
				daemonCore->Close_Pipe(pipe_end);
			}
			pipe_end = m_worker.m_result_buffer.getPipeEnd();
			if( pipe_end > 0 ) {
				daemonCore->Close_Pipe(pipe_end);
			}
			pipe_end = m_worker.m_stderr_buffer.getPipeEnd();
			if( pipe_end > 0 ) {
				daemonCore->Close_Pipe(pipe_end);
			}
		}
		m_worker.m_request_buffer.setPipeEnd(-1);
		m_worker.m_result_buffer.setPipeEnd(-1);
		m_worker.m_stderr_buffer.setPipeEnd(-1);
	}

	if( vmgahp_mode != VMGAHP_WORKER_MODE ) {
		// kill all processes such as worker and existing VMs
		killAllProcess();
	}

	if( m_need_output_for_quit ) {
		returnOutputSuccess();
		sleep(2);
	}
	m_need_output_for_quit = false;
}

int 
VMGahp::getNewVMId(void)
{
	m_max_vm_id++;
	return m_max_vm_id;
}

int 
VMGahp::numOfVM(void)
{
	 return m_vm_list.Number();
}

int
VMGahp::numOfReq(void)
{
	 return numOfPendingReq() + numOfReqWithResult();
}

int
VMGahp::numOfPendingReq(void)
{
	 return m_pending_req_table.getNumElements();
}

int
VMGahp::numOfReqWithResult(void)
{
	 return m_result_list.number();
}

VMRequest *
VMGahp::addNewRequest(const char *cmd)
{
	if(!cmd) {
		return NULL;
	}
	VMRequest *new_req = new VMRequest(cmd);
	ASSERT(new_req);

	m_pending_req_table.insert(new_req->m_reqid, new_req);
	return new_req;
}

void 
VMGahp::removePendingRequest(int req_id)
{
	VMRequest *req = findPendingRequest(req_id);
	if( req != NULL ) {
		m_pending_req_table.remove(req_id);
		delete req;
		return;
	}
}

void 
VMGahp::removePendingRequest(VMRequest *req)
{
	if(!req) {
		return;
	}
	m_pending_req_table.remove(req->m_reqid);
	delete req;
	return;
}

void
VMGahp::movePendingReqToResultList(VMRequest *req)
{
	if(!req) {
		return;
	}
	m_result_list.append(make_result_line(req));
	removePendingRequest(req);
}

void
VMGahp::movePendingReqToOutputStream(VMRequest *req)
{
	if(!req) {
		return;
	}
	MyString output = make_result_line(req);
	output += "\n";

	write_to_daemoncore_pipe(vmgahp_stdout_pipe, 
			output.Value(), output.Length());

	removePendingRequest(req);
}

void
VMGahp::printAllReqsWithResult()
{
	char *one_result = NULL;
	MyString output;
	m_result_list.rewind();
	while( (one_result = m_result_list.next()) != NULL ) {
		output = one_result;
		output += "\n";
		write_to_daemoncore_pipe(vmgahp_stdout_pipe, 
				output.Value(), output.Length());
		m_result_list.deleteCurrent();
	}
}

VMRequest *
VMGahp::findPendingRequest(int req_id)
{
	VMRequest *req = NULL;

	m_pending_req_table.lookup(req_id, req);

	return req;
}

void
VMGahp::addVM(VMType *new_vm)
{
	m_vm_list.Append(new_vm);
}

void
VMGahp::removeVM(int vm_id)
{
	VMType *vm = findVM(vm_id);
	if( vm != NULL ) {
		m_vm_list.Delete(vm);
		delete vm;
		return;
	}
}

void
VMGahp::removeVM(VMType *vm)
{
	if( !vm ) {
		return;
	}
	m_vm_list.Delete(vm);
	delete vm;
	return;
}

VMType *
VMGahp::findVM(int vm_id)
{
	VMType *vm = NULL;

	m_vm_list.Rewind();
	while( m_vm_list.Next(vm) ) {
		if( vm->getVMId() == vm_id ) {
			return vm;
		}
	}
	return NULL;
}

int 
VMGahp::waitForCommand()
{
	MyString *line = NULL;

	while((line = m_request_buffer.GetNextLine()) != NULL) {

		const char *command = line->Value();

		Gahp_Args args;
		VMRequest *new_req = NULL;

		if( m_inClassAd )  {
			if( strcasecmp(command, VMGAHP_COMMAND_CLASSAD_END) == 0 ) {
				m_inClassAd = false;

				// Everything is Ok. Now we got vmClassAd
				returnOutputSuccess();
				if( vmgahp_mode == VMGAHP_IO_MODE ) {
					sendClassAdToWorker();
				}
			}else {
				if( !m_jobAd->Insert(command) ) {
					vmprintf(D_ALWAYS, "Failed to insert \"%s\" into classAd, " 
							"ignoring this attribute\n", command);
				}
			}
		}else {
			if(parse_vmgahp_command(command, args) && 
					verifyCommand(args.argv, args.argc)) {
				new_req = preExecuteCommand(command, &args);

				if( new_req != NULL ) {
					if( vmgahp_mode == VMGAHP_IO_MODE ) {
						// Send a new request to Worker
						sendRequestToWorker(command);
					}else {
						// Execute the new request
						executeCommand(new_req);
						if(new_req->m_has_result) {
							if( vmgahp_mode == VMGAHP_WORKER_MODE ) {
								movePendingReqToOutputStream(new_req);
							}else {
								movePendingReqToResultList(new_req);
								if (m_async_mode) {
									if (!m_new_results_signaled) {
										write_to_daemoncore_pipe("R\n");
									}
									// So that we only do it once
									m_new_results_signaled = true;    
								} 
							}
						}
					}
				}
			}else {
				returnOutputError();
			}
		}

		delete line;
		line = NULL;
	}

	// check if GetNextLine() returned NULL because of an error or EOF
	if(m_request_buffer.IsError() || m_request_buffer.IsEOF()) {
		vmprintf(D_ALWAYS, "Request buffer closed, exiting\n");
		cleanUp();
		DC_Exit(0);
	}
	return true;
}

// Check the validity of the given parameters
bool VMGahp::verifyCommand(char **argv, int argc) {
	if(strcasecmp(argv[0], VMGAHP_COMMAND_VM_START) == 0 ) {
		// Expecting: VMGAHP_COMMAND_VM_START <req_id> <type>
		return verify_number_args(argc, 3) && 
			verify_request_id(argv[1]) &&
			verify_vm_type(argv[2]);

	} else if(strcasecmp(argv[0], VMGAHP_COMMAND_VM_STOP) == 0 || 
		strcasecmp(argv[0], VMGAHP_COMMAND_VM_SUSPEND) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_VM_SOFT_SUSPEND) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_VM_RESUME) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_VM_CHECKPOINT) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_VM_STATUS) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_VM_GETPID) == 0) {
		// Expecting: VMGAHP_COMMAND_VM_STOP <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_SUSPEND <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_SOFT_SUSPEND <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_RESUME <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_CHECKPOINT <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_STATUS <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_GETPID <req_id> <vmid>
		
		return verify_number_args(argc, 3) && 
			verify_request_id(argv[1]) &&
			verify_vm_id(argv[2]);

	} else if(strcasecmp(argv[0], VMGAHP_COMMAND_ASYNC_MODE_ON) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_ASYNC_MODE_OFF) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_QUIT) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_VERSION) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_COMMANDS) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_SUPPORT_VMS) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_RESULTS) == 0 || 
		strcasecmp(argv[0], VMGAHP_COMMAND_CLASSAD) == 0 ||
		strcasecmp(argv[0], VMGAHP_COMMAND_CLASSAD_END) == 0 ) {
		//These commands need no-args
		return verify_number_args(argc,1);
	}

	vmprintf(D_ALWAYS, "Unknown command\n");
	return false;
}

bool
VMGahp::verify_request_id(const char *s) 
{
	if( verify_digit_arg(s) == false) {
		return false;
	}

	int req_id = (int)strtol(s, (char **)NULL, 10);
	if( req_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid Request id(%s)\n", s);
		return false;
	}
	// check duplicated req_id
	if( findPendingRequest(req_id) != NULL ) {
		vmprintf(D_ALWAYS, "Request id(%s) is conflict with "
						"the existing one\n", s);
		return false;
	}

	return true;
}

bool
VMGahp::verify_vm_id(const char *s)
{
	if( verify_digit_arg(s) == false) {
		return false;
	}
	return true;
}


void
VMGahp::returnOutput(const char **results, const int count) 
{
	int i=0;

	for( i=0; i<count; i++) {
		write_to_daemoncore_pipe("%s", results[i]);
		if( i < (count -1)) {
			write_to_daemoncore_pipe(" ");
		}
	}

	write_to_daemoncore_pipe("\n");
}

void
VMGahp::returnOutputSuccess(void) 
{
	// In worker mode, we don't need to send this output to IO server
	if( vmgahp_mode == VMGAHP_WORKER_MODE ) {
		return;
	}
	const char* result[] = {VMGAHP_RESULT_SUCCESS};
	returnOutput(result,1);
}

void
VMGahp::returnOutputError(void) 
{
	// In worker mode, we don't need to send this output to IO server
	if( vmgahp_mode == VMGAHP_WORKER_MODE ) {
		return;
	}
	const char* result[] = {VMGAHP_RESULT_ERROR};
	returnOutput(result,1);
}

VMRequest *
VMGahp::preExecuteCommand(const char* cmd, Gahp_Args *args) 
{
	char *command = args->argv[0];

	vmprintf(D_FULLDEBUG, "Command: %s\n", command);

	// Special commands first
	if(strcasecmp(command, VMGAHP_COMMAND_ASYNC_MODE_ON) == 0 ) {
		m_async_mode = true;
		m_new_results_signaled = false;
		returnOutputSuccess();
	} else if(strcasecmp(command, VMGAHP_COMMAND_ASYNC_MODE_OFF) == 0 ) {
		m_async_mode = false;
		returnOutputSuccess();
	} else if(strcasecmp(command, VMGAHP_COMMAND_QUIT) == 0 ) {
		if( vmgahp_mode != VMGAHP_IO_MODE) {
			executeQuit();
		}else {
			m_need_output_for_quit = true;

			daemonCore->Register_Timer( QUIT_FAST_TIME, 0,
				(TimerHandlercpp)&VMGahp::quitFast,
				"VMGahp::quitFast",
				this);

			vmprintf( D_FULLDEBUG, "Started timer to call quitFast "
								"in %d seconds\n", QUIT_FAST_TIME);

			if( Termlog && (vmgahp_stderr_pipe != -1 ) ) {
				daemonCore->Cancel_Timer(vmgahp_stderr_tid);
				vmgahp_stderr_tid = -1;
				vmgahp_stderr_pipe = -1;
				vmgahp_stderr_buffer.setPipeEnd(-1);
			}

			// In IO mode, QUIT command will be relayed to Worker
			sendRequestToWorker(command);
		}
	} else if(strcasecmp(command, VMGAHP_COMMAND_VERSION) == 0 ) {
		executeVersion();
	} else if(strcasecmp(command, VMGAHP_COMMAND_COMMANDS) == 0 ) {
		executeCommands();
	} else if(strcasecmp(command, VMGAHP_COMMAND_SUPPORT_VMS) == 0 ) {
		executeSupportVMS();
	} else if(strcasecmp(command, VMGAHP_COMMAND_RESULTS) == 0 ) {
		executeResults();
	} else if(strcasecmp(command, VMGAHP_COMMAND_CLASSAD) == 0 ) {
		if( m_jobAd != NULL ) {
			delete m_jobAd;
			m_jobAd = NULL;
		}
		m_jobAd = new ClassAd;
		ASSERT(m_jobAd);
		m_inClassAd = true;
		returnOutputSuccess();
	} else if(strcasecmp(command, VMGAHP_COMMAND_CLASSAD_END) == 0 ) {
		if( m_jobAd != NULL ) {
			delete m_jobAd;
			m_jobAd = NULL;
		}
		m_inClassAd = false;
		returnOutputSuccess();
	} else {
		VMRequest *new_req;
		new_req = addNewRequest(cmd);		
		returnOutputSuccess();
		return new_req;
	}
	return NULL;
}

void
VMGahp::executeCommand(VMRequest *req)
{
	char *command = req->m_args.argv[0];

	priv_state priv = vmgahp_set_user_priv();

	if(strcasecmp(command, VMGAHP_COMMAND_VM_START) == 0 ) {
		executeStart(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_STOP) == 0 ) {
		executeStop(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_SUSPEND) == 0 ) {
		executeSuspend(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_SOFT_SUSPEND) == 0 ) {
		executeSoftSuspend(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_RESUME) == 0 ) {
		executeResume(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_CHECKPOINT) == 0 ) {
		executeCheckpoint(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_STATUS) == 0 ) {
		executeStatus(req);
	} else if(strcasecmp(command, VMGAHP_COMMAND_VM_GETPID) == 0 ) {
		executeGetpid(req);
	} else {
		vmprintf(D_ALWAYS, "Unknown command(%s)\n", command);
	}

	vmgahp_set_priv(priv);
}

void
VMGahp::executeStart(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_START <req_id> <type>
	char* vmtype = req->m_args.argv[2];

	if( m_jobAd == NULL ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_JOBCLASSAD_INFO;
		return; 
	}

	MyString vmworkingdir;
	if( m_jobAd->LookupString( "VM_WORKING_DIR", vmworkingdir) != 1 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_JOBCLASSAD_INFO;
		vmprintf(D_ALWAYS, "VM_WORKING_DIR cannot be found in vm classAd\n");
		return;
	}

	MyString job_vmtype;
	if( m_jobAd->LookupString( ATTR_JOB_VM_TYPE, job_vmtype) != 1 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_JOBCLASSAD_INFO;
		vmprintf(D_ALWAYS, "VM_TYPE('%s') cannot be found in vm classAd\n", 
							ATTR_JOB_VM_TYPE);
		return;
	}

	if(strcasecmp(vmtype, job_vmtype.Value()) != 0 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_SUPPORTED_VM_TYPE;
		vmprintf(D_ALWAYS, "Argument is %s but VM_TYPE in job classAD "
						"is %s\n", vmtype, job_vmtype.Value());
		return;
	}

	if(strcasecmp(vmtype, m_gahp_config->m_vm_type.Value()) != 0 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_SUPPORTED_VM_TYPE;
		return;
	}

	VMType *new_vm = NULL;
#if defined(LINUX)
	if(strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == 0 ) {
		new_vm = new XenType(m_gahp_config->m_vm_script.Value(), 
				vmworkingdir.Value(), m_jobAd);
		ASSERT(new_vm);
	}else 
#endif
	if(strcasecmp(vmtype, CONDOR_VM_UNIVERSE_VMWARE) == 0 ) {
		new_vm = new VMwareType(m_gahp_config->m_prog_for_script.Value(), 
				m_gahp_config->m_vm_script.Value(), 
				vmworkingdir.Value(), m_jobAd);
		ASSERT(new_vm);
	}else {
		// We should not reach here
		vmprintf(D_ALWAYS, "vmtype(%s) is not yet implemented\n", vmtype);
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_SUPPORTED_VM_TYPE;
		return; 
	}

	if( new_vm->CreateConfigFile() == false ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = makeErrorMessage(new_vm->m_result_msg.Value());
		delete new_vm;
		new_vm = NULL;
		vmprintf(D_FULLDEBUG, "CreateConfigFile fails in executeStart!\n");
		return; 
	}

	int result = new_vm->Start();

	if(result == false) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = makeErrorMessage(new_vm->m_result_msg.Value());
		delete new_vm;
		new_vm = NULL;
		vmprintf(D_FULLDEBUG, "executeStart fail!\n");
		return;
	} else {
		// Success to create a new VM
		req->m_has_result = true;
		req->m_is_success = true;

		// Result is set to a new vm_id
		req->m_result = "";
		req->m_result += new_vm->getVMId();

		addVM(new_vm);
		vmprintf(D_FULLDEBUG, "executeStart success!\n");
		return;
	}
}

void
VMGahp::executeStop(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_STOP <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_STOP command\n", 
					req->m_args.argv[2]);
		return;
	}

	MyString err_message;
	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_VM_NOT_FOUND;
		vmprintf(D_FULLDEBUG, "VM(id=%d) is not found in executeStop\n", vm_id);
		return;
	}else {
		int result = vm->Shutdown();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.Value());
			removeVM(vm);
			vmprintf(D_FULLDEBUG, "executeStop fail!\n");
			return;
		} else {
			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = "";
			removeVM(vm);
			vmprintf(D_FULLDEBUG, "executeStop success!\n");
			return;
		}
	}
}

void
VMGahp::executeSuspend(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_SUSPEND <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_SUSPEND command\n", 
				req->m_args.argv[2]);
		return;
	}

	MyString err_message;
	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_VM_NOT_FOUND;
		vmprintf(D_FULLDEBUG, "VM(id=%d) is not found in executeSuspend\n", vm_id);
		return;
	}else {
		int result = vm->Suspend();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.Value());
			vmprintf(D_FULLDEBUG, "executeSuspend fail!\n");
			return;
		} else {
			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = "";
			vmprintf(D_FULLDEBUG, "executeSuspend success!\n");
			return;
		}
	}
}

void
VMGahp::executeSoftSuspend(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_SOFT_SUSPEND <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_SOFT_SUSPEND command\n", 
				req->m_args.argv[2]);
		return;
	}

	MyString err_message;
	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_VM_NOT_FOUND;
		vmprintf(D_FULLDEBUG, "VM(id=%d) is not found in executeSoftSuspend\n",
			   	vm_id);
		return;
	}else {
		int result = vm->SoftSuspend();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.Value());
			vmprintf(D_FULLDEBUG, "executeSoftSuspend fail!\n");
			return;
		} else {
			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = "";
			vmprintf(D_FULLDEBUG, "executeSoftSuspend success!\n");
			return;
		}
	}
}

void
VMGahp::executeResume(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_RESUME <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_RESUME command\n", 
				req->m_args.argv[2]);
		return;
	}

	MyString err_message;
	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_VM_NOT_FOUND;
		vmprintf(D_FULLDEBUG, "VM(id=%d) is not found in executeResume\n", 
					vm_id);
		return;
	}else {
		int result = vm->Resume();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.Value());
			vmprintf(D_FULLDEBUG, "executeResume fail!\n");
			return;
		} else {
			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = "";
			vmprintf(D_FULLDEBUG, "executeResume success!\n");
			return;
		}
	}
}

void
VMGahp::executeCheckpoint(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_CHECKPOINT <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_CHECKPOINT command\n", 
				req->m_args.argv[2]);
		return;
	}

	MyString err_message;
	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_VM_NOT_FOUND;
		vmprintf(D_FULLDEBUG, "VM(id=%d) is not found in executeCheckpoint\n", 
					vm_id);
		return;
	}else {
		int result = vm->Checkpoint();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.Value());
			vmprintf(D_FULLDEBUG, "executeCheckpoint fail!\n");
			return;
		} else {
			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = "";
			vmprintf(D_FULLDEBUG, "executeCheckpoint success!\n");
			return;
		}
	}
}

void
VMGahp::executeStatus(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_STATUS <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_STATUS command\n", 
				req->m_args.argv[2]);
		return;
	}

	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = true;
		req->m_result = "Stopped";
		return;
	}else {
		int result = vm->Status();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.Value());
			return;
		} else {

			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = vm->m_result_msg;

			// If we have valid status of VM, we stash it.
			vm->setLastStatus(req->m_result.Value());
			return;
		}
	}
}

void
VMGahp::executeGetpid(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_GETPID <req_id> <vmid>
	int vm_id = strtol(req->m_args.argv[2],(char **)NULL, 10);
	if( vm_id <= 0 ) {
		vmprintf(D_ALWAYS, "Invalid vmid(%s) in VM_GETPID command\n", 
				req->m_args.argv[2]);
		return;
	}

	MyString err_message;
	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_VM_NOT_FOUND;
		vmprintf(D_FULLDEBUG, "VM(id=%d) is not found in executeGetPid\n", 
					vm_id);
		return;
	}else {
		req->m_has_result = true;
		req->m_is_success = true;

		// Result is set to the pid of actual process for VM
		req->m_result = "";
		req->m_result += vm->PidOfVM();
		return;
	}
}

void
VMGahp::executeQuit(void) 
{
	m_need_output_for_quit = true;
	cleanUp();
	DC_Exit(0);
}

void
VMGahp::executeVersion(void) 
{
	write_to_daemoncore_pipe("S %s\n", vmgahp_version);
}

void
VMGahp::executeCommands(void) 
{
	MyString result;
	result += "S";

	int i = 0;
	while(commands_list[i] != NULL) {
		result += " ";
		result += commands_list[i];
		i++;
	}

	write_to_daemoncore_pipe("%s\n", result.Value());
}

void
VMGahp::executeSupportVMS(void)
{
	MyString result;
	result += "S";

	int i = 0;
	while(support_vms_list[i] != NULL) {
		result += " ";
		result += support_vms_list[i];
		i++;
	}

	write_to_daemoncore_pipe("%s\n", result.Value());
}

void
VMGahp::executeResults(void) 
{
	write_to_daemoncore_pipe("S %d\n", numOfReqWithResult());

	// Print each result line
	printAllReqsWithResult();
	m_new_results_signaled = false;
}

int
VMGahp::flushPendingRequests()
{
	m_worker.m_request_buffer.Write();

	if( m_worker.m_request_buffer.IsError() ) { 
		vmprintf( D_ALWAYS, "VM GAHP Worker request buffer error, exiting...\n"); 
		cleanUp(); 
		DC_Exit(1); 
	} 

	return TRUE;
}

const char*
VMGahp::make_result_line(VMRequest *req)
{
	static MyString res_str;
	res_str = "";

	if(req->m_is_success) {
		// Success
		// Format: <req_id> 0 <result string> 
		res_str += req->m_reqid;
		res_str += " ";
		res_str += 0;
		res_str += " ";
		if( req->m_result.Length() == 0) {
			res_str += "NULL";
		} else {
			res_str += req->m_result.Value();
		}
	}else {
		// Error
		// Format: <req_id> 1 <result string>
		res_str += req->m_reqid;
		res_str += " ";
		res_str += 1;
		res_str += " ";
		if( req->m_result.Length() == 0) {
			res_str += "NULL";
		} else {
			res_str += req->m_result.Value();
		}
	}
	return res_str.Value();
}

void
VMGahp::sendRequestToWorker(const char* command)
{
	if( m_flush_request_tid == -1 ) {
		return;
	}
	MyString strRequest = command;
	strRequest += "\r\n";

	m_worker.m_request_buffer.Write(strRequest.Value());

	daemonCore->Reset_Timer(m_flush_request_tid, 0, 1);
}

void 
VMGahp::sendClassAdToWorker()
{
	vmprintf (D_FULLDEBUG, "Sending Job ClassAd to worker\n");

	if( !m_jobAd ) {
		return;
	}

	sendRequestToWorker(VMGAHP_COMMAND_CLASSAD);
	sleep(1);

	ExprTree *expr = NULL;
	MyString vmAttr;
	m_jobAd->ResetExpr();
	while( (expr = m_jobAd->NextExpr()) != NULL ) {
		expr->PrintToStr(vmAttr);
		sendRequestToWorker(vmAttr.Value());
	}

	sendRequestToWorker(VMGAHP_COMMAND_CLASSAD_END);

	// give some time to worker
	sleep(1);
}

int
VMGahp::workerResultHandler() 
{
	MyString* line = NULL;
	int req_id = 0;
	MyString reqid_str;
	while ((line = m_worker.m_result_buffer.GetNextLine()) != NULL) {

		vmprintf (D_FULLDEBUG, "Worker[%d]: Result \"%s\"\n", 
				m_worker.m_pid, line->Value());

		// Validate result string
		if( validate_vmgahp_result_string(line->Value()) == false ) {
			delete line;
			line = NULL;
			continue;
		}

		// Add this to the list
		m_result_list.append(line->Value());
	
		// Remove req from pending table
		reqid_str = parse_result_string(line->Value(), 1);
		req_id = atoi(reqid_str.Value());
		removePendingRequest(req_id);

		if (m_async_mode) {
			if (!m_new_results_signaled) {
				write_to_daemoncore_pipe("R\n");
			}
			m_new_results_signaled = true;	// So that we only do it once
		}

		delete line;
		line = NULL;
	}

	if (m_worker.m_result_buffer.IsError() || m_worker.m_result_buffer.IsEOF()) {
		vmprintf(D_ALWAYS, "VM GAHP Worker result buffer closed, exiting...\n");
		cleanUp();
		DC_Exit(0);
	}

	return TRUE;
}

int
VMGahp::workerStderrHandler() 
{
	if( m_worker.m_stderr_buffer.getPipeEnd() == -1 ) {
		return FALSE;
	}

	MyString* line = NULL;
	while ((line = m_worker.m_stderr_buffer.GetNextLine()) != NULL) {
		vmprintf(D_ALWAYS, "Worker[%d]: %s\n", m_worker.m_pid, line->Value());
		delete line;
	}

	if (m_worker.m_stderr_buffer.IsError() || m_worker.m_stderr_buffer.IsEOF()) {
		vmprintf(D_ALWAYS, "VM GAHP Worker stderr buffer closed, exiting...\n");
		cleanUp();
		DC_Exit(0);
	}

	return TRUE;
}

int
VMGahp::workerReaper(int pid, int exit_status)
{
	if( WIFSIGNALED(exit_status) ) {
		vmprintf( D_ALWAYS, "Worker exited, pid=%d, signal=%d\n", pid,
				WTERMSIG(exit_status) );
	} else {
		vmprintf( D_ALWAYS, "Worker exited, pid=%d, status=%d\n", pid,
				WEXITSTATUS(exit_status) );
	}

	if( m_need_output_for_quit ) {
		returnOutputSuccess();
		sleep(2);
	}
	m_need_output_for_quit = false;

	// Our worker exited (presumably b/c we got a QUIT command)
	// If we're in this function there shouldn't be much cleanup to be done
	// except the command queue
	DC_Exit(0);
	return TRUE;
}

int
VMGahp::quitFast()
{
	cleanUp();
	DC_Exit(0);
	return TRUE;
}

void
VMGahp::killAllProcess()
{
	static bool sent_signal = false;

	// To make sure that worker exits
	if( m_worker.m_pid > 0 ) {
		if( !sent_signal ) {
			priv_state priv = vmgahp_set_root_priv();
			daemonCore->Send_Signal(m_worker.m_pid, SIGKILL);
			vmgahp_set_priv(priv);
			sent_signal = true;
		}
	}

	if( !m_jobAd ) {
		// Virtual machine is absolutely not created.
		return;
	}
	
#if defined(LINUX)
	if( strcasecmp(m_gahp_config->m_vm_type.Value(), 
				CONDOR_VM_UNIVERSE_XEN ) == 0 ) {
		priv_state priv = vmgahp_set_root_priv();
		if( m_jobAd && XenType::checkXenParams(m_gahp_config) ) {
			MyString vmname;
			if( VMType::createVMName(m_jobAd, vmname) ) {
				XenType::killVMFast(m_gahp_config->m_vm_script.Value(), 
						vmname.Value(), m_gahp_config->m_controller.Value());
				vmprintf( D_FULLDEBUG, "killVMFast is called\n");
			}
		}
		vmgahp_set_priv(priv);
	}else
#endif
	if( strcasecmp(m_gahp_config->m_vm_type.Value(), 
				CONDOR_VM_UNIVERSE_VMWARE ) == 0 ) {
		priv_state priv = vmgahp_set_user_priv();
		if( VMwareType::checkVMwareParams(m_gahp_config) ) {
			VMwareType::killVMFast(m_gahp_config->m_prog_for_script.Value(), 
					m_gahp_config->m_vm_script.Value(), m_workingdir.GetCStr());
			vmprintf( D_FULLDEBUG, "killVMFast is called\n");
		}
		vmgahp_set_priv(priv);
	}
}
