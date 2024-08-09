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
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "condor_vm_universe_types.h"
#include "vmgahp_common.h"
#include "vmgahp.h"
#include "vm_type.h"
#include "xen_type.linux.h"
#include "vmgahp_error_codes.h"
#include <algorithm>

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
{
	m_async_mode = true;
	m_new_results_signaled = false;
	m_jobAd = NULL;
	m_inClassAd = false;
	m_gahp_config = config;
	m_workingdir = iwd;

	m_max_vm_id = 0;

	m_need_output_for_quit = false;
	vmprintf(D_FULLDEBUG, "Constructed VMGahp\n");
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

	if ( daemonCore->InfoCommandSinfulString(daemonCore->getppid()) == NULL) {

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
			static_cast<PipeHandlercpp>(&VMGahp::waitForCommand),
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
	m_pending_req_table.clear();

	m_result_list.clear();

	// delete VMs
	for (auto vm : m_vm_list) {
		delete vm;
	}
	m_vm_list.clear();

	// Cancel registered timers
	if( vmgahp_stderr_tid != -1 ) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(vmgahp_stderr_tid);
		}
		vmgahp_stderr_tid = -1;
	}

	m_request_buffer.setPipeEnd(-1);
	vmgahp_stderr_pipe = -1;
	vmgahp_stderr_buffer.setPipeEnd(-1);

	// kill all processes such as worker and existing VMs
	killAllProcess();

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

VMRequest *
VMGahp::addNewRequest(const char *cmd)
{
	if(!cmd) {
		return NULL;
	}

	VMRequest new_req(cmd);
	int req_id = new_req.m_reqid;
	auto [itr, rc] = m_pending_req_table.emplace(req_id, std::move(new_req));
	if (rc) {
		return &itr->second;
	} else {
		return NULL;
	}
}

void
VMGahp::movePendingReqToResultList(VMRequest *req)
{
	if(!req) {
		return;
	}
	m_result_list.emplace_back(make_result_line(req));

	m_pending_req_table.erase(req->m_reqid);
}

void
VMGahp::printAllReqsWithResult()
{
	for (auto& one_result: m_result_list) {
		one_result += '\n';
		write_to_daemoncore_pipe(vmgahp_stdout_pipe,
				one_result.c_str(), one_result.size());
	}
	m_result_list.clear();
}

void
VMGahp::addVM(VMType *new_vm)
{
	m_vm_list.push_back(new_vm);
}

void
VMGahp::removeVM(int vm_id)
{
	VMType *vm = findVM(vm_id);
	if( vm != nullptr ) {
		auto it = std::find(m_vm_list.begin(), m_vm_list.end(), vm);
		if (it != m_vm_list.end()) {
			delete *it;
			m_vm_list.erase(it);
		}
		return;
	}
}

void
VMGahp::removeVM(VMType *vm)
{
	if( !vm ) {
		return;
	}
	auto it = std::find(m_vm_list.begin(), m_vm_list.end(), vm);
	if (it != m_vm_list.end()) {
		delete *it;
		m_vm_list.erase(it);
	}
	return;
}

VMType *
VMGahp::findVM(int vm_id)
{
	for (auto vm : m_vm_list) {
		if( vm->getVMId() == vm_id ) {
			return vm;
		}
	}
	return nullptr;
}




int
VMGahp::waitForCommand(int   /*pipe_end*/)
{
	std::string *line = NULL;

	while((line = m_request_buffer.GetNextLine()) != NULL) {

		const char *command = line->c_str();

		std::vector<std::string> args;
		VMRequest *new_req = NULL;

		if( m_inClassAd )  {
			if( strcasecmp(command, VMGAHP_COMMAND_CLASSAD_END) == 0 ) {
				m_inClassAd = false;

				// Everything is Ok. Now we got vmClassAd
				returnOutputSuccess();
			}else {
				if( !m_jobAd->Insert(command) ) {
					vmprintf(D_ALWAYS, "Failed to insert \"%s\" into classAd, "
							"ignoring this attribute\n", command);
				}
			}
		}else {
			if(parse_vmgahp_command(command, args) &&
					verifyCommand(args)) {
				new_req = preExecuteCommand(command, args);

				if( new_req != NULL ) {
					// Execute the new request
					executeCommand(new_req);
					if(new_req->m_has_result) {
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
bool VMGahp::verifyCommand(const std::vector<std::string>& args) {
	istring_view cmd(args[0].c_str(), args[0].size());
	if(cmd == istring_view(VMGAHP_COMMAND_VM_START)) {
		// Expecting: VMGAHP_COMMAND_VM_START <req_id> <type>
		return verify_number_args(args.size(), 3) &&
			verify_request_id(args[1].c_str()) &&
			verify_vm_type(args[2].c_str());

	} else if(cmd == istring_view(VMGAHP_COMMAND_VM_STOP) ||
		cmd == istring_view(VMGAHP_COMMAND_VM_SUSPEND) ||
		cmd == istring_view(VMGAHP_COMMAND_VM_SOFT_SUSPEND) ||
		cmd == istring_view(VMGAHP_COMMAND_VM_RESUME) ||
		cmd == istring_view(VMGAHP_COMMAND_VM_CHECKPOINT) ||
		cmd == istring_view(VMGAHP_COMMAND_VM_STATUS) ||
		cmd == istring_view(VMGAHP_COMMAND_VM_GETPID)) {
		// Expecting: VMGAHP_COMMAND_VM_STOP <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_SUSPEND <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_SOFT_SUSPEND <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_RESUME <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_CHECKPOINT <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_STATUS <req_id> <vmid>
		// Expecting: VMGAHP_COMMAND_VM_GETPID <req_id> <vmid>

		return verify_number_args(args.size(), 3) &&
			verify_request_id(args[1].c_str()) &&
			verify_vm_id(args[2].c_str());

	} else if(cmd == istring_view(VMGAHP_COMMAND_ASYNC_MODE_ON) ||
		cmd == istring_view(VMGAHP_COMMAND_ASYNC_MODE_OFF) ||
		cmd == istring_view(VMGAHP_COMMAND_QUIT) ||
		cmd == istring_view(VMGAHP_COMMAND_VERSION) ||
		cmd == istring_view(VMGAHP_COMMAND_COMMANDS) ||
		cmd == istring_view(VMGAHP_COMMAND_SUPPORT_VMS) ||
		cmd == istring_view(VMGAHP_COMMAND_RESULTS) ||
		cmd == istring_view(VMGAHP_COMMAND_CLASSAD) ||
		cmd == istring_view(VMGAHP_COMMAND_CLASSAD_END)) {
		//These commands need no-args
		return verify_number_args(args.size(),1);
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
	if (m_pending_req_table.find(req_id) != m_pending_req_table.end()) {
		vmprintf(D_ALWAYS, "Request id(%s) conflicts with "
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
	const char* result[] = {VMGAHP_RESULT_SUCCESS};
	returnOutput(result,1);
}

void
VMGahp::returnOutputError(void)
{
	const char* result[] = {VMGAHP_RESULT_ERROR};
	returnOutput(result,1);
}

VMRequest *
VMGahp::preExecuteCommand(const char* cmd, const std::vector<std::string>& args)
{
	const char *command = args[0].c_str();

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
		executeQuit();
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
		if ( new_req ) {
			returnOutputSuccess();
		} else {
			returnOutputError();
		}
		return new_req;
	}
	return NULL;
}

void
VMGahp::executeCommand(VMRequest *req)
{
	const char *command = req->m_args[0].c_str();

	priv_state priv = set_user_priv();

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

	set_priv(priv);
}

void
VMGahp::executeStart(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_START <req_id> <type>
	const char* vmtype = req->m_args[2].c_str();

	if( m_jobAd == NULL ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_JOBCLASSAD_INFO;
		return;
	}

	std::string vmworkingdir;
	if( m_jobAd->LookupString( "VM_WORKING_DIR", vmworkingdir) != 1 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_JOBCLASSAD_INFO;
		vmprintf(D_ALWAYS, "VM_WORKING_DIR cannot be found in vm classAd\n");
		return;
	}

	std::string job_vmtype;
	if( m_jobAd->LookupString( ATTR_JOB_VM_TYPE, job_vmtype) != 1 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_JOBCLASSAD_INFO;
		vmprintf(D_ALWAYS, "VM_TYPE('%s') cannot be found in vm classAd\n",
							ATTR_JOB_VM_TYPE);
		return;
	}

	if(strcasecmp(vmtype, job_vmtype.c_str()) != 0 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_SUPPORTED_VM_TYPE;
		vmprintf(D_ALWAYS, "Argument is %s but VM_TYPE in job classAD "
						"is %s\n", vmtype, job_vmtype.c_str());
		return;
	}

	if(strcasecmp(vmtype, m_gahp_config->m_vm_type.c_str()) != 0 ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_SUPPORTED_VM_TYPE;
		return;
	}

	VMType *new_vm = NULL;

	// TBD: tstclair this totally needs to be re-written
	if(strcasecmp(vmtype, CONDOR_VM_UNIVERSE_XEN) == 0 ) {
		new_vm = new XenType( vmworkingdir.c_str(), m_jobAd );
		ASSERT(new_vm);
	}else if(strcasecmp(vmtype, CONDOR_VM_UNIVERSE_KVM) == 0) {
	  new_vm = new KVMType(
				vmworkingdir.c_str(), m_jobAd);
		ASSERT(new_vm);
	}else
	{
		// We should not reach here
		vmprintf(D_ALWAYS, "vmtype(%s) is not yet implemented\n", vmtype);
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = VMGAHP_ERR_NO_SUPPORTED_VM_TYPE;
		return;
	}

	new_vm->Config();
	if( new_vm->CreateConfigFile() == false ) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = makeErrorMessage(new_vm->m_result_msg.c_str());
		delete new_vm;
		new_vm = NULL;
		vmprintf(D_FULLDEBUG, "CreateConfigFile fails in executeStart!\n");
		return;
	}

	int result = new_vm->Start();

	if(result == false) {
		req->m_has_result = true;
		req->m_is_success = false;
		req->m_result = makeErrorMessage(new_vm->m_result_msg.c_str());
		delete new_vm;
		new_vm = NULL;
		vmprintf(D_FULLDEBUG, "executeStart fail!\n");
		return;
	} else {
		// Success to create a new VM
		req->m_has_result = true;
		req->m_is_success = true;

		// Result is set to a new vm_id
		formatstr( req->m_result, "%d", new_vm->getVMId() );

		addVM(new_vm);
		vmprintf(D_FULLDEBUG, "executeStart success!\n");
		return;
	}
}

void
VMGahp::executeStop(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_STOP <req_id> <vmid>
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

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
			req->m_result = makeErrorMessage(vm->m_result_msg.c_str());
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
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

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
			req->m_result = makeErrorMessage(vm->m_result_msg.c_str());
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
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

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
			req->m_result = makeErrorMessage(vm->m_result_msg.c_str());
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
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

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
			req->m_result = makeErrorMessage(vm->m_result_msg.c_str());
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
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

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
			req->m_result = makeErrorMessage(vm->m_result_msg.c_str());
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
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

	VMType *vm = findVM(vm_id);

	if(vm == NULL) {
		req->m_has_result = true;
		req->m_is_success = true;
		req->m_result = VMGAHP_STATUS_COMMAND_STATUS;
		req->m_result += "=Stopped";
		return;
	}else {
		int result = vm->Status();

		if(result == false) {
			req->m_has_result = true;
			req->m_is_success = false;
			req->m_result = makeErrorMessage(vm->m_result_msg.c_str());
			return;
		} else {

			req->m_has_result = true;
			req->m_is_success = true;
			req->m_result = vm->m_result_msg;

			// If we have valid status of VM, we stash it.
			vm->setLastStatus(req->m_result.c_str());
			return;
		}
	}
}

void
VMGahp::executeGetpid(VMRequest *req)
{
	// Expecting: VMGAHP_COMMAND_VM_GETPID <req_id> <vmid>
	int vm_id = strtol(req->m_args[2].c_str(), (char **)NULL, 10);

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
		formatstr( req->m_result, "%d", vm->PidOfVM() );
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
	std::string result;
	result += "S";

	int i = 0;
	while(commands_list[i] != NULL) {
		result += " ";
		result += commands_list[i];
		i++;
	}

	write_to_daemoncore_pipe("%s\n", result.c_str());
}

void
VMGahp::executeSupportVMS(void)
{
	std::string result;
	result += "S";

	int i = 0;
	while(support_vms_list[i] != NULL) {
		result += " ";
		result += support_vms_list[i];
		i++;
	}

	vmprintf(D_FULLDEBUG, "Execute commands: %s\n", result.c_str());
	write_to_daemoncore_pipe("%s\n", result.c_str());
}

void
VMGahp::executeResults(void)
{
	write_to_daemoncore_pipe("S %zu\n", m_result_list.size());

	// Print each result line
	printAllReqsWithResult();
	m_new_results_signaled = false;
}

const char*
VMGahp::make_result_line(VMRequest *req)
{
	static std::string res_str;
	res_str = "";

	if(req->m_is_success) {
		// Success
		// Format: <req_id> 0 <result string>
		formatstr( res_str, "%d 0 %s", req->m_reqid,
		           req->m_result.length() ? req->m_result.c_str() : "NULL" );
	}else {
		// Error
		// Format: <req_id> 1 <result string>
		formatstr( res_str, "%d 1 %s", req->m_reqid,
		           req->m_result.length() ? req->m_result.c_str() : "NULL" );
	}
	return res_str.c_str();
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
	if( !m_jobAd ) {
		// Virtual machine is absolutely not created.
		return;
	}

	if( strcasecmp(m_gahp_config->m_vm_type.c_str(),
				CONDOR_VM_UNIVERSE_XEN ) == 0 ) {
		priv_state priv = set_root_priv();
		if( m_jobAd && XenType::checkXenParams(m_gahp_config) ) {
			std::string vmname;
			if( VMType::createVMName(m_jobAd, vmname) ) {
				XenType::killVMFast(vmname.c_str());
				vmprintf( D_FULLDEBUG, "killVMFast is called\n");
			}
		}
		set_priv(priv);
	} else if(strcasecmp(m_gahp_config->m_vm_type.c_str(),
			     CONDOR_VM_UNIVERSE_KVM ) == 0 ) {
		priv_state priv = set_root_priv();
		if( m_jobAd && KVMType::checkXenParams(m_gahp_config) ) {
			std::string vmname;
			if( VMType::createVMName(m_jobAd, vmname) ) {
				KVMType::killVMFast(vmname.c_str());
				vmprintf( D_FULLDEBUG, "killVMFast is called\n");
			}
		}
		set_priv(priv);
	}
}
