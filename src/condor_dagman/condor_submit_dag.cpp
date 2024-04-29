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
#include "condor_config.h"
#include "read_multiple_logs.h"
#include "condor_version.h"
#include "my_popen.h"
#include "setenv.h"

namespace shallow = DagmanShallowOptions;
namespace deep = DagmanDeepOptions;

int printUsage(int iExitCode=1); // NOTE: printUsage calls exit(1), so it doesn't return
void parseCommandLine(DagmanOptions &dagOpts, size_t argc, const char * const argv[]);
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
			fprintf(stderr, "Recursive submit(s) failed; exiting without attempting top-level submit\n");
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

	if ( ! param_boolean("DAGMAN_WRITE_PARTIAL_RESCUE", true)) {
		fprintf(stdout, "WARNING: DAGMAN_WRITE_PARTIAL_RESCUE = False.\n"
		                "   The use of full Rescue DAG's is deprecated and slated\n"
		                "   for removal during the lifetime of V24 feature series of HTCondor.\n\n");
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

	char **args = arglist.GetStringArray();
	for (size_t argNum = 0; argNum < arglist.Count(); argNum++) {
		std::string strArg = arglist.GetArg(argNum);
		// Check if arg is a preserved one
		if (strncasecmp(strArg.c_str(), "-Max", 4) != MATCH) { continue; }
		std::string errMsg;
		if ( ! dagOpts.AutoParse(strArg, argNum, arglist.Count(), args, errMsg)) {
			fprintf(stderr, "%s\n", errMsg.c_str());
			printUsage();
		}
	}
	deleteStringArray(args);

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
			if (strArg.find("-vers") != std::string::npos) { // -version
				printf("%s\n%s\n", CondorVersion(), CondorPlatform());
				exit(0);

			} else if (strArg.find("-help") != std::string::npos || strArg.find("-h") != std::string::npos) { // -help
				printUsage(0);

			} else {
				// Make note of overriding AppendFile config value via -insert_sub_file
				if (strArg.find("-insert_s") != std::string::npos) {
					static bool flagUsed = false;
					if ( ! flagUsed && ! dagOpts[shallow::str::AppendFile].empty()) {
						printf("Note: -insert_sub_file value (%s) overriding DAGMAN_INSERT_SUB_FILE setting (%s)\n",
						       argv[iArg+1], dagOpts[shallow::str::AppendFile].c_str());
					}
					flagUsed = true;
				}

				// Process Arg
				std::string errMsg;
				if ( ! dagOpts.AutoParse(strArg, iArg, argc, argv, errMsg)) {
					fprintf(stderr, "%s\n", errMsg.c_str());
					printUsage();
				}
			}

		}
	}

	if ( ! dagOpts[shallow::str::ConfigFile].empty()) {
		std::string errMsg;
		if ( ! dagmanUtils.MakePathAbsolute(dagOpts[shallow::str::ConfigFile], errMsg)) {
			fprintf(stderr, "%s\n", errMsg.c_str());
			exit(1);
		}
	}

	if (dagOpts.primaryDag().empty()) {
		fprintf(stderr, "ERROR: no dag file specified; aborting.\n");
		printUsage();
	}

	if (dagOpts[deep::i::DoRescueFrom] < 0) {
		fprintf(stderr, "-DoRescueFrom value must be non-negative; aborting.\n");
		printUsage();
	}
}

//---------------------------------------------------------------------------
int printUsage(int iExitCode) 
{
	printf("Usage: condor_submit_dag [options] dag_file [dag_file_2 ... dag_file_n]\n");
	printf("  where dag_file1, etc., is the name of a DAG input file\n");
	printf("  and where [options] is one or more of:\n");
	printf("    -help                          (print usage info and exit)\n");
	printf("    -version                       (print version and exit)\n");
	dagmanUtils.DisplayDAGManOptions("    %-30s (%s)\n", DagOptionSrc::CONDOR_SUBMIT_DAG);
	exit(iExitCode);
}
