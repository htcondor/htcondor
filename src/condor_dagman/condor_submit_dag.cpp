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

namespace shallow = DagmanShallowOptions;
namespace deep = DagmanDeepOptions;

int printUsage(int iExitCode=1); // NOTE: printUsage calls exit(1), so it doesn't return
void parseCommandLine(DagmanOptions &dagOpts, size_t argc, const char * const argv[]);
bool parsePreservedArgs(const std::string &strArg, size_t &argNum, size_t argc,
			const char * const argv[], DagmanOptions &dagOpts);
int doRecursionNew(DagmanOptions &dagOpts);
int getOldSubmitFlags(DagmanOptions &dagOpts);
int parseArgumentsLine(const std::string &subLine, DagmanOptions &dagOpts);
int submitDag(DagmanOptions &dagOpts);

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
	DagmanOptions dagOpts;

		// We're setting strScheddDaemonAdFile and strScheddAddressFile
		// here so that the classad updating feature (see gittrac #1782)
		// works properly.  The problem is that the schedd daemon ad and
		// address files are normally defined relative to the $LOG value.
		// Because we specify a different log directory on the condor_dagman
		// command line, if we don't set the values here, condor_dagman
		// won't be able to find those files when it tries to communicate
		/// with the schedd.  wenger 2013-03-11
	std::string param_val;
	if (param(param_val, "SCHEDD_DAEMON_AD_FILE")) {
		dagOpts.set("ScheddDaemonAdFile", param_val);
	}

	if (param(param_val, "SCHEDD_ADDRESS_FILE")) {
		dagOpts.set("ScheddAddressFile", param_val);
	}

	parseCommandLine(dagOpts, argc, argv);

	int tmpResult;

		// Recursively run ourself on nested DAGs.  We need to do this
		// depth-first so all of the lower-level .condor.sub files already
		// exist when we check for log files.
	if (dagOpts[deep::b::Recurse]) {
		tmpResult = doRecursionNew(dagOpts);
		if ( tmpResult != 0) {
			fprintf( stderr, "Recursive submit(s) failed; exiting without "
						"attempting top-level submit\n" );
			return tmpResult;
		}
	}

	if ( ! dagmanUtils.setUpOptions(dagOpts, dagFileAttrLines)) { exit(1); }

		// Post setUpOptions() will determine custom dagman configuration file
		// If we have a config file then process it for further DAGMan setup options
	if ( ! dagOpts[shallow::str::ConfigFile].empty()) {
		if (access(dagOpts[shallow::str::ConfigFile].c_str(), R_OK) != 0 &&
			! is_piped_command(dagOpts[shallow::str::ConfigFile].c_str())) {
				fprintf(stderr, "ERROR: Can't read DAGMan config file: %s\n",
				        dagOpts[shallow::str::ConfigFile].c_str());
				exit(1);
		}
		process_config_source(dagOpts[shallow::str::ConfigFile].c_str(), 0, "DAGMan config", NULL, true);
	}

		// Check whether the output files already exist; if so, we may
		// abort depending on the -f flag and whether we're running
		// a rescue DAG.
	if ( ! dagmanUtils.ensureOutputFilesExist(dagOpts)) {
		exit(1);
	}

		// Make sure that all node jobs have log files, the files
		// aren't on NFS, etc.

		// Note that this MUST come after recursion, otherwise we'd
		// pass down the "preserved" values from the current .condor.sub
		// file.
	if (dagOpts[deep::b::UpdateSubmit]) {
		tmpResult = getOldSubmitFlags(dagOpts);
		if (tmpResult != 0) return tmpResult;
	}

		// Write the actual submit file for DAGMan.
	if ( ! dagmanUtils.writeSubmitFile(dagOpts, dagFileAttrLines)) {
		exit(1);
	}

	return submitDag(dagOpts);
}

//---------------------------------------------------------------------------
/** Recursively call condor_submit_dag on nested DAGs.
	@param dagOpts: the condor_submit_dag DAGMan options
	@return 0 if successful, 1 if failed
*/
int
doRecursionNew(DagmanOptions &dagOpts)
{
	int result = 0;
	int priority = dagOpts[shallow::i::Priority];

		// Go through all DAG files specified on the command line...
	for (const auto &dagfile : dagOpts[shallow::slist::DagFiles]) {

			// Get logical lines from this DAG file.
		MultiLogFiles::FileReader reader;
		std::string errMsg = reader.Open(dagfile.c_str());
		if ( ! errMsg.empty()) {
			fprintf(stderr, "Error reading DAG file: %s\n",
			        errMsg.c_str() );
			return 1;
		}


			// Find and parse JOB and SUBDAG lines.
		std::string dagLine;
		while (reader.NextLogicalLine(dagLine)) {
			StringTokenIterator tokens(dagLine);

			const char *first = tokens.first();
			if (first && strcasecmp(first, "SUBDAG") == MATCH) {

				const char *inlineOrExt = tokens.next();
				if (strcasecmp(inlineOrExt, "EXTERNAL") != MATCH) {
					fprintf(stderr, "ERROR: only SUBDAG EXTERNAL is supported at this time (line: <%s>)\n",
					        dagLine.c_str() );
					return 1;
				}

				const char *nestedDagFile = nullptr;
				const char *directory = nullptr;
				const char *tempToken = tokens.next();

				// Next token should be node name
				if ( ! tempToken) {
					fprintf(stderr, "No node name specified in line: <%s>\n", dagLine.c_str());
					return 1;
				}

				nestedDagFile = tokens.next();
				if ( ! nestedDagFile) {
					fprintf(stderr, "No DAG file specified in line: <%s>\n", dagLine.c_str());
					return 1;
				}

				// Check for DIR <path>
				tempToken = tokens.next();
				if (tempToken && strcasecmp(tempToken, "DIR") == MATCH) {
					directory = tokens.next();
					if ( ! directory) {
						fprintf(stderr, "No directory specified in line: <%s>\n", dagLine.c_str());
						return 1;
					}
				}

				if (dagmanUtils.runSubmitDag(dagOpts, nestedDagFile, directory, priority, false) != 0) {
					result = 1;
				}
			}
		}

		reader.Close();
	}

	return result;
}

//---------------------------------------------------------------------------
/** Submit the DAGMan submit file unless the -no_submit option was given.
	@param dagOpts: the condor_submit_dag DAGMan options
	@return 0 if successful, 1 if failed
*/
int
submitDag(DagmanOptions &dagOpts)
{
	printf("-----------------------------------------------------------------------\n");
	printf("File for submitting this DAG to HTCondor           : %s\n",
	       dagOpts[shallow::str::SubFile].c_str());
	printf("Log of DAGMan debugging messages                   : %s\n",
	       dagOpts[shallow::str::DebugLog].c_str());
	printf("Log of HTCondor library output                     : %s\n", 
	       dagOpts[shallow::str::LibOut].c_str());
	printf("Log of HTCondor library error messages             : %s\n", 
	       dagOpts[shallow::str::LibErr].c_str());
	printf("Log of the life of condor_dagman itself            : %s\n",
	       dagOpts[shallow::str::SchedLog].c_str());
	printf("\n");

	if (dagOpts[shallow::b::DoSubmit]) {
		ArgList args;
		args.AppendArg( "condor_submit" );
		if( ! dagOpts[shallow::str::RemoteSchedd].empty()) {
			args.AppendArg("-r");
			args.AppendArg(dagOpts[shallow::str::RemoteSchedd]);
		}
		args.AppendArg(dagOpts[shallow::str::SubFile]);

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
		if ( ! dagOpts[shallow::str::ScheddDaemonAdFile].empty()) {
			SetEnv("_CONDOR_SCHEDD_DAEMON_AD_FILE",
			       dagOpts[shallow::str::ScheddDaemonAdFile].c_str());
		}
		if ( ! dagOpts[shallow::str::ScheddAddressFile].empty()) {
			SetEnv("_CONDOR_SCHEDD_ADDRESS_FILE",
			       dagOpts[shallow::str::ScheddAddressFile].c_str());
		}

		int retval = my_system(args);
		if( retval != 0 ) {
			fprintf(stderr, "ERROR: condor_submit failed; aborting.\n");
			return 1;
		}
	} else {
		printf("-no_submit given, not submitting DAG to HTCondor. You can do this with:\n");
		printf("\"condor_submit %s\"\n", dagOpts[shallow::str::SubFile].c_str());
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
getOldSubmitFlags(DagmanOptions &dagOpts)
{
		// It's not an error for the submit file to not exist.
	if (dagmanUtils.fileExists(dagOpts[shallow::str::SubFile])) {
		MultiLogFiles::FileReader reader;
		std::string error = reader.Open(dagOpts[shallow::str::SubFile]);
		if ( ! error.empty()) {
			fprintf(stderr, "Error reading submit file: %s\n",
			        error.c_str());
			return 1;
		}

		std::string subLine;
		while (reader.NextLogicalLine(subLine)) {
			// Initialize list of tokens from subLine
			trim(subLine);
			StringTokenIterator tokens(subLine);

			const char *first = tokens.first();
			if ( first && strcasecmp(first, "arguments") == MATCH) {
				if (parseArgumentsLine(subLine, dagOpts) != 0) {
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
	@param dagOpts: the condor_submit_dag DAGMan options
	@return 0 if successful, 1 if failed
*/
int
parseArgumentsLine(const std::string &subLine, DagmanOptions &dagOpts)
{
	const char *line = subLine.c_str();
	const char *start = strchr(line, '"');
	const char *end = strrchr(line, '"');

	std::string arguments;
	if (start && end) {
		arguments = subLine.substr(start - line, 1 + end - start);
	} else {
		fprintf(stderr, "Missing quotes in arguments line: <%s>\n",
		        subLine.c_str());
		return 1;
	}

	ArgList arglist;
	std::string error;
	if ( ! arglist.AppendArgsV2Quoted(arguments.c_str(), error)) {
		fprintf(stderr, "Error parsing arguments: %s\n", error.c_str());
		return 1;
	}

	for (size_t argNum = 0; argNum < arglist.Count(); argNum++) {
		std::string strArg = arglist.GetArg(argNum);
		lower_case(strArg);
		char **args = arglist.GetStringArray();
		(void)parsePreservedArgs(strArg, argNum, arglist.Count(), args, dagOpts);
		deleteStringArray(args);
	}

	return 0;
}

//---------------------------------------------------------------------------
void
parseCommandLine(DagmanOptions &dagOpts, size_t argc, const char * const argv[])
{
	for (size_t iArg = 1; iArg < argc; iArg++) {
		std::string strArg = argv[iArg];

		if (strArg[0] != '-') {
			// Assume non-hyphened arg is a DAG file
			dagOpts.addDAGFile(strArg);
		} else if ( ! dagOpts.primaryDag().empty()) {
			// Disallow hyphen args after DAG file name(s).
			printf("ERROR: no arguments allowed after DAG file name(s)\n");
			printUsage();
		} else {
			lower_case(strArg);

			// Note: in checking the argument names here, we only check for
			// as much of the full name as we need to unambiguously define
			// the argument.
			if (strArg.find("-no_s") != std::string::npos) { // -no_submit
				dagOpts.set("DoSubmit", false);

			} else if (strArg.find("-vers") != std::string::npos) { // -version
				printf("%s\n%s\n", CondorVersion(), CondorPlatform());
				exit(0);

			} else if (strArg.find("-help") != std::string::npos || strArg.find("-h") != std::string::npos) { // -help
				printUsage(0);

			} else if (strArg.find("-schedd-daemon-ad-file") != std::string::npos) { // -schedd-daemon-ad-file
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-schedd-daemon-ad-file argument needs a value\n");
					printUsage();
				}
				dagOpts.set("ScheddDaemonAdFile", argv[++iArg]);

			} else if (strArg.find("-schedd-address-file") != std::string::npos) { // -schedd-address-file
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-schedd-address-file argument needs a value\n");
					printUsage();
				}
				dagOpts.set("ScheddAddressFile", argv[++iArg]);

			} else if (strArg.find("-f") != std::string::npos) { // -force
				dagOpts.set("Force", true);

			} else if (strArg.find("-not") != std::string::npos) { // -notification
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-notification argument needs a value\n");
					printUsage();
				}
				dagOpts.set("Notification", argv[++iArg]);

			} else if (strArg.find("-r") != std::string::npos) { // -r (remote schedd)
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-r argument needs a value\n");
					printUsage();
				}
				dagOpts.set("RemoteSchedd", argv[++iArg]);

			} else if (strArg.find("-dagman") != std::string::npos) { // -dagman
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-dagman argument needs a value\n");
					printUsage();
				}
				dagOpts.set("DagmanPath", argv[++iArg]);

			} else if (strArg.find("-de") != std::string::npos) { // -debug
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-debug argument needs a value\n");
					printUsage();
				}
				dagOpts.set("DebugLevel", argv[++iArg]);

			} else if (strArg.find("-use") != std::string::npos) { // -usedagdir
				dagOpts.set("UseDagDir", true);

			} else if (strArg.find("-out") != std::string::npos) { // -outfile_dir
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-outfile_dir argument needs a value\n");
					printUsage();
				}
				dagOpts.set("OutfileDir", argv[++iArg]);

			} else if (strArg.find("-con") != std::string::npos) { // -config
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-config argument needs a value\n");
					printUsage();
				}
				dagOpts.set("ConfigFile", argv[++iArg]);
					// Internally we deal with all configuration file paths
					// as full paths, to make it easier to determine whether
					// several paths point to the same file.
				std::string	errMsg;
				if ( ! dagmanUtils.MakePathAbsolute(dagOpts[shallow::str::ConfigFile], errMsg)) {
					fprintf(stderr, "%s\n", errMsg.c_str());
					exit(1);
				}

			} else if (strArg.find("-app") != std::string::npos) { // -append
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-append argument needs a value\n");
					printUsage();
				}
				dagOpts.set("AppendLines", argv[++iArg]);

			} else if (strArg.find("-bat") != std::string::npos) { // -batch-name
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-batch-name argument needs a value\n");
					printUsage();
				}
				std::string batchName = argv[++iArg];
				trim_quotes(batchName, "\""); // trim "" if any
				dagOpts.set("BatchName", batchName);

			} else if (strArg.find("-insert") != std::string::npos && strArg.find("_env") == std::string::npos) { // -insert_sub_file
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-insert_sub_file argument needs a value\n");
					printUsage();
				}
				++iArg;
				if ( ! dagOpts[shallow::str::AppendFile].empty()) {
					printf("Note: -insert_sub_file value (%s) overriding DAGMAN_INSERT_SUB_FILE setting (%s)\n",
					       argv[iArg], dagOpts[shallow::str::AppendFile].c_str());
				}
				dagOpts.set("AppendFile", argv[iArg]);

			} else if (strArg.find("-autor") != std::string::npos) {// -autorescue
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-autorescue argument needs a value\n");
					printUsage();
				}
				dagOpts.set("AutoRescue", argv[++iArg]);

			} else if (strArg.find("-dores") != std::string::npos) { // -dorescuefrom
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-dorescuefrom argument needs a value\n");
					printUsage();
				}
				dagOpts.set("DoRecueFrom", argv[++iArg]);

			} else if (strArg.find("-load") != std::string::npos) { // -load_save
				if (iArg + 1 >= argc) {
					fprintf(stderr, "-load_save requires a filename\n");
					printUsage();
				}
				dagOpts.set("SaveFile", argv[++iArg]);

			} else if (strArg.find("-allowver") != std::string::npos) { // -AllowVersionMismatch
				dagOpts.set("AllowVersionMismatch", true);

			} else if (strArg.find("-no_rec") != std::string::npos) { // -no_recurse
				dagOpts.set("Recurse", false);

			} else if (strArg.find("-do_rec") != std::string::npos) { // -do_recurse
				dagOpts.set("Recurse", true);

			} else if (strArg.find("-updat") != std::string::npos) { // -update_submit
				dagOpts.set("UpdateSubmit", true);

			} else if (strArg.find("-import_env") != std::string::npos) { // -import_env
				dagOpts.set("ImportEnv", true);

			} else if (strArg.find("-include_env") != std::string::npos) { // -include_env
				if(iArg + 1 >= argc) {
					fprintf(stderr, "-include_env argument needs a comma separated list of variables i.e \"Var1,Var2...\"\n");
					printUsage();
				}
				dagOpts.append("GetFromEnv", argv[++iArg]);

			} else if (strArg.find("-insert_env") != std::string::npos) { // -insert_env
				if(iArg + 1 >= argc) {
					fprintf(stderr, "-insert_env argument needs a key=value pair i.e. \"EXAMPLE_VAR=True\"\n");
					printUsage();
				}
				std::string kv_pairs(argv[++iArg]);
				trim(kv_pairs);
				dagOpts.set("AddToEnv", kv_pairs);

			} else if (strArg.find("-dumpr") != std::string::npos) { // -DumpRescue
				dagOpts.set("DumpRescueDag", true);

			} else if (strArg.find("-valgrind") != std::string::npos) { // -valgrind
				dagOpts.set("RunValgrind", true);

			} else if (strArg.find("-dontalwaysrun") != std::string::npos) { // -DontAlwaysRunPost
				if (dagOpts[shallow::b::PostRun]) {
					fprintf(stderr, "ERROR: -DontAlwaysRunPost and -AlwaysRunPost are both set!\n");
					exit(1);
				}
				dagOpts.set("PostRun", false);

			} else if (strArg.find("-alwaysrun") != std::string::npos) { // -AlwaysRunPost
				if (dagOpts[shallow::b::PostRun].set() && ! dagOpts[shallow::b::PostRun]) {
					fprintf(stderr, "ERROR: -DontAlwaysRunPost and -AlwaysRunPost are both set!\n");
					exit(1);
				}
				dagOpts.set("PostRun", true);

			} else if (strArg.find("-suppress_notification") != std::string::npos) { // -suppress_notification
				dagOpts.set("SuppressNotification", true);

			} else if (strArg.find("-dont_suppress_notification") != std::string::npos) { // -dont_suppress_notification
				dagOpts.set("SuppressNotification", false);

			} else if( (strArg.find("-prio") != std::string::npos) ) { // -priority
				if(iArg + 1 >= argc) {
					fprintf(stderr, "-priority argument needs a value\n");
					printUsage();
				}
				dagOpts.set("Priority", argv[++iArg]);

			} else if (strArg.find("-dorecov") != std::string::npos) { // -DoRecovery
				dagOpts.set("DoRecovery", true);

			} else if ( (strArg.find("-v") != std::string::npos) ) { // -verbose
				// This must come last, so we can have other arguments
				// that start with -v.
				dagOpts.set("Verbose", true);

			} else if (parsePreservedArgs(strArg, iArg, argc, argv, dagOpts)) {
				// No-op here

			} else {
				fprintf(stderr, "ERROR: unknown option %s\n", strArg.c_str());
				printUsage();
			}
		}
	}

	if (dagOpts.primaryDag().empty()) {
		fprintf(stderr, "ERROR: no dag file specified; aborting.\n");
		printUsage();
	}

	if (dagOpts[deep::i::DoRescueFrom] < 0) {
		fprintf(stderr, "-dorescuefrom value must be non-negative; aborting.\n");
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
	@param dagOpts: the condor_submit_dag DAGMan options
	@return true iff the argument vector contained any arguments
		processed by this function
*/
bool
parsePreservedArgs(const std::string &strArg, size_t &argNum, size_t argc,
                   const char * const argv[], DagmanOptions &dagOpts)
{
	bool result = false;

	if (strArg.find("-maxi") != std::string::npos) { // -maxidle
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxidle argument needs a value\n");
			printUsage();
		}
		dagOpts.set("MaxIdle", argv[++argNum]);;
		result = true;
	} else if (strArg.find("-maxj") != std::string::npos) { // -maxjobs
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxjobs argument needs a value\n");
			printUsage();
		}
		dagOpts.set("MaxJobs", argv[++argNum]);
		result = true;
	} else if (strArg.find("-maxpr") != std::string::npos) { // -maxpre
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxpre argument needs a value\n");
			printUsage();
		}
		dagOpts.set("MaxPre", argv[++argNum]);
		result = true;
	} else if (strArg.find("-maxpo") != std::string::npos) { // -maxpost
		if (argNum + 1 >= argc) {
			fprintf(stderr, "-maxpost argument needs a value\n");
			printUsage();
		}
		dagOpts.set("MaxPost", argv[++argNum]);
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
	printf("    -schedd-address-file <path>    (Submit to the schedd who dropped the address file)\n");
	printf("    -maxidle <number>   (Maximum number of idle nodes to allow)\n");
	printf("    -maxjobs <number>   (Maximum number of jobs ever submitted at once)\n");
	printf("    -MaxPre <number>    (Maximum number of PRE scripts to run at once)\n");
	printf("    -MaxPost <number>   (Maximum number of POST scripts to run at once)\n");
	printf("    -notification <value> (Determines how much email you get from HTCondor.\n");
	printf("                           See the condor_submit man page for values.)\n");
	printf("    -DontAlwaysRunPost  (Don't run POST script if PRE script fails)\n");
	printf("    -AlwaysRunPost      (Run POST script if PRE script fails)\n");
	printf("    -UseDagDir          (Run DAGs in directories specified in DAG file paths)\n");
	printf("    -debug <number>     (Determines how verbosely DAGMan logs its work about the\n");
	printf("                         life of the condor_dagman job. 'value' must be an\n");
	printf("                         integer with a value of 0-7 inclusive.)\n");
	printf("    -outfile_dir <path> (Directory into which to put the dagman.out file,\n");
	printf("                         instead of the default\n");
	printf("    -config <filename>  (Specify a DAGMan configuration file)\n");
	printf("    -append <command>   (Append specified command to .condor.sub file)\n");
	printf("    -insert_sub_file <filename>   (Insert specified file into .condor.sub file)\n");
	printf("    -batch-name <name>  (Set the batch name for the dag)\n");
	printf("    -AutoRescue 0|1     (whether to automatically run newest rescue DAG;\n");
	printf("                         0 = false, 1 = true)\n");
	printf("    -DoRescueFrom <number> (run rescue DAG of given number)\n");
	printf("    -load_save <filename>  (File with optional path to start DAG with saved progress)");
	printf("    -AllowVersionMismatch  (allow version mismatch between the.condor.sub file\n");
	printf("                            and the condor_dagman binary)\n");
	printf("    -no_recurse         (don't recurse in nested DAGs)\n");
	printf("    -do_recurse         (do recurse in nested DAGs)\n");
	printf("    -update_submit      (update submit file if it exists)\n");
	printf("    -import_env         (explicitly import env into submit file)\n");
	printf("    -include_env <Variables> (Comma seperated list of variables to add to .condor.sub files getenv filter)\n");
	printf("    -insert_env  <Key=Value> (Delimited Key=Value pairs to explicitly set in the .condor.sub files environment)\n");
	printf("    -DumpRescue         (DAGMan dumps rescue DAG and exits)\n");
	printf("    -valgrind           (create submit file to run valgrind on DAGMan)\n");
	printf("    -priority <priority> (jobs will run with this priority by default)\n");
	printf("    -suppress_notification (Set \"notification = never\" in all jobs submitted by this DAGMan)\n");
	printf("    -dont_suppress_notification (Allow jobs to specify notification)\n");
	printf("    -DoRecovery         (run in recovery mode)\n");
	exit(iExitCode);
}
