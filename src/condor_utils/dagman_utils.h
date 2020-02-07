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
#include "string_list.h"


#define DAG_SUBMIT_FILE_SUFFIX ".condor.sub"

#ifdef WIN32
#define dagman_exe "condor_dagman.exe"
#define valgrind_exe "valgrind.exe"
#else
#define dagman_exe "condor_dagman"
#define valgrind_exe "valgrind"
#endif

// The default maximum rescue DAG number.
const int MAX_RESCUE_DAG_DEFAULT = 100;

// The absolute maximum allowed rescue DAG number (the real maximum
// is normally configured lower).
const int ABS_MAX_RESCUE_DAG_NUM = 999;

class EnvFilter : public Env
{
public:
    EnvFilter( void ) { };
    virtual ~EnvFilter( void ) { };
    virtual bool ImportFilter( const MyString & /*var*/,
                               const MyString & /*val*/ ) const;
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
    StringList appendLines; // append to .condor.sub file before queue
    MyString strConfigFile;
    bool dumpRescueDag;
    bool runValgrind;
    MyString primaryDagFile;
    StringList    dagFiles;
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
    MyString strDagmanPath; // path to dagman binary
    bool useDagDir;
    MyString strOutfileDir;
    MyString batchName; // optional value from -batch-name argument, will be double quoted if it exists.
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
        /* const */ StringList &dagFileAttrLines );
    
    int runSubmitDag( const SubmitDagDeepOptions &deepOpts,
        const char *dagFile, const char *directory, int priority,
        bool isRetry );

    int setUpOptions( SubmitDagDeepOptions &deepOpts,
        SubmitDagShallowOptions &shallowOpts,
        StringList &dagFileAttrLines );

    bool GetConfigAndAttrs( /* const */ StringList &dagFiles, bool useDagDir, 
        MyString &configFile, StringList &attrLines, MyString &errMsg );

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

};


#endif