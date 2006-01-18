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

#include "condor_common.h"
#include "read_multiple_logs.h"
#include "basename.h"
#include "tmp_dir.h"

// Just so we can link in the ReadMultipleUserLogs class.
MULTI_LOG_HASH_INSTANCE;

//-------------------------------------------------------------------------
static void
AppendError(MyString &errMsg, const MyString &newError)
{
	if ( errMsg != "" ) errMsg += "; ";
	errMsg += newError;
}

//-------------------------------------------------------------------------
bool
GetLogFiles(/* const */ StringList &dagFiles, bool useDagDir, 
			StringList &condorLogFiles, StringList &storkLogFiles,
			MyString &errMsg)
{
	bool		result = true;

	TmpDir		dagDir;

	dagFiles.rewind();
	char *dagFile;
	while ( (dagFile = dagFiles.next()) != NULL ) {

		const char *	file;
		if ( useDagDir ) {
			MyString	tmpErrMsg;
			if ( !dagDir.Cd2TmpDirFile( dagFile, tmpErrMsg ) ) {
				AppendError( errMsg,
						MyString("Unable to change to DAG directory ") +
						tmpErrMsg );
				return false;
			}
			file = condor_basename( dagFile );
		} else {
			file = dagFile;
		}

			// Note: this returns absolute paths to the log files.
		MyString msg = MultiLogFiles::getJobLogsFromSubmitFiles(
				file, "job", "dir", condorLogFiles);
		if ( msg != "" ) {
			AppendError( errMsg,
					MyString("Failed to locate Condor job log files: ") +
					msg );
			result = false;
		}

			// Note: this returns absolute paths to the log files.
		msg = MultiLogFiles::getJobLogsFromSubmitFiles(
				file, "data", "dir", storkLogFiles);
		if ( msg != "" ) {
#if 1
				// Get rid of this after 6.7.15 release.  wenger 2006-01-11.
			dprintf( D_ALWAYS, "Warning: failed to locate Stork job log "
					"files (%s) but continuing anyhow\n", msg.Value() );
#else
			AppendError( errMsg,
					MyString("Failed to locate Stork job log files: ") +
					msg );
			result = false;
#endif
		}

		MyString	tmpErrMsg;
		if ( !dagDir.Cd2MainDir( tmpErrMsg ) ) {
			AppendError( errMsg,
					MyString("Unable to change to original directory ") +
					tmpErrMsg );
			result = false;
		}

	}

	return result;
}
