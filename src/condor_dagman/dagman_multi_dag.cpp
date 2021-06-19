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

#if 0 // Moved to dagman_utils
//-------------------------------------------------------------------------
static void
AppendError(MyString &errMsg, const MyString &newError)
{
	if ( errMsg != "" ) errMsg += "; ";
	errMsg += newError;
}

//-------------------------------------------------------------------------
bool
GetConfigAndAttrs( /* const */ std::list<std::string> &dagFiles, bool useDagDir, 
			MyString &configFile, std::list<std::string> &attrLines, MyString &errMsg )
{
	bool		result = true;

		// Note: destructor will change back to original directory.
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
			std::string	tmpErrMsg;
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

		std::list<std::string>		configFiles;

			// Note: destructor will close file.
		MultiLogFiles::FileReader reader;
		errMsg = reader.Open( newDagFile );
		if ( errMsg != "" ) {
			return false;
		}

		MyString logicalLine;
		while ( reader.NextLogicalLine( logicalLine ) ) {
			if ( logicalLine != "" ) {
				std::list<std::string> tokens( logicalLine.Value(), " \t" );
				tokens.rewind();

				const char *firstToken = tokens.next();
				if ( !strcasecmp( firstToken, "config" ) ) {

						// Get the value.
					const char *newValue = tokens.next();
					if ( !newValue || !strcmp( newValue, "" ) ) {
						AppendError( errMsg, "Improperly-formatted "
									"file: value missing after keyword "
									"CONFIG" );
			    		result = false;
					} else {

							// Add the value we just found to the config
							// files list (if it's not already in the
							// list -- we don't want duplicates).
						configFiles.rewind();
						char *existingValue;
						bool alreadyInList = false;
						while ( ( existingValue = configFiles.next() ) ) {
							if ( !strcmp( existingValue, newValue ) ) {
								alreadyInList = true;
							}
						}

						if ( !alreadyInList ) {
								// Note: append copies the string here.
							configFiles.append( newValue );
						}
					}

					//some DAG commands are needed for condor_submit_dag, too...
				} else if ( !strcasecmp( firstToken, "SET_JOB_ATTR" ) ) {
						// Strip of DAGMan-specific command name; the
						// rest we pass to the submit file.
					logicalLine.replaceString( "SET_JOB_ATTR", "" );
					logicalLine.trim();
					if ( logicalLine == "" ) {
						AppendError( errMsg, "Improperly-formatted "
									"file: value missing after keyword "
									"SET_JOB_ATTR" );
						result = false;
					} else {
						attrLines.append( logicalLine.Value() );
					}
				}
			}
		}
	
		reader.Close();

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
		std::string	tmpErrMsg;
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
			formatstr( errMsg, "condor_getcwd() failed with errno %d (%s) at %s:%d",
			           errno, strerror(errno), __FILE__, __LINE__ );
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
			// Unlink here to be safe on Windows.
		tolerant_unlink( newName.Value() );
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

//-------------------------------------------------------------------------
void
tolerant_unlink( const char *pathname )
{
	if ( unlink( pathname ) != 0 ) {
		if ( errno == ENOENT ) {
			dprintf( D_SYSCALLS,
						"Warning: failure (%d (%s)) attempting to unlink file %s\n",
						errno, strerror( errno ), pathname );
		} else {
			dprintf( D_ALWAYS,
						"Error (%d (%s)) attempting to unlink file %s\n",
						errno, strerror( errno ), pathname );

		}
	}
}
#endif
