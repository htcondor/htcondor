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


#if !defined(_CONDOR_SANDBOX_MANAGER_H)
#define _CONDOR_SANDBOX_MANAGER_H

//#include "condor_daemon_core.h"
//#include "list.h"
#include "dc_collector.h"
#include "condor_classad.h"
#include "condor_adtypes.h"
#include "condor_debug.h"
#include "condor_attributes.h"
#include "util_lib_proto.h"
#include "internet.h"
#include "my_hostname.h"
#include "condor_state.h"
#include "condor_string.h"
#include "string_list.h"
#include "MyString.h"
#include "get_full_hostname.h"
#include "condor_random_num.h"
#include "../condor_procapi/procapi.h"
#include "misc_utils.h"
#include "get_daemon_name.h"
#include "enum_utils.h"
#include "condor_version.h"
#include "classad_command_util.h"
#include "sandbox.h"
#include <string>
#include <iostream>
#include <map>
#include <vector>
using namespace std;

/*
#include "../condor_starter.V6.1/user_proc.h"
#include "../condor_starter.V6.1/job_info_communicator.h"
#include "../condor_starter.V6.1/condor_privsep_helper.h"

#if defined(LINUX)
#include "../condor_starter.V6.1/glexec_privsep_helper.h"
#endif
*/


/*
 The Condor Sandbox Manager class.
 It does some initialization and manages asynchronous sandbox transfers
*/
class CSandboxManager
{
public:
	// Constructor
	CSandboxManager();

	// Destructor
	virtual ~CSandboxManager();

	// This is called at the end of main_init().  It calls
	// Config(), registers a bunch of signals, registers a
	// reaper, makes the courier's working dir and moves there,
	// sets resource limits, then calls Transfer Sandbox

	//virtual bool Init(const char*, int, int, int);

	// Given the base dir, create a local are to move the sandbox, 
	// assign a sandboxId to the sandbox and set its expiry
	virtual char* registerSandbox(const char*);

	// Given the sandboxId give the handle to the sandbox location
	virtual int transferSandbox(const char*);

	// Return a list of ids containing expired sandboxes
	virtual std::vector<string> getExpiredSandboxIds(void);

	// List details about registered sandboxes
	virtual void listRegisteredSandboxes(void);

	// Unregister the sandbox with the given id
	virtual void unregisterSandbox(const char*);

	// Unregister the sandbox with the given id
	virtual void unregisterSandbox(const string);

	// Unregister all sandboxes
	virtual void unregisterAllSandboxes(void);

protected:


private:
	// Map of sandboxIds and a pointer to the sandbox object

	std::map<string, CSandbox*>sandboxMap;
	
	void init(void);
};
#endif

