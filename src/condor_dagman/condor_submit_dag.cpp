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
#include "../condor_utils/dagman_utils.h"
#include "which.h"
#include "condor_distribution.h"
#include "condor_config.h"
#include "env.h"
#include "basename.h"
#include "read_multiple_logs.h"
#include "condor_getcwd.h"
#include "condor_string.h" // for getline()
#include "condor_version.h"
#include "tmp_dir.h"
#include "my_popen.h"
#include "setenv.h"
#include "condor_attributes.h"


int printUsage(int iExitCode=1); // NOTE: printUsage calls exit(1), so it doesn't return
void parseCommandLine(SubmitDagDeepOptions &deepOpts,
			SubmitDagShallowOptions &shallowOpts, int argc,
			const char * const argv[]);
bool parsePreservedArgs(const std::string &strArg, int &argNum, int argc,
			const char * const argv[], SubmitDagShallowOptions &shallowOpts);
int doRecursionNew( SubmitDagDeepOptions &deepOpts,
			SubmitDagShallowOptions &shallowOpts );
int parseJobOrDagLine( const char *dagLine, dag_tokener &tokens,
			const char *fileType, const char *&submitOrDagFile,
			const char *&directory );
			int getOldSubmitFlags( SubmitDagShallowOptions &shallowOpts );
int parseArgumentsLine( const std::string &subLine,
			SubmitDagShallowOptions &shallowOpts );
int submitDag( SubmitDagShallowOptions &shallowOpts );

// Initial DagmanUtils object
DagmanUtils dagmanUtils;

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	printf("\n");

		// Set up the dprintf stuff to write to stderr, so that HTCondor
		// libraries which use it will write to the right place...
	dprintf_set_tool_debug("TOOL", 0);
	set_debug_flags(NULL, D_ALWAYS | D_NOHEADER);
	set_priv_initialize(); // allow uid switching if root
	config();

		// Save submit append lines from DAG file here (see gittrac #5107).
	std::list<std::string> dagFileAttrLines;

		// Load command-line arguments into the deepOpts and shallowOpts
		// structures.
	SubmitDagDeepOptions deepOpts;
	SubmitDagShallowOptions shallowOpts;

		// We're setting strScheddDaemonAdFile and strScheddAddressFile
		// here so that the classad updating feature (see gittrac #1782)
		// works properly.  The problem is that the schedd daemon ad and
		// address files are normally defined relative to the $LOG value.
		// Because we specify a different log directory on the condor_dagman
		// command line, if we don't set the values here, condor_dagman
		// won't be able to find those files when it tries to communicate
		/// with the schedd.  wenger 2013-03-11
	char * schedd_daemon_ad_file = param( "SCHEDD_DAEMON_AD_FILE" );
	shallowOpts.strScheddDaemonAdFile = schedd_daemon_ad_file;
	if (schedd_daemon_ad_file) free(schedd_daemon_ad_file);

	char * schedd_address_file = param( "SCHEDD_ADDRESS_FILE" );
	shallowOpts.strScheddAddressFile = schedd_address_file;
	if (schedd_address_file) free(schedd_address_file);

	parseCommandLine(deepOpts, shallowOpts, argc, argv);

	int tmpResult;

		// Recursively run ourself on nested DAGs.  We need to do this
		// depth-first so all of the lower-level .condor.sub files already
		// exist when we check for log files.
	if ( deepOpts.recurse ) {
		if ( param_boolean( "DAGMAN_USE_OLD_DAG_READER", false ) ) {
			fprintf( stderr, "Warning: DAGMAN_USE_OLD_DAG_READER "
						"is no longer supported\n" );
		}
		tmpResult = doRecursionNew( deepOpts, shallowOpts );
		if ( tmpResult != 0) {
			fprintf( stderr, "Recursive submit(s) failed; exiting without "
						"attempting top-level submit\n" );
			return tmpResult;
		}
	}
	
		// Further work to get the shallowOpts structure set up properly.
	tmpResult = dagmanUtils.setUpOptions( deepOpts, shallowOpts, dagFileAttrLines );
	if ( tmpResult != 0 ) return tmpResult;

		// Check whether the output files already exist; if so, we may
		// abort depending on the -f flag and whether we're running
		// a rescue DAG.
	if ( !dagmanUtils.ensureOutputFilesExist( deepOpts, shallowOpts ) ) {
		exit( 1 );
	}

		// Make sure that all node jobs have log files, the files
		// aren't on NFS, etc.

		// Note that this MUST come after recursion, otherwise we'd
		// pass down the "preserved" values from the current .condor.sub
		// file.
	if ( deepOpts.updateSubmit ) {
		tmpResult = getOldSubmitFlags( shallowOpts );
		if ( tmpResult != 0 ) return tmpResult;
	}

		// Write the actual submit file for DAGMan.
	if ( !dagmanUtils.writeSubmitFile( deepOpts, shallowOpts, dagFileAttrLines ) ) {
		exit( 1 );
	}

	return submitDag( shallowOpts );
}

//---------------------------------------------------------------------------
/** Recursively call condor_submit_dag on nested DAGs.
	@param deepOpts: the condor_submit_dag deep options
	@return 0 if successful, 1 if failed
*/
int
doRecursionNew( SubmitDagDeepOptions &deepOpts,
			SubmitDagShallowOptions &shallowOpts )
{
	int result = 0;

		// Go through all DAG files specified on the command line...
	for (auto dagfile_it = shallowOpts.dagFiles.begin(); dagfile_it != shallowOpts.dagFiles.end(); ++dagfile_it) {

			// Get logical lines from this DAG file.
		MultiLogFiles::FileReader reader;
		std::string errMsg = reader.Open( *dagfile_it );
		if ( errMsg != "" ) {
			fprintf( stderr, "Error reading DAG file: %s\n",
						errMsg.c_str() );
			return 1;
		}


			// Find and parse JOB and SUBDAG lines.
		std::string dagLine;
		while ( reader.NextLogicalLine( dagLine ) ) {
			dag_tokener tokens( dagLine.c_str() );
			tokens.rewind();
			const char *first = tokens.next();

			if ( first && !strcasecmp( first, "JOB" ) ) {

					// Get the submit file and directory from the DAG
					// file line.
				const char *subFile;
				const char *directory;
				if ( parseJobOrDagLine( dagLine.c_str(), tokens, "submit",
							subFile, directory ) != 0 ) {
					return 1;
				}

					// Now figure out whether JOB line is a nested DAG.
				std::string submitFile( subFile );

					// If submit file ends in ".condor.sub", we assume it
					// refers to a sub-DAG.
				std::size_t start = submitFile.find( DAG_SUBMIT_FILE_SUFFIX );
				if ( start != std::string::npos &&
							start + (int)strlen( DAG_SUBMIT_FILE_SUFFIX) ==
							submitFile.length() ) {

						// Change submit file name to DAG file name.
					submitFile.replace( submitFile.find(DAG_SUBMIT_FILE_SUFFIX), strlen(DAG_SUBMIT_FILE_SUFFIX), "" );

						// Now run condor_submit_dag on the DAG file.
					if ( dagmanUtils.runSubmitDag( deepOpts, submitFile.c_str(),
								directory, shallowOpts.priority,
								false ) != 0 ) {
						result = 1;
					}
				}

			} else if ( first && !strcasecmp( first, "SUBDAG" ) ) {

				const char *inlineOrExt = tokens.next();
				if ( strcasecmp( inlineOrExt, "EXTERNAL" ) ) {
					fprintf( stderr, "ERROR: only SUBDAG EXTERNAL is supported "
								"at this time (line: <%s>)\n", dagLine.c_str() );
					return 1;
				}

					// Get the nested DAG file and directory from the DAG
					// file line.
				const char *nestedDagFile;
				const char *directory;
				if ( parseJobOrDagLine( dagLine.c_str(), tokens, "DAG",
							nestedDagFile, directory ) != 0 ) {
					return 1;
				}

					// Now run condor_submit_dag on the DAG file.
				if ( dagmanUtils.runSubmitDag( deepOpts, nestedDagFile, directory,
							shallowOpts.priority, false ) != 0 ) {
					result = 1;
				}
			}
		}

		reader.Close();
	}

	return result;
}

//---------------------------------------------------------------------------
/** Parse a JOB or SUBDAG line from a DAG file.
	@param dagLine: the line we're parsing
	@param tokens: tokens of this line
	@param fileType: "submit" or "DAG" (to be used in error message)
	@param submitOrDagFile: if successful, this will point to submit or
		nested DAG file name
	@param directory: if successful, this will point to directory (NULL
		if not specified)
	@return 0 if successful, 1 if failed
*/
int
parseJobOrDagLine( const char *dagLine, dag_tokener &tokens,
			const char *fileType, const char *&submitOrDagFile,
			const char *&directory )
{
	const char *nodeName = tokens.next();
	if ( !nodeName) {
		fprintf( stderr, "No node name specified in line: <%s>\n", dagLine );
		return 1;
	}

	submitOrDagFile = tokens.next();
	if ( !submitOrDagFile ) {
		fprintf( stderr, "No %s file specified in "
					"line: <%s>\n", fileType, dagLine );
		return 1;
	}

	directory = NULL;
	const char *dirKeyword = tokens.next();
	if ( dirKeyword && !strcasecmp( dirKeyword, "DIR" ) ) {
		directory = tokens.next();
		if ( !directory ) {
			fprintf( stderr, "No directory specified in "
						"line: <%s>\n", dagLine );
			return 1;
		}
	}

	return 0;
}

//---------------------------------------------------------------------------
/** Submit the DAGMan submit file unless the -no_submit option was given.
	@param shallowOpts: the condor_submit_dag shallow options
	@return 0 if successful, 1 if failed
*/
int
submitDag( SubmitDagShallowOptions &shallowOpts )
{
	printf("-----------------------------------------------------------------------\n");
	printf("File for submitting this DAG to HTCondor           : %s\n", 
			shallowOpts.strSubFile.c_str());
	printf("Log of DAGMan debugging messages                 : %s\n",
		   	shallowOpts.strDebugLog.c_str());
	printf("Log of HTCondor library output                     : %s\n", 
			shallowOpts.strLibOut.c_str());
	printf("Log of HTCondor library error messages             : %s\n", 
			shallowOpts.strLibErr.c_str());
	printf("Log of the life of condor_dagman itself          : %s\n",
		   	shallowOpts.strSchedLog.c_str());
	printf("\n");

	if (shallowOpts.bSubmit)
	{
		ArgList args;
		args.AppendArg( "condor_submit" );
		if( shallowOpts.strRemoteSchedd != "" ) {
			args.AppendArg( "-r" );
			args.AppendArg( shallowOpts.strRemoteSchedd );
		}
		args.AppendArg( shallowOpts.strSubFile );

			// It is important to set the destination Schedd before
			// calling condor_submit, otherwise it may submit to the
			// wrong Schedd.
			//
			// my_system() has a variant that takes an Env.
			// Unfortunately, it results in an execve and no path
			// searching, which makes the relative path to
			// "condor_submit" above not work. Instead, we'll set the
			// env before execvp is called. It may be more correct to
			// fix my_system to inject the Env after the fork() and
			// before the execvp().
		if ( shallowOpts.strScheddDaemonAdFile != "" ) {
			SetEnv("_CONDOR_SCHEDD_DAEMON_AD_FILE",
				   shallowOpts.strScheddDaemonAdFile.c_str());
		}
		if ( shallowOpts.strScheddAddressFile != "" ) {
			SetEnv("_CONDOR_SCHEDD_ADDRESS_FILE",
				   shallowOpts.strScheddAddressFile.c_str());
		}

		int retval = my_system( args );
		if( retval != 0 ) {
			fprintf( stderr, "ERROR: condor_submit failed; aborting.\n" );
			return 1;
		}
	}
	else
	{
		printf("-no_submit given, not submitting DAG to HTCondor.  "
					"You can do this with:\n");
		printf("\"condor_submit %s\"\n", shallowOpts.strSubFile.c_str());
	}
	printf("-----------------------------------------------------------------------\n");

	return 0;
}

//---------------------------------------------------------------------------
/** Get the command-line options we want to preserve from the .condor.sub
    file we're overwriting, and plug them into the shallowOpts structure.
	Note that it's *not* an error for the .condor.sub file to not exist.
	@param shallowOpts: the condor_submit_dag shallow options
	@return 0 if successful, 1 if failed
*/
int
getOldSubmitFlags(SubmitDagShallowOptions &shallowOpts)
{
		// It's not an error for the submit file to not exist.
	if ( dagmanUtils.fileExists( shallowOpts.strSubFile ) ) {
		MultiLogFiles::FileReader reader;
		std::string error = reader.Open( shallowOpts.strSubFile );
		if ( error != "" ) {
			fprintf( stderr, "Error reading submit file: %s\n",
						error.c_str() );
			return 1;
		}

		std::string subLine;
		while ( reader.NextLogicalLine( subLine ) ) {
			// Initialize list of tokens from subLine
			std::list<std::string> tokens;
			trim( subLine );
			Tokenize( subLine );
			while( const char* token = GetNextToken( " \t", true ) ) {
				tokens.emplace_back( token );
			}

			const char *first = tokens.front().c_str();
			if ( first && !strcasecmp( first, "arguments" ) ) {
				if ( parseArgumentsLine( subLine, shallowOpts ) != 0 ) {
					return 1;
				}
			}
		}

		reader.Close();
	}

	return 0;
}

//---------------------------------------------------------------------------
/** Parse the arguments line of an existing .condor.sub file, extracing
    the arguments we want to preserve when updating the .condor.sub file.
	@param subLine: the arguments line from the .condor.sub file
	@param shallowOpts: the condor_submit_dag shallow options
	@return 0 if successful, 1 if failed
*/
int
parseArgumentsLine( const std::string &subLine,
			SubmitDagShallowOptions &shallowOpts )
{
	const char *line = subLine.c_str();
	const char *start = strchr( line, '"' );
	const char *end = strrchr( line, '"' );

	std::string arguments;
	if ( start && end ) {
		arguments = subLine.substr( start - line, 1 + end - start );
	} else {
		fprintf( stderr, "Missing quotes in arguments line: <%s>\n",
					subLine.c_str() );
		return 1;
	}

	ArgList arglist;
	std::string error;
	if ( !arglist.AppendArgsV2Quoted( arguments.c_str(),
				error ) ) {
		fprintf( stderr, "Error parsing arguments: %s\n", error.c_str() );
		return 1;
	}

	for ( int argNum = 0; argNum < arglist.Count(); argNum++ ) {
		std::string strArg = arglist.GetArg( argNum );
		lower_case(strArg);
		char **args = arglist.GetStringArray();
		(void)parsePreservedArgs( strArg, argNum, arglist.Count(),
					args, shallowOpts);
		deleteStringArray(args);
	}

	return 0;
}

//---------------------------------------------------------------------------
void
parseCommandLine(SubmitDagDeepOptions &deepOpts,
			SubmitDagShallowOptions &shallowOpts, int argc,
			const char * const argv[])
{
	for (int iArg = 1; iArg < argc; iArg++)
	{
		std::string strArg = argv[iArg];

		if (strArg[0] != '-')
		{
				// We assume an argument without a leading hyphen is
				// a DAG file name.
			shallowOpts.dagFiles.emplace_back(strArg.c_str());
			if ( shallowOpts.primaryDagFile == "" ) {
				shallowOpts.primaryDagFile = strArg;
			}
		}
		else if (shallowOpts.primaryDagFile != "")
		{
				// Disallow hyphen args after DAG file name(s).
			printf("ERROR: no arguments allowed after DAG file name(s)\n");
			printUsage();
		}
		else
		{
			lower_case(strArg);

			// Note: in checking the argument names here, we only check for
			// as much of the full name as we need to unambiguously define
			// the argument.
			if (strArg.find("-no_s") != std::string::npos) // -no_submit
			{
				shallowOpts.bSubmit = false;
			}
			else if (strArg.find("-vers") != std::string::npos) // -version
			{
				printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
				exit( 0 );
			}
			else if (strArg.find("-help") != std::string::npos || strArg.find("-h") != std::string::npos) // -help
			{
				printUsage(0);
			}
				// submit and stick to a specific schedd
			else if (strArg.find("-schedd-daemon-ad-file") != std::string::npos)
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-schedd-daemon-ad-file argument needs a value\n");
					printUsage();
				}
				shallowOpts.strScheddDaemonAdFile = argv[++iArg];
			}
				// submit and stick to a specific schedd
			else if (strArg.find("-schedd-address-file") != std::string::npos)
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-schedd-address-file argument needs a value\n");
					printUsage();
				}
				shallowOpts.strScheddAddressFile = argv[++iArg];
			}
			else if (strArg.find("-f") != std::string::npos) // -force
			{
				deepOpts.bForce = true;
			}
			else if (strArg.find("-not") != std::string::npos) // -notification
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-notification argument needs a value\n");
					printUsage();
				}
				deepOpts.strNotification = argv[++iArg];
			}
			else if (strArg.find("-r") != std::string::npos) // submit to remote schedd
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-r argument needs a value\n");
					printUsage();
				}
				shallowOpts.strRemoteSchedd = argv[++iArg];
			}
			else if (strArg.find("-dagman") != std::string::npos)
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-dagman argument needs a value\n");
					printUsage();
				}
				deepOpts.strDagmanPath = argv[++iArg];
			}
			else if (strArg.find("-de") != std::string::npos) // -debug
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-debug argument needs a value\n");
					printUsage();
				}
				shallowOpts.iDebugLevel = atoi(argv[++iArg]);
			}
			else if (strArg.find("-noev") != std::string::npos) // -noeventchecks
			{
				printf( "Warning: -NoEventChecks is ignored; please use "
							"the DAGMAN_ALLOW_EVENTS config parameter instead\n");
			}
			else if (strArg.find("-allowlog") != std::string::npos) // -allowlogerror
			{
				fprintf( stderr, "Warning: -AllowLogError is no longer supported\n" );
			}
			else if (strArg.find("-use") != std::string::npos) // -usedagdir
			{
				deepOpts.useDagDir = true;
			}
			else if (strArg.find("-out") != std::string::npos) // -outfile_dir
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-outfile_dir argument needs a value\n");
					printUsage();
				}
				deepOpts.strOutfileDir = argv[++iArg];
			}
			else if (strArg.find("-con") != std::string::npos) // -config
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-config argument needs a value\n");
					printUsage();
				}
				shallowOpts.strConfigFile = argv[++iArg];
					// Internally we deal with all configuration file paths
					// as full paths, to make it easier to determine whether
					// several paths point to the same file.
				std::string	errMsg;
				if (!dagmanUtils.MakePathAbsolute(shallowOpts.strConfigFile, errMsg)) {
					fprintf( stderr, "%s\n", errMsg.c_str() );
   					exit( 1 );
				}
			}
			else if (strArg.find("-app") != std::string::npos) // -append
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-append argument needs a value\n");
					printUsage();
				}
				shallowOpts.appendLines.emplace_back(argv[++iArg]);
			}
			else if (strArg.find("-bat") != std::string::npos) // -batch-name
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-batch-name argument needs a value\n");
					printUsage();
				}
				deepOpts.batchName = argv[++iArg];
				trim_quotes(deepOpts.batchName, "\""); // trim "" if any
			}
			else if (strArg.find("-insert") != std::string::npos) // -insert_sub_file
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-insert_sub_file argument needs a value\n");
					printUsage();
				}
				++iArg;
				if (shallowOpts.appendFile != "") {
					printf("Note: -insert_sub_file value (%s) overriding "
								"DAGMAN_INSERT_SUB_FILE setting (%s)\n",
								argv[iArg], shallowOpts.appendFile.c_str());
				}
				shallowOpts.appendFile = argv[iArg];
			}
			else if (strArg.find("-autor") != std::string::npos) // -autorescue
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-autorescue argument needs a value\n");
					printUsage();
				}
				deepOpts.autoRescue = (atoi(argv[++iArg]) != 0);
			}
			else if (strArg.find("-dores") != std::string::npos) // -dorescuefrom
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-dorescuefrom argument needs a value\n");
					printUsage();
				}
				deepOpts.doRescueFrom = atoi(argv[++iArg]);
			}
			else if (strArg.find("-allowver") != std::string::npos) // -AllowVersionMismatch
			{
				deepOpts.allowVerMismatch = true;
			}
			else if (strArg.find("-no_rec") != std::string::npos) // -no_recurse
			{
				deepOpts.recurse = false;
			}
			else if (strArg.find("-do_rec") != std::string::npos) // -do_recurse
			{
				deepOpts.recurse = true;
			}
			else if (strArg.find("-updat") != std::string::npos) // -update_submit
			{
				deepOpts.updateSubmit = true;
			}
			else if (strArg.find("-import_env") != std::string::npos) // -import_env
			{
				deepOpts.importEnv = true;
			}			     
			else if (strArg.find("-dumpr") != std::string::npos) // -DumpRescue
			{
				shallowOpts.dumpRescueDag = true;
			}
			else if (strArg.find("-valgrind") != std::string::npos) // -valgrind
			{
				shallowOpts.runValgrind = true;
			}
				// This must come last, so we can have other arguments
				// that start with -v.
			else if ( (strArg.find("-v") != std::string::npos) ) // -verbose
			{
				deepOpts.bVerbose = true;
			}
			else if ( (strArg.find("-dontalwaysrun") != std::string::npos) ) // DontAlwaysRunPost
			{
				if ( shallowOpts.bPostRunSet && shallowOpts.bPostRun ) {
					fprintf( stderr, "ERROR: -DontAlwaysRunPost and -AlwaysRunPost are both set!\n" );
					exit(1);
				}
				shallowOpts.bPostRunSet = true;
				shallowOpts.bPostRun = false;
			}
			else if ( (strArg.find("-alwaysrun") != std::string::npos) ) // AlwaysRunPost
			{
				if ( shallowOpts.bPostRunSet && !shallowOpts.bPostRun ) {
					fprintf( stderr, "ERROR: -DontAlwaysRunPost and -AlwaysRunPost are both set!\n" );
					exit(1);
				}
				shallowOpts.bPostRunSet = true;
				shallowOpts.bPostRun = true;
			}
			else if ( (strArg.find("-dont_use_default_node_log") != std::string::npos) )
			{
				fprintf( stderr, "Error: -dont_use_default_node_log is no longer allowed\n" );
				printUsage();
			}
			else if ( (strArg.find("-suppress_notification") != std::string::npos) )
			{
				deepOpts.suppress_notification = true;
			}
			else if ( (strArg.find("-dont_suppress_notification") != std::string::npos) )
			{
				deepOpts.suppress_notification = false;
			}
			else if( (strArg.find("-prio") != std::string::npos) ) // -priority
			{
				if(iArg + 1 >= argc) {
					fprintf(stderr, "-priority argument needs a value\n");
					printUsage();
				}
				shallowOpts.priority = atoi(argv[++iArg]);
			}
			else if ( (strArg.find("-dorecov") != std::string::npos) )
			{
				shallowOpts.doRecovery = true;
			}
			else if ( parsePreservedArgs( strArg, iArg, argc, argv,
						shallowOpts) )
			{
				// No-op here
			}
			else
			{
				fprintf( stderr, "ERROR: unknown option %s\n", strArg.c_str() );
				printUsage();
			}
		}
	}

	if (shallowOpts.primaryDagFile == "")
	{
		fprintf( stderr, "ERROR: no dag file specified; aborting.\n" );
		printUsage();
	}

	if (deepOpts.doRescueFrom < 0)
	{
		fprintf( stderr, "-dorescuefrom value must be non-negative; aborting.\n");
		printUsage();
	}
}

//---------------------------------------------------------------------------
/** Parse arguments that are to be preserved when updating a .condor.sub
	file.  If the given argument such an argument, parse it and update the
	shallowOpts structure accordingly.  (This function is meant to be called
	both when parsing "normal" command-line arguments, and when parsing the
	existing arguments line of a .condor.sub file we're overwriting.)
	@param strArg: the argument we're parsing
	@param argNum: the argument number of the current argument
	@param argc: the argument count (passed to get value for flag)
	@param argv: the argument vector (passed to get value for flag)
	@param shallowOpts: the condor_submit_dag shallow options
	@return true iff the argument vector contained any arguments
		processed by this function
*/
bool
parsePreservedArgs(const std::string &strArg, int &argNum, int argc,
			const char * const argv[], SubmitDagShallowOptions &shallowOpts)
{
	bool result = false;

	if (strArg.find("-maxi") != std::string::npos) // -maxidle
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxidle argument needs a value\n");
			printUsage();
		}
		shallowOpts.iMaxIdle = atoi(argv[++argNum]);
		result = true;
	}
	else if (strArg.find("-maxj") != std::string::npos) // -maxjobs
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxjobs argument needs a value\n");
			printUsage();
		}
		shallowOpts.iMaxJobs = atoi(argv[++argNum]);
		result = true;
	}
	else if (strArg.find("-maxpr") != std::string::npos) // -maxpre
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxpre argument needs a value\n");
			printUsage();
		}
		shallowOpts.iMaxPre = atoi(argv[++argNum]);
		result = true;
	}
	else if (strArg.find("-maxpo") != std::string::npos) // -maxpost
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxpost argument needs a value\n");
			printUsage();
		}
		shallowOpts.iMaxPost = atoi(argv[++argNum]);
		result = true;
	}

	return result;
}

//---------------------------------------------------------------------------
int printUsage(int iExitCode) 
{
    printf("Usage: condor_submit_dag [options] dag_file [dag_file_2 ... dag_file_n]\n");
    printf("  where dag_file1, etc., is the name of a DAG input file\n");
    printf("  and where [options] is one or more of:\n");
	printf("    -help               (print usage info and exit)\n");
	printf("    -version            (print version and exit)\n");
	printf("    -dagman <path>      (Full path to an alternate condor_dagman executable)\n");
    printf("    -no_submit          (DAG is not submitted to HTCondor)\n");
    printf("    -verbose            (Verbose error messages from condor_submit_dag)\n");
    printf("    -force              (Overwrite files condor_submit_dag uses if they exist)\n");
    printf("    -r <schedd_name>    (Submit to the specified remote schedd)\n");
    printf("    -schedd-daemon-ad-file <path>  (Submit to the schedd who dropped the ad file)\n");
    printf("    -schedd-address-file <path>  (Submit to the schedd who dropped the address file)\n");
	printf("    -maxidle <number>   (Maximum number of idle nodes to allow)\n");
    printf("    -maxjobs <number>   (Maximum number of jobs ever submitted at once)\n");
    printf("    -MaxPre <number>    (Maximum number of PRE scripts to run at once)\n");
    printf("    -MaxPost <number>   (Maximum number of POST scripts to run at once)\n");
    printf("    -notification <value> (Determines how much email you get from HTCondor.\n");
    printf("        See the condor_submit man page for values.)\n");
    printf("    -NoEventChecks      (Now ignored -- use DAGMAN_ALLOW_EVENTS)\n"); 
    printf("    -DontAlwaysRunPost  (Don't run POST script if PRE script fails)\n");
    printf("    -AlwaysRunPost      (Run POST script if PRE script fails)\n");
	printf("    -AllowLogError      (Allows the DAG to attempt execution even if the log\n");
    printf("        reading code finds errors when parsing the submit files)\n"); 
	printf("        (-AllowLogError is no longer supported as of 8.5.5)\n");
	printf("    -UseDagDir          (Run DAGs in directories specified in DAG file paths)\n");
    printf("    -debug <number>     (Determines how verbosely DAGMan logs its work\n");
    printf("         about the life of the condor_dagman job.  'value' must be\n");
    printf("         an integer with a value of 0-7 inclusive.)\n");
    printf("    -outfile_dir <path> (Directory into which to put the dagman.out file,\n");
	printf("         instead of the default\n");
    printf("    -config <filename>  (Specify a DAGMan configuration file)\n");
	printf("    -append <command>   (Append specified command to .condor.sub file)\n");
	printf("    -insert_sub_file <filename>   (Insert specified file into .condor.sub file)\n");
	printf("    -batch-name <name>  (Set the batch name for the dag)\n");
	printf("    -AutoRescue 0|1     (whether to automatically run newest rescue DAG;\n");
	printf("         0 = false, 1 = true)\n");
	printf("    -DoRescueFrom <number>  (run rescue DAG of given number)\n");
	printf("    -AllowVersionMismatch (allow version mismatch between the\n");
	printf("         .condor.sub file and the condor_dagman binary)\n");
	printf("    -no_recurse         (don't recurse in nested DAGs)\n");
	printf("    -do_recurse         (do recurse in nested DAGs)\n");
	printf("    -update_submit      (update submit file if it exists)\n");
	printf("    -import_env         (explicitly import env into submit file)\n");
	printf("    -DumpRescue         (DAGMan dumps rescue DAG and exits)\n");
	printf("    -valgrind           (create submit file to run valgrind on DAGMan)\n");
	printf("    -priority <priority> (jobs will run with this priority by default)\n");
	printf("    -dont_use_default_node_log (Restore pre-7.9.0 behavior of using UserLog only)\n");
	printf("        (-dont_use_default_node_log is no longer allowed as of 8.3.1)\n");
	printf("    -suppress_notification (Set \"notification = never\" in all jobs submitted by this DAGMan)\n");
	printf("    -dont_suppress_notification (Allow jobs to specify notification)\n");
	printf("    -DoRecov            (run in recovery mode)\n");
	exit(iExitCode);
}
