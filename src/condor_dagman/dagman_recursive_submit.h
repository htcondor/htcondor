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

#ifndef DAGMAN_RECURSIVE_SUBMIT_H
#define DAGMAN_RECURSIVE_SUBMIT_H

#include "condor_common.h"
#include "MyString.h"
#include "string_list.h"
#include "condor_config.h"

static const char *DAG_SUBMIT_FILE_SUFFIX = ".condor.sub";

	//
	// These are options that are *not* passed to lower levels of
	// condor_submit_dag when creating the submit files for nested
	// DAGs.
	//
struct SubmitDagShallowOptions
{
	bool bSubmit;//TEMPTEMP -- this might be deep
	MyString strRemoteSchedd;//TEMPTEMP?
	int iMaxIdle;
	int iMaxJobs;
	int iMaxPre;
	int iMaxPost;
	MyString strConfigFile;//TEMPTEMP?
	MyString primaryDagFile;
	StringList	dagFiles;

	// non-command line options
	MyString strLibOut;
	MyString strLibErr;
	MyString strDebugLog;
	MyString strSchedLog;
	MyString strSubFile;
	MyString strRescueFile;
	MyString strLockFile;

	SubmitDagShallowOptions() 
	{ 
		bSubmit = true;
		strRemoteSchedd = "";
		iMaxIdle = 0;
		iMaxJobs = 0;
		iMaxPre = 0;
		iMaxPost = 0;
		strConfigFile = "";
		primaryDagFile = "";
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
	bool bNoEventChecks;//TEMPTEMP?
	bool bAllowLogError;//TEMPTEMP?
	int iDebugLevel;//TEMPTEMP?
	MyString strDagmanPath; // path to dagman binary
	bool useDagDir;
	MyString strDebugDir;
	MyString appendFile; // append to .condor.sub file before queue//TEMPTEMP?
	StringList appendLines; // append to .condor.sub file before queue//TEMPTEMP?
	bool oldRescue;
	bool autoRescue;
	int doRescueFrom;
	bool allowVerMismatch;//TEMPTEMP?
	bool recurse; // whether to recursively run condor_submit_dag on nested DAGs
	bool updateSubmit; // allow updating submit file w/o -force//TEMPTEMP?
	bool copyToSpool;//TEMPTEMP?
	bool importEnv; // explicitly import environment into .condor.sub file//TEMPTEMP?
	bool dumpRescueDag;//TEMPTEMP?
	bool runValgrind;//TEMPTEMP?
	
	SubmitDagDeepOptions() 
	{ 
		bVerbose = false;
		bForce = false;
		strNotification = "";
		bNoEventChecks = false;
		bAllowLogError = false;
		iDebugLevel = 3;
		useDagDir = false;
		appendFile = param("DAGMAN_INSERT_SUB_FILE");
		oldRescue = param_boolean( "DAGMAN_OLD_RESCUE", false );
		autoRescue = param_boolean( "DAGMAN_AUTO_RESCUE", true );
		doRescueFrom = 0; // 0 means no rescue DAG specified
		allowVerMismatch = false;
		recurse = false;
		updateSubmit = false;
		copyToSpool = param_boolean( "DAGMAN_COPY_TO_SPOOL", false );
		importEnv = false;
		dumpRescueDag = false;
		runValgrind = false;
	}
};

/** Run condor_submit_dag on the given DAG file.
	@param opts: the condor_submit_dag options
	@param dagFile: the DAG file to process
	@param directory: the directory from which the DAG file should
		be processed (ignored if NULL)
	@return 0 if successful, 1 if failed
*/
int runSubmitDag( const SubmitDagDeepOptions &deepOpts,
			const char *dagFile, const char *directory );

#endif	// ifndef DAGMAN_RECURSIVE_SUBMIT_H
