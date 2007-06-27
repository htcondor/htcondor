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

#ifndef VM_GAHP_H
#define VM_GAHP_H

#include "condor_common.h"
#include "condor_debug.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"
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

class VMWorker {
	public:
		VMWorker() { m_pid = 0;}

		PBuffer m_request_buffer;
		PBuffer m_result_buffer;
		PBuffer m_stderr_buffer;

		int m_stdin_pipefds[2];
		int m_stdout_pipefds[2];
		int m_stderr_pipefds[2];

		int m_pid;
};

class VMGahp : public Service {
	public:
		VMGahp(VMGahpConfig* config, const char *iwd);
		virtual ~VMGahp();

		void startUp();
		void startWorker();
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
		void movePendingReqToOutputStream(VMRequest *req);

		VMRequest *findPendingRequest(int req_id);
		void printAllReqsWithResult();

		// Interfaces for VM
		void addVM(VMType *vm);
		void removeVM(int vm_id);
		void removeVM(VMType *vm);
		VMType *findVM(int vm_id);

		VMGahpConfig *m_gahp_config; // Gahp config file
		MyString m_workingdir;		 // working directory

	private:
		int waitForCommand();
		int flushPendingRequests();
		const char* make_result_line(VMRequest *req);

		int quitFast();
		void killAllProcess();
		int workerReaper(int pid, int exit_status);
		void sendRequestToWorker(const char* command);
		void sendClassAdToWorker();
		int workerResultHandler();
		int workerStderrHandler();

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

		int m_stdout_pipe;
		int m_stderr_pipe;

		int m_flush_request_tid;
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

		VMWorker m_worker;
};

#endif /* VM_GAHP_H */
