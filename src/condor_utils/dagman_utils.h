/***************************************************************
 *
 * Copyright (C) 1990-2018, Condor Team, Computer Sciences Department,
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

#ifndef DAGMAN_UTILS_H
#define DAGMAN_UTILS_H

#include "condor_common.h"
#include "condor_config.h"
#include "../condor_dagman/debug.h"
#include "condor_string.h"
#include "env.h"

#include <array>

#define DAG_SUBMIT_FILE_SUFFIX ".condor.sub"

#ifdef WIN32
#define dagman_exe "condor_dagman.exe"
#define valgrind_exe "valgrind.exe"
#else
#define dagman_exe "condor_dagman"
#define valgrind_exe "valgrind"
#endif

const int UTIL_MAX_LINE_LENGTH = 1024;

// The default maximum rescue DAG number.
const int MAX_RESCUE_DAG_DEFAULT = 100;

// The absolute maximum allowed rescue DAG number (the real maximum
// is normally configured lower).
const int ABS_MAX_RESCUE_DAG_NUM = 999;

enum DagStatus {
    DAG_STATUS_OK = 0,
    DAG_STATUS_ERROR = 1, // Error not enumerated below
    DAG_STATUS_NODE_FAILED = 2, // Node(s) failed
    DAG_STATUS_ABORT = 3, // Hit special DAG abort value
    DAG_STATUS_RM = 4, // DAGMan job condor rm'ed
    DAG_STATUS_CYCLE = 5, // A cycle in the DAG
    DAG_STATUS_HALTED = 6, // DAG was halted and submitted jobs finished
};

#include "enum.h"
typedef std::list<std::string> str_list;

// Enum to represent booleans passed by command line
enum class CLI_BOOL {
	UNSET = -1,  // Boolean not set
	FALSE,       // Boolean was set to False
	TRUE,        // Boolean was set to True
};

// Wrapper Struct around CLI_BOOL enum to overload various operators
// for setting and checking values.
// Note: For comparison/boolean evaluation the value CLI_BOOL::UNSET is
//       considered False. Use set() to distinguish between UNSET & FALSE
struct CLI_BOOL_FLAG {

	// CLI_BOOL enum value held in struct
	CLI_BOOL value;

	// Default struct constructors
	CLI_BOOL_FLAG() { value = CLI_BOOL::UNSET; }
	CLI_BOOL_FLAG(CLI_BOOL val) : value(val) {}

	// Function to display the current value as a string
	const char* display() const {
		if (value == CLI_BOOL::UNSET)
			return "UNSET";
		else if (value == CLI_BOOL::TRUE)
			return "TRUE";
		else if (value == CLI_BOOL::FALSE)
			return "FALSE";
		else
			return "UNKNOWN";
	}

	// Function to declare if value is not UNSET
	bool set() const { return value != CLI_BOOL::UNSET; }

	// Overload set (=) operator for member = bool
	CLI_BOOL_FLAG& operator=(const bool& b_val) {
		if (b_val) { value = CLI_BOOL::TRUE; }
		else { value = CLI_BOOL::FALSE; }
		return *this;
	}
	// Overload set (=) operator for member = enum
	CLI_BOOL_FLAG& operator=(const CLI_BOOL& val) {
		value = val;
		return *this;
	}

	// Overload == comparison operator for member == bool
	bool operator==(const bool& std_bool) const {
		if (std_bool) { return value == CLI_BOOL::TRUE; }
		else { return value != CLI_BOOL::TRUE; }
	}
	// Overload == comparison operator for member == enum
	bool operator==(const CLI_BOOL& other) const {
		return value == other;
	}

	// Overload != comparison operator for member != bool
	bool operator!=(const bool& std_bool) const {
		if (std_bool) { return value != CLI_BOOL::TRUE; }
		else { return value == CLI_BOOL::TRUE; }
	}
	// Overload != comparison operator for member != eunm
	bool operator!=(const CLI_BOOL& other) const {
		return value != other;
	}

	// Overload raw boolean evaluation for if(member)/if(!member)
	operator bool() const { return value == CLI_BOOL::TRUE; }
};

namespace DagmanShallowOptions {
	BETTER_ENUM(str, long,
		ScheddDaemonAdFile = 0, ScheddAddressFile, ConfigFile, SaveFile, RemoteSchedd, AppendFile,
		PrimaryDagFile, LibOut, LibErr, DebugLog, SchedLog, SubFile, RescueFile, LockFile
	);

	BETTER_ENUM(i, long,
		MaxIdle = 0, MaxJobs, MaxPre, MaxPost, DebugLevel, Priority
	);

	BETTER_ENUM(b, long,
		PostRun = 0, DumpRescueDag, RunValgrind, DoSubmit, DoRecovery, CopyToSpool
	);

	BETTER_ENUM(slist, long,
		AppendLines = 0, DagFiles
	);
}

namespace DagmanDeepOptions {
	BETTER_ENUM(str, long,
		DagmanPath = 0, OutfileDir, BatchName, GetFromEnv, Notification,
		BatchId, AcctGroup, AcctGroupUser
	);

	BETTER_ENUM(i, long,
		DoRescueFrom = 0
	);

	BETTER_ENUM(b, long,
		Force = 0, ImportEnv, UseDagDir, AutoRescue/*Fix to be int*/, AllowVersionMismatch,
		Recurse, UpdateSubmit, SuppressNotification, Verbose
	);

	BETTER_ENUM(slist, long,
		AddToEnv = 0
	);
}

enum class SetDagOpt {
	SUCCESS = 0,
	NO_KEY = 1,
	NO_VALUE = 2,
	INVALID_VALUE = 3,
	KEY_DNE = 4,
};

struct DSO {
	typedef DagmanDeepOptions::str str;
	typedef DagmanDeepOptions::slist slist;
	typedef DagmanDeepOptions::b b;
	typedef DagmanDeepOptions::i i;
};

struct SSO {
	typedef DagmanShallowOptions::str str;
	typedef DagmanShallowOptions::slist slist;
	typedef DagmanShallowOptions::b b;
	typedef DagmanShallowOptions::i i;
};

template<typename T>
struct DagOptionData {
	std::array<str_list, T::slist::_size()> slistOpts;
	std::array<std::string, T::str::_size()> stringOpts;
	std::array<int, T::i::_size()> intOpts;
	std::array<CLI_BOOL_FLAG, T::b::_size()> boolOpts;
};

class DagmanOptions {
public:
	DagmanOptions() {
		{ //Initialize Shallow Options
			using namespace DagmanShallowOptions;
			std::string appendFile;
			param(appendFile, "DAGMAN_INSERT_SUB_FILE");
			shallow.stringOpts[str::AppendFile] = appendFile;
			shallow.boolOpts[b::DoSubmit] = true;
			shallow.boolOpts[b::CopyToSpool] = param_boolean( "DAGMAN_COPY_TO_SPOOL", false );
			shallow.intOpts[i::MaxIdle] = 0;
			shallow.intOpts[i::MaxJobs] = 0;
			shallow.intOpts[i::MaxPre] = 0;
			shallow.intOpts[i::MaxPost] = 0;
			shallow.intOpts[i::DebugLevel] = DEBUG_UNSET;
			shallow.intOpts[i::Priority] = 0;
		} //End Shallow Option Initialization

		{ //Initialize Deep Options
			using namespace DagmanDeepOptions;
			deep.intOpts[i::DoRescueFrom] = 0;
			deep.boolOpts[b::AutoRescue] = param_boolean( "DAGMAN_AUTO_RESCUE", true );
		} //End Deep Option Initialization
	}

	// Get value type needed to set to option (int, bool, str)
	std::string OptValueType(std::string& opt);

	// Set a DAGMan option to given value
	SetDagOpt set(const char* opt, const std::string& value);
	SetDagOpt set(const char* opt, const char* value);
	SetDagOpt set(const char* opt, bool value);
	SetDagOpt set(const char* opt, int value);

	// Append a string to a delimited string list of items: "foo,bar,barz"
	SetDagOpt append(const char* opt, const std::string& value, const char delim = ',');
	SetDagOpt append(const char* opt, const char* value, const char delim = ',');

	// Extend (push_back) value to list option
	SetDagOpt extend(const char* opt, const std::string& value);
	SetDagOpt extend(const char* opt, const char* value);

	void addDeepArgs(ArgList& args, bool inWriteSubmit = true) const;
	void addDAGFile(std::string& dagFile);

	inline std::string primaryDag() const { return shallow.stringOpts[DagmanShallowOptions::str::PrimaryDagFile]; }
	inline str_list dagFiles() const { return shallow.slistOpts[DagmanShallowOptions::slist::DagFiles]; }
	inline bool isMultiDag() const { return is_MultiDag; }


	// Const shallow options access operator declarations
	const str_list & operator[]( SSO::slist opt ) const { return shallow.slistOpts[opt._to_integral()]; }
	const std::string & operator[]( SSO::str opt ) const { return shallow.stringOpts[opt._to_integral()]; }
	CLI_BOOL_FLAG operator[]( SSO::b opt ) const { return shallow.boolOpts[opt._to_integral()]; }
	int operator[]( SSO::i opt ) const { return shallow.intOpts[opt._to_integral()]; }

	// Shallow options access operator declarations
	str_list & operator[]( SSO::slist opt ) { return shallow.slistOpts[opt._to_integral()]; }
	std::string & operator[]( SSO::str opt ) { return shallow.stringOpts[opt._to_integral()]; }
	CLI_BOOL_FLAG & operator[]( SSO::b opt ) { return shallow.boolOpts[opt._to_integral()]; }
	int & operator[]( SSO::i opt ) { return shallow.intOpts[opt._to_integral()]; }

	// Const deep option access operator declarations
	const str_list & operator[]( DSO::slist opt ) const { return deep.slistOpts[opt._to_integral()]; }
	const std::string & operator[]( DSO::str opt ) const { return deep.stringOpts[opt._to_integral()]; }
	CLI_BOOL_FLAG operator[]( DSO::b opt ) const { return deep.boolOpts[opt._to_integral()]; }
	int operator[]( DSO::i opt ) const { return deep.intOpts[opt._to_integral()]; }

	// Deep option access operator declarations
	str_list & operator[]( DSO::slist opt ) { return deep.slistOpts[opt._to_integral()]; }
	std::string & operator[]( DSO::str opt ) { return deep.stringOpts[opt._to_integral()]; }
	CLI_BOOL_FLAG & operator[]( DSO::b opt ) { return deep.boolOpts[opt._to_integral()]; }
	int & operator[]( DSO::i opt ) { return deep.intOpts[opt._to_integral()]; }

private:
	//Shallow options used only by this DAG
	DagOptionData<SSO> shallow;
	//Deep options passed down to subdags
	DagOptionData<DSO> deep;
	bool is_MultiDag{false};
};

class DagmanUtils {

public:

	bool usingPythonBindings = false;

	bool writeSubmitFile(DagmanOptions &options, str_list &dagFileAttrLines) const;

	int runSubmitDag(const DagmanOptions &options, const char *dagFile,
	                 const char *directory, int priority, bool isRetry);

	bool setUpOptions(DagmanOptions &options, str_list &dagFileAttrLines);

	bool processDagCommands(DagmanOptions &options, str_list &attrLines, std::string &errMsg);

	bool MakePathAbsolute(std::string &filePath, std::string &errMsg);

	int FindLastRescueDagNum(const std::string &primaryDagFile, bool multiDags, int maxRescueDagNum);

	bool fileExists(const std::string &strFile);

	bool ensureOutputFilesExist(const DagmanOptions &options);

	std::string RescueDagName(const std::string &primaryDagFile, bool multiDags, int rescueDagNum);

	void RenameRescueDagsAfter(const std::string &primaryDagFile, bool multiDags, int rescueDagNum, int maxRescueDagNum);

	static inline std::string HaltFileName(const std::string &primaryDagFile) { return primaryDagFile + ".halt"; }

	void tolerant_unlink(const std::string &pathname);

	/** Determine whether the strictness setting turns a warning into a fatal
	error.
	@param strictness: The strictness level of the warning.
	@param quit_if_error: Whether to exit immediately if the warning is
	       treated as an error
	@return true iff the warning is treated as an error
	*/
	bool check_warning_strictness(strict_level_t strictness, bool quit_if_error = true);

	/** Execute a command, printing verbose messages and failure warnings.
	    @param cmd The command or script to execute
	    @return The return status of the command
	*/
	int popen (ArgList &args);

	/** Create the given lock file, containing the PID of this process.
	    @param lockFileName: the name of the lock file to create
	    @return: 0 if successful, -1 if not
	*/
	int create_lock_file(const char *lockFileName, bool abortDuplicates);

	/** Check the given lock file and see whether the PID given in it
	    does, in fact, exist.
	    @param lockFileName: the name of the lock file to check
	    @return: 0 if successful, -1 if there was an error, 1 if the
	             relevant PID does exist and this DAGMan should abort
	*/
	int check_lock_file(const char *lockFileName);

};


#endif
