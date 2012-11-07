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
#include "condor_classad.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "exit.h"
#include "env.h"
#include "user_proc.h"
#include "os_proc.h"
#include "vm_proc.h"
#include "starter.h"
#include "condor_config.h"
#include "condor_mkstemp.h"
#include "basename.h"
#include "directory.h"
#include "filename_tools.h"
#include "daemon.h"
#include "daemon_types.h"
#include "vm_gahp_request.h"
#include "../condor_vm-gahp/vmgahp_error_codes.h"
#include "vm_univ_utils.h"

extern CStarter *Starter;

#define NULLSTRING "NULL"

VMProc::VMProc(ClassAd *jobAd) : OsProc(jobAd)
{
	m_vmgahp = NULL;
	m_is_cleanuped = false;

	m_vm_id = 0;
	m_vm_pid = 0;

	memset(&m_vm_exited_pinfo, 0, sizeof(m_vm_exited_pinfo));
	memset(&m_vm_alive_pinfo, 0, sizeof(m_vm_alive_pinfo));

	m_vm_checkpoint = false;
	m_is_vacate_ckpt = false;
	m_last_ckpt_result = false;

	m_is_soft_suspended = false;

	m_vm_ckpt_count = 0;

	m_vmstatus_tid = -1;
	m_vmstatus_notify_tid = -1;
	m_status_req = NULL;
	m_status_error_count = 0;
	m_vm_cputime = 0;
	m_vm_utilization = -1.0;

	//Find the interval of sending vm status command to vmgahp server
	m_vmstatus_interval = param_integer( "VM_STATUS_INTERVAL", 
			VM_DEFAULT_STATUS_INTERVAL);
	if( m_vmstatus_interval < VM_MIN_STATUS_INTERVAL ) {
		// vm_status interval must be at least more than 
		// VM_MIN_STATUS_INTERVAL for performance issue
		dprintf(D_ALWAYS,"Even if Condor config file defines %d secs "
				"for vm status interval, vm status interval is set to "
				"%d seconds for performance.\n", 
			   m_vmstatus_interval, VM_MIN_STATUS_INTERVAL);
		m_vmstatus_interval = VM_MIN_STATUS_INTERVAL;	
	}

	m_vmstatus_max_error_cnt = param_integer( "VM_STATUS_MAX_ERROR", 
			VM_STATUS_MAX_ERROR_COUNT);

	m_vmoperation_timeout = param_integer( "VM_GAHP_REQ_TIMEOUT", 
			VM_GAHP_REQ_TIMEOUT );

	m_use_soft_suspend = param_boolean("VM_SOFT_SUSPEND", false); 
}

VMProc::~VMProc()
{
	// destroy the vmgahp server
	cleanup();
}

void
VMProc::cleanup()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::cleanup()\n");

	if( m_is_cleanuped ) {
		return;
	}
	m_is_cleanuped = true;

	if(m_status_req) {
		delete m_status_req;
		m_status_req = NULL;
	}

	if(m_vmgahp) {
		delete m_vmgahp;
		m_vmgahp = NULL;
	}

	if( m_vmstatus_tid != -1 ) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(m_vmstatus_tid);
		}
		m_vmstatus_tid = -1;
	}

	if( m_vmstatus_notify_tid != -1 ) {
		if( daemonCore ) {
			daemonCore->Cancel_Timer(m_vmstatus_notify_tid);
		}
		m_vmstatus_notify_tid = -1;
	}

	m_vm_id = 0;
	m_vm_pid = 0;

	m_vm_type = "";
	m_vmgahp_server = "";

	// set is_suspended to false in os_proc.h 
	is_suspended = false;
	m_is_soft_suspended = false;
	is_checkpointed = false;
	m_is_vacate_ckpt = false;
}

int
VMProc::StartJob()
{
	MyString err_msg;
	dprintf(D_FULLDEBUG,"Inside VMProc::StartJob()\n");

	// set up a FamilyInfo structure to register a family
	// with the ProcD in its call to DaemonCore::Create_Process
	
	FamilyInfo fi;

	// take snapshots at no more than 15 seconds in between, by default
	fi.max_snapshot_interval = param_integer("PID_SNAPSHOT_INTERVAL", 15);

	m_dedicated_account = Starter->jic->getExecuteAccountIsDedicated();

	if (m_dedicated_account) {
		// using login-based family tracking
		fi.login = m_dedicated_account;
			// The following message is documented in the manual as the
			// way to tell whether the dedicated execution account
			// configuration is being used.
		dprintf(D_ALWAYS,
		        "Tracking process family by login \"%s\"\n",
		        fi.login);
	}

	// Find vmgahp server location
	char* vmgahpfile = param( "VM_GAHP_SERVER" );
	if( !vmgahpfile || (access(vmgahpfile,X_OK) < 0) ) {
		// make certain the vmgahp server program is executable
		dprintf(D_ALWAYS, "vmgahp server cannot be found/executed\n");
		reportErrorToStartd();
		if (vmgahpfile) { free(vmgahpfile); vmgahpfile = NULL; }
		return false;
	}
	m_vmgahp_server = vmgahpfile;
	free(vmgahpfile);

	if( !JobAd ) {
		dprintf(D_ALWAYS, "No JobAd in VMProc::StartJob()!\n");
		return false;
	}

	// // // // // // 
	// Get IWD 
	// // // // // // 
	//const char* job_iwd = Starter->jic->jobRemoteIWD();
	//dprintf( D_ALWAYS, "IWD: %s\n", job_iwd );

	// // // // // // 
	// Environment 
	// // // // // // 
	// Now, instantiate an Env object so we can manipulate the
	// environment as needed.
	Env job_env;

	char *env_str = param( "STARTER_JOB_ENVIRONMENT" );

	MyString env_errors;
	if( ! job_env.MergeFromV1RawOrV2Quoted(env_str,&env_errors) ) {
		dprintf( D_ALWAYS, "Aborting VMProc::StartJob: "
				"%s\nThe full value for STARTER_JOB_ENVIRONMENT: "
				"%s\n",env_errors.Value(),env_str);
		if( env_str ) {
			free(env_str);
		}
		return false;
	}
	if( env_str ) {
		free(env_str);
	}

	// Now, let the starter publish any env vars it wants to into
	// the mainjob's env...
	Starter->PublishToEnv( &job_env );

	// // // // // // 
	// Misc + Exec
	// // // // // // 

	// compute job's renice value by evaluating the machine's
	// JOB_RENICE_INCREMENT in the context of the job ad...
	int nice_inc = 0;
    char* ptmp = param( "JOB_RENICE_INCREMENT" );
	if( ptmp ) {
			// insert renice expr into our copy of the job ad
		MyString reniceAttr = "Renice = ";
		reniceAttr += ptmp;
		if( !JobAd->Insert( reniceAttr.Value() ) ) {
			dprintf( D_ALWAYS, "ERROR: failed to insert JOB_RENICE_INCREMENT "
				"into job ad, Aborting OsProc::StartJob...\n" );
			free( ptmp );
			return 0;
		}
			// evaluate
		if( JobAd->EvalInteger( "Renice", NULL, nice_inc ) ) {
			dprintf( D_ALWAYS, "Renice expr \"%s\" evaluated to %d\n",
					 ptmp, nice_inc );
		} else {
			dprintf( D_ALWAYS, "WARNING: job renice expr (\"%s\") doesn't "
					 "eval to int!  Using default of 10...\n", ptmp );
			nice_inc = 10;
		}

			// enforce valid ranges for nice_inc
		if( nice_inc < 0 ) {
			dprintf( D_FULLDEBUG, "WARNING: job renice value (%d) is too "
					 "low: adjusted to 0\n", nice_inc );
			nice_inc = 0;
		}
		else if( nice_inc > 19 ) {
			dprintf( D_FULLDEBUG, "WARNING: job renice value (%d) is too "
					 "high: adjusted to 19\n", nice_inc );
			nice_inc = 19;
		}

		ASSERT( ptmp );
		free( ptmp );
		ptmp = NULL;
	} else {
			// if JOB_RENICE_INCREMENT is undefined, default to 10
		nice_inc = 10;
	}

	// Get job name
	MyString vm_job_name;
	if( JobAd->LookupString( ATTR_JOB_CMD, vm_job_name) != 1 ) {
		err_msg.formatstr("%s cannot be found in job classAd.", ATTR_JOB_CMD);
		dprintf(D_ALWAYS, "%s\n", err_msg.Value());
		Starter->jic->notifyStarterError( err_msg.Value(), true, 
				CONDOR_HOLD_CODE_FailedToCreateProcess, 0);
		return false;
	}
	m_job_name = vm_job_name;

	// vm_type should be from ClassAd
	MyString vm_type_name;
	if( JobAd->LookupString( ATTR_JOB_VM_TYPE, vm_type_name) != 1 ) {
		err_msg.formatstr("%s cannot be found in job classAd.", ATTR_JOB_VM_TYPE);
		dprintf(D_ALWAYS, "%s\n", err_msg.Value());
		Starter->jic->notifyStarterError( err_msg.Value(), true, 
				CONDOR_HOLD_CODE_FailedToCreateProcess, 0);
		return false;
	}
	vm_type_name.lower_case();
	m_vm_type = vm_type_name;

	// get vm checkpoint flag from ClassAd
	m_vm_checkpoint = false;
	JobAd->LookupBool(ATTR_JOB_VM_CHECKPOINT, m_vm_checkpoint);

	// If there exists MAC or IP address for a checkpointed VM,
	// we use them as initial values.
	MyString string_value;
	if( JobAd->LookupString(ATTR_VM_CKPT_MAC, string_value) == 1 ) {
		m_vm_mac = string_value;
	}
	/*
	string_value = "";
	if( JobAd->LookupString(ATTR_VM_CKPT_IP, string_value) == 1 ) {
		m_vm_ip = string_value;
	}
	*/

	ClassAd recovery_ad = *JobAd;
	MyString vm_name;
	if ( strcasecmp( m_vm_type.Value(), CONDOR_VM_UNIVERSE_KVM ) == MATCH ||
		 strcasecmp( m_vm_type.Value(), CONDOR_VM_UNIVERSE_XEN ) == MATCH ) {
		ASSERT( create_name_for_VM( JobAd, vm_name ) );
	} else {
		vm_name = Starter->GetWorkingDir();
	}
	recovery_ad.Assign( "JobVMId", vm_name.Value() );
	Starter->WriteRecoveryFile( &recovery_ad );

	// //
	// Now everything is ready to start a vmgahp server 
	// //
	dprintf( D_ALWAYS, "About to start new VM\n");
	Starter->jic->notifyJobPreSpawn();

	//create vmgahp server
	m_vmgahp = new VMGahpServer(m_vmgahp_server.Value(),
	                            m_vm_type.Value(),
	                            JobAd);
	ASSERT(m_vmgahp);

	m_vmgahp->start_err_msg = "";
	if( m_vmgahp->startUp(&job_env, Starter->GetWorkingDir(), nice_inc, 
				&fi) == false ) {
		JobPid = -1;
		err_msg = "Failed to start vm-gahp server";
		dprintf( D_ALWAYS, "%s\n", err_msg.Value());
		if( m_vmgahp->start_err_msg.Length() > 0 ) {
			m_vmgahp->start_err_msg.chomp();
			err_msg = m_vmgahp->start_err_msg;
		}
		reportErrorToStartd();
		Starter->jic->notifyStarterError( err_msg.Value(), true, 0, 0);

		delete m_vmgahp;
		m_vmgahp = NULL; 
		return false;
	}

	// Set JobPid and num_pids in user_proc.h and os_proc.h
	JobPid = m_vmgahp->getVMGahpServerPid();
	num_pids++;

	VMGahpRequest *new_req = new VMGahpRequest(m_vmgahp);
	ASSERT(new_req);
	new_req->setMode(VMGahpRequest::BLOCKING);

	// When we call vmStart, vmStart may create an ISO file.
	// So we need to give some more time to vmgahp.
	new_req->setTimeout(m_vmoperation_timeout + 120);

	int p_result;
	p_result = new_req->vmStart(m_vm_type.Value(), Starter->GetWorkingDir());

	// Because req is blocking mode, result should be VMGAHP_REQ_COMMAND_DONE
	if(p_result != VMGAHP_REQ_COMMAND_DONE) {
		err_msg = "Failed to create a new VM";
		dprintf(D_ALWAYS, "%s\n", err_msg.Value());
		m_vmgahp->printSystemErrorMsg();

		reportErrorToStartd();
		Starter->jic->notifyStarterError( err_msg.Value(), true, 0, 0);

		delete new_req;
		delete m_vmgahp;
		m_vmgahp = NULL;
		// To make sure that vmgahp server exits
		//daemonCore->Send_Signal(JobPid, SIGKILL);
		daemonCore->Kill_Family(JobPid);
		return false;
	}

	if( new_req->checkResult(err_msg) == false ) {
		dprintf(D_ALWAYS, "%s\n", err_msg.Value());
		m_vmgahp->printSystemErrorMsg();

		if( !strcmp(err_msg.Value(), VMGAHP_ERR_INTERNAL) ||
			!strcmp(err_msg.Value(), VMGAHP_ERR_CRITICAL) )  {
			reportErrorToStartd();
		}

		Starter->jic->notifyStarterError( err_msg.Value(), true, 
				CONDOR_HOLD_CODE_FailedToCreateProcess, 0);

		delete new_req;
		delete m_vmgahp;
		m_vmgahp = NULL;
		// To make sure that vmgahp server exits
		//daemonCore->Send_Signal(JobPid, SIGKILL);
		daemonCore->Kill_Family(JobPid);
		return false;
	}

	Gahp_Args *result_args;
	result_args = new_req->getResult();

	// Set virtual machine id
	m_vm_id = (int)strtol(result_args->argv[2], (char **)NULL, 10);
	if( m_vm_id <= 0 ) {
		m_vm_id = 0;
		dprintf(D_ALWAYS, "Received invalid virtual machine id from vm-gahp\n");
		m_vmgahp->printSystemErrorMsg();

		reportErrorToStartd();
		Starter->jic->notifyStarterError( "VMGahp internal error", true, 0, 0);

		delete new_req;
		delete m_vmgahp;
		m_vmgahp = NULL;
		// To make sure that vmgahp server exits
		//daemonCore->Send_Signal(JobPid, SIGKILL);
		daemonCore->Kill_Family(JobPid);
		return false;
	}
	delete new_req;
	new_req = NULL;

	m_vmgahp->setVMid(m_vm_id);

	// We give considerable time(30 secs) to bring 
	// the just created VM into a fully compliant state
	sleep(30);

	// Initialize data structures for VM process
	m_vm_pid = 0;
	memset(&m_vm_exited_pinfo, 0, sizeof(m_vm_exited_pinfo));
	memset(&m_vm_alive_pinfo, 0, sizeof(m_vm_alive_pinfo));

	// Find the actual process dealing with VM
	// Most virtual machine programs except Xen creates a process 
	// that actually deals with a created VM. The process may be 
	// directly created by a virtual machine program and 
	// the parent pid of the process may be 1.
	// So our default Procd daemon is unable to include this process.
	// Here, we don't need to create a new Procd daemon for this process.
	// In VMware, this process is a single process that has no childs. 
	// So we will just use simple ProcAPI to get usage of this process.
	// PIDofVM will return the pid of such process. 
	// If there is no such process like in Xen, PIDofVM will return 0.
	int vm_pid = PIDofVM();
	setVMPID(vm_pid);

	m_vmstatus_tid = daemonCore->Register_Timer(m_vmstatus_interval, 
			m_vmstatus_interval, 
			(TimerHandlercpp)&VMProc::CheckStatus, 
			"VMProc::CheckStatus", this);

	// Set job_start_time in user_proc.h
	job_start_time.getTime();
	dprintf( D_ALWAYS, "StartJob for VM succeeded\n");
	return true;
}

bool 
VMProc::process_vm_status_result(Gahp_Args *result_args)
{
	// status result
	// argv[1] : should be 0, it means success.
	// From argv[2] : representing some info about VM
	
	if( !result_args || ( result_args->argc < 3) ) {
		dprintf(D_ALWAYS, "Bad Result for VM status\n");
		vm_status_error();
		return true;
	}

	int tmp_argv = (int)strtol(result_args->argv[1], (char **)NULL, 10);
	if( tmp_argv != 0 || !strcasecmp(result_args->argv[2], NULLSTRING)) {
		dprintf(D_ALWAYS, "Received VM status, result(%s,%s)\n", 
				result_args->argv[1], result_args->argv[2]);
		vm_status_error();
		return true;
	}

	// We got valid info about VM
	MyString vm_status;
	float cpu_time = 0;
	int vm_pid = 0;
	MyString vm_ip;
	MyString vm_mac;

	MyString tmp_name;
	MyString tmp_value;
	MyString one_arg;
	int i = 2;
	m_vm_utilization = 0.0;
	for( ; i < result_args->argc; i++ ) {
		one_arg = result_args->argv[i];
		one_arg.trim();

		if(one_arg.IsEmpty()) {
			continue;
		}

		parse_param_string(one_arg.Value(), tmp_name, tmp_value, true);
		if( tmp_name.IsEmpty() || tmp_value.IsEmpty() ) {
			continue;
		}

		if(!strcasecmp(tmp_name.Value(), VMGAHP_STATUS_COMMAND_STATUS)) {
			vm_status = tmp_value;
		}else if( !strcasecmp(tmp_name.Value(), VMGAHP_STATUS_COMMAND_CPUTIME) ) {
			cpu_time = (float)strtod(tmp_value.Value(), (char **)NULL);
			if( cpu_time <= 0 ) {
				cpu_time = 0;
			}
		}else if( !strcasecmp(tmp_name.Value(), VMGAHP_STATUS_COMMAND_PID) ) {
			vm_pid = (int)strtol(tmp_value.Value(), (char **)NULL, 10);
			if( vm_pid <= 0 ) {
				vm_pid = 0;
			}
		}else if( !strcasecmp(tmp_name.Value(), VMGAHP_STATUS_COMMAND_MAC) ) {
			vm_mac = tmp_value;
		}else if( !strcasecmp(tmp_name.Value(), VMGAHP_STATUS_COMMAND_IP) ) {
			vm_ip = tmp_value;
		}else if ( !strcasecmp(tmp_name.Value(),VMGAHP_STATUS_COMMAND_CPUUTILIZATION) ) {
		      /* This is here for vm's which are spun via libvirt*/
		      m_vm_utilization = (float)strtod(tmp_value.Value(), (char **)NULL);
		}
	}

	if( vm_status.IsEmpty() ) {
		// We don't receive status of VM
		dprintf(D_ALWAYS, "No VM status in result\n");
		vm_status_error();
		return true;
	}

	if( vm_mac.IsEmpty() == false ) {
		setVMMAC(vm_mac.Value());
	}
	if( vm_ip.IsEmpty() == false ) {
		setVMIP(vm_ip.Value());
	}

	int old_status_error_count = m_status_error_count;
	// Reset status error count to 0
	m_status_error_count = 0;

	if( !strcasecmp(vm_status.Value(),"Stopped") ) {
		dprintf(D_ALWAYS, "Virtual machine is stopped\n");

		is_suspended = false;
		m_is_soft_suspended = false;
		is_checkpointed = false;

		// VM finished.
		setVMPID(0);

		// destroy the vmgahp server
		cleanup();
		return false;
	}else {
		dprintf(D_FULLDEBUG, "Virtual machine status is %s, utilization is %f\n", vm_status.Value(), m_vm_utilization );
		if( !strcasecmp(vm_status.Value(), "Running") ) {
			is_suspended = false;
			m_is_soft_suspended = false;
			is_checkpointed = false;

			if( cpu_time > 0 ) {
				// update statistics for Xen
				m_vm_cputime = cpu_time;
			}
			if( vm_pid > 0 ) {
				setVMPID(vm_pid);
			}

			// Update usage of process for a VM
			updateUsageOfVM();
		}else if( !strcasecmp(vm_status.Value(), "Suspended") ) {
			if( !is_checkpointed ) {
				is_suspended = true;
			}
			m_is_soft_suspended = false;

			// VM is suspended
			setVMPID(0);
		}else if( !strcasecmp(vm_status.Value(), "SoftSuspended") ) {
			is_suspended = true;
			m_is_soft_suspended = true;
			is_checkpointed = false;
		}else {
			dprintf(D_ALWAYS, "Unknown VM status: %s\n", vm_status.Value());

			// Restore status error count 
			m_status_error_count = old_status_error_count;
			vm_status_error();
		}
	}
	return true;
}

void
VMProc::notify_status_fn()
{
	// this function will be called from timer function
	bool has_error = false;

	if( !m_status_req || !m_vm_id ) {
		return;
	}

	dprintf( D_FULLDEBUG, "VM status notify function is called\n"); 
	
	//reset timer id for this function
	m_status_req->setNotificationTimerId(-1);
	m_vmstatus_notify_tid = -1;

	// check the status of VM status command
	reqstatus v_status = m_status_req->getPendingStatus();

	if(v_status == REQ_DONE) {
		Gahp_Args *result_args;
		result_args = m_status_req->getResult();

		// If vm_status is 'stopped', cleanup function will be called 
		//  inside this function
		process_vm_status_result(result_args);
		if(m_status_req) {
			delete m_status_req;
			m_status_req = NULL;
		}
		return;
	}else {
		// We didn't get result from vmgahp server in timeout.
		dprintf(D_ALWAYS, "Failed to receive VM status\n");
		has_error = true;
	}

	if(m_status_req) {
		delete m_status_req;
		m_status_req = NULL;
	}

	if( has_error ) {
		vm_status_error();
	}

	return;
}

void
VMProc::CheckStatus()
{
	if( !m_vm_id || !m_vmgahp ) {
		return;
	}

	if( m_status_req != NULL ) {
		delete m_status_req;
		m_status_req = NULL;
		m_vmstatus_notify_tid = -1;
	}

	m_status_req = new VMGahpRequest(m_vmgahp);
	ASSERT(m_status_req);
	m_status_req->setMode(VMGahpRequest::NORMAL);
	m_status_req->setTimeout(m_vmstatus_interval - 3);

	m_vmstatus_notify_tid = daemonCore->Register_Timer( m_vmstatus_interval - 1,
			(TimerHandlercpp)&VMProc::notify_status_fn,
			"VMProc::notify_status_fn", this);

	if( m_vmstatus_notify_tid == -1 ) {
		dprintf( D_ALWAYS, "Failed to regiseter timer for vm status Timeout\n");
		delete m_status_req;
		m_status_req = NULL;
		return;
	}

	m_status_req->setNotificationTimerId(m_vmstatus_notify_tid);

	int p_result;
	p_result = m_status_req->vmStatus(m_vm_id);

	if( p_result == VMGAHP_REQ_COMMAND_PENDING ) {
		return;
	}else if( p_result == VMGAHP_REQ_COMMAND_DONE) {
		m_status_req->setNotificationTimerId(-1);
		if( m_vmstatus_notify_tid != -1 ) {
			daemonCore->Cancel_Timer(m_vmstatus_notify_tid);
			m_vmstatus_notify_tid = -1;
		}

		Gahp_Args *result_args;
		result_args = m_status_req->getResult();

		// If vm_status is 'stopped', cleanup function will be called 
		//  inside this function
		process_vm_status_result(result_args);
		if(m_status_req) {
			delete m_status_req;
			m_status_req = NULL;
		}
		return;
	}else {
		m_status_req->setNotificationTimerId(-1);
		if( m_vmstatus_notify_tid != -1 ) {
			daemonCore->Reset_Timer(m_vmstatus_notify_tid, 0);
		}else {
			delete m_status_req;
			m_status_req = NULL;
		}
		return;
	}
	return;
}

bool
VMProc::JobReaper(int pid, int status)
{
	dprintf(D_FULLDEBUG,"Inside VMProc::JobReaper()\n");

	// Ok, vm_gahp server exited
	// Make sure that all allocated structures are freed
	cleanup();

	// This will reset num_pids for us, too.
	return OsProc::JobReaper( pid, status );
}

bool
VMProc::JobExit(void)
{
	dprintf(D_FULLDEBUG,"Inside VMProc::JobExit()\n");

	// Do nothing here, just call OsProc::JobExit
	return OsProc::JobExit();
}

void
VMProc::Suspend()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::Suspend()\n");

	if( !m_vm_id || !m_vmgahp ) {
		return;
	}

	if( is_suspended ) {
		// VM is already suspended
		if( is_checkpointed ) {
			// VM has been hard suspended
			m_is_soft_suspended = false;
			is_checkpointed = false;
		}
		return;
	}

	VMGahpRequest *new_req = new VMGahpRequest(m_vmgahp);
	ASSERT(new_req);

	new_req->setMode(VMGahpRequest::BLOCKING);
	new_req->setTimeout(m_vmoperation_timeout);

	int p_result;
	if( m_use_soft_suspend ) {
		dprintf(D_FULLDEBUG,"Calling Soft Suspend in VMProc::Suspend()\n");
		p_result = new_req->vmSoftSuspend(m_vm_id);
	}else {
		dprintf(D_FULLDEBUG,"Calling Hard Suspend in VMProc::Suspend()\n");
		p_result = new_req->vmSuspend(m_vm_id);
	}

	// Because req is blocking mode, result should be VMGAHP_REQ_COMMAND_DONE
	if(p_result != VMGAHP_REQ_COMMAND_DONE) {
		dprintf(D_ALWAYS, "Failed to suspend the VM\n");
		m_vmgahp->printSystemErrorMsg();
		delete new_req;
		internalVMGahpError();
		return;
	}

	MyString gahpmsg;
	if( new_req->checkResult(gahpmsg) == false ) {
		dprintf(D_ALWAYS, "Failed to suspend the VM(%s)", gahpmsg.Value());
		m_vmgahp->printSystemErrorMsg();
		delete new_req;

		if( strcmp(gahpmsg.Value(), VMGAHP_ERR_VM_NO_SUPPORT_SUSPEND) ) {
			// It is possible that a VM job is just finished.
			// So we reset the timer for status 
			if( m_vmstatus_tid != -1 ) {
				daemonCore->Reset_Timer(m_vmstatus_tid, 0, m_vmstatus_interval);
			}
		}
		return;
	}
	delete new_req;
	new_req = NULL;

	if( !m_use_soft_suspend ) {
#if defined(LINUX)
		// To avoid lazy-write behavior to disk
		sync();
#endif

		// After hard suspending, there is no VM process.
		setVMPID(0);
	}

	// set is_suspended to true in os_proc.h 
	is_suspended = true;
	m_is_soft_suspended = m_use_soft_suspend;
	is_checkpointed = false;

	return;
}

void
VMProc::Continue()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::Continue()\n");

	if( !m_vm_id || !m_vmgahp ) {
		return;
	}

	VMGahpRequest *new_req = new VMGahpRequest(m_vmgahp);
	ASSERT(new_req);
	new_req->setMode(VMGahpRequest::BLOCKING);
	new_req->setTimeout(m_vmoperation_timeout);

	int p_result;
	p_result = new_req->vmResume(m_vm_id);

	// Because req is blocking mode, result should be VMGAHP_REQ_COMMAND_DONE
	if(p_result != VMGAHP_REQ_COMMAND_DONE) {
		dprintf(D_ALWAYS, "Failed to resume the VM\n");
		m_vmgahp->printSystemErrorMsg();
		delete new_req;
		internalVMGahpError();
		return;
	}

	MyString gahpmsg;
	if( new_req->checkResult(gahpmsg) == false ) {
		dprintf(D_ALWAYS, "Failed to resume the VM(%s)", gahpmsg.Value());
		m_vmgahp->printSystemErrorMsg();
		delete new_req;
		internalVMGahpError();
		return;
	}
	delete new_req;
	new_req = NULL;

	// When we resume a suspended VM, 
	// PID of process for the VM may change
	// So, we may have a new PID of the process.
	int vm_pid = PIDofVM();
	setVMPID(vm_pid);

	// set is_suspended to false in os_proc.h 
	is_suspended = false;
	m_is_soft_suspended = false;
	is_checkpointed = false;
	return;
}

bool
VMProc::ShutdownGraceful()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::ShutdownGraceful()\n");

	if( !m_vmgahp ) {
		return true;
	}

	if( JobPid == -1 ) {
		delete m_vmgahp;
		m_vmgahp = NULL;
		return true;
	}

	bool delete_working_files = true;

	if( m_vm_checkpoint ) {
		// We need to do checkpoint before vacating.
		// The reason we call checkpoint explicitly here
		// is to make sure the file uploading for checkpoint.
		// If file uploading failed, we will not update job classAd 
		// such as the total count of checkpoint and last checkpoint time.
		m_is_vacate_ckpt = true;
		is_checkpointed = false;

		Starter->RemotePeriodicCkpt(1);

		// Check the success of checkpoint and file transfer
		if( is_checkpointed && !m_last_ckpt_result ) {
			// This means the checkpoint succeeded but file transfer failed.
			// We will not delete files in the working directory so that
			// file transfer will be retried
			delete_working_files = false;
			dprintf(D_ALWAYS, "Vacating checkpoint succeeded but "
					"file transfer failed\n");
		}
	}

	// stop the running VM
	StopVM();

	is_suspended = false;
	m_is_soft_suspended = false;
	is_checkpointed = false;
	requested_exit = true;

	// destroy vmgahp server
	if( m_vmgahp->cleanup() == false ) {
		//daemonCore->Send_Signal(JobPid, SIGKILL);
		daemonCore->Kill_Family(JobPid);

		// To make sure that the process dealing with a VM exits,
		killProcessForVM();
	}

	// final cleanup.. 
	cleanup();

	// Because we already performed checkpoint, 
	// we don't need to keep files in the working directory.
	// So file transfer will not be called again.
	if( delete_working_files ) {
		Directory working_dir( Starter->GetWorkingDir(), PRIV_USER );
		working_dir.Remove_Entire_Directory();
	}

	return false;	// return false says shutdown is pending	
}

bool
VMProc::ShutdownFast()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::ShutdownFast()\n");

	if( !m_vmgahp ) {
		return true;
	}

	if( JobPid == -1 ) {
		delete m_vmgahp;
		m_vmgahp = NULL;
		return true;
	}

	// stop the running VM
	StopVM();

	is_suspended = false;
	m_is_soft_suspended = false;
	is_checkpointed = false;
	requested_exit = true;

	// destroy vmgahp server
	if( m_vmgahp->cleanup() == false ) {
		//daemonCore->Send_Signal(JobPid, SIGKILL);
		daemonCore->Kill_Family(JobPid);

		// To make sure that the process dealing with a VM exits,
		killProcessForVM();
	}

	// final cleanup.. 
	cleanup();

	return false;	// shutdown is pending, so return false
}

bool
VMProc::Remove()
{
	return ShutdownFast();
}


bool
VMProc::Hold()
{
	return ShutdownFast();
}

bool
VMProc::StopVM()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::StopVM\n");

	if( !m_vm_id || !m_vmgahp ) {
		return true;
	}

	VMGahpRequest *new_req = new VMGahpRequest(m_vmgahp);
	ASSERT(new_req);
	new_req->setMode(VMGahpRequest::BLOCKING);
	new_req->setTimeout(m_vmoperation_timeout);

	int p_result;
	p_result = new_req->vmStop(m_vm_id);

	// Because req is blocking mode, result should be VMGAHP_REQ_COMMAND_DONE
	if(p_result != VMGAHP_REQ_COMMAND_DONE) {
		dprintf(D_ALWAYS, "Failed to stop VM\n");
		m_vmgahp->printSystemErrorMsg();
		reportErrorToStartd();
		delete new_req;
		return false;
	}

	MyString gahpmsg;
	if( new_req->checkResult(gahpmsg) == false ) {
		dprintf(D_ALWAYS, "Failed to stop the VM(%s)\n", gahpmsg.Value());
		m_vmgahp->printSystemErrorMsg();
		reportErrorToStartd();
		delete new_req;
		return false;
	}
	delete new_req;
	new_req = NULL;

	// VM is successfully stopped.
	setVMPID(0);
	is_suspended = false;
	m_is_soft_suspended = false;
	is_checkpointed = false;

	return true;
}

bool 
VMProc::Ckpt()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::Ckpt()\n");

	if( !m_vm_id || !m_vmgahp ) {
		return false;
	}

	// Check the flag for vm checkpoint in job classAd
	if( !m_vm_checkpoint ) {
		return false;
	}

	if( (strcasecmp(m_vm_type.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH) || (strcasecmp(m_vm_type.Value(), CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		if( !m_is_vacate_ckpt ) {
			// Xen doesn't support periodic checkpoint
			return false;
		}
	}

	VMGahpRequest *new_req = new VMGahpRequest(m_vmgahp);
	ASSERT(new_req);
	new_req->setMode(VMGahpRequest::BLOCKING);
	new_req->setTimeout(m_vmoperation_timeout);

	int p_result;
	p_result = new_req->vmCheckpoint(m_vm_id);

	// Because req is blocking mode, result should be VMGAHP_REQ_COMMAND_DONE
	if(p_result != VMGAHP_REQ_COMMAND_DONE) {
		dprintf(D_ALWAYS, "Failed to checkpoint the VM\n");
		m_vmgahp->printSystemErrorMsg();
		delete new_req;
		internalVMGahpError();
		return false;
	}

	MyString gahpmsg;
	if( new_req->checkResult(gahpmsg) == false ) {
		dprintf(D_ALWAYS, "Failed to checkpoint the VM(%s)", gahpmsg.Value());
		m_vmgahp->printSystemErrorMsg();
		delete new_req;

		if( strcmp(gahpmsg.Value(), VMGAHP_ERR_VM_NO_SUPPORT_CHECKPOINT) ) {
			// It is possible that a VM job is just finished.
			// So we reset the timer for status 
			if( m_vmstatus_tid != -1 ) {
				daemonCore->Reset_Timer(m_vmstatus_tid, 0, m_vmstatus_interval);
			}
		}
		return false;
	}
	delete new_req;
	new_req = NULL;

#if defined(LINUX)
	// To avoid lazy-write behavior to disk
	sync();
#endif

	// After checkpointing, VM process exits.
	setVMPID(0);

	m_is_soft_suspended = false;
	is_checkpointed = true;

	return true;
}

void 
VMProc::CkptDone(bool success)
{
	if( !is_checkpointed ) {
		return;
	}

	dprintf(D_FULLDEBUG,"Inside VMProc::CkptDone()\n");

	m_last_ckpt_result = success;

	if( success ) {
		// File uploading succeeded
		// update checkpoint counter and last ckpt timestamp
		m_vm_ckpt_count++;
		m_vm_last_ckpt_time.getTime();
	}

	if( m_is_vacate_ckpt ) {
		// This is a vacate checkpoint.
		// So we don't need to call continue.
		return;
	}

	if( is_suspended ) {
		// The status before checkpoint was suspended.
		Starter->RemoteSuspend(1);
	}else {
		Starter->RemoteContinue(1);
	}
}

int
VMProc::PIDofVM()
{
	dprintf(D_FULLDEBUG,"Inside VMProc::PIDofVM()\n");

	if( !m_vm_id || !m_vmgahp ) {
		return 0;
	}

	VMGahpRequest *new_req = new VMGahpRequest(m_vmgahp);
	ASSERT(new_req);

	new_req->setMode(VMGahpRequest::BLOCKING);
	new_req->setTimeout(m_vmoperation_timeout);

	int p_result;
	p_result = new_req->vmGetPid(m_vm_id);

	// Because req is blocking mode, result should be VMGAHP_REQ_COMMAND_DONE
	if(p_result != VMGAHP_REQ_COMMAND_DONE) {
		dprintf(D_ALWAYS, "Failed to get PID of VM\n");
		m_vmgahp->printSystemErrorMsg();
		delete new_req;
		internalVMGahpError();
		return 0;
	}

	MyString gahpmsg;
	if( new_req->checkResult(gahpmsg) == false ) {
		dprintf(D_ALWAYS, "Failed to get PID of VM(%s)", gahpmsg.Value());
		m_vmgahp->printSystemErrorMsg();
		delete new_req;
		return 0;
	}

	Gahp_Args *result_args;
	result_args = new_req->getResult();

	// Get PID of VM
	int return_pid = (int)strtol(result_args->argv[2], (char **)NULL, 10);
	if( return_pid <= 0 ) {
		return_pid = 0;
	}
	delete new_req;
	new_req = NULL;

	return return_pid;
}

bool
VMProc::PublishUpdateAd( ClassAd* ad )
{
	dprintf( D_FULLDEBUG, "Inside VMProc::PublishUpdateAd()\n" );

	std::string memory_usage;
	if (param(memory_usage, "MEMORY_USAGE_METRIC_VM", ATTR_VM_MEMORY)) {
		ad->AssignExpr(ATTR_MEMORY_USAGE, memory_usage.c_str());
	}

	MyString buf;
	if( (strcasecmp(m_vm_type.Value(), CONDOR_VM_UNIVERSE_XEN) == MATCH) || (strcasecmp(m_vm_type.Value(), CONDOR_VM_UNIVERSE_KVM) == MATCH) ) {
		float sys_time = m_vm_cputime;
		float user_time = 0.0;

		// Publish it into the ad.
		ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, sys_time );
		ad->Assign(ATTR_JOB_REMOTE_USER_CPU, user_time );
		ad->Assign(ATTR_IMAGE_SIZE, (int)0);
		ad->Assign(ATTR_RESIDENT_SET_SIZE, (int)0);
		ad->Assign(ATTR_JOB_VM_CPU_UTILIZATION, m_vm_utilization);
	}else {
		// Update usage of process for VM
		long sys_time = 0;
		long user_time = 0;
		unsigned long max_image = 0;
        unsigned long rss = 0;
		unsigned long pss = 0;
		bool pss_available = false;

		getUsageOfVM(sys_time, user_time, max_image, rss, pss, pss_available);
		
		// Added to update CPU Usage of VM in ESX
		if ( long(m_vm_cputime) > user_time ) {
			user_time = long(m_vm_cputime);
		}

		// Publish it into the ad.
		ad->Assign(ATTR_JOB_REMOTE_SYS_CPU, float(sys_time));
		ad->Assign(ATTR_JOB_REMOTE_USER_CPU, float(user_time));

		buf.formatstr("%s=%lu", ATTR_IMAGE_SIZE, max_image );
		ad->InsertOrUpdate( buf.Value());
		buf.formatstr("%s=%lu", ATTR_RESIDENT_SET_SIZE, rss );
		ad->InsertOrUpdate( buf.Value());
		if( pss_available ) {
			ad->Assign(ATTR_PROPORTIONAL_SET_SIZE,pss);
		}
	}

	if( m_vm_checkpoint ) {
		if( m_vm_mac.IsEmpty() == false ) {
			// Update MAC addresss of VM
			ad->Assign(ATTR_VM_CKPT_MAC, m_vm_mac);
		}
		if( m_vm_ip.IsEmpty() == false ) {
			// Update IP addresss of VM
			ad->Assign(ATTR_VM_CKPT_IP, m_vm_ip);
		}
	}
			
	// Now, call our parent class's version
	return OsProc::PublishUpdateAd(ad);
}

void 
VMProc::internalVMGahpError()
{
	// Reports vmgahp error to local startd
	// thus local startd will disable vm universe
	if( reportErrorToStartd() == false ) {
		dprintf(D_ALWAYS,"ERROR: Failed to report a VMGahp error to local startd\n");
	}

	daemonCore->Send_Signal(daemonCore->getpid(), DC_SIGHARDKILL);
}

MSC_DISABLE_WARNING(6262) // function uses 60844 bytes of stack.
bool 
VMProc::reportErrorToStartd()
{
	Daemon startd(DT_STARTD, NULL);

	if( !startd.locate() ) {
		dprintf(D_ALWAYS,"ERROR: %s\n", startd.error());
		return false;
	}

	char* addr = startd.addr();
	if( !addr ) {
		dprintf(D_ALWAYS,"Can't find the address of local startd\n");
		return false;
	}

	// Using udp packet
	SafeSock ssock;

	ssock.timeout( 5 ); // 5 seconds timeout
	ssock.encode();

	if( !ssock.connect(addr) ) {
		dprintf( D_ALWAYS, "Failed to connect to local startd(%s)\n", addr);
		return false;
	}

	int cmd = VM_UNIV_GAHP_ERROR;
	if( !startd.startCommand(cmd, &ssock) ) {
		dprintf( D_ALWAYS, "Failed to send UDP command(%s) to local startd %s\n",
			  			 	getCommandString(cmd), addr);
		return false;
	}

	// Send pid of this starter
	MyString s_pid;
	s_pid += (int)daemonCore->getpid();

	char *buffer = strdup(s_pid.Value());
	ASSERT(buffer);

	ssock.code(buffer);

	if( !ssock.end_of_message() ) {
		dprintf( D_FULLDEBUG, "Failed to send EOM to local startd %s\n", addr);
		free(buffer);
		return false;
	}
	free(buffer);

	sleep(1);
	return true;
}

bool 
VMProc::reportVMInfoToStartd(int cmd, const char *value)
{
	Daemon startd(DT_STARTD, NULL);

	if( !startd.locate() ) {
		dprintf(D_ALWAYS,"ERROR: %s\n", startd.error());
		return false;
	}

	char* addr = startd.addr();
	if( !addr ) {
		dprintf(D_ALWAYS,"Can't find the address of local startd\n");
		return false;
	}

	// Using udp packet
	SafeSock ssock;

	ssock.timeout( 5 ); // 5 seconds timeout
	ssock.encode();

	if( !ssock.connect(addr) ) {
		dprintf( D_ALWAYS, "Failed to connect to local startd(%s)\n", addr);
		return false;
	}

	if( !startd.startCommand(cmd, &ssock) ) {
		dprintf( D_ALWAYS, "Failed to send UDP command(%s) "
					"to local startd %s\n", getCommandString(cmd), addr);
		return false;
	}

	// Send the pid of this starter
	MyString s_pid;
	s_pid += (int)daemonCore->getpid();

	char *starter_pid = strdup(s_pid.Value());
	ASSERT(starter_pid);
	ssock.code(starter_pid);

	// Send vm info 
	char *vm_value = strdup(value);
	ASSERT(vm_value);
	ssock.code(vm_value);

	if( !ssock.end_of_message() ) {
		dprintf( D_FULLDEBUG, "Failed to send EOM to local startd %s\n", addr);
		free(starter_pid);
		free(vm_value);
		return false;
	}
	free(starter_pid);
	free(vm_value);

	sleep(1);
	return true;
}
MSC_RESTORE_WARNING(6262) // function uses 60844 bytes of stack.

bool 
VMProc::vm_univ_detect()
{
	return true; 
}

void
VMProc::setVMPID(int vm_pid)
{
	if( m_vm_pid == vm_pid ) {
		// PID doesn't change
		return;
	}

	dprintf(D_FULLDEBUG,"PID for VM is changed from [%d] to [%d]\n", 
			m_vm_pid, vm_pid);

	//PID changes
	m_vm_pid = vm_pid;
	
	// Add the old usage to m_vm_exited_pinfo
	m_vm_exited_pinfo.sys_time += m_vm_alive_pinfo.sys_time;
	m_vm_exited_pinfo.user_time += m_vm_alive_pinfo.user_time;
	if( m_vm_alive_pinfo.rssize > m_vm_exited_pinfo.rssize ) {
		m_vm_exited_pinfo.rssize = m_vm_alive_pinfo.rssize;
	}
	if( m_vm_alive_pinfo.imgsize > m_vm_exited_pinfo.imgsize ) {
		m_vm_exited_pinfo.imgsize = m_vm_alive_pinfo.imgsize;
	}

	// Reset usage of the current process for VM
	memset(&m_vm_alive_pinfo, 0, sizeof(m_vm_alive_pinfo));

	// Get initial usage of the process	
	updateUsageOfVM();

	MyString pid_string;
	pid_string += (int)m_vm_pid;

	// Report this PID to local startd
	reportVMInfoToStartd(VM_UNIV_VMPID, pid_string.Value());
}

void
VMProc::setVMMAC(const char* mac)
{
	if( !strcasecmp(m_vm_mac.Value(), mac) ) {
		// MAC for VM doesn't change
		return;
	}

	dprintf(D_FULLDEBUG,"MAC for VM is changed from [%s] to [%s]\n", 
			m_vm_mac.Value(), mac);

	m_vm_mac = mac;

	// Report this MAC to local startd
	reportVMInfoToStartd(VM_UNIV_GUEST_MAC, m_vm_mac.Value());
}

void
VMProc::setVMIP(const char* ip)
{
	if( !strcasecmp(m_vm_ip.Value(), ip) ) {
		// IP for VM doesn't change
		return;
	}

	dprintf(D_FULLDEBUG,"IP for VM is changed from [%s] to [%s]\n", 
			m_vm_ip.Value(), ip);

	m_vm_ip = ip;

	// Report this IP to local startd
	reportVMInfoToStartd(VM_UNIV_GUEST_IP, m_vm_ip.Value());
}

void
VMProc::updateUsageOfVM()
{
	if( m_vm_pid == 0 ) {
		return;
	}

	int proc_status = PROCAPI_OK;
	struct procInfo pinfo;
	memset(&pinfo, 0, sizeof(pinfo));

	piPTR pi = &pinfo;
	if( ProcAPI::getProcInfo(m_vm_pid, pi, proc_status) == 
			PROCAPI_SUCCESS ) {
		memcpy(&m_vm_alive_pinfo, &pinfo, sizeof(m_vm_alive_pinfo));
		dprintf(D_FULLDEBUG,"Usage of process[%d] for a VM is updated\n", m_vm_pid);
#if defined(WIN32)
		dprintf(D_FULLDEBUG,"sys_time=%lu, user_time=%lu, image_size=%lu\n", 
				pinfo.sys_time, pinfo.user_time, pinfo.rssize);
#else
		dprintf(D_FULLDEBUG,"sys_time=%lu, user_time=%lu, image_size=%lu\n", 
				pinfo.sys_time, pinfo.user_time, pinfo.imgsize);
#endif
	}
}

void
VMProc::getUsageOfVM(long &sys_time, long& user_time, unsigned long &max_image, unsigned long& rss, unsigned long& pss, bool &pss_available)
{
	updateUsageOfVM();
	sys_time = m_vm_exited_pinfo.sys_time + m_vm_alive_pinfo.sys_time;
	user_time = m_vm_exited_pinfo.user_time + m_vm_alive_pinfo.user_time;

	rss = (m_vm_exited_pinfo.rssize > m_vm_alive_pinfo.rssize) ? 
		   m_vm_exited_pinfo.rssize : m_vm_alive_pinfo.rssize;

#if HAVE_PSS
	pss = (m_vm_exited_pinfo.pssize > m_vm_alive_pinfo.pssize) ? 
		   m_vm_exited_pinfo.pssize : m_vm_alive_pinfo.pssize;
	pss_available = m_vm_exited_pinfo.pssize_available || m_vm_alive_pinfo.pssize_available;
#else
	pss_available = false;
	pss = 0;
#endif

#if defined(WIN32)
	max_image = (m_vm_exited_pinfo.rssize > m_vm_alive_pinfo.rssize) ? 
		m_vm_exited_pinfo.rssize : m_vm_alive_pinfo.rssize;
#else
	max_image = (m_vm_exited_pinfo.imgsize > m_vm_alive_pinfo.imgsize) ? 
		m_vm_exited_pinfo.imgsize : m_vm_alive_pinfo.imgsize;
#endif
}

void
VMProc::killProcessForVM()
{
	if( m_vm_pid > 0 ) {
		updateUsageOfVM();
		dprintf(D_FULLDEBUG,"Sending SIGKILL to process for VM\n");
		daemonCore->Send_Signal(m_vm_pid, SIGKILL);
	}
	return;
}

void
VMProc::vm_status_error()
{
	// If there is a error, we will retry up to m_vmstatus_max_error_cnt
	// If we still have a problem after that, we will destroy vmgahp.
	// Unless there is an error, m_status_error_count will be reset to 0

	m_status_error_count++;
	if( m_status_error_count >= m_vmstatus_max_error_cnt ) {
		if( m_vmgahp ) {
			m_vmgahp->printSystemErrorMsg();
		}

		dprintf(D_ALWAYS, "Repeated attempts to receive valid VM status "
				"failed up to %d times\n", m_vmstatus_max_error_cnt);
		// something is wrong
		internalVMGahpError();
	}
	return;
}

