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

struct SubmitDagOptions
{
	// these options come from the command line
	bool bSubmit;
	bool bVerbose;
	bool bForce;
	MyString strNotification;
	MyString strJobLog;
        MyString strStorkLog;
	int iMaxJobs;
	int iMaxPre;
	int iMaxPost;
        bool bNoPostFail;
        MyString strRemoteSchedd;
	int iDebugLevel;
	MyString strDagFile;
	
	// non-command line options
	MyString strLibLog;
	MyString strDebugLog;
	MyString strSchedLog;
	MyString strSubFile;
	MyString strRescueFile;

	SubmitDagOptions() 
	{ 
		bSubmit = true;
		bVerbose = false;
		bForce = false;
		strNotification = "";
		strJobLog = "";
		strStorkLog = "";
		iMaxJobs = 0;
		iMaxPre = 0;
		iMaxPost = 0;
		bNoPostFail = false;
		strRemoteSchedd = "";
		iDebugLevel = 3;
		strDagFile = "";

		strLibLog = "";
		strDebugLog = "";
		strSchedLog = "";
		strSubFile = "";
		strRescueFile = "";
	}

};

int printUsage(); // NOTE: printUsage calls exit(1), so it doesnt return
void parseCommandLine(SubmitDagOptions &opts, int argc, char *argv[]);
void submitDag(SubmitDagOptions &opts);

int main(int argc, char *argv[])
{
	printf("\n");

	SubmitDagOptions opts;
	myDistro->Init( argc, argv );
	parseCommandLine(opts, argc, argv);
	
	if (opts.strDagFile == "")
	{
		printf("No dag file specified.\n");
		printUsage();
	}

	opts.strLibLog = opts.strDagFile + ".lib.out";
	opts.strDebugLog = opts.strDagFile + ".dagman.out";
	opts.strSchedLog = opts.strDagFile + ".dagman.log";
	opts.strSubFile = opts.strDagFile + ".condor.sub";
	opts.strRescueFile = opts.strDagFile + ".rescue";

	submitDag(opts);
	
	return 0;
}

// utility fcns for submitDag
void ensureOutputFilesExist(const SubmitDagOptions &opts);
void writeSubmitFile(const MyString &strDagmanPath, const SubmitDagOptions &opts);
void getJobLogFilenameFromSubmitFiles(SubmitDagOptions &opts);
MyString loadLogFileNameFromSubFile(const MyString &strSubFile);

void submitDag(SubmitDagOptions &opts)
{
#ifdef WIN32
	MyString strDagmanPath = which("condor_dagman.exe");
#else
	MyString strDagmanPath = which("condor_dagman");
#endif

	if (strDagmanPath == "")
	{
		printf("Can't find condor_dagman in your path, aborting.\n");
		exit(2);
	}
		
	ensureOutputFilesExist(opts);

	if (opts.strJobLog != "")
	{
		printf("Assuming %s is the log file shared by all jobs in this DAG.\n", opts.strJobLog.Value());
		printf("Please be sure you know what you're doing and that all jobs use this file.\n");
	}
	else
	{
		printf("Checking all your submit files for a consistent log file name.\n");
		printf("This might take a while... \n");
		getJobLogFilenameFromSubmitFiles(opts);
		if (opts.strJobLog == "")
			return;
		printf("Done.\n");
	}

	writeSubmitFile(strDagmanPath, opts);

	printf("-----------------------------------------------------------------------\n");
	printf("File for submitting this DAG to Condor   : %s\n", opts.strSubFile.Value());
	printf("Log of DAGMan debugging messages         : %s\n", opts.strDebugLog.Value());
	printf("Log of Condor library debug messages     : %s\n", opts.strLibLog.Value());
	printf("Log of the life of condor_dagman itself  : %s\n", opts.strSchedLog.Value());
	printf("\n");
	printf("Condor Log file for all jobs of this DAG : %s\n", opts.strJobLog.Value());

	if (opts.bSubmit)
	{
		MyString strCmdLine = "condor_submit " + opts.strRemoteSchedd + " " + opts.strSubFile;
		system(strCmdLine.Value());
	}
	else
	{
		printf("-no_submit given, not submitting DAG to Condor.  You can do this with:\n");
		printf("\"condor_submit %s\"\n", opts.strSubFile.Value());
	}
	printf("-----------------------------------------------------------------------\n");
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

MyString readFileToString(const MyString &strFilename)
{
	FILE *pFile = fopen(strFilename.Value(), "r");
	if (!pFile)
	{
		return "";
	}

	fseek(pFile, 0, SEEK_END);
	int iLength = ftell(pFile);
	MyString strToReturn;
	strToReturn.reserve_at_least(iLength+1);
	fseek(pFile, 0, SEEK_SET);
	char *psBuf = new char[iLength+1];
	fread(psBuf, 1, iLength, pFile);
	psBuf[iLength] = 0;
	strToReturn = psBuf;
	delete [] psBuf;

	fclose(pFile);
	return strToReturn;
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
		printf("ERROR: \"%s\" already exists.\n", opts.strSubFile.Value());
		bHadError = true;
	}
	if (fileExists(opts.strLibLog))
	{
		printf("ERROR: \"%s\" already exists.\n", opts.strLibLog.Value());
		bHadError = true;
	}
	if (fileExists(opts.strDebugLog))
	{
		printf("ERROR: \"%s\" already exists.\n", opts.strDebugLog.Value());
		bHadError = true;
	}
	if (fileExists(opts.strSchedLog))
	{
		printf("ERROR: \"%s\" already exists.\n", opts.strSchedLog.Value());
		bHadError = true;
	}
	if (fileExists(opts.strRescueFile))
	{
		printf("ERROR: \"%s\" already exists.\n", opts.strRescueFile.Value());
	    printf("  You may want to resubmit your DAG using that file, instead of ");
	    printf("\"%s\"\n", opts.strDagFile.Value());
	    printf("  Look at the Condor manual for details about DAG rescue files.\n");
	    printf("  Please investigate and either remove \"%s\",\n", opts.strRescueFile.Value());
	    printf("  or use that as the input to condor_submit_dag.\n");

		bHadError = true;
	}

	if (bHadError) 
	{
	    printf("\nSome file(s) needed by condor_submit_dag already exist.  ");
	    printf("Either rename them,\nor use the \"-f\" option to force them ");
	    printf("to be overwritten.\n");
	    exit(3);
	}
}

void writeSubmitFile(const MyString &strDagmanPath, const SubmitDagOptions &opts)
{
	FILE *pSubFile = fopen(opts.strSubFile.Value(), "w");
	if (!pSubFile)
	{
		printf("ERROR: unable to create submit file %s\n", opts.strSubFile.Value());
		exit(1);
	}

    fprintf(pSubFile, "# Filename: %s\n", opts.strSubFile.Value());
    fprintf(pSubFile, "# Generated by condor_submit_dag %s\n", 
			opts.strDagFile.Value());
    fprintf(pSubFile, "universe\t= scheduler\n");
    fprintf(pSubFile, "executable\t= %s\n", strDagmanPath.Value());
	fprintf(pSubFile, "getenv\t\t= True\n");
	fprintf(pSubFile, "output\t\t= %s\n", opts.strLibLog.Value());
    fprintf(pSubFile, "error\t\t= %s\n", opts.strLibLog.Value());
    fprintf(pSubFile, "log\t\t= %s\n", opts.strSchedLog.Value());
    fprintf(pSubFile, "remove_kill_sig\t= SIGUSR1\n" );

	MyString strArgs;
	strArgs.reserve_at_least(256);

	strArgs = "-f -l . -Debug " + makeString(opts.iDebugLevel) + " -Lockfile " + opts.strDagFile + ".lock";
    strArgs += " -Condorlog " + opts.strJobLog + " -Dag " + opts.strDagFile;
    strArgs += " -Rescue " + opts.strRescueFile;
    if(opts.strStorkLog != "")
        {
	strArgs += " -Storklog " + opts.strStorkLog;
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
    if(opts.bNoPostFail) 
	{
		strArgs += " -NoPostFail";
    }

    fprintf(pSubFile, "arguments\t= %s\n", strArgs.Value());
    fprintf(pSubFile, "environment\t= _CONDOR_DAGMAN_LOG=%s"
		"%c_CONDOR_MAX_DAGMAN_LOG=0\n", opts.strDebugLog.Value(), env_delimiter);
    if(opts.strNotification != "") 
	{	
		fprintf(pSubFile, "notification\t= %s\n", opts.strNotification.Value());
    }
    fprintf(pSubFile, "queue\n");

	fclose(pSubFile);
	
}

void getJobLogFilenameFromSubmitFiles(SubmitDagOptions &opts)
{
	//printf("This isn't working yet, use -log for now\n");

	MyString strDagFile = readFileToString(opts.strDagFile);
	if (strDagFile == "")
	{
		printf("Unable to read dag file %s\n", opts.strDagFile.Value());
		exit(2);
	}

	StringList listLines( strDagFile.Value(), "\n");
	listLines.rewind();
	const char *psLine;
	MyString strPreviousLogFilename;
	while( (psLine = listLines.next()) )
	{
		MyString strLine = psLine;
		
		// this internal loop is for '/' line continuation
		while (strLine[strLine.Length()-1] == '\\')
		{
			strLine.setChar(strLine.Length()-1, 0);
			psLine = listLines.next();
			if (psLine)
				strLine += psLine;
		}

		if (strLine.Length() <= 3)
			continue;

		MyString strFirstThree = strLine.Substr(0, 2);
		if (!stricmp(strFirstThree.Value(), "job"))
		{
			int iLastWhitespace = strLine.Length();
			int iPos;
			for (iPos = strLine.Length()-1; iPos >= 0; iPos--)
			{
				if (strLine[iPos] == '\t' || strLine[iPos] == ' ')
				{	
					iLastWhitespace = iPos;
					break;
				}
			}

			MyString strSubFile = strLine.Substr(iLastWhitespace+1, strLine.Length()-1);

			// get the log= value from the sub file

			MyString strLogFilename = loadLogFileNameFromSubFile(strSubFile);

			if (strLogFilename != strPreviousLogFilename && strPreviousLogFilename != "")
			{
				printf("ERROR: submit files use different log files (detected in: %s)\n", strSubFile.Value());
				opts.strJobLog = "";
				return;
			}

			strPreviousLogFilename = strLogFilename;
		}
	}	

	opts.strJobLog = strPreviousLogFilename;
}

void parseCommandLine(SubmitDagOptions &opts, int argc, char *argv[])
{
	int iArg;

	if (argc <= 1)
		printUsage();

	for (iArg = 1; iArg < argc; iArg++)
	{
		MyString strArg = argv[iArg];
		bool bIsLastArg = (iArg == argc-1);

		if (strArg[0] != '-')
		{
			if (!bIsLastArg)  // non-hyphen arg (dag file name) must be last 
			{
				printUsage();
			}
			opts.strDagFile = strArg;
			break;
		}

		if (strArg.find("-no_s") != -1)
		{
			opts.bSubmit = false;
		}
		else if (strArg.find("-v") != -1)
		{
			opts.bVerbose = true;
		}
		else if (strArg.find("-f") != -1)
		{
			opts.bForce = true;
		}
		else if (strArg.find("-not") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.strNotification = argv[++iArg];
		}
		else if (strArg.find("-l") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.strJobLog = argv[++iArg];
		}
		else if (strArg.find("-maxj") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.iMaxJobs = atoi(argv[++iArg]);
		}
		else if (strArg.find("-MaxPr") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.iMaxPre = atoi(argv[++iArg]);
		}
		else if (strArg.find("-MaxPo") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.iMaxPost = atoi(argv[++iArg]);
		}
		else if (strArg.find("-Storklog") != -1)
		{
		  if (bIsLastArg)
		    printUsage();
		  opts.strStorkLog = argv[++iArg];
	        }
		else if (strArg.find("-NoPo") != -1)
		{
			opts.bNoPostFail = true;
		}
		else if (strArg.find("-r") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.strRemoteSchedd = argv[++iArg];
		}
		else if (strArg.find("-d") != -1)
		{
			if (bIsLastArg)
				printUsage();
			opts.iDebugLevel = atoi(argv[++iArg]);
		}
		else
		{
			printf("Unknown option %s\n", strArg.Value());
			printUsage();
		}
	}
}
// condor_submit_dag diamond.dag
int printUsage() 
{
    printf("Usage: condor_submit_dag [options] filename\n");
    printf("  where filename is the name of your DAG input file\n");
    printf("  and where [options] is one or more of:\n");
    printf("    -no_submit          (DAG is not submitted to Condor)\n");
    printf("    -verbose            (Verbose error messages from condor_submit_dag)\n");
    printf("    -force              (Overwrite files condor_submit_dag uses if they exist)\n");
    printf("    -r schedd_name      (Submit to the specified remote schedd)\n");
    printf("    -maxjobs number     (Maximum number of jobs ever submitted at once)\n");
    printf("    -MaxPre number      (Maximum number of PRE scripts to run at once)\n");
    printf("    -MaxPost number     (Maximum number of POST scripts to run at once)\n");
    printf("    -NoPostFail         (Don't run POST scripts after failed jobs)\n");
    printf("    -log filename       (Specify the log file shared by all jobs in the DAG)\n");
    printf("    -Storklog filename  (Specify the Stork log file for jobs in this DAG)\n");
    printf("    -notification value (Determines how much email you get from Condor)\n");
    printf("    -debug number       (Determines how verbosely DAGMan logs its work)\n");
    printf("         about the life of the condor_dagman job.  'value' must be\n");
    printf("         one of \"always\", \"never\", \"error\", or \"complete\".\n");
    printf("         See the condor_submit man page for details.)\n");
	exit(1);
}

MyString loadLogFileNameFromSubFile(const MyString &strSubFilename)
{
	MyString strSubFile = readFileToString(strSubFilename);
	
	StringList listLines( strSubFile.Value(), "\r\n");
	listLines.rewind();
	const char *psLine;
	MyString strPreviousLogFilename;
	while( (psLine = listLines.next()) )
	{
		MyString strLine = psLine;
		if (!stricmp(strLine.Substr(0, 2).Value(), "log"))
		{
			int iEqPos = strLine.FindChar('=',0);
			if (iEqPos == -1)
				return "";

			iEqPos++;
			while (iEqPos < strLine.Length() && (strLine[iEqPos] == ' ' || strLine[iEqPos] == '\t'))
				iEqPos++;

			if (iEqPos >= strLine.Length())
				return "";

			MyString strToReturn = strLine.Substr(iEqPos, strLine.Length());

			return strToReturn;
		}
	}
	return "";
}
