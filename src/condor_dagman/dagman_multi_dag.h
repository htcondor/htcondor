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


// Functions for multi-DAG support.

// Note that this is used by both condor_submit_dag and condor_dagman
// itself.

#ifndef DAGMAN_MULTI_DAG_H
#define DAGMAN_MULTI_DAG_H

/** Get the log files from an entire set of DAGs, dealing with
    DAG paths if necessary.
	@param dagFiles: all of the DAG files we're using.
	@param useDagDir: run DAGs in directories from DAG file paths 
               if true
	@param logFiles: a StringList to recieve the log file names.
	@param errMsg: a MyString to receive any error message.
	@return true if successful, false otherwise
*/ 
bool GetLogFiles(/* const */ StringList &dagFiles, bool useDagDir,
			StringList &condorLogFiles, StringList &storkLogFiles,
			MyString &errMsg);

/** Determine whether any log files are on NFS, and this is configured
	to be an error.
	@param condorLogFiles: a list of the Condor log files
	@param storkLogFiles: a list of the Stork log files
	@return true iff a log file is on NFS, and this is configured to be
		a fatal error (as opposed to a warning)
*/
bool LogFileNfsError(/* const */ StringList &condorLogFiles,
			/* const */ StringList &storkLogFiles);

/** Get the configuration file, if any, specified by the given list of
	DAG files.  If more than one DAG file specifies a configuration file,
	they must specify the same file.
	@param dagFiles: the list of DAG files
	@param useDagDir: run DAGs in directories from DAG file paths 
			if true
	@param configFile: holds the path to the config file; if a config
			file is specified on the command line, configFile should
			be set to that value before this method is called; the
			value of configFile will be changed if it's not already
			set and the DAG file(s) specify a configuration file
	@param errMsg: a MyString to receive any error message
	@return true if the operation succeeded; otherwise false
*/
bool GetConfigFile(/* const */ StringList &dagFiles, bool useDagDir,
			MyString &configFile, MyString &errMsg);

/** Make the given path into an absolute path, if it is not already.
	@param filePath: the path to make absolute (filePath is changed)
	@param errMsg: a MyString to receive any error message.
	@return true if the operation succeeded; otherwise false
*/
bool MakePathAbsolute(MyString &filePath, MyString &errMsg);

#endif /* #ifndef DAGMAN_MULTI_DAG_H */
