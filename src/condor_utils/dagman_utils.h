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
#include "MyString.h"


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

class EnvFilter : public Env
{
public:
    EnvFilter( void ) { };
    virtual ~EnvFilter( void ) { };
    virtual bool ImportFilter( const MyString & /*var*/,
                               const MyString & /*val*/ ) const;
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

    //
    // These are options that are *not* passed to lower levels of
    // condor_submit_dag when creating the submit files for nested
    // DAGs.
    //
struct SubmitDagShallowOptions
{
    bool bSubmit;
    MyString strRemoteSchedd;
    MyString strScheddDaemonAdFile;
    MyString strScheddAddressFile;
    int iMaxIdle;
    int iMaxJobs;
    int iMaxPre;
    int iMaxPost;
    MyString appendFile; // append to .condor.sub file before queue
    std::list<std::string> appendLines; // append to .condor.sub file before queue
    MyString strConfigFile;
    bool dumpRescueDag;
    bool runValgrind;
    MyString primaryDagFile;
    std::list<std::string> dagFiles;
    bool doRecovery;
    bool bPostRun;
    bool bPostRunSet; // whether this was actually set on the command line
    int priority;

    // non-command line options
    MyString strLibOut;
    MyString strLibErr;
    MyString strDebugLog; // the dagman.out file
    MyString strSchedLog; // the user log of condor_dagman's events
    MyString strSubFile;
    MyString strRescueFile;
    MyString strLockFile;
    bool copyToSpool;
    int iDebugLevel;

    SubmitDagShallowOptions() 
    { 
        bSubmit = true;
        bPostRun = false;
        bPostRunSet = false;
        strRemoteSchedd = "";
        strScheddDaemonAdFile = "";
        strScheddAddressFile = "";
        iMaxIdle = 0;
        iMaxJobs = 0;
        iMaxPre = 0;
        iMaxPost = 0;
        appendFile = param( "DAGMAN_INSERT_SUB_FILE" );
        strConfigFile = "";
        dumpRescueDag = false;
        runValgrind = false;
        primaryDagFile = "";
        doRecovery = false;
        copyToSpool = param_boolean( "DAGMAN_COPY_TO_SPOOL", false );
        iDebugLevel = DEBUG_UNSET;
        priority = 0;
    }
};

    //
    // These are options that *are* passed to lower levels of
    // condor_submit_dag when creating the submit files for nested
    // DAGs.
    //
struct SubmitDagDeepOptions
{
    // these options come from the command line
    bool bVerbose;
    bool bForce;
    MyString strNotification;
	std::string strDagmanPath; // path to dagman binary
    bool useDagDir;
    MyString strOutfileDir;
    MyString batchName; // optional value from -batch-name argument, will be double quoted if it exists.
    std::string batchId;
    bool autoRescue;
    int doRescueFrom;
    bool allowVerMismatch;
    bool recurse; // whether to recursively run condor_submit_dag on nested DAGs
    bool updateSubmit; // allow updating submit file w/o -force
    bool importEnv; // explicitly import environment into .condor.sub file

    bool suppress_notification;
    MyString acctGroup;
    MyString acctGroupUser;

    SubmitDagDeepOptions() 
    { 
        bVerbose = false;
        bForce = false;
        strNotification = "";
        useDagDir = false;
        autoRescue = param_boolean( "DAGMAN_AUTO_RESCUE", true );
        doRescueFrom = 0; // 0 means no rescue DAG specified
        allowVerMismatch = false;
        recurse = false;
        updateSubmit = false;
        importEnv = false;
        suppress_notification = true;
        acctGroup = "";
        acctGroupUser = "";
    }
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

    bool GetConfigAndAttrs( /* const */ std::list<std::string> &dagFiles, bool useDagDir, 
        MyString &configFile, std::list<std::string> &attrLines, MyString &errMsg );

    bool MakePathAbsolute(MyString &filePath, MyString &errMsg);

    int FindLastRescueDagNum(const char *primaryDagFile,
        bool multiDags, int maxRescueDagNum);

    bool fileExists(const MyString &strFile);

    bool ensureOutputFilesExist(const SubmitDagDeepOptions &deepOpts,
        SubmitDagShallowOptions &shallowOpts);

    MyString RescueDagName(const char *primaryDagFile,
        bool multiDags, int rescueDagNum);

    void RenameRescueDagsAfter(const char *primaryDagFile, bool multiDags, 
        int rescueDagNum, int maxRescueDagNum);

    MyString HaltFileName( const MyString &primaryDagFile );

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
