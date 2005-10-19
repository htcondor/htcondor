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
#include "MyString.h"
#include "which.h"
#include "string_list.h"
#include "condor_distribution.h"
#include "env.h"
#include "dagman_multi_dag.h"
#include "basename.h"

#ifdef WIN32
const char* dagman_exe = "condor_dagman.exe";
#else
const char* dagman_exe = "condor_dagman";
#endif

struct SubmitDagOptions
{
	// these options come from the command line
	bool bSubmit;
	bool bVerbose;
	bool bForce;
	MyString strNotification;
	MyString strJobLog;
	MyString strStorkLog;
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
	MyString strDagmanPath;
	bool useDagDir;
	
	// non-command line options
	MyString strLibLog;
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
		strJobLog = "";
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
	}

};

int printUsage(); // NOTE: printUsage calls exit(1), so it doesnt return
void parseCommandLine(SubmitDagOptions &opts, int argc, char *argv[]);
int submitDag( SubmitDagOptions &opts );
bool readFileToString(const MyString &strFilename, MyString &strFileData);

int main(int argc, char *argv[])
{
	printf("\n");

		// Set up the dprintf stuff to write to stderr, so that Condor
		// libraries which use it will write to the right place...
	Termlog = true;
	dprintf_config("condor_submit_dag", 2); 
	DebugFlags = D_ALWAYS;

	SubmitDagOptions opts;
	myDistro->Init( argc, argv );
	parseCommandLine(opts, argc, argv);
	
	opts.strLibLog = opts.primaryDagFile + ".lib.out";

		// if the DAG input filename was not already specified as an
		// absolute PATH, use the cwd to make it into one so that
		// dprintf still works inside condor_dagman when it cds to
		// other directories.
	if ( !fullpath( opts.primaryDagFile.Value() ) ) {
		char	currentDir[_POSIX_PATH_MAX];
		if ( !getcwd( currentDir, sizeof(currentDir) ) ) {
			fprintf( stderr, "ERROR: unable to get cwd: %d, %s\n",
					errno, strerror(errno) );
			return 1;
		}
		if ( opts.useDagDir ) {
			opts.strDebugLog = currentDir;
			opts.strDebugLog += DIR_DELIM_STRING;
		}
	}
	opts.strDebugLog += opts.primaryDagFile + ".dagman.out";

	opts.strSchedLog = opts.primaryDagFile + ".dagman.log";
	opts.strSubFile = opts.primaryDagFile + ".condor.sub";

	MyString	rescueDagBase;

		// If we're running each DAG in its own directory, write any rescue
		// DAG to the current directory, to avoid confusion (since the
		// rescue DAG must be run from the current directory).
	if ( opts.useDagDir ) {
		char	currentDir[_POSIX_PATH_MAX];
		if ( !getcwd( currentDir, sizeof(currentDir) ) ) {
			fprintf( stderr, "ERROR: unable to get cwd: %d, %s\n",
					errno, strerror(errno) );
			return 1;
		}
		rescueDagBase = currentDir;
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

	return submitDag( opts );
}

// utility fcns for submitDag
void ensureOutputFilesExist(const SubmitDagOptions &opts);
void writeSubmitFile(/* const */ SubmitDagOptions &opts);

int
submitDag( SubmitDagOptions &opts )
{
	if (opts.strDagmanPath == "" ) {
		opts.strDagmanPath = which( dagman_exe );
	}

	if (opts.strDagmanPath == "")
	{
		fprintf( stderr, "ERROR: can't find %s in PATH, aborting.\n",
				 dagman_exe );
		return 1;
	}
		
	ensureOutputFilesExist(opts);

	if (opts.strJobLog != "")
	{
		printf("Assuming %s is the log file shared by all jobs in this DAG.\n", opts.strJobLog.Value());
		printf("Please be sure you know what you're doing and that all jobs use this file.\n");
	}
	else
	{
		printf("Checking all your submit files for log file names.\n");
		printf("This might take a while... \n");

		StringList	logFiles;

		MyString	msg;
		if ( !GetLogFiles( opts.dagFiles, opts.useDagDir, logFiles, msg ) ) {
			fprintf( stderr, "ERROR: %s\n", msg.Value() );
			if ( opts.bAllowLogError ) {
				fprintf( stderr, "Continuing anyhow because of "
							"-AllowLogError flag (beware!)\n" );
			} else {
				fprintf( stderr, "Aborting -- try again with the "
							"-AllowLogError flag if you *really* think "
							"this shouldn't be a fatal error\n" );
				return 1;
			}
		}

		logFiles.rewind();
		opts.strJobLog = logFiles.next();
		printf("Done.\n");
	}

	writeSubmitFile(opts);

	printf("-----------------------------------------------------------------------\n");
	printf("File for submitting this DAG to Condor           : %s\n", 
			opts.strSubFile.Value());
	printf("Log of DAGMan debugging messages                 : %s\n",
		   	opts.strDebugLog.Value());
	printf("Log of Condor library debug messages             : %s\n", 
			opts.strLibLog.Value());
	printf("Log of the life of condor_dagman itself          : %s\n",
		   	opts.strSchedLog.Value());
	printf("\n");
	printf("Condor Log file for all jobs of this DAG         : %s\n", 
			opts.strJobLog.Value());

	if (opts.strStorkLog != "") {
		printf("Stork Log file for all DaP jobs of this DAG      : %s\n",
			   	opts.strStorkLog.Value());
	}

	if (opts.bSubmit)
	{
		MyString strCmdLine = "condor_submit " + opts.strRemoteSchedd + " " + opts.strSubFile;
		int retval = system(strCmdLine.Value());
		if( retval != 0 ) {
			fprintf( stderr, "ERROR: condor_submit failed; aborting.\n" );
			return 1;
		}
	}
	else
	{
		printf("-no_submit given, not submitting DAG to Condor.  You can do this with:\n");
		printf("\"condor_submit %s\"\n", opts.strSubFile.Value());
	}
	printf("-----------------------------------------------------------------------\n");

	return 0;
}

MyString makeString(int iValue)
{
	char psToReturn[16];
	sprintf(psToReturn,"%d",iValue);
	return MyString(psToReturn);
}

bool fileExists(const MyString &strFile)
{
	int fd = open(strFile.Value(), O_RDONLY);
	if (fd == -1)
		return false;
	close(fd);
	return true;
}

bool readFileToString(const MyString &strFilename, MyString &strFileData)
{
	FILE *pFile = fopen(strFilename.Value(), "r");
	if (!pFile)
	{
		return false;
	}

	fseek(pFile, 0, SEEK_END);
	int iLength = ftell(pFile);

	strFileData.reserve_at_least(iLength+1);
	fseek(pFile, 0, SEEK_SET);
	char *psBuf = new char[iLength+1];
	fread(psBuf, 1, iLength, pFile);
	psBuf[iLength] = 0;
	strFileData = psBuf;
	delete [] psBuf;

	fclose(pFile);
	return true;
}

void ensureOutputFilesExist(const SubmitDagOptions &opts)
{
	if (opts.bForce)
	{
		unlink(opts.strSubFile.Value());
		unlink(opts.strDebugLog.Value());
		unlink(opts.strSchedLog.Value());
		unlink(opts.strLibLog.Value());
		unlink(opts.strRescueFile.Value());
	}

	bool bHadError = false;
	if (fileExists(opts.strSubFile))
	{
		fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 opts.strSubFile.Value() );
		bHadError = true;
	}
	if (fileExists(opts.strLibLog))
	{
		fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 opts.strLibLog.Value() );
		bHadError = true;
	}
	if (fileExists(opts.strDebugLog))
	{
		fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 opts.strDebugLog.Value() );
		bHadError = true;
	}
	if (fileExists(opts.strSchedLog))
	{
		fprintf( stderr, "ERROR: \"%s\" already exists.\n",
				 opts.strSchedLog.Value() );
		bHadError = true;
	}
	if (fileExists(opts.strRescueFile))
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
	    fprintf( stderr, "Either rename them,\nor use the \"-f\" option to "
				 "force them to be overwritten.\n" );
	    exit( 1 );
	}
}

void writeSubmitFile(/* const */ SubmitDagOptions &opts)
{
	FILE *pSubFile = fopen(opts.strSubFile.Value(), "w");
	if (!pSubFile)
	{
		fprintf( stderr, "ERROR: unable to create submit file %s\n",
				 opts.strSubFile.Value() );
		exit( 1 );
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
    fprintf(pSubFile, "executable\t= %s\n", opts.strDagmanPath.Value());
	fprintf(pSubFile, "getenv\t\t= True\n");
	fprintf(pSubFile, "output\t\t= %s\n", opts.strLibLog.Value());
    fprintf(pSubFile, "error\t\t= %s\n", opts.strLibLog.Value());
    fprintf(pSubFile, "log\t\t= %s\n", opts.strSchedLog.Value());
    fprintf(pSubFile, "remove_kill_sig\t= SIGUSR1\n" );
		// this is so that DAGMan is automatically requeued if killed
		// by something other than the schedd (e.g., a fast reboot)
    fprintf(pSubFile, "on_exit_remove\t= (ExitBySignal == false || ExitSignal =!= 9)\n" );

	MyString strArgs;
	strArgs.reserve_at_least(256);

	strArgs = "-f -l . -Debug " + makeString(opts.iDebugLevel) +
				" -Lockfile " + opts.strLockFile;
	if ( opts.strJobLog == "" ) {
			// Condor_dagman can't parse command-line args if we have
			// an empty value for the log file name.
		opts.strJobLog = opts.primaryDagFile + ".log";
	}
    strArgs += " -Condorlog " + opts.strJobLog;
	
	opts.dagFiles.rewind();
	while ( (dagFile = opts.dagFiles.next()) != NULL ) {
		strArgs += " -Dag ";
		strArgs += dagFile;
	}

    strArgs += " -Rescue " + opts.strRescueFile;
    if(opts.iMaxIdle) 
	{
		strArgs += " -MaxIdle " + makeString(opts.iMaxIdle);
    }
    if(opts.iMaxJobs) 
	{
		strArgs += " -MaxJobs " + makeString(opts.iMaxJobs);
    }
    if(opts.iMaxPre) 
	{
		strArgs += " -MaxPre " + makeString(opts.iMaxPre);
    }
    if(opts.iMaxPost) 
	{
		strArgs += " -MaxPost " + makeString(opts.iMaxPost);
    }
    if(opts.strStorkLog != "") 
	{
		strArgs += " -Storklog " + opts.strStorkLog;
    }
	if(opts.bNoEventChecks)
	{
		// strArgs += " -NoEventChecks";
		printf( "Warning: -NoEventChecks is deprecated; please use "
					"the DAGMAN_ALLOW_EVENTS config parameter instead\n");
	}
	if(opts.bAllowLogError)
	{
		strArgs += " -AllowLogError";
	}
	if(opts.useDagDir)
	{
		strArgs += " -UseDagDir";
	}

    fprintf(pSubFile, "arguments\t= %s\n", strArgs.Value());

	Env env;
	env.Put("_CONDOR_DAGMAN_LOG",opts.strDebugLog.Value());
	env.Put("_CONDOR_MAX_DAGMAN_LOG=0");

	char *env_str = env.getDelimitedString();
    fprintf(pSubFile, "environment\t=%s\n",env_str);
	delete[] env_str;

    if(opts.strNotification != "") 
	{	
		fprintf(pSubFile, "notification\t= %s\n", opts.strNotification.Value());
    }
    fprintf(pSubFile, "queue\n");

	fclose(pSubFile);
	
}

void parseCommandLine(SubmitDagOptions &opts, int argc, char *argv[])
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
			else if (strArg.find("-v") != -1) // -verbose
			{
				opts.bVerbose = true;
			}
			else if (strArg.find("-f") != -1) // -force
			{
				opts.bForce = true;
			}
			else if (strArg.find("-not") != -1) // -notification
			{
				opts.strNotification = argv[++iArg];
			}
			else if (strArg.find("-l") != -1) // -log
			{
				opts.strJobLog = argv[++iArg];
			}
			else if (strArg.find("-maxi") != -1) // -maxidle
			{
				opts.iMaxIdle = atoi(argv[++iArg]);
			}
			else if (strArg.find("-maxj") != -1) // -maxjobs
			{
				opts.iMaxJobs = atoi(argv[++iArg]);
			}
			else if (strArg.find("-maxpr") != -1) // -maxpre
			{
				opts.iMaxPre = atoi(argv[++iArg]);
			}
			else if (strArg.find("-maxpo") != -1) // -maxpost
			{
				opts.iMaxPost = atoi(argv[++iArg]);
			}
			else if (strArg.find("-r") != -1) // submit to remote schedd
			{
				opts.strRemoteSchedd = MyString("-r ") + argv[++iArg];
			}
			else if (strArg.find("-dagman") != -1)
			{
				opts.strDagmanPath = argv[++iArg];
			}
			else if (strArg.find("-storklog") != -1)
			{
				opts.strStorkLog = argv[++iArg];
			}
			else if (strArg.find("-d") != -1) // -debug
			{
				opts.iDebugLevel = atoi(argv[++iArg]);
			}
			else if (strArg.find("-noev") != -1) // -noeventchecks
			{
				opts.bNoEventChecks = true;
			}
			else if (strArg.find("-allow") != -1) // -allowlogerror
			{
				opts.bAllowLogError = true;
			}
			else if (strArg.find("-usedagdir") != -1) // -usedagdir
			{
				opts.useDagDir = true;
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
}

// condor_submit_dag diamond.dag
int printUsage() 
{
    printf("Usage: condor_submit_dag [options] dag_file [dag_file_2 ... dag_file_n]\n");
    printf("  where dag_file1, etc., is the name of a DAG input file\n");
    printf("  and where [options] is one or more of:\n");
	printf("    -dagman             (Full path to an alternate condor_dagman executable)\n");
    printf("    -no_submit          (DAG is not submitted to Condor)\n");
    printf("    -verbose            (Verbose error messages from condor_submit_dag)\n");
    printf("    -force              (Overwrite files condor_submit_dag uses if they exist)\n");
    printf("    -r schedd_name      (Submit to the specified remote schedd)\n");
	printf("    -maxidle number     (Maximum number of idle nodes to allow)\n");
    printf("    -maxjobs number     (Maximum number of jobs ever submitted at once)\n");
    printf("    -MaxPre number      (Maximum number of PRE scripts to run at once)\n");
    printf("    -MaxPost number     (Maximum number of POST scripts to run at once)\n");
    printf("    -log filename       (Deprecated -- don't use)\n");
// -->STORK
    printf("    -storklog filename  (Specify the Stork log file shared by all DaP jobs\n");
    printf("        in the DAG)\n");
// <--STORK
    printf("    -notification value (Determines how much email you get from Condor.\n");
    printf("        See the condor_submit man page for values.)\n");
    printf("    -NoEventChecks      (Turns off checks for \"bad\" events)\n"); 
    printf("    -AllowLogError      (Allows the DAG to attempt execution even if the log\n");
    printf("        reading code finds errors when parsing the submit files)\n"); 
	printf("    -UseDagDir          (Run DAGs in directories specified in DAG file paths)\n");
    printf("    -debug number       (Determines how verbosely DAGMan logs its work\n");
    printf("         about the life of the condor_dagman job.  'value' must be\n");
    printf("         an integer with a value of 0-7 inclusive.)\n");
	exit(1);
}
