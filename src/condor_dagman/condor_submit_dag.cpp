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
#include "MyString.h"
#include "which.h"
#include "string_list.h"
#include "condor_distribution.h"
#include "condor_config.h"
#include "env.h"
#include "dagman_multi_dag.h"
#include "basename.h"
#include "read_multiple_logs.h"
#include "condor_getcwd.h"
#include "condor_string.h" // for getline()
#include "condor_version.h"
#include "tmp_dir.h"


#ifdef WIN32
const char* dagman_exe = "condor_dagman.exe";
const char* valgrind_exe = "valgrind.exe";
#else
const char* dagman_exe = "condor_dagman";
const char* valgrind_exe = "valgrind";
#endif

struct SubmitDagOptions
{
	// these options come from the command line
	bool bSubmit;
	bool bVerbose;
	bool bForce;
	MyString strNotification;
	int iMaxIdle;
	int iMaxJobs;
	int iMaxPre;
	int iMaxPost;
	MyString strRemoteSchedd;
	bool bNoEventChecks;
	bool bAllowLogError;
	int iDebugLevel;
	MyString primaryDagFile;
	StringList	dagFiles;
	MyString strDagmanPath; // path to dagman binary
	bool useDagDir;
	MyString strDebugDir;
	MyString strConfigFile;
	MyString appendFile; // append to .condor.sub file before queue
	StringList appendLines; // append to .condor.sub file before queue
	bool oldRescue;
	bool autoRescue;
	int doRescueFrom;
	bool allowVerMismatch;
	bool recurse; // whether to recursively run condor_submit_dag on nested DAGs
	bool updateSubmit; // allow updating submit file w/o -force
	bool copyToSpool;
	bool importEnv; // explicitly import environment into .condor.sub file
	bool dumpRescueDag;
	bool runValgrind;
	
	// non-command line options
	MyString strLibOut;
	MyString strLibErr;
	MyString strDebugLog;
	MyString strSchedLog;
	MyString strSubFile;
	MyString strRescueFile;
	MyString strLockFile;

	SubmitDagOptions() 
	{ 
		bSubmit = true;
		bVerbose = false;
		bForce = false;
		strNotification = "";
		iMaxIdle = 0;
		iMaxJobs = 0;
		iMaxPre = 0;
		iMaxPost = 0;
		strRemoteSchedd = "";
		bNoEventChecks = false;
		bAllowLogError = false;
		iDebugLevel = 3;
		primaryDagFile = "";
		useDagDir = false;
		strConfigFile = "";
		appendFile = param("DAGMAN_INSERT_SUB_FILE");
		oldRescue = param_boolean( "DAGMAN_OLD_RESCUE", false );
		autoRescue = param_boolean( "DAGMAN_AUTO_RESCUE", true );
		doRescueFrom = 0; // 0 means no rescue DAG specified
		allowVerMismatch = false;
		recurse = true;
		updateSubmit = false;
		copyToSpool = param_boolean( "DAGMAN_COPY_TO_SPOOL", false );
		importEnv = false;
		dumpRescueDag = false;
		runValgrind = false;
	}

};

int printUsage(); // NOTE: printUsage calls exit(1), so it doesn't return
void parseCommandLine(SubmitDagOptions &opts, int argc,
			const char * const argv[]);
bool parsePreservedArgs(const MyString &strArg, int &argNum, int argc,
			const char * const argv[], SubmitDagOptions &opts);
int doRecursion( SubmitDagOptions &opts );
int parseJobOrDagLine( const char *dagLine, StringList &tokens,
			const char *fileType, const char *&submitOrDagFile,
			const char *&directory );
int runSubmit( const SubmitDagOptions &opts, const char *dagFile,
			const char *directory );
int setUpOptions( SubmitDagOptions &opts );
void ensureOutputFilesExist(const SubmitDagOptions &opts);
int getOldSubmitFlags( SubmitDagOptions &opts );
int parseArgumentsLine( const MyString &subLine , SubmitDagOptions &opts );
void writeSubmitFile(/* const */ SubmitDagOptions &opts);
int submitDag( SubmitDagOptions &opts );

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
	printf("\n");

		// Set up the dprintf stuff to write to stderr, so that Condor
		// libraries which use it will write to the right place...
	Termlog = true;
	dprintf_config("condor_submit_dag"); 
	DebugFlags = D_ALWAYS | D_NOHEADER;
	config();

		// Initialize our Distribution object -- condor vs. hawkeye, etc.
	myDistro->Init( argc, argv );

		// Load command-line arguments into the opts structure.
	SubmitDagOptions opts;
	parseCommandLine(opts, argc, argv);

	int tmpResult;

		// Recursively run ourself on nested DAGs.  We need to do this
		// depth-first so all of the lower-level .condor.sub files already
		// exist when we check for log files.
	if ( opts.recurse ) {
		tmpResult = doRecursion( opts );
		if ( tmpResult != 0) {
			fprintf( stderr, "Recursive submit(s) failed; exiting without "
						"attempting top-level submit\n" );
			return tmpResult;
		}
	}
	
		// Further work to get the opts structure set up properly.
	tmpResult = setUpOptions( opts );
	if ( tmpResult != 0 ) return tmpResult;

		// Check whether the output files already exist; if so, we may
		// abort depending on the -f flag and whether we're running
		// a rescue DAG.
	ensureOutputFilesExist(opts);

		// Make sure that all node jobs have log files, the files
		// aren't on NFS, etc.

		// Note that this MUST come after recursion, otherwise we'd
		// pass down the "preserved" values from the current .condor.sub
		// file.
	if ( opts.updateSubmit ) {
		tmpResult = getOldSubmitFlags( opts );
		if ( tmpResult != 0 ) return tmpResult;
	}

		// Write the actual submit file for DAGMan.
	writeSubmitFile( opts );

	return submitDag( opts );
}

//---------------------------------------------------------------------------
/** Recursively call condor_submit_dag on nested DAGs.
	@param opts: the condor_submit_dag options
	@return 0 if successful, 1 if failed
*/
int
doRecursion( SubmitDagOptions &opts )
{
	int result = 0;

	opts.dagFiles.rewind();

		// Go through all DAG files specified on the command line...
	StringList submitFiles;
	const char *dagFile;
	while ( (dagFile = opts.dagFiles.next()) ) {

			// Get logical lines from this DAG file.
		StringList logicalLines;
		MyString error = MultiLogFiles::fileNameToLogicalLines(
					dagFile, logicalLines );
		if ( error != "" ) {
			fprintf( stderr, "Error reading DAG file: %s\n",
						error.Value() );
			return 1;
		}

			// Find and parse JOB and SUBDAG lines.
		logicalLines.rewind();
		const char *dagLine;
		while ( (dagLine = logicalLines.next()) ) {
			StringList tokens( dagLine, " \t" );
			tokens.rewind();
			const char *first = tokens.next();

			if ( first && !strcasecmp( first, "JOB" ) ) {

					// Get the submit file and directory from the DAG
					// file line.
				const char *subFile;
				const char *directory;
				if ( parseJobOrDagLine( dagLine, tokens, "submit",
							subFile, directory ) != 0 ) {
					return 1;
				}

					// Now figure out whether JOB line is a nested DAG.
				const char *DAG_SUBMIT_FILE_SUFFIX = ".condor.sub";
				MyString submitFile( subFile );

					// If submit file ends in ".condor.sub", we assume it
					// refers to a sub-DAG.
				int start = submitFile.find( DAG_SUBMIT_FILE_SUFFIX );
				if ( start >= 0 &&
							start + (int)strlen( DAG_SUBMIT_FILE_SUFFIX) ==
							submitFile.Length() ) {

						// Change submit file name to DAG file name.
					submitFile.replaceString( DAG_SUBMIT_FILE_SUFFIX, "" );

						// Now run condor_submit_dag on the DAG file.
					if ( runSubmit( opts, submitFile.Value(),
								directory ) != 0 ) {
						result = 1;
					}
				}

			} else if ( first && !strcasecmp( first, "SUBDAG" ) ) {

				const char *inlineOrExt = tokens.next();
				if ( strcasecmp( inlineOrExt, "EXTERNAL" ) ) {
					fprintf( stderr, "ERROR: only SUBDAG EXTERNAL is supported "
								"at this time (line: <%s>)\n", dagLine );
					return 1;
				}

					// Get the nested DAG file and directory from the DAG
					// file line.
				const char *nestedDagFile;
				const char *directory;
				if ( parseJobOrDagLine( dagLine, tokens, "DAG",
							nestedDagFile, directory ) != 0 ) {
					return 1;
				}

					// Now run condor_submit_dag on the DAG file.
				if ( runSubmit( opts, nestedDagFile, directory ) != 0 ) {
					result = 1;
				}
			}
		}
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
parseJobOrDagLine( const char *dagLine, StringList &tokens,
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
/** Run condor_submit_dag on the given DAG file.
	@param opts: the condor_submit_dag options
	@param dagFile: the DAG file to process
	@param directory: the directory from which the DAG file should
		be processed (ignored if NULL)
	@return 0 if successful, 1 if failed
*/
int
runSubmit( const SubmitDagOptions &opts, const char *dagFile,
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

		// Now actually run the command.
	int retval = system( cmdLine.Value() );
	if ( retval != 0 ) {
		fprintf( stderr, "ERROR: condor_submit failed; aborting.\n" );
		result = 1;
	}

		// Change back to the directory we started from.
	if ( !tmpDir.Cd2MainDir( errMsg ) ) {
		fprintf( stderr, "Error (%s) changing back to original directory\n",
					errMsg.Value() );
	}

	return result;
}

//---------------------------------------------------------------------------
/** Set up things in opts that aren't directly specified on the command line.
	@param opts: the condor_submit_dag options
	@return 0 if successful, 1 if failed
*/
int
setUpOptions( SubmitDagOptions &opts )
{
	opts.strLibOut = opts.primaryDagFile + ".lib.out";
	opts.strLibErr = opts.primaryDagFile + ".lib.err";

	if ( opts.strDebugDir != "" ) {
		opts.strDebugLog = opts.strDebugDir + DIR_DELIM_STRING +
					condor_basename( opts.primaryDagFile.Value() );
	} else {
		opts.strDebugLog = opts.primaryDagFile;
	}
	opts.strDebugLog += ".dagman.out";

	opts.strSchedLog = opts.primaryDagFile + ".dagman.log";
	opts.strSubFile = opts.primaryDagFile + ".condor.sub";

	MyString	rescueDagBase;

		// If we're running each DAG in its own directory, write any rescue
		// DAG to the current directory, to avoid confusion (since the
		// rescue DAG must be run from the current directory).
	if ( opts.useDagDir ) {
		if ( !condor_getcwd( rescueDagBase ) ) {
			fprintf( stderr, "ERROR: unable to get cwd: %d, %s\n",
					errno, strerror(errno) );
			return 1;
		}
		rescueDagBase += DIR_DELIM_STRING;
		rescueDagBase += condor_basename(opts.primaryDagFile.Value());
	} else {
		rescueDagBase = opts.primaryDagFile;
	}

		// If we're running multiple DAGs, put "_multi" in the rescue
		// DAG name to indicate that the rescue DAG is for *all* of
		// the DAGs we're running.
	if ( opts.dagFiles.number() > 1 ) {
		rescueDagBase += "_multi";
	}

	opts.strRescueFile = rescueDagBase + ".rescue";

	opts.strLockFile = opts.primaryDagFile + ".lock";

	if (opts.strDagmanPath == "" ) {
		opts.strDagmanPath = which( dagman_exe );
	}

	if (opts.strDagmanPath == "")
	{
		fprintf( stderr, "ERROR: can't find %s in PATH, aborting.\n",
				 dagman_exe );
		return 1;
	}

	MyString	msg;
	if ( !GetConfigFile( opts.dagFiles, opts.useDagDir,
				opts.strConfigFile, msg) ) {
		fprintf( stderr, "ERROR: %s\n", msg.Value() );
		return 1;
	}

	return 0;
}

//---------------------------------------------------------------------------
/** Submit the DAGMan submit file unless the -no_submit option was given.
	@param opts: the condor_submit_dag options
	@return 0 if successful, 1 if failed
*/
int
submitDag( SubmitDagOptions &opts )
{
	printf("-----------------------------------------------------------------------\n");
	printf("File for submitting this DAG to Condor           : %s\n", 
			opts.strSubFile.Value());
	printf("Log of DAGMan debugging messages                 : %s\n",
		   	opts.strDebugLog.Value());
	printf("Log of Condor library output                     : %s\n", 
			opts.strLibOut.Value());
	printf("Log of Condor library error messages             : %s\n", 
			opts.strLibErr.Value());
	printf("Log of the life of condor_dagman itself          : %s\n",
		   	opts.strSchedLog.Value());
	printf("\n");

	if (opts.bSubmit)
	{
		MyString strCmdLine = "condor_submit " + opts.strRemoteSchedd +
					" " + opts.strSubFile;
		int retval = system(strCmdLine.Value());
		if( retval != 0 ) {
			fprintf( stderr, "ERROR: condor_submit failed; aborting.\n" );
			return 1;
		}
	}
	else
	{
		printf("-no_submit given, not submitting DAG to Condor.  "
					"You can do this with:\n");
		printf("\"condor_submit %s\"\n", opts.strSubFile.Value());
	}
	printf("-----------------------------------------------------------------------\n");

	return 0;
}

//---------------------------------------------------------------------------
bool fileExists(const MyString &strFile)
{
	int fd = safe_open_wrapper(strFile.Value(), O_RDONLY);
	if (fd == -1)
		return false;
	close(fd);
	return true;
}

//---------------------------------------------------------------------------
void ensureOutputFilesExist(const SubmitDagOptions &opts)
{
	int maxRescueDagNum = param_integer("DAGMAN_MAX_RESCUE_NUM",
				MAX_RESCUE_DAG_DEFAULT, 0, ABS_MAX_RESCUE_DAG_NUM);

	if (opts.doRescueFrom > 0)
	{
		MyString rescueDagName = RescueDagName(opts.primaryDagFile.Value(),
				opts.dagFiles.number() > 1, opts.doRescueFrom);
		if (!fileExists(rescueDagName))
		{
			fprintf( stderr, "-dorescuefrom %d specified, but rescue "
						"DAG file %s does not exist!\n", opts.doRescueFrom,
						rescueDagName.Value() );
	    	exit( 1 );
		}
	}

	if (opts.bForce)
	{
		unlink(opts.strSubFile.Value());
		unlink(opts.strSchedLog.Value());
		unlink(opts.strLibOut.Value());
		unlink(opts.strLibErr.Value());
		if (opts.oldRescue) {
			unlink(opts.strRescueFile.Value());
		} else {
			RenameRescueDagsAfter(opts.primaryDagFile.Value(),
						opts.dagFiles.number() > 1, 0, maxRescueDagNum);
		}
	}

		// Check whether we're automatically running a rescue DAG -- if
		// so, allow things to continue even if the files generated
		// by condor_submit_dag already exist.
	bool autoRunningRescue = false;
	if (opts.autoRescue) {
		int rescueDagNum = FindLastRescueDagNum(opts.primaryDagFile.Value(),
					opts.dagFiles.number() > 1, maxRescueDagNum);
		if (rescueDagNum > 0) {
			printf("Running rescue DAG %d\n", rescueDagNum);
			autoRunningRescue = true;
		}
	}

	bool bHadError = false;
		// If not running a rescue DAG, check for existing files
		// generated by condor_submit_dag...
	if (!autoRunningRescue && opts.doRescueFrom < 1 && !opts.updateSubmit) {
		if (fileExists(opts.strSubFile))
		{
			fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 	opts.strSubFile.Value() );
			bHadError = true;
		}
		if (fileExists(opts.strLibOut))
		{
			fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 	opts.strLibOut.Value() );
			bHadError = true;
		}
		if (fileExists(opts.strLibErr))
		{
			fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 	opts.strLibErr.Value() );
			bHadError = true;
		}
		if (fileExists(opts.strSchedLog))
		{
			fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 	opts.strSchedLog.Value() );
			bHadError = true;
		}
	}

		// This is checking for the existance of an "old-style" rescue
		// DAG file.
	if (!opts.autoRescue && opts.doRescueFrom < 1 &&
				fileExists(opts.strRescueFile))
	{
		fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 opts.strRescueFile.Value() );
	    fprintf( stderr, "  You may want to resubmit your DAG using that "
				 "file, instead of \"%s\"\n", opts.primaryDagFile.Value());
	    fprintf( stderr, "  Look at the Condor manual for details about DAG "
				 "rescue files.\n" );
	    fprintf( stderr, "  Please investigate and either remove \"%s\",\n",
				 opts.strRescueFile.Value() );
	    fprintf( stderr, "  or use it as the input to condor_submit_dag.\n" );
		bHadError = true;
	}

	if (bHadError) 
	{
	    fprintf( stderr, "\nSome file(s) needed by %s already exist.  ",
				 dagman_exe );
	    fprintf( stderr, "Either rename them,\nuse the \"-f\" option to "
				 "force them to be overwritten, or use\n"
				 "the \"-update_submit\" option to update the submit "
				 "file and continue.\n" );
	    exit( 1 );
	}
}

//---------------------------------------------------------------------------
/** Get the command-line options we want to preserve from the .condor.sub
    file we're overwriting, and plug them into the opts structure.  Note
	that it's *not* an error for the .condor.sub file to not exist.
	@param opts: the condor_submit_dag options
	@return 0 if successful, 1 if failed
*/
int
getOldSubmitFlags(SubmitDagOptions &opts)
{
		// It's not an error for the submit file to not exist.
	if ( fileExists( opts.strSubFile ) ) {
		StringList logicalLines;
		MyString error = MultiLogFiles::fileNameToLogicalLines(
					opts.strSubFile, logicalLines );
		if ( error != "" ) {
			fprintf( stderr, "Error reading submit file: %s\n",
						error.Value() );
			return 1;
		}

		logicalLines.rewind();
		const char *subLine;
		while ( (subLine = logicalLines.next()) ) {
			StringList tokens( subLine, " \t" );
			tokens.rewind();
			const char *first = tokens.next();
			if ( first && !strcasecmp( first, "arguments" ) ) {
				if ( parseArgumentsLine( subLine, opts ) != 0 ) {
					return 1;
				}
			}
		}
	}

	return 0;
}

//---------------------------------------------------------------------------
/** Parse the arguments line of an existing .condor.sub file, extracing
    the arguments we want to preserve when updating the .condor.sub file.
	@param subLine: the arguments line from the .condor.sub file
	@param opts: the condor_submit_dag options
	@return 0 if successful, 1 if failed
*/
int
parseArgumentsLine( const MyString &subLine , SubmitDagOptions &opts )
{
	const char *line = subLine.Value();
	const char *start = strchr( line, '"' );
	const char *end = strrchr( line, '"' );

	MyString arguments;
	if ( start && end ) {
		arguments = subLine.Substr( start - line, end - line );
	} else {
		fprintf( stderr, "Missing quotes in arguments line: <%s>\n",
					subLine.Value() );
		return 1;
	}

	ArgList arglist;
	MyString error;
	if ( !arglist.AppendArgsV2Quoted( arguments.Value(),
				&error ) ) {
		fprintf( stderr, "Error parsing arguments: %s\n", error.Value() );
		return 1;
	}

	for ( int argNum = 0; argNum < arglist.Count(); argNum++ ) {
		MyString strArg = arglist.GetArg( argNum );
		strArg.lower_case();
		(void)parsePreservedArgs( strArg, argNum, arglist.Count(),
					arglist.GetStringArray(), opts);
	}

	return 0;
}


class EnvFilter : public Env
{
public:
	EnvFilter( void ) { };
	virtual ~EnvFilter( void ) { };
	virtual bool ImportFilter( const MyString & /*var*/,
							   const MyString & /*val*/ ) const;
};
bool
EnvFilter::ImportFilter( const MyString &var, const MyString &val ) const
{
	if ( (var.find(";") >= 0) || (val.find(";") >= 0) ) {
		return false;
	}
	return IsSafeEnvV2Value( val.Value() );
}

//---------------------------------------------------------------------------
void writeSubmitFile(/* const */ SubmitDagOptions &opts)
{
	FILE *pSubFile = safe_fopen_wrapper(opts.strSubFile.Value(), "w");
	if (!pSubFile)
	{
		fprintf( stderr, "ERROR: unable to create submit file %s\n",
				 opts.strSubFile.Value() );
		exit( 1 );
	}

	const char *executable = NULL;
	MyString valgrindPath; // outside if so executable is valid!
	if ( opts.runValgrind ) {
		valgrindPath = which( valgrind_exe );
		if ( valgrindPath == "" ) {
			fprintf( stderr, "ERROR: can't find %s in PATH, aborting.\n",
				 		valgrind_exe );
			exit( 1 );
		} else {
			executable = valgrindPath.Value();
		}
	} else {
		executable = opts.strDagmanPath.Value();
	}

    fprintf(pSubFile, "# Filename: %s\n", opts.strSubFile.Value());

    fprintf(pSubFile, "# Generated by condor_submit_dag ");
	opts.dagFiles.rewind();
	char *dagFile;
	while ( (dagFile = opts.dagFiles.next()) != NULL ) {
    	fprintf(pSubFile, "%s ", dagFile);
	}
    fprintf(pSubFile, "\n");

    fprintf(pSubFile, "universe\t= scheduler\n");
    fprintf(pSubFile, "executable\t= %s\n", executable);
	fprintf(pSubFile, "getenv\t\t= True\n");
	fprintf(pSubFile, "output\t\t= %s\n", opts.strLibOut.Value());
    fprintf(pSubFile, "error\t\t= %s\n", opts.strLibErr.Value());
    fprintf(pSubFile, "log\t\t= %s\n", opts.strSchedLog.Value());
#if !defined ( WIN32 )
    fprintf(pSubFile, "remove_kill_sig\t= SIGUSR1\n" );
#endif

		// ensure DAGMan is automatically requeued by the schedd if it
		// exits abnormally or is killed (e.g., during a reboot)
	const char *defaultRemoveExpr = "( ExitSignal =?= 11 || "
				"(ExitCode =!= UNDEFINED && ExitCode >=0 && ExitCode <= 2))";
	MyString removeExpr(defaultRemoveExpr);
	char *tmpRemoveExpr = param("DAGMAN_ON_EXIT_REMOVE");
	if ( tmpRemoveExpr ) {
		removeExpr = tmpRemoveExpr;
		free(tmpRemoveExpr);
	}
    fprintf(pSubFile, "# Note: default on_exit_remove expression:\n");
	fprintf(pSubFile, "# %s\n", defaultRemoveExpr);
	fprintf(pSubFile, "# attempts to ensure that DAGMan is automatically\n");
	fprintf(pSubFile, "# requeued by the schedd if it exits abnormally or\n");
    fprintf(pSubFile, "# is killed (e.g., during a reboot).\n");
    fprintf(pSubFile, "on_exit_remove\t= %s\n", removeExpr.Value() );

    fprintf(pSubFile, "copy_to_spool\t= %s\n", opts.copyToSpool ?
				"True" : "False" );

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Be sure to change MIN_SUBMIT_FILE_VERSION in dagman_main.cpp
	// if the arguments passed to condor_dagman change in an
	// incompatible way!!
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	ArgList args;

	if ( opts.runValgrind ) {
		args.AppendArg("--tool=memcheck");
		args.AppendArg("--leak-check=yes");
		args.AppendArg("--show-reachable=yes");
		args.AppendArg(opts.strDagmanPath.Value());
	}

	args.AppendArg("-f");
	args.AppendArg("-l");
	args.AppendArg(".");
	args.AppendArg("-Debug");
	args.AppendArg(opts.iDebugLevel);
	args.AppendArg("-Lockfile");
	args.AppendArg(opts.strLockFile.Value());
	args.AppendArg("-AutoRescue");
	args.AppendArg(opts.autoRescue);
	args.AppendArg("-DoRescueFrom");
	args.AppendArg(opts.doRescueFrom);

	opts.dagFiles.rewind();
	while ( (dagFile = opts.dagFiles.next()) != NULL ) {
		args.AppendArg("-Dag");
		args.AppendArg(dagFile);
	}

	if(opts.oldRescue) {
		args.AppendArg("-Rescue");
		args.AppendArg(opts.strRescueFile.Value());
	}
    if(opts.iMaxIdle != 0) 
	{
		args.AppendArg("-MaxIdle");
		args.AppendArg(opts.iMaxIdle);
    }
    if(opts.iMaxJobs != 0) 
	{
		args.AppendArg("-MaxJobs");
		args.AppendArg(opts.iMaxJobs);
    }
    if(opts.iMaxPre != 0) 
	{
		args.AppendArg("-MaxPre");
		args.AppendArg(opts.iMaxPre);
    }
    if(opts.iMaxPost != 0) 
	{
		args.AppendArg("-MaxPost");
		args.AppendArg(opts.iMaxPost);
    }
	if(opts.bNoEventChecks)
	{
		// strArgs += " -NoEventChecks";
		printf( "Warning: -NoEventChecks is ignored; please use "
					"the DAGMAN_ALLOW_EVENTS config parameter instead\n");
	}
	if(opts.bAllowLogError)
	{
		args.AppendArg("-AllowLogError");
	}
	if(opts.useDagDir)
	{
		args.AppendArg("-UseDagDir");
	}

	args.AppendArg("-CsdVersion");
	args.AppendArg(CondorVersion());
	if(opts.allowVerMismatch) {
		args.AppendArg("-AllowVersionMismatch");
	}
	if(opts.dumpRescueDag) {
		args.AppendArg("-DumpRescue");
	}

	MyString arg_str,args_error;
	if(!args.GetArgsStringV1WackedOrV2Quoted(&arg_str,&args_error)) {
		fprintf(stderr,"Failed to insert arguments: %s",args_error.Value());
		exit(1);
	}
    fprintf(pSubFile, "arguments\t= %s\n", arg_str.Value());

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// Be sure to change MIN_SUBMIT_FILE_VERSION in dagman_main.cpp
	// if the environment passed to condor_dagman changes in an
	// incompatible way!!
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	EnvFilter env;
	if ( opts.importEnv ) {
		env.Import( );
	}
	env.SetEnv("_CONDOR_DAGMAN_LOG",opts.strDebugLog.Value());
	env.SetEnv("_CONDOR_MAX_DAGMAN_LOG=0");
	if ( opts.strConfigFile != "" ) {
		if ( access( opts.strConfigFile.Value(), F_OK ) != 0 ) {
			fprintf( stderr, "ERROR: unable to read config file %s "
						"(error %d, %s)\n",
						opts.strConfigFile.Value(), errno, strerror(errno) );
			exit(1);
		}
		env.SetEnv("_CONDOR_DAGMAN_CONFIG_FILE", opts.strConfigFile.Value());
	}

	MyString env_str;
	MyString env_errors;
	if(!env.getDelimitedStringV1RawOrV2Quoted(&env_str,&env_errors)) {
		fprintf(stderr,"Failed to insert environment: %s",env_errors.Value());
		exit(1);
	}
    fprintf(pSubFile, "environment\t= %s\n",env_str.Value());

    if(opts.strNotification != "") 
	{	
		fprintf(pSubFile, "notification\t= %s\n", opts.strNotification.Value());
    }

		// Append user-specified stuff to submit file...
		// ...first, the insert file, if any...
	if (opts.appendFile.Value() != "") {
		FILE *aFile = safe_fopen_wrapper(opts.appendFile.Value(), "r");
		if (!aFile)
		{
			fprintf( stderr, "ERROR: unable to read submit append file (%s)\n",
				 	opts.appendFile.Value() );
			exit( 1 );
		}

		char *line;
		while ((line = getline(aFile)) != NULL) {
    		fprintf(pSubFile, "%s\n", line);
		}

		fclose(aFile);
	}

		// ...now things specified directly on the command line.
	opts.appendLines.rewind();
	char *command;
	while ((command = opts.appendLines.next()) != NULL) {
    	fprintf(pSubFile, "%s\n", command);
	}

    fprintf(pSubFile, "queue\n");

	fclose(pSubFile);
}

//---------------------------------------------------------------------------
void
parseCommandLine(SubmitDagOptions &opts, int argc, const char * const argv[])
{
	for (int iArg = 1; iArg < argc; iArg++)
	{
		MyString strArg = argv[iArg];

		if (strArg[0] != '-')
		{
				// We assume an argument without a leading hyphen is
				// a DAG file name.
			opts.dagFiles.append(strArg.Value());
			if ( opts.primaryDagFile == "" ) {
				opts.primaryDagFile = strArg;
			}
		}
		else if (opts.primaryDagFile != "")
		{
				// Disallow hyphen args after DAG file name(s).
			printf("ERROR: no arguments allowed after DAG file name(s)\n");
			printUsage();
		}
		else
		{
			strArg.lower_case();

			// Note: in checking the argument names here, we only check for
			// as much of the full name as we need to unambiguously define
			// the argument.
			if (strArg.find("-no_s") != -1) // -no_submit
			{
				opts.bSubmit = false;
			}
			else if (strArg.find("-vers") != -1) // -version
			{
				printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
				exit( 0 );
			}
			else if (strArg.find("-help") != -1) // -help
			{
				printUsage();
				exit( 0 );
			}
			else if (strArg.find("-f") != -1) // -force
			{
				opts.bForce = true;
			}
			else if (strArg.find("-not") != -1) // -notification
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-notification argument needs a value\n");
					printUsage();
				}
				opts.strNotification = argv[++iArg];
			}
			else if (strArg.find("-r") != -1) // submit to remote schedd
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-r argument needs a value\n");
					printUsage();
				}
				opts.strRemoteSchedd = MyString("-r ") + argv[++iArg];
			}
			else if (strArg.find("-dagman") != -1)
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-dagman argument needs a value\n");
					printUsage();
				}
				opts.strDagmanPath = argv[++iArg];
			}
			else if (strArg.find("-de") != -1) // -debug
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-debug argument needs a value\n");
					printUsage();
				}
				opts.iDebugLevel = atoi(argv[++iArg]);
			}
			else if (strArg.find("-noev") != -1) // -noeventchecks
			{
				opts.bNoEventChecks = true;
			}
			else if (strArg.find("-allowlog") != -1) // -allowlogerror
			{
				opts.bAllowLogError = true;
			}
			else if (strArg.find("-use") != -1) // -usedagdir
			{
				opts.useDagDir = true;
			}
			else if (strArg.find("-out") != -1) // -outfile_dir
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-outfile_dir argument needs a value\n");
					printUsage();
				}
				opts.strDebugDir = argv[++iArg];
			}
			else if (strArg.find("-con") != -1) // -config
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-config argument needs a value\n");
					printUsage();
				}
				opts.strConfigFile = argv[++iArg];
					// Internally we deal with all configuration file paths
					// as full paths, to make it easier to determine whether
					// several paths point to the same file.
				MyString	errMsg;
				if (!MakePathAbsolute(opts.strConfigFile, errMsg)) {
					fprintf( stderr, "%s\n", errMsg.Value() );
   					exit( 1 );
				}
			}
			else if (strArg.find("-app") != -1) // -append
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-append argument needs a value\n");
					printUsage();
				}
				opts.appendLines.append(argv[++iArg]);
			}
			else if (strArg.find("-insert") != -1) // -insert_sub_file
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-insert_sub_file argument needs a value\n");
					printUsage();
				}
				++iArg;
				if (opts.appendFile != "") {
					printf("Note: -insert_sub_file value (%s) overriding "
								"DAGMAN_INSERT_SUB_FILE setting (%s)\n",
								argv[iArg], opts.appendFile.Value());
				}
				opts.appendFile = argv[iArg];
			}
			else if (strArg.find("-oldr") != -1) // -oldrescue
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-oldrescue argument needs a value\n");
					printUsage();
				}
				opts.oldRescue = (atoi(argv[++iArg]) != 0);
			}
			else if (strArg.find("-autor") != -1) // -autorescue
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-autorescue argument needs a value\n");
					printUsage();
				}
				opts.autoRescue = (atoi(argv[++iArg]) != 0);
			}
			else if (strArg.find("-dores") != -1) // -dorescuefrom
			{
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-dorescuefrom argument needs a value\n");
					printUsage();
				}
				opts.doRescueFrom = atoi(argv[++iArg]);
			}
			else if (strArg.find("-allowver") != -1) // -AllowVersionMismatch
			{
				opts.allowVerMismatch = true;
			}
			else if (strArg.find("-no_rec") != -1) // -no_recurse
			{
				opts.recurse = false;
			}
			else if (strArg.find("-updat") != -1) // -update_submit
			{
				opts.updateSubmit = true;
			}
			else if (strArg.find("-import_env") != -1) // -import_env
			{
				opts.importEnv = true;
			}			     
			else if (strArg.find("-dumpr") != -1) // -DumpRescue
			{
				opts.dumpRescueDag = true;
			}
			else if (strArg.find("-valgrind") != -1) // -valgrind
			{
				opts.runValgrind = true;
			}
				// This must come last, so we can have other arguments
				// that start with -v.
			else if ( (strArg.find("-v") != -1) ) // -verbose
			{
				opts.bVerbose = true;
			}
			else if ( parsePreservedArgs( strArg, iArg, argc, argv, opts) )
			{
				// No-op here
			}
			else
			{
				fprintf( stderr, "ERROR: unknown option %s\n", strArg.Value() );
				printUsage();
			}
		}
	}

	if (opts.primaryDagFile == "")
	{
		fprintf( stderr, "ERROR: no dag file specified; aborting.\n" );
		printUsage();
	}

	if (opts.oldRescue && opts.autoRescue)
	{
		fprintf( stderr, "Error: DAGMAN_OLD_RESCUE and DAGMAN_AUTO_RESCUE "
					"are both true.\n" );
	    exit( 1 );
	}

	if (opts.doRescueFrom < 0)
	{
		fprintf( stderr, "-dorescuefrom value must be non-negative; aborting.\n");
		printUsage();
	}
}

//---------------------------------------------------------------------------
/** Parse arguments that are to be preserved when updating a .condor.sub
	file.  If the given argument such an argument, parse it and update the
	opts structure accordingly.  (This function is meant to be called both
	when parsing "normal" command-line arguments, and when parsing the
	existing arguments line of a .condor.sub file we're overwriting.)
	@param strArg: the argument we're parsing
	@param argNum: the argument number of the current argument
	@param argc: the argument count (passed to get value for flag)
	@param argv: the argument vector (passed to get value for flag)
	@param opts: the condor_submit_dag options
	@return true iff the argument vector contained any arguments
		processed by this function
*/
bool
parsePreservedArgs(const MyString &strArg, int &argNum, int argc,
			const char * const argv[], SubmitDagOptions &opts)
{
	bool result = false;

	if (strArg.find("-maxi") != -1) // -maxidle
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxidle argument needs a value\n");
			printUsage();
		}
		opts.iMaxIdle = atoi(argv[++argNum]);
		result = true;
	}
	else if (strArg.find("-maxj") != -1) // -maxjobs
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxjobs argument needs a value\n");
			printUsage();
		}
		opts.iMaxJobs = atoi(argv[++argNum]);
		result = true;
	}
	else if (strArg.find("-maxpr") != -1) // -maxpre
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxpre argument needs a value\n");
			printUsage();
		}
		opts.iMaxPre = atoi(argv[++argNum]);
		result = true;
	}
	else if (strArg.find("-maxpo") != -1) // -maxpost
	{
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxpost argument needs a value\n");
			printUsage();
		}
		opts.iMaxPost = atoi(argv[++argNum]);
		result = true;
	}

	return result;
}

//---------------------------------------------------------------------------
int printUsage() 
{
    printf("Usage: condor_submit_dag [options] dag_file [dag_file_2 ... dag_file_n]\n");
    printf("  where dag_file1, etc., is the name of a DAG input file\n");
    printf("  and where [options] is one or more of:\n");
	printf("    -help               (print usage info and exit)\n");
	printf("    -version            (print version and exit)\n");
	printf("    -dagman <path>      (Full path to an alternate condor_dagman executable)\n");
    printf("    -no_submit          (DAG is not submitted to Condor)\n");
    printf("    -verbose            (Verbose error messages from condor_submit_dag)\n");
    printf("    -force              (Overwrite files condor_submit_dag uses if they exist)\n");
    printf("    -r <schedd_name>    (Submit to the specified remote schedd)\n");
	printf("    -maxidle <number>   (Maximum number of idle nodes to allow)\n");
    printf("    -maxjobs <number>   (Maximum number of jobs ever submitted at once)\n");
    printf("    -MaxPre <number>    (Maximum number of PRE scripts to run at once)\n");
    printf("    -MaxPost <number>   (Maximum number of POST scripts to run at once)\n");
    printf("    -notification <value> (Determines how much email you get from Condor.\n");
    printf("        See the condor_submit man page for values.)\n");
    printf("    -NoEventChecks      (Now ignored -- use DAGMAN_ALLOW_EVENTS)\n"); 
    printf("    -AllowLogError      (Allows the DAG to attempt execution even if the log\n");
    printf("        reading code finds errors when parsing the submit files)\n"); 
	printf("    -UseDagDir          (Run DAGs in directories specified in DAG file paths)\n");
    printf("    -debug <number>     (Determines how verbosely DAGMan logs its work\n");
    printf("         about the life of the condor_dagman job.  'value' must be\n");
    printf("         an integer with a value of 0-7 inclusive.)\n");
    printf("    -outfile_dir <path> (Directory into which to put the dagman.out file,\n");
	printf("         instead of the default\n");
    printf("    -config <filename>  (Specify a DAGMan configuration file)\n");
	printf("    -append <command>   (Append specified command to .condor.sub file)\n");
	printf("    -insert_sub_file <filename>   (Insert specified file into .condor.sub file)\n");
	printf("    -OldRescue 0|1      (whether to write rescue DAG the old way;\n");
	printf("         0 = false, 1 = true)\n");
	printf("    -AutoRescue 0|1     (whether to automatically run newest rescue DAG;\n");
	printf("         0 = false, 1 = true)\n");
	printf("    -DoRescueFrom <number>  (run rescue DAG of given number)\n");
	printf("    -AllowVersionMismatch (allow version mismatch between the\n");
	printf("         .condor.sub file and the condor_dagman binary)\n");
	printf("    -no_recurse         (don't recurse in nested DAGs)\n");
	printf("    -update_submit      (update submit file if it exists)\n");
	printf("    -import_env         (explicitly import env into submit file)\n");
	printf("    -DumpRescue         (DAGMan dumps rescue DAG and exits)\n");
	printf("    -valgrind           (create submit file to run valgrind on DAGMan)\n");
	exit(1);
}
