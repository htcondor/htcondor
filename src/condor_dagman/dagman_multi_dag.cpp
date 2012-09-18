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


#include "condor_common.h"
#include "condor_config.h"
#include "read_multiple_logs.h"
#include "basename.h"
#include "tmp_dir.h"
#include "dagman_multi_dag.h"
#include "condor_getcwd.h"

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
GetConfigFile(/* const */ StringList &dagFiles, bool useDagDir, 
			MyString &configFile, MyString &errMsg)
{
	bool		result = true;

	TmpDir		dagDir;

	dagFiles.rewind();
	char *dagFile;
	while ( (dagFile = dagFiles.next()) != NULL ) {

			//
			// Change to the DAG file's directory if necessary, and
			// get the filename we need to use for it from that directory.
			//
		const char *	newDagFile;
		if ( useDagDir ) {
			MyString	tmpErrMsg;
			if ( !dagDir.Cd2TmpDirFile( dagFile, tmpErrMsg ) ) {
				AppendError( errMsg,
						MyString("Unable to change to DAG directory ") +
						tmpErrMsg );
				return false;
			}
			newDagFile = condor_basename( dagFile );
		} else {
			newDagFile = dagFile;
		}

			//
			// Get the list of config files from the current DAG file.
			//
		StringList		configFiles;
		MyString msg = MultiLogFiles::getValuesFromFile( newDagFile, "config",
					configFiles);
		if ( msg != "" ) {
			AppendError( errMsg,
					MyString("Failed to locate Condor job log files: ") +
					msg );
			result = false;
		}

			//
			// Check the specified config file(s) against whatever we
			// currently have, setting the config file if it hasn't
			// been set yet, flagging an error if config files conflict.
			//
		configFiles.rewind();
		char *		cfgFile;
		while ( (cfgFile = configFiles.next()) ) {
			MyString	cfgFileMS = cfgFile;
			MyString	tmpErrMsg;
			if ( MakePathAbsolute( cfgFileMS, tmpErrMsg ) ) {
				if ( configFile == "" ) {
					configFile = cfgFileMS;
				} else if ( configFile != cfgFileMS ) {
					AppendError( errMsg, MyString("Conflicting DAGMan ") +
								"config files specified: " + configFile +
								" and " + cfgFileMS );
					result = false;
				}
			} else {
				AppendError( errMsg, tmpErrMsg );
				result = false;
			}
		}

			//
			// Go back to our original directory.
			//
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

//-------------------------------------------------------------------------
bool
MakePathAbsolute(MyString &filePath, MyString &errMsg)
{
	bool		result = true;

	if ( !fullpath( filePath.Value() ) ) {
		MyString    currentDir;
		if ( ! condor_getcwd( currentDir ) ) {
			errMsg = MyString( "condor_getcwd() failed with errno " ) +
						errno + " (" + strerror(errno) + ") at " + __FILE__
						+ ":" + __LINE__;
			result = false;
		}

		filePath = currentDir + DIR_DELIM_STRING + filePath;
	}

	return result;
}

//-------------------------------------------------------------------------
int
FindLastRescueDagNum( const char *primaryDagFile, bool multiDags,
			int maxRescueDagNum )
{
	int lastRescue = 0;

	for ( int test = 1; test <= maxRescueDagNum; test++ ) {
		MyString testName = RescueDagName( primaryDagFile, multiDags,
					test );
		if ( access( testName.Value(), F_OK ) == 0 ) {
			if ( test > lastRescue + 1 ) {
					// This should probably be a fatal error if
					// DAGMAN_USE_STRICT is set, but I'm avoiding
					// that for now because the fact that this code
					// is used in both condor_dagman and condor_submit_dag
					// makes that harder to implement. wenger 2011-01-28
				dprintf( D_ALWAYS, "Warning: found rescue DAG "
							"number %d, but not rescue DAG number %d\n",
							test, test - 1);
			}
			lastRescue = test;
		}
	}
	
	if ( lastRescue >= maxRescueDagNum ) {
		dprintf( D_ALWAYS,
					"Warning: FindLastRescueDagNum() hit maximum "
					"rescue DAG number: %d\n", maxRescueDagNum );
	}

	return lastRescue;
}

//-------------------------------------------------------------------------
MyString
RescueDagName(const char *primaryDagFile, bool multiDags,
			int rescueDagNum)
{
	ASSERT( rescueDagNum >= 1 );

	MyString fileName(primaryDagFile);
	if ( multiDags ) {
		fileName += "_multi";
	}
	fileName += ".rescue";
	fileName.formatstr_cat( "%.3d", rescueDagNum );

	return fileName;
}

//-------------------------------------------------------------------------
void
RenameRescueDagsAfter(const char *primaryDagFile, bool multiDags,
			int rescueDagNum, int maxRescueDagNum)
{
		// Need to allow 0 here so condor_submit_dag -f can rename all
		// rescue DAGs.
	ASSERT( rescueDagNum >= 0 );

	dprintf( D_ALWAYS, "Renaming rescue DAGs newer than number %d\n",
				rescueDagNum );

	int firstToDelete = rescueDagNum + 1;
	int lastToDelete = FindLastRescueDagNum( primaryDagFile, multiDags,
				maxRescueDagNum );

	for ( int rescueNum = firstToDelete; rescueNum <= lastToDelete;
				rescueNum++ ) {
		MyString rescueDagName = RescueDagName( primaryDagFile, multiDags,
					rescueNum );
		dprintf( D_ALWAYS, "Renaming %s\n", rescueDagName.Value() );
		MyString newName = rescueDagName + ".old";
		if ( rename( rescueDagName.Value(), newName.Value() ) != 0 ) {
			EXCEPT( "Fatal error: unable to rename old rescue file "
						"%s: error %d (%s)\n", rescueDagName.Value(),
						errno, strerror( errno ) );
		}
	}
}

//-------------------------------------------------------------------------
MyString
HaltFileName( const MyString &primaryDagFile )
{
	MyString haltFile = primaryDagFile + ".halt";

	return haltFile;
}
