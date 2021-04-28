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


#if !defined(_CONDOR_VM_PROC_H)
#define _CONDOR_VM_PROC_H

#include "os_proc.h"
#include "vm_gahp_request.h"
#include "vm_gahp_server.h"
#include "condor_vm_universe_types.h"
#include "../condor_procapi/procapi.h"

/** The VM-type job process class.  Uses VMGahpServer to do its
	dirty work.
 */

class VMGahpServer;
class VMGahpRequest;

class VMProc : public OsProc
{
	public:
		static bool vm_univ_detect();

		VMProc( ClassAd * jobAd );
		virtual ~VMProc();

		// create new vmgahp server and send a VM_START command
		virtual int StartJob();

		virtual bool JobReaper(int pid, int status);

		virtual bool JobExit(void);

		virtual void Suspend();

		virtual void Continue();

		// The following functions will generate no checkpoint file 
		// before destroying a VM
		virtual bool Remove();
		virtual bool Hold();
		virtual bool ShutdownFast();

		// This function will generate a checkpoint file 
		// before destroying a VM
		virtual bool ShutdownGraceful();

		// This function will generate checkpoint files
		virtual bool Ckpt();
		virtual void CkptDone(bool success);

		virtual void CheckStatus();

		/** Publish all attributes we care about for updating the job
		  controller into the given ClassAd.  This function is just
		  virtual, not pure virtual, since OsProc and any derived
		  classes should implement a version of this that publishes
		  any info contained in each class, and each derived version
		  should also call it's parent's version, too.
		  @param ad pointer to the classad to publish into
		  @return true if success, false if failure
		  */
		virtual bool PublishUpdateAd( ClassAd* ad );

	private:
		bool StopVM();

		void cleanup();
		void notify_status_fn();
		/*
		 *  This function will return the PID of actual process for VM.
		 *  For example, VMware creates one process for each VM.
		 *  When we need to calculate CPU usage, we should use the process. 
		 *  Unfortunately, Procfamily can't deal with the process 
		 *   because the parent PID of the process will be 1.
		 *  In Xen, this function will always return 0 
		 *  because Xen doesn't use actual process for a VM.
		*/
		int PIDofVM();
		void setVMPID(int vm_pid);
		void setVMMAC(const char *mac);
		void setVMIP(const char *ip);

		void updateUsageOfVM();
		void getUsageOfVM(long &sys_time, long& user_time, unsigned long &max_image, unsigned long& rss, unsigned long &pss, bool& pss_available);
		void killProcessForVM();

		// If interal vmgahp error occurs, call this function 
		void internalVMGahpError();

		// Report vmgahp error to local startd
		// thus local startd will test vm universe again.
		bool reportErrorToStartd();

		// Report VM Info to local startd
		bool reportVMInfoToStartd(int cmd, const char *value);

		// If vm is stopped, this function will return false.
		// Otherwise it will return true.
		bool process_vm_status_result(Gahp_Args *result);
		void vm_status_error();

		VMGahpServer *m_vmgahp;
		bool m_is_cleanuped;
		std::string m_job_name;

		int m_vm_id;
		int m_vm_pid;
		std::string m_vm_mac;
		std::string m_vm_ip;

		struct procInfo m_vm_exited_pinfo;
		struct procInfo m_vm_alive_pinfo;

		std::string m_vm_type;
		std::string m_vmgahp_server;

		bool m_vm_checkpoint;
		bool m_is_vacate_ckpt;
		// Result of success of the last checkpoint
		bool m_last_ckpt_result;

		// Number of checkpointing executed during current running
		int m_vm_ckpt_count;
		// Time at which the job last performed a successful checkpoint.
		time_t m_vm_last_ckpt_time;


		/* 
		 Usually, when we suspend a VM, the memory being used by the VM 
		 will be freed and the memory will be saved into a file.
		 However, when we use soft suspend, the memory being used by the VM 
		 will not be freed and no file for the memory will be created.

		 Here is how we implement soft suspension.
		 In VMware, we send SIGSTOP to a process for VM in order to 
		 stop the VM temporarily and send SIGCONT to resume the VM.
		 In Xen, we pause CPU. 
		 Pausing CPU doesn't save the memory of VM into a file.
		 It just stops the execution of a VM temporarily.
		*/
		bool m_use_soft_suspend;
		bool m_is_soft_suspended;

		// timer id of sending VM status command periodically 
		int m_vmstatus_tid;

		// timer id of function to be called when the result of vm status is received
		int m_vmstatus_notify_tid;

		VMGahpRequest *m_status_req;
		int m_status_error_count;

#define VM_GAHP_REQ_TIMEOUT		300	// 5 mins
		int m_vmoperation_timeout;

#define VM_MIN_STATUS_INTERVAL 		30	//seconds
#define VM_DEFAULT_STATUS_INTERVAL	60	//seconds
		int m_vmstatus_interval;

#define VM_STATUS_MAX_ERROR_COUNT	5
		int m_vmstatus_max_error_cnt;

		// How much CPU time (in seconds) the domain has used so far. 
		// Only used for Xen.
		float m_vm_cputime;
		float m_vm_utilization; 
		
		// How much memory, in kbytes, the domain is using.
		unsigned long m_vm_memory;
		// How much memory, in kbytes, the domain could use.
		unsigned long m_vm_max_memory;
};

#endif
