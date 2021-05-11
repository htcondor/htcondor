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

#if 0 // Moved to dagman_utils
#ifndef DAGMAN_RECURSIVE_SUBMIT_H
#define DAGMAN_RECURSIVE_SUBMIT_H

#include "condor_common.h"
#include "MyString.h"
#include "condor_config.h"
#include "debug.h"

#define DAG_SUBMIT_FILE_SUFFIX ".condor.sub"


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
	std::list<std::string>	dagFiles;
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

extern "C" {
/** Run condor_submit_dag on the given DAG file.
	@param opts: the condor_submit_dag options
	@param dagFile: the DAG file to process
	@param directory: the directory from which the DAG file should
		be processed (ignored if NULL)
	@param priority: the priority of this DAG
	@param isRetry: whether this is a retry
	@return 0 if successful, 1 if failed
*/
int runSubmitDag( const SubmitDagDeepOptions &deepOpts,
			const char *dagFile, const char *directory, int priority,
			bool isRetry );
}

#endif	// ifndef DAGMAN_RECURSIVE_SUBMIT_H
#endif
