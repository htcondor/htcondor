/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef CONDOR_VM_GAHP_REQUEST_H
#define CONDOR_VM_GAHP_REQUEST_H

#include "condor_common.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
#include "gahp_common.h"
#include "MyString.h"
#include "condor_string.h"
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

		const char* getCommand() { return m_command.Value(); }
		void setMode(reqmode m);
		void setTimeout(int t);
		int getTimeout();

		void setNotificationTimerId(int tid);
		int getNotificationTimerId();
		int resetUserTimer();
		bool isPendingTimeout();

		void detachVMGahpServer();
		VMGahpServer* getVMGahpServer();

		int getReqId();
		Gahp_Args* getResult();
		int checkResult(MyString& errmsg);

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
		int pending_timer_fn();

		reqmode m_mode;
		MyString m_command;
		VMGahpServer *m_server;

		int m_timeout;
		time_t m_expire_time;
		int m_user_timer_id;
		int m_pending_timeout_tid;

		reqstatus m_pending_status;
		MyString m_pending_args;
		int m_pending_reqid;

		Gahp_Args *m_pending_result;
};

#endif //CONDOR_VM_GAHP_REQUEST_H
