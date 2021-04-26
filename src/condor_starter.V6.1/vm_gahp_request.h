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


#ifndef CONDOR_VM_GAHP_REQUEST_H
#define CONDOR_VM_GAHP_REQUEST_H

#include "condor_common.h"
#include "condor_daemon_core.h"
#include "gahp_common.h"
#include "condor_arglist.h"
#include "vm_proc.h"
#include "vm_gahp_server.h"

// Values that functions related with gahp request return
static const int VMGAHP_REQ_COMMAND_PENDING = 0;
static const int VMGAHP_REQ_COMMAND_DONE = 1;
static const int VMGAHP_REQ_COMMAND_TIMED_OUT = 2;
static const int VMGAHP_REQ_COMMAND_NOT_SUPPORTED = 3;
static const int VMGAHP_REQ_VMTYPE_NOT_SUPPORTED = 4;
static const int VMGAHP_REQ_COMMAND_ERROR = 5;

extern const char * VMGAHP_REQ_RETURN_TABLE[];

enum reqstatus {
	REQ_INITIALIZED,	//set during initializing
	REQ_SUBMITTED,		//req is with vmgahp
	REQ_CANCELLED,		//req is cancelled
	REQ_REJECTED,		//req is rejected from server
	REQ_RESULT_ERROR,	//req has invalid result
	REQ_DONE			//req has the result
};

class VMGahpServer;
class VMGahpRequest : public Service {
	friend class VMGahpServer;

	public:
		VMGahpRequest(VMGahpServer *server);
		~VMGahpRequest();
	
		enum reqmode {
			NORMAL,
			BLOCKING
		};

		const char* getCommand() { return m_command.c_str(); }
		void setMode(reqmode m);
		void setTimeout(int t);
		int getTimeout() const;

		void setNotificationTimerId(int tid);
		int getNotificationTimerId() const;
		int resetUserTimer();
		bool isPendingTimeout();

		void detachVMGahpServer();
		VMGahpServer* getVMGahpServer();

		int getReqId() const;
		Gahp_Args* getResult();
		bool hasValidResult();
		bool checkResult(std::string& errmsg);

		reqstatus getPendingStatus();
		void clearPending();

		//------------------------------------------------------------
		// VMGahp methods
		//------------------------------------------------------------

		int vmStart(const char *vm_type, const char *workingdir);
		int vmStop(int vm_id);
		int vmSuspend(int vm_id);
		int vmSoftSuspend(int vm_id);
		int vmResume(int vm_id);
		int vmCheckpoint(int vm_id);
		int vmStatus(int vm_id);
		int vmGetPid(int vm_id);
	
		int executeBasicCmd(const char *command, int vm_id);

	private:
		void startPendingTimer();
		void stopPendingTimer();
		void setReqId(int id);
		void setPendingStatus(reqstatus status);
		void setResult(Gahp_Args *result);
		void pending_timer_fn();

		reqmode m_mode;
		std::string m_command;
		VMGahpServer *m_server;

		int m_timeout;
		time_t m_expire_time;
		int m_user_timer_id;
		int m_pending_timeout_tid;

		reqstatus m_pending_status;
		int m_pending_reqid;

		Gahp_Args *m_pending_result;
};

#endif //CONDOR_VM_GAHP_REQUEST_H
