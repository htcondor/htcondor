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
#include "process_control.WINDOWS.h"
#include "ntsysinfo.WINDOWS.h"
#include "HashTable.h"
#include "condor_softkill.h"

static char* soft_kill_binary = NULL;

void
set_windows_soft_kill_binary(char* binary)
{
	if (soft_kill_binary != NULL) {
		free(soft_kill_binary);
	}
	soft_kill_binary = strdup(binary);
	ASSERT(soft_kill_binary != NULL);
}

bool
windows_soft_kill(DWORD pid)
{
	// softkill on Windows is implemented by posting a WM_CLOSE to
	// a top-level window owned by the process (if the process owns
	// more than 1 top-level window, it is undefined which one will
	// get the WM_CLOSE). finding the window and posting the message
	// are done using the condor_softkill tool
	//

	// bail out if the binary wasn't set
	//
	if (soft_kill_binary == NULL) {
		dprintf(D_ALWAYS,
		        "windows_soft_kill error: softkill binary not set\n");
		return false;
	}

	// construct the command line for the call to CreateProcess
	//
	char command_line[1024];
	int ret = _snprintf(command_line,
	                    1024,
	                    "\"%s\" %u debug.txt",
	                    soft_kill_binary,
	                    pid);
	command_line[1023] = 0;
	if (ret < 0) {
		dprintf(D_ALWAYS,
		        "windows:soft_kill: snprintf error: %s (%d)\n",
		        strerror(errno),
		        errno);
		return false;
	}
	if (command_line[1023] != '\0') {
		dprintf(D_ALWAYS,
		        "windows_soft_kill: command line too long\n");
		return false;
	}

	// call CreateProcess to run the softkill executable
	//
	STARTUPINFO si;
	memset(&si, 0, sizeof(STARTUPINFO));
	PROCESS_INFORMATION pi;
	if (CreateProcess(NULL,         // get the executable from the command line
	                  command_line,
	                  NULL,         // default process security
	                  NULL,         // default security for primary thread
	                  FALSE,        // don't inherit handles
	                  0,            // no flags
	                  NULL,         // inherit our environment
	                  NULL,         // inherit our working directory
	                  &si,
	                  &pi) == FALSE)
	{
		dprintf(D_ALWAYS,
		        "windows_soft_kill: CreateProcess error: %u\n",
		        GetLastError());
		return false;
	}
	CloseHandle(pi.hThread);

	// wait for the exit code; fail if we can't get it or if
	// it indicates failure
	//
	if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0) {
		dprintf(D_ALWAYS,
		        "windows_soft_kill: WaitForSingleObject error: %u\n",
		        GetLastError());
	}
	DWORD exit_code;
	BOOL result = GetExitCodeProcess(pi.hProcess, &exit_code);
	DWORD error = GetLastError();
	CloseHandle(pi.hProcess);
	if (result == FALSE) {
		dprintf(D_ALWAYS,
		        "windows_soft_kill: GetExitCodeProcess error: %u\n",
		        GetLastError());
		return false;
	}
	if (exit_code != SOFTKILL_SUCCESS) {
		dprintf(D_ALWAYS,
		        "windows_soft_kill: error code from condor_softkill: %u\n",
		        exit_code);
		return false;
	}

	// i guess it worked
	//
	return true;
}

bool
windows_hard_kill(DWORD pid)
{
	HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (process == NULL) {
		dprintf(D_ALWAYS,
		        "windows_hard_kill: OpenProcess error: %u\n",
		        GetLastError());
		return false;
	}
	BOOL ok = TerminateProcess(process, 0);
	DWORD err = GetLastError();
	CloseHandle(process);
	if (ok == FALSE) {
		dprintf(D_ALWAYS,
		        "windows_hard_kill: TerminateProcess error: %u\n",
		        (unsigned)err);
		return false;
	}
	return true;
}

// stupid hash function for DWORD (need by the hash tables
// used in the suspend/contiunue code below)
//
static size_t
hash_func(const DWORD& n)
{
	return n;
}

// stupid object for getting the list of threads for a process
//
static CSysinfo sys_info;

// record to keep track of the threads in a suspended process
//
struct SuspendedThread {

	// cache a handle to each thread while the process is suspended
	//
	HANDLE handle;

	// how many suspends did we send this thread? (so we know how
	// many resumes to send!)
	//
	int num_suspends;
};

// for each suspended process we keep a hash table of its
// SuspendedThread structures (keyed on thread id)
//
typedef HashTable<DWORD, SuspendedThread*> SuspendedProcess;

// we keep track of all suspended processes via a hash table
// keyed on process id
//
static HashTable<DWORD, SuspendedProcess*> suspended_processes(hash_func);

bool
windows_suspend(DWORD pid)
{
	// get the list of threads for this process
	std::vector<DWORD> tid_array;
	if (sys_info.GetTIDs(pid, tid_array) == 0) {
		dprintf(D_ALWAYS, "Error: Windows Suspend Process failed to get threads for pid %u\n", pid);
		return false;
	}

	// make sure we haven't already suspended this process, then allocate
	// a SuspendedProcess object and add it to the hash table
	//
	SuspendedProcess* sp;
	int ret = suspended_processes.lookup(pid, sp);
	ASSERT(ret == -1);
	sp = new SuspendedProcess(hash_func);
	ASSERT(sp != NULL);
	ret = suspended_processes.insert(pid, sp);
	ASSERT(ret != -1);

	// we need to loop until iterating through the thread list for this
	// process shows that all threads are suspended
	//
	bool finished = false;
	while (!finished) {

		// assume we're going to succeed on this iteration until
		// proven otherwise
		//
		finished = true;

		// go through the thread list, calling SuspendThread on each
		//
		for (size_t i = 0; i < tid_array.size(); i++) {

			// see if we already have a record for this thread
			//
			SuspendedThread* st;
			ret = sp->lookup(tid_array[i], st);
			if (ret == -1) {

				// no record yet; open up this thread's handle
				//
				HANDLE handle = OpenThread(THREAD_SUSPEND_RESUME, FALSE, tid_array[i]);
				if (handle == NULL) {

					// the thread probably exited; however, it may have called ResumeThread
					// or CreateThread first, so we'll have to loop again
					//
					finished = false;
					dprintf(D_FULLDEBUG,
					        "windows_suspend_process: OpenThread error: %u\n",
					        GetLastError());
					continue;
				}

				// now that we have a handle, create a record for this thread
				//
				st = new SuspendedThread;
				ASSERT(st != NULL);
				st->handle = handle;
				st->num_suspends = 0;
				ret = sp->insert(tid_array[i], st);
				ASSERT(ret != -1);
			}

			// now suspend the thread
			//
			DWORD result = SuspendThread(st->handle);
			if (result == (DWORD)-1) {

				// how can this happen? maybe the thread exited?
				//
				finished = false;
				dprintf(D_FULLDEBUG,
						"windows_suspend_process: SuspendThread error: %u\n",
						GetLastError());
				continue;
			}

			// if the thread was not suspended prior to our SuspendThread call,
			// it may have called CreateThread or ResumeThread so we need to loop
			// at least one more time
			//
			if (result == 0) {
				finished = false;
			}

			// keep track of how many suspends we've sent this thread
			//
			st->num_suspends++;
		}
	}

	return true;
}

bool
windows_continue(DWORD pid)
{
	// we should have a record from when we suspended this process
	//
	SuspendedProcess* sp;
	int ret = suspended_processes.lookup(pid, sp);

	// NOTE: In testing windows continuation the 
	// procd can at times pass invalid pid's to this function 
	// as a result, exit gracefully vs. assert.
	if (ret == -1)
	   return false;

	SuspendedThread* st;
	sp->startIterations();
	while (sp->iterate(st)) {

		// call ResumeThread as many times as we called SuspendThread
		// for this thread above
		//
		for (int i = 0; i < st->num_suspends; i++) {
			if (ResumeThread(st->handle) == (DWORD)-1) {
				dprintf(D_FULLDEBUG,
				        "windows_continue_process: ResumeThread error: %u\n",
				        GetLastError());
			}
		}

		// close our handle to this thread
		//
		if (CloseHandle(st->handle) == FALSE) {
			dprintf(D_FULLDEBUG,
			        "windows_continue_process: CloseHandle error: %u\n",
			        GetLastError());
		}

		// clean up this SuspendedThread object
		//
		delete st;
	}

	// clean up the SuspendedProcess object and remove it from the
	// hash table of processes
	//
	delete sp;
	ret = suspended_processes.remove(pid);
	ASSERT(ret != -1);

	return true;
}
