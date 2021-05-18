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


#ifndef VM_GAHP_H
#define VM_GAHP_H

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_attributes.h"
#include "gahp_common.h"
#include "condor_uid.h"
#include "HashTable.h"
#include "simplelist.h"
#include "pbuffer.h"
#include "condor_vm_universe_types.h"
#include "vmgahp_common.h"
#include "vmgahp_config.h"
#include "vm_request.h"
#include "vm_type.h"

#define VMGAHP_COMMAND_ASYNC_MODE_ON	"ASYNC_MODE_ON"
#define VMGAHP_COMMAND_ASYNC_MODE_OFF	"ASYNC_MODE_OFF"
#define VMGAHP_COMMAND_VERSION			"VERSION"
#define VMGAHP_COMMAND_COMMANDS			"COMMANDS"
#define VMGAHP_COMMAND_SUPPORT_VMS		"SUPPORT_VMS"
#define VMGAHP_COMMAND_QUIT				"QUIT"
#define VMGAHP_COMMAND_RESULTS			"RESULTS"
#define VMGAHP_COMMAND_CLASSAD			"CLASSAD"
#define VMGAHP_COMMAND_CLASSAD_END		"CLASSAD_END"

#define VMGAHP_COMMAND_VM_START			"CONDOR_VM_START"
#define VMGAHP_COMMAND_VM_STOP			"CONDOR_VM_STOP"
#define VMGAHP_COMMAND_VM_SUSPEND		"CONDOR_VM_SUSPEND"
#define VMGAHP_COMMAND_VM_SOFT_SUSPEND	"CONDOR_VM_SOFT_SUSPEND"
#define VMGAHP_COMMAND_VM_RESUME		"CONDOR_VM_RESUME"
#define VMGAHP_COMMAND_VM_CHECKPOINT	"CONDOR_VM_CHECKPOINT"
#define VMGAHP_COMMAND_VM_STATUS		"CONDOR_VM_STATUS"
#define VMGAHP_COMMAND_VM_GETPID		"CONDOR_VM_GETPID"

#define VMGAHP_RESULT_SUCCESS		"S"
#define VMGAHP_RESULT_ERROR			"E"
#define VMGAHP_RESULT_FAILURE		"F"


class VMGahp : public Service {
	public:
		VMGahp(VMGahpConfig* config, const char *iwd);
		virtual ~VMGahp();

		void startUp();
		void cleanUp();

		int getNewVMId(void);
		int numOfVM(void); // the number of current VM
		int numOfReq(void); // the total request number 
							// Equal to numOfPendingReq + numOfReqWithResult
		int numOfPendingReq(void); // the number of request without result
		int numOfReqWithResult(void); // the number of request with result

		VMRequest *addNewRequest(const char* raw);

		void removePendingRequest(int req_id);
		void removePendingRequest(VMRequest *req);

		void movePendingReqToResultList(VMRequest *req);

		VMRequest *findPendingRequest(int req_id);
		void printAllReqsWithResult();

		// Interfaces for VM
		void addVM(VMType *vm);
		void removeVM(int vm_id);
		void removeVM(VMType *vm);
		VMType *findVM(int vm_id);

		VMGahpConfig *m_gahp_config; // Gahp config file
		std::string m_workingdir;		 // working directory

	private:
		int waitForCommand(int pipe_end);
		const char* make_result_line(VMRequest *req);

		int quitFast();
		void killAllProcess();

		bool verifyCommand(char **argv, int argc);
		bool verify_request_id(const char *s);
		bool verify_vm_id(const char *s);

		void returnOutput(const char **results, const int count);
		void returnOutputSuccess(void);
		void returnOutputError(void); 

		VMRequest* preExecuteCommand(const char* cmd, Gahp_Args *args);
		void executeCommand(VMRequest *req);

		void executeQuit(void);
		void executeVersion(void);
		void executeCommands(void);
		void executeSupportVMS(void);
		void executeResults(void); 
		void executeStart(VMRequest *req);
		void executeStop(VMRequest *req);
		void executeCkptstop(VMRequest *req);
		void executeSuspend(VMRequest *req);
		void executeSoftSuspend(VMRequest *req);
		void executeResume(VMRequest *req);
		void executeCheckpoint(VMRequest *req);
		void executeStatus(VMRequest *req);
		void executeGetpid(VMRequest *req);

		PBuffer m_request_buffer;
		ClassAd *m_jobAd;	// Job ClassAd received from Starter 
		bool m_inClassAd;	// indicating that vmgahp is receiving ClassAd from Starter 

		int m_async_mode;
		int m_new_results_signaled;

		int m_max_vm_id; // next vm_id will be (m_max_vm_id + 1)

		HashTable<int,VMRequest*> m_pending_req_table;
		StringList m_result_list;
		SimpleList<VMType*> m_vm_list;

		bool m_need_output_for_quit;
};

#endif /* VM_GAHP_H */
