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
#include "dagman_recursive_submit.h"
#include "MyString.h"
#include "tmp_dir.h"

//---------------------------------------------------------------------------
/** Run condor_submit_dag on the given DAG file.
	@param opts: the condor_submit_dag options
	@param dagFile: the DAG file to process
	@param directory: the directory from which the DAG file should
		be processed (ignored if NULL)
	@return 0 if successful, 1 if failed
*/
int
runSubmitDag( const SubmitDagOptions &opts, const char *dagFile,
			const char *directory )
{
	int result = 0;

		// Change to the appropriate directory if necessary.
	TmpDir tmpDir;
	MyString errMsg;
	if ( directory ) {
		if ( !tmpDir.Cd2TmpDir( directory, errMsg ) ) {
			fprintf( stderr, "Error (%s) changing to node directory\n",
						errMsg.Value() );
			result = 1;
			return result;
		}
	}

		// Build up the command line for the recursive run of
		// condor_submit_dag.  We need -no_submit so we don't
		// actually run the subdag now; we need -update_submit
		// so the lower-level .condor.sub file will get
		// updated, in case it came from an earlier version
		// of condor_submit_dag.
	MyString cmdLine = "condor_submit_dag -no_submit -update_submit ";

		// Add in arguments we're passing along.
	if ( opts.bVerbose ) {
		cmdLine += "-verbose ";
	}

	if ( opts.bForce ) {
		cmdLine += "-force ";
	}

	if ( opts.strNotification != "" ) {
		cmdLine += MyString( "-notification " ) +
					opts.strNotification.Value() + " ";
	}

	if ( opts.strDagmanPath != "" ) {
		cmdLine += MyString( "-dagman " ) +
				opts.strDagmanPath.Value() + " ";
	}

	cmdLine += MyString( "-debug " ) + opts.iDebugLevel + " ";

	if ( opts.bAllowLogError ) {
		cmdLine += "-allowlogerror ";
	}

	if ( opts.useDagDir ) {
		cmdLine += "-usedagdir ";
	}

	if ( opts.strDebugDir != "" ) {
		cmdLine += MyString( "-outfile_dir " ) + 
				opts.strDebugDir.Value() + " ";
	}

	cmdLine += MyString( "-oldrescue " ) + opts.oldRescue + " ";

	cmdLine += MyString( "-autorescue " ) + opts.autoRescue + " ";

	if ( opts.doRescueFrom != 0 ) {
		cmdLine += MyString( "-dorescuefrom " ) +
				opts.doRescueFrom + " ";
	}

	if ( opts.allowVerMismatch ) {
		cmdLine += "-allowver ";
	}

	if ( opts.importEnv ) {
		cmdLine += "-import_env ";
	}

	cmdLine += dagFile;

	dprintf( D_FULLDEBUG, "Recursive submit command: <%s>\n",
				cmdLine.Value() );

		// Now actually run the command.
	int retval = system( cmdLine.Value() );
	if ( retval != 0 ) {
		dprintf( D_ALWAYS, "ERROR: condor_submit failed; aborting.\n" );
		result = 1;
	}

		// Change back to the directory we started from.
	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		dprintf( D_ALWAYS, "Error (%s) changing back to original directory\n",
					errMsg.Value() );
	}

	return result;
}
