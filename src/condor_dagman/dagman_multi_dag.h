/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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

#endif /* #ifndef DAGMAN_MULTI_DAG_H */
