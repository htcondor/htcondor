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

// this is a simple tokenizer class for parsing keywords out of a line of text
// token separator defaults to whitespace, "" or '' can be used to have tokens
// containing whitespace, but there is no way to escape " inside a "" string or
// ' inside a '' string. outer "" and '' are not considered part of the token.

class dag_tokener {
public:
	dag_tokener(const char * line_in);
	void rewind() { tokens.Rewind(); }
	const char * next() { return tokens.AtEnd() ? NULL : tokens.Next()->c_str(); }
protected:
	List<std::string> tokens; // parsed tokens
};


#include "enum.h"

namespace DagmanShallowOptions {

  BETTER_ENUM(str, long,
    ScheddDaemonAdFile = 0, ScheddAddressFile, ConfigFile, SaveFile
  );

  BETTER_ENUM(b, long,
    PostRun = 0, DumpRescueDag, RunValgrind
  );

  BETTER_ENUM(i, long,
    MaxIdle = 0, MaxJobs, MaxPre, MaxPost, DebugLevel, Priority
  );

}


class SubmitDagShallowOptions {

  public:

    SubmitDagShallowOptions()
    {
        using namespace DagmanShallowOptions;

        param(appendFile, "DAGMAN_INSERT_SUB_FILE");
        copyToSpool = param_boolean( "DAGMAN_COPY_TO_SPOOL", false );

        boolOpts[b::PostRun] = false;
        boolOpts[b::DumpRescueDag] = false;
        boolOpts[b::RunValgrind] = false;

        intOpts[i::MaxIdle] = 0;
        intOpts[i::MaxJobs] = 0;
        intOpts[i::MaxPre] = 0;
        intOpts[i::MaxPost] = 0;
        intOpts[i::DebugLevel] = DEBUG_UNSET;
        intOpts[i::Priority] = 0;
    }


    // Options offered by the Python bindings in version 1.
    const std::string & operator[]( DagmanShallowOptions::str opt ) const;
    bool operator[]( DagmanShallowOptions::b opt ) const;
    int operator[]( DagmanShallowOptions::i opt ) const;

    std::string & operator[]( DagmanShallowOptions::str opt );
    bool & operator[]( DagmanShallowOptions::b opt );
    int & operator[]( DagmanShallowOptions::i opt );

    // Command-line options.
    bool bSubmit = true;
    std::string strRemoteSchedd;
    std::string appendFile; // append to .condor.sub file before queue
    std::list<std::string> appendLines; // append to .condor.sub file before queue
    std::string primaryDagFile;
    std::list<std::string> dagFiles;
    bool doRecovery = false;
    bool bPostRunSet = false; // whether this was actually set on the command line

    // Non-command-line options.
    std::string strLibOut;
    std::string strLibErr;
    std::string strDebugLog; // the dagman.out file
    std::string strSchedLog; // the user log of condor_dagman's events
    std::string strSubFile;
    std::string strRescueFile;
    std::string strLockFile;
    bool copyToSpool;

  private:

    std::array<std::string, DagmanShallowOptions::str::_size()> stringOpts;
    std::array<bool, DagmanShallowOptions::b::_size()> boolOpts;
    std::array<int, DagmanShallowOptions::i::_size()> intOpts;
};


namespace DagmanDeepOptions {

  BETTER_ENUM(str, long,
    DagmanPath = 0, OutfileDir, BatchName, GetFromEnv
  );

  BETTER_ENUM(b, long,
    Force = 0, ImportEnv, UseDagDir, AutoRescue, AllowVersionMismatch,
    Recurse, UpdateSubmit, SuppressNotification
  );

}


class SubmitDagDeepOptions {

  public:

    SubmitDagDeepOptions()
    {
        using namespace DagmanDeepOptions;

        boolOpts[b::Force] = false;
        boolOpts[b::UseDagDir] = false;
        boolOpts[b::AutoRescue] = param_boolean( "DAGMAN_AUTO_RESCUE", true );
        boolOpts[b::AllowVersionMismatch] = false;
        boolOpts[b::Recurse] = false;
        boolOpts[b::UpdateSubmit] = false;
        boolOpts[b::ImportEnv] = false;
        boolOpts[b::SuppressNotification] = false;
    }


    // Options offered by the Python bindings in version 1.
    const std::string & operator[]( DagmanDeepOptions::str opt ) const;
    bool operator[]( DagmanDeepOptions::b opt ) const;
    // int operator[]( DagmanDeepOptions::i opt ) const;

    std::string & operator[]( DagmanDeepOptions::str opt );
    bool & operator[]( DagmanDeepOptions::b opt );
    // int & operator[]( DagmanDeepOptions::i opt );


    // Command-line options.
    std::string strNotification;
    std::string batchName; // optional value from -batch-name argument, will be double quoted if it exists.
    std::string batchId;
    std::string acctGroup;
    std::string acctGroupUser;

    bool bVerbose = false;

    int doRescueFrom = 0; // 0 means no rescue DAG specified

    std::vector<std::string> addToEnv; // explicitly add var=value envrionment info into .condor.sub file

  private:

    std::array<std::string, DagmanDeepOptions::str::_size()> stringOpts;
    std::array<bool, DagmanDeepOptions::b::_size()> boolOpts;
    // std::array<int, DagmanDeepOptions::i::_size()> intOpts;
};


class DagmanUtils {

public:

    bool usingPythonBindings = false;

    bool writeSubmitFile( /* const */ SubmitDagDeepOptions &deepOpts,
        /* const */ SubmitDagShallowOptions &shallowOpts,
        /* const */ std::list<std::string> &dagFileAttrLines ) const;
    
    int runSubmitDag( const SubmitDagDeepOptions &deepOpts,
        const char *dagFile, const char *directory, int priority,
        bool isRetry );

    int setUpOptions( SubmitDagDeepOptions &deepOpts,
        SubmitDagShallowOptions &shallowOpts,
        std::list<std::string> &dagFileAttrLines );

    bool processDagCommands( SubmitDagDeepOptions& deepOpts, SubmitDagShallowOptions& shallowOpts,
                             std::list<std::string> &attrLines, std::string &errMsg );

    bool MakePathAbsolute(std::string &filePath, std::string &errMsg);

    int FindLastRescueDagNum(const char *primaryDagFile,
        bool multiDags, int maxRescueDagNum);

    bool fileExists(const std::string &strFile);

    bool ensureOutputFilesExist(const SubmitDagDeepOptions &deepOpts,
        SubmitDagShallowOptions &shallowOpts);

    std::string RescueDagName(const char *primaryDagFile,
        bool multiDags, int rescueDagNum);

    void RenameRescueDagsAfter(const char *primaryDagFile, bool multiDags, 
        int rescueDagNum, int maxRescueDagNum);

    std::string HaltFileName( const std::string &primaryDagFile );

    void tolerant_unlink( const char *pathname );

    /** Determine whether the strictness setting turns a warning into a fatal
    error.
    @param strictness: The strictness level of the warning.
    @param quit_if_error: Whether to exit immediately if the warning is
        treated as an error
    @return true iff the warning is treated as an error
    */
    bool check_warning_strictness( strict_level_t strictness,
                bool quit_if_error = true );

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
