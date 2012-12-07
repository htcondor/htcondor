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
#include "condor_arglist.h"
#include "my_popen.h"

//---------------------------------------------------------------------------
/** Run condor_submit_dag on the given DAG file.
	@param opts: the condor_submit_dag options
	@param dagFile: the DAG file to process
	@param directory: the directory from which the DAG file should
		be processed (ignored if NULL)
	@param isRetry: whether this is a retry of a sub-DAG node
	@return 0 if successful, 1 if failed
*/
int
runSubmitDag( const SubmitDagDeepOptions &deepOpts,
			const char *dagFile, const char *directory, bool isRetry )
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
	ArgList args;
	args.AppendArg( "condor_submit_dag" );
	args.AppendArg( "-no_submit" );
	args.AppendArg( "-update_submit" );

		// Add in arguments we're passing along.
	if ( deepOpts.bVerbose ) {
		args.AppendArg( "-verbose" );
	}

	if ( deepOpts.bForce && !isRetry ) {
		args.AppendArg( "-force" );
	}

	if (deepOpts.strNotification != "" ) {
		args.AppendArg( "-notification" );
		if(deepOpts.suppress_notification) {
			args.AppendArg( "never" );
		} else { 
			args.AppendArg( deepOpts.strNotification.Value() );
		}
	}

	if ( deepOpts.strDagmanPath != "" ) {
		args.AppendArg( "-dagman" );
		args.AppendArg( deepOpts.strDagmanPath.Value() );
	}

	if ( deepOpts.bAllowLogError ) {
		args.AppendArg( "-allowlogerror" );
	}

	if ( deepOpts.useDagDir ) {
		args.AppendArg( "-usedagdir" );
	}

	if ( deepOpts.strOutfileDir != "" ) {
		args.AppendArg( "-outfile_dir" );
		args.AppendArg( deepOpts.strOutfileDir.Value() );
	}

	args.AppendArg( "-autorescue" );
	args.AppendArg( deepOpts.autoRescue );

	if ( deepOpts.doRescueFrom != 0 ) {
		args.AppendArg( "-dorescuefrom" );
		args.AppendArg( deepOpts.doRescueFrom );
	}

	if ( deepOpts.allowVerMismatch ) {
		args.AppendArg( "-allowver" );
	}

	if ( deepOpts.importEnv ) {
		args.AppendArg( "-import_env" );
	}

	if ( deepOpts.recurse ) {
		args.AppendArg( "-do_recurse" );
	}

	if ( deepOpts.updateSubmit ) {
		args.AppendArg( "-update_submit" );
	}

	if( deepOpts.priority != 0) {
		args.AppendArg( "-Priority" );
		args.AppendArg( deepOpts.priority );
	}

	if( !deepOpts.always_use_node_log ) {
		args.AppendArg( "-dont_use_default_node_log" );
	}

	if( deepOpts.suppress_notification ) {
		args.AppendArg( "-suppress_notification" );
	} else {
		args.AppendArg( "-dont_suppress_notification" );
	}

	args.AppendArg( dagFile );

	MyString cmdLine;
	args.GetArgsStringForDisplay( &cmdLine );
	dprintf( D_ALWAYS, "Recursive submit command: <%s>\n",
				cmdLine.Value() );

		// Now actually run the command.
	int retval = my_system( args );
	if ( retval != 0 ) {
		dprintf( D_ALWAYS, "ERROR: condor_submit_dag -no_submit "
					"failed on DAG file %s.\n", dagFile );
		result = 1;
	}

		// Change back to the directory we started from.
	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		dprintf( D_ALWAYS, "Error (%s) changing back to original directory\n",
					errMsg.Value() );
	}

	return result;
}
