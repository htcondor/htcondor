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
#include "condor_daemon_core.h"
#include "gahp_common.h"
#include "user_proc.h"
#include "os_proc.h"
#include "vm_proc.h"

#include "vm_gahp_server.h"
#include "vm_gahp_request.h"
#include "../condor_vm-gahp/vmgahp_error_codes.h"

#define NULLSTRING "NULL"

const char * VMGAHP_REQ_RETURN_TABLE[] = {
	"VMGAHP_REQ_COMMAND_PENDING",
	"VMGAHP_REQ_COMMAND_DONE",
	"VMGAHP_REQ_COMMAND_TIMED_OUT",
	"VMGAHP_REQ_COMMAND_NOT_SUPPORTED",
	"VMGAHP_REQ_VMTYPE_NOT_SUPPORTED",
	"VMGAHP_REQ_COMMAND_ERROR"
	};

VMGahpRequest::VMGahpRequest(VMGahpServer *server)
{
	m_mode = NORMAL;
	m_server = server;

	m_pending_status = REQ_INITIALIZED;
	m_pending_reqid = 0;
	m_pending_result = NULL;
	
	m_timeout = 0;
	m_expire_time = 0;
	m_user_timer_id = -1;
	m_pending_timeout_tid = -1;
}

VMGahpRequest::~VMGahpRequest()
{
	clearPending();
}

void
VMGahpRequest::clearPending()
{
	if( m_pending_reqid && m_server &&
			(m_pending_status != REQ_INITIALIZED)) {
		m_server->cancelPendingRequest(m_pending_reqid);
	}
	m_server = NULL;
	m_mode = NORMAL;

	m_pending_reqid = 0;
	if(m_pending_result) {
		delete m_pending_result;
		m_pending_result = NULL;
	}

	m_pending_status = REQ_INITIALIZED;
	m_timeout = 0;
	m_expire_time = 0;
	if( m_pending_timeout_tid != -1 ) {
		daemonCore->Cancel_Timer(m_pending_timeout_tid);
		m_pending_timeout_tid = -1;
	}

	if( m_user_timer_id != -1 ) {
		daemonCore->Cancel_Timer(m_user_timer_id);
		m_user_timer_id = -1;
	}
}

void
VMGahpRequest::pending_timer_fn()
{
	if( m_user_timer_id != -1 ) {
		daemonCore->Reset_Timer(m_user_timer_id, 0);
		m_user_timer_id = -1;
	}

	m_pending_timeout_tid = -1;
}

void
VMGahpRequest::startPendingTimer()
{
	if( m_pending_timeout_tid != -1 ) {
		daemonCore->Cancel_Timer(m_pending_timeout_tid);
		m_pending_timeout_tid = -1;
	}
	m_expire_time = 0;

	if( m_timeout ) {
		m_pending_timeout_tid = daemonCore->Register_Timer(m_timeout, 
				(TimerHandlercpp)&VMGahpRequest::pending_timer_fn,
				"VMGahpRequest::pending_timer_fn", this);
		m_expire_time = time(NULL) + m_timeout;
	}
}

void
VMGahpRequest::stopPendingTimer()
{
	if( m_pending_timeout_tid != -1 ) {
		daemonCore->Cancel_Timer(m_pending_timeout_tid);
	}
	m_pending_timeout_tid = -1;
	m_expire_time = 0;
}

bool
VMGahpRequest::isPendingTimeout()
{
	if( getPendingStatus() == REQ_REJECTED ) {
		//It means that this req is rejected by server
		return true;
	}

	if( getPendingStatus() == REQ_SUBMITTED ) {
		if( m_expire_time && (time(NULL) > m_expire_time) )
			return true;
	}

	return false;
}

int
VMGahpRequest::resetUserTimer()
{
	int retval = TRUE;

	//Before resetting user_timer, we need to cancel pending_timer
	if( m_pending_timeout_tid != -1 ) {
		daemonCore->Cancel_Timer(m_pending_timeout_tid);
		m_pending_timeout_tid = -1;
	}

	if( m_user_timer_id != -1 ) {
		retval = daemonCore->Reset_Timer(m_user_timer_id, 0);
		m_user_timer_id = -1;
	}

	return retval;
}

void 
VMGahpRequest::detachVMGahpServer()
{
	m_server = NULL;

	setPendingStatus(REQ_REJECTED);

	if( m_pending_timeout_tid != -1 ) {
		daemonCore->Reset_Timer(m_pending_timeout_tid,0);
	}
}

int
VMGahpRequest::vmStart(const char *vm_type, const char *workingdir)
{
	static const char *command = "CONDOR_VM_START";

	if( !vm_type || (getPendingStatus() != REQ_INITIALIZED)) {
		return VMGAHP_REQ_COMMAND_ERROR;
	}

	if(m_server->isSupportedCommand(command) == FALSE) {
		return VMGAHP_REQ_COMMAND_NOT_SUPPORTED;
	}

	if(m_server->isSupportedVMType(vm_type) == FALSE) {
		return VMGAHP_REQ_VMTYPE_NOT_SUPPORTED;
	}

	//Before sending VM_START command, we send some of JobClassAd to vmgahp
	if(m_server->publishVMClassAd(workingdir) == FALSE ) {
		return VMGAHP_REQ_COMMAND_ERROR;
	}

	std::string reqline;
	formatstr(reqline, "%s", vm_type);

	//Now sending a command to vm-gahp
	//Req_id is gonna be set in nowPending function
	if( m_server->nowPending(command, reqline.c_str(), this) == false ) {
		return VMGAHP_REQ_COMMAND_ERROR;
	}

	//If we make it here, command is pending
	m_command = command;
	startPendingTimer();

	// Check whether command is completed
	if( m_mode == BLOCKING ) {
		m_server->getPendingResult(m_pending_reqid, true);
	} else {
		m_server->getPendingResult(m_pending_reqid, false);
	}

	if( m_pending_status == REQ_DONE ) {
		stopPendingTimer();
		return VMGAHP_REQ_COMMAND_DONE;
	}else if ( m_pending_status == REQ_SUBMITTED ) {
		//Check whether timeout expired
		if( isPendingTimeout() ) {
			dprintf(D_ALWAYS,"(%s %s) timed out\n",command,reqline.c_str());
			return VMGAHP_REQ_COMMAND_TIMED_OUT;
		}

		//If we made it here, command is still pending
		return VMGAHP_REQ_COMMAND_PENDING;
	}else {
		// Error
		stopPendingTimer();
		return VMGAHP_REQ_COMMAND_ERROR;
	}
}

int
VMGahpRequest::vmStop(int vm_id)
{
	static const char *command = "CONDOR_VM_STOP";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::vmSuspend(int vm_id)
{
	static const char *command = "CONDOR_VM_SUSPEND";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::vmSoftSuspend(int vm_id)
{
	static const char *command = "CONDOR_VM_SOFT_SUSPEND";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::vmResume(int vm_id)
{
	static const char *command = "CONDOR_VM_RESUME";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::vmCheckpoint(int vm_id)
{
	static const char *command = "CONDOR_VM_CHECKPOINT";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::vmStatus(int vm_id)
{
	static const char *command = "CONDOR_VM_STATUS";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::vmGetPid(int vm_id)
{
	static const char *command = "CONDOR_VM_GETPID";

	return executeBasicCmd(command, vm_id);
}

int
VMGahpRequest::executeBasicCmd(const char *command, int vm_id)
{
	if( getPendingStatus() != REQ_INITIALIZED ) {
		return VMGAHP_REQ_COMMAND_ERROR;
	}

	if(m_server->isSupportedCommand(command) == FALSE) {
		return VMGAHP_REQ_COMMAND_NOT_SUPPORTED;
	}

	std::string reqline;
	formatstr(reqline, "%d", vm_id);

	//Now sending a command to vm-gahp
	//Req_id is gonna be set in nowPending function
	if( m_server->nowPending(command, reqline.c_str(), this) == false ) {
		return VMGAHP_REQ_COMMAND_ERROR;
	}

	//If we make it here, command is pending
	m_command = command;
	startPendingTimer();
	
	// Check whether command is completed
	if( m_mode == BLOCKING ) {
		m_server->getPendingResult(m_pending_reqid, true);
	} else { 
		m_server->getPendingResult(m_pending_reqid, false);
	}

	if( m_pending_status == REQ_DONE ) {
		stopPendingTimer();
		return VMGAHP_REQ_COMMAND_DONE;
	}else if ( m_pending_status == REQ_SUBMITTED ) {
		//Check whether timeout expired
		if( isPendingTimeout() ) {
			dprintf(D_ALWAYS,"(%s %s) timed out\n",command,reqline.c_str());
			return VMGAHP_REQ_COMMAND_TIMED_OUT;
		}

		//If we made it here, command is still pending
		return VMGAHP_REQ_COMMAND_PENDING;
	}else {
		// Error
		stopPendingTimer();
		return VMGAHP_REQ_COMMAND_ERROR;
	}
}

void
VMGahpRequest::setMode(reqmode m) 
{ 
	m_mode = m; 
}

void 
VMGahpRequest::setTimeout(int t) 
{
	m_timeout = t;
}

int 
VMGahpRequest::getTimeout() const 
{
	return m_timeout;
}

void 
VMGahpRequest::setNotificationTimerId(int tid) 
{
	m_user_timer_id = tid;
}

int 
VMGahpRequest::getNotificationTimerId() const 
{
	return m_user_timer_id;
}

VMGahpServer* 
VMGahpRequest::getVMGahpServer() 
{
	return m_server;
}

void 
VMGahpRequest::setReqId(int id) 
{
	m_pending_reqid = id;
} 

int 
VMGahpRequest::getReqId() const 
{
	return m_pending_reqid;
} 

void 
VMGahpRequest::setResult(Gahp_Args *result) 
{
	m_pending_result = result;
}

Gahp_Args*
VMGahpRequest::getResult() {
	return m_pending_result;
}

bool
VMGahpRequest::hasValidResult() {
	if( !m_pending_result || m_pending_result->argc != 3 ) {
		dprintf( D_ALWAYS, "Bad Result of VM Request('%s')\n", m_command.c_str());
		return false;
	}
	return true;
}

bool
VMGahpRequest::checkResult(std::string& errmsg) {
	if( ! hasValidResult() ) {
		errmsg = VMGAHP_ERR_INTERNAL;
		return false;
	}

	int resultno = (int)strtol(m_pending_result->argv[1], (char **)NULL, 10);
	if( resultno != 0 ) {
		dprintf(D_ALWAYS, "Failed to execute command('%s'), "
				"vmgahp error string('%s')\n", 
				m_command.c_str(), m_pending_result->argv[2]);

		if( !strcmp(m_pending_result->argv[2], "NULL") ) {
			errmsg = VMGAHP_ERR_INTERNAL;
			return false;
		}else {
			errmsg = m_pending_result->argv[2];
			return false;
		}
	}
	return true;
}

void 
VMGahpRequest::setPendingStatus(reqstatus status) {
	m_pending_status = status;
} 

reqstatus VMGahpRequest::getPendingStatus() {
	return m_pending_status;
} 
