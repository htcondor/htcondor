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


#if !defined(_CONDOR_SANDBOX_H)
#define _CONDOR_SANDBOX_H

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
#include <string>
#include <sstream>
//#include<stringstream>

using namespace std;

/*
 The Condor Sandbox class. This holds details about the actual sandbox
*/
class CSandbox
{
public:
	// Constructors
	// Params: Source Dir
	CSandbox(const char*);
	// Params: Source Dir, lifetime
	CSandbox(const char*, const int);
	// Params: Source Dir, lifetime, Id length
	CSandbox(const char*, const int, const int);

	// Destructor
	virtual ~CSandbox();

	// Return my Id
	string getId(void);

	// Return the creation time
	time_t getCreateTime(void);

	// Return the expiry time
	time_t getExpiry(void);

	// Check if the sandbox is expired
	bool isExpired(void);

	// Extend the expiry by given secs
	void renewExpiry(const int);

	// Set the expiry to given time
	void setExpiry(const int);

	// Return the location to the sandboxDir
	string getSandboxDir(void);

	// Given the sandboxId give the handle to the sandbox location
	virtual string getDetails(void);
protected:


private:
	string id;		// Sandbox Id
	string srcDir;		// Dir where sandboxe is cached
	string sandboxDir;	// Dir where sandboxe is moved to
	time_t createTime;	// Sandbox creation time
	time_t expiry;		// Sandbox expiry time

	// Initialize the created sandbox
	void init(const char*, const int, const int);
	string createId(const int);

	static const int _MIN_SANDBOXID_LENGTH = 64;
	static const int _MIN_LIFETIME = 3600;
};
#endif

