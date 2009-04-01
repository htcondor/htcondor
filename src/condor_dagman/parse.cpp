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


//----------------------------------------------------------------
// IMPORTANT NOTE:  Any changes in the DAG file implemented here
// must also be reflected in Dag::Rescue().
// Except for the concept of splicing--when the spliciing is finished all of
// the nodes are merged into the main dag as if they had physically appeared
// in the main dag file with the correct names.
//----------------------------------------------------------------

#include "condor_common.h"

#include "job.h"
#include "parse.h"
#include "util.h"
#include "debug.h"
#include "list.h"
#include "util_lib_proto.h"
#include "dagman_commands.h"
#include "dagman_main.h"
#include "tmp_dir.h"
#include "basename.h"
#include "extArray.h"
#include "condor_string.h"  /* for strnewp() */

static const char   COMMENT    = '#';
static const char * DELIMITERS = " \t";

static ExtArray<char*> _spliceScope;
static bool _useDagDir = false;

static int _thisDagNum = -1;
static bool _mungeNames = true;

static bool parse_subdag( Dag *dag, Job::job_type_t nodeType,
						const char* nodeTypeKeyword,
						const char* dagFile, int lineNum,
						const char *directory);

static bool parse_node( Dag *dag, Job::job_type_t nodeType,
						const char* nodeTypeKeyword,
						const char* dagFile, int lineNum,
						const char *directory, const char *inlineOrExt,
						const char *submitOrDagFile);

static bool parse_script(const char *endline, Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_parent(Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_retry(Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_abort(Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_dot(Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_vars(Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_priority(Dag *dag, 
		const char *filename, int lineNumber);
static bool parse_category(Dag *dag, const char *filename, int lineNumber);
static bool parse_maxjobs(Dag *dag, const char *filename, int lineNumber);
static bool parse_splice(Dag *dag, const char *filename, int lineNumber);
static MyString munge_job_name(const char *jobName);

static MyString current_splice_scope(void);

void exampleSyntax (const char * example) {
    debug_printf( DEBUG_QUIET, "Example syntax is: %s\n", example);
}


bool
isReservedWord( const char *token )
{
    static const char * keywords[] = { "PARENT", "CHILD" };
    static const unsigned int numKeyWords = sizeof(keywords) / 
		                                    sizeof(const char *);

    for (unsigned int i = 0 ; i < numKeyWords ; i++) {
        if (!strcasecmp (token, keywords[i])) return true;
    }
    return false;
}


bool
isDelimiter( char c ) {
	char const* tmp = strchr( DELIMITERS, (int)c );
	return tmp ? true : false;
}

//-----------------------------------------------------------------------------
void parseSetDoNameMunge(bool doit)
{
	_mungeNames = doit;
}

//-----------------------------------------------------------------------------
void parseSetThisDagNum(int num)
{
	_thisDagNum = num;
}

//-----------------------------------------------------------------------------
bool parse (Dag *dag, const char *filename, bool useDagDir) {
	ASSERT( dag != NULL );

	++_thisDagNum;

	_useDagDir = useDagDir;

		//
		// If useDagDir is true, we have to cd into the directory so we can
		// parse the submit files correctly.
		// 
	MyString		tmpDirectory("");
	const char *	tmpFilename = filename;
	TmpDir		dagDir;

	if ( useDagDir ) {
			// Use a MyString here so we don't have to manually free memory
			// at all of the places we return.
		char *dirname = condor_dirname( filename );
		tmpDirectory = dirname;
		free(dirname);

		MyString	errMsg;
		if ( !dagDir.Cd2TmpDir( tmpDirectory.Value(), errMsg ) ) {
			debug_printf( DEBUG_QUIET,
					"Could not change to DAG directory %s: %s\n",
					tmpDirectory.Value(), errMsg.Value() );
			return false;
		}
		tmpFilename = condor_basename( filename );
	}

	FILE *fp = safe_fopen_wrapper(tmpFilename, "r");
	if(fp == NULL) {
		if(DEBUG_LEVEL(DEBUG_QUIET)) {
			debug_printf( DEBUG_QUIET, "Could not open file %s for input\n", filename);
		}
		return false;
   	}

	char *line;
	int lineNumber = 0;

	//
	// This loop will read every line of the input file
	//
	while ( ((line=getline(fp)) != NULL) ) {
		lineNumber++;

		//
		// Find the terminating '\0'
		//
		char * endline = line;
		while (*endline != '\0') endline++;


		// Note that getline will truncate leading spaces (as defined by isspace())
		// so we don't need to do that before checking for empty lines or comments.
		if (line[0] == 0)       continue;  // Ignore blank lines
		if (line[0] == COMMENT) continue;  // Ignore comments

			// Note: strtok() could be replaced by MyString::Tokenize(),
			// which is much safer, but I don't want to deal with that
			// right now.  wenger 2005-02-02.
		char *token = strtok(line, DELIMITERS);
		if ( !token ) continue; // so Coverity is happy

		bool parsed_line_successfully;

		// Handle a Job spec
		// Example Syntax is:  JOB j1 j1.condor [DONE]
		//
		if(strcasecmp(token, "JOB") == 0) {
			parsed_line_successfully = parse_node( dag, Job::TYPE_CONDOR, token,
					   filename, lineNumber, tmpDirectory.Value(), "",
					   "submitfile" );
		}

		// Handle a Stork job spec
		// Example Syntax is:  DATA j1 j1.dapsubmit [DONE]
		//
		else if	(strcasecmp(token, "DAP") == 0) {	// DEPRECATED!
			parsed_line_successfully = parse_node( dag, Job::TYPE_STORK, token,
					   filename, lineNumber, tmpDirectory.Value(), "",
					   "submitfile" );
			debug_printf( DEBUG_QUIET, "%s (line %d): "
				"Warning: the DAP token is deprecated and may be unsupported "
				"in a future release.  Use the DATA token\n",
				filename, lineNumber );
		}

		else if	(strcasecmp(token, "DATA") == 0) {
			parsed_line_successfully = parse_node( dag, Job::TYPE_STORK, token,
					   filename, lineNumber, tmpDirectory.Value(), "",
					   "submitfile");
		}

		else if	(strcasecmp(token, "SUBDAG") == 0) {
			parsed_line_successfully = parse_subdag( dag, Job::TYPE_CONDOR,
						token, filename, lineNumber, tmpDirectory.Value() );
		}

		// Handle a SCRIPT spec
		// Example Syntax is:  SCRIPT (PRE|POST) JobName ScriptName Args ...
		else if ( strcasecmp(token, "SCRIPT") == 0 ) {
			parsed_line_successfully = parse_script(endline, dag, 
				filename, lineNumber);
		}

		// Handle a Dependency spec
		// Example Syntax is:  PARENT p1 p2 p3 ... CHILD c1 c2 c3 ...
		else if (strcasecmp(token, "PARENT") == 0) {
			parsed_line_successfully = parse_parent(dag, filename, lineNumber);
		}
			
		// Handle a Retry spec
		// Example Syntax is:  Retry JobName 3 UNLESS-EXIT 42
		else if( strcasecmp( token, "Retry" ) == 0 ) {
			parsed_line_successfully = parse_retry(dag, filename, lineNumber);
		} 

		// Handle an Abort spec
		// Example Syntax is:  ABORT-DAG-ON JobName 2
		else if( strcasecmp( token, "ABORT-DAG-ON" ) == 0 ) {
			parsed_line_successfully = parse_abort(dag, filename, lineNumber);
		} 

		// Handle a Dot spec
		// Example syntax is: Dot dotfile [UPDATE | DONT-UPDATE] 
		//                    [OVERWRITE | DONT-OVERWRITE]
		//                    [INCLUDE dot-file-header]
		else if( strcasecmp( token, "DOT" ) == 0 ) {
			parsed_line_successfully = parse_dot(dag, filename, lineNumber);
		} 

		// Handle a Vars spec
		// Example syntax is: Vars JobName var1="val1" var2="val2"
		else if(strcasecmp(token, "VARS") == 0) {
			parsed_line_successfully = parse_vars(dag, filename, lineNumber);
		}

		// Handle a Priority spec
		// Example syntax is: Priority JobName 2
		else if(strcasecmp(token, "PRIORITY") == 0) {
			parsed_line_successfully = parse_priority(dag, filename,
						lineNumber);
		}

		// Handle a Category spec
		// Example syntax is: Category JobName Simulation
		else if(strcasecmp(token, "CATEGORY") == 0) {
			parsed_line_successfully = parse_category(dag, filename,
						lineNumber);
		}

		// Handle a MaxJobs spec
		// Example syntax is: MaxJobs Category 10
		else if(strcasecmp(token, "MAXJOBS") == 0) {
			parsed_line_successfully = parse_maxjobs(dag, filename,
						lineNumber);
		}

		// Allow a CONFIG spec, but ignore it here because it
		// is actually parsed by condor_submit_dag (config
		// files must be processed before any other code runs)
		else if(strcasecmp(token, "CONFIG") == 0) {
			parsed_line_successfully = true;
		}

		// Handle a Splice spec
		else if(strcasecmp(token, "SPLICE") == 0) {
			parsed_line_successfully = parse_splice(dag, filename,
						lineNumber);
		}
		
		// None of the above means that there was bad input.
		else {
			debug_printf( DEBUG_QUIET, "%s (line %d): "
				"Expected JOB, DATA, SCRIPT, PARENT, RETRY, ABORT-DAG-ON, "
				"DOT, VARS, PRIORITY, CATEGORY, MAXJOBS or CONFIG token\n",
				filename, lineNumber );
			parsed_line_successfully = false;
		}
		
		if (!parsed_line_successfully) {
			fclose(fp);
			return false;
		}
	}

	fclose(fp);


	// always remember which were the inital and final nodes for this dag.
	// If this dag is used as a splice, then this information is very
	// important to preserve when building dependancy links.
	dag->RecordInitialAndFinalNodes();

	if ( useDagDir ) {
		MyString	errMsg;
		if ( !dagDir.Cd2MainDir( errMsg ) ) {
			debug_printf( DEBUG_QUIET,
					"Could not change to original directory: %s\n",
					errMsg.Value() );
			return false;
		}
	}

	return true;
}

static bool 
parse_subdag( Dag *dag, Job::job_type_t nodeType, const char* nodeTypeKeyword,
			const char* dagFile, int lineNum, const char *directory )
{
	const char *inlineOrExt = strtok( NULL, DELIMITERS );
	if ( !strcasecmp( inlineOrExt, "EXTERNAL" ) ) {
		return parse_node( dag, nodeType, nodeTypeKeyword, dagFile, lineNum,
					directory, " EXTERNAL", "dagfile" );
	} else {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): only SUBDAG "
					"EXTERNAL is supported at this time\n", dagFile, lineNum);
		return false;
	}
}

static bool 
parse_node( Dag *dag, Job::job_type_t nodeType, const char* nodeTypeKeyword,
			const char* dagFile, int lineNum, const char *directory,
			const char *inlineOrExt, const char *submitOrDagFile)
{
	MyString example;
	MyString whynot;
	bool done = false;
	Dag *tmp = NULL;

	MyString expectedSyntax;
	expectedSyntax.sprintf( "Expected syntax: %s%s nodename %s "
				"[DIR directory] [DONE]", nodeTypeKeyword, inlineOrExt,
				submitOrDagFile );

		// If this is a DAP/DATA node, make sure we have a Stork log
		// file specified.
	if ( nodeType == Job::TYPE_STORK ) {
		if ( dag->GetStorkLogCount() == 0 ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): DAP/DATA node, "
						"but no Stork log file is specified (-Storklog "
						"command-line argument)\n", dagFile, lineNum);
			return false;
		}
	}

		// NOTE: fear not -- any missing tokens resulting in NULL
		// strings will be error-handled correctly by AddNode()

		// first token is the node name
	const char *nodeName = strtok( NULL, DELIMITERS );
	MyString tmpNodeName = munge_job_name(nodeName);
	nodeName = tmpNodeName.Value();

		// next token is the submit file name
	const char *submitFile = strtok( NULL, DELIMITERS );

		// next token (if any) is "DIR" or "DONE"
	const char *doneKey = NULL;
	const char* nextTok = strtok( NULL, DELIMITERS );
	TmpDir nodeDir;
	if ( nextTok && (strcasecmp(nextTok, "DIR") == 0) ) {
		if ( strcmp(directory, "") ) {
			debug_printf( DEBUG_QUIET, "ERROR: DIR specification in node "
						"lines not allowed with -UseDagDir command-line "
						"argument\n");
			return false;
		}

		directory = strtok( NULL, DELIMITERS );
		if ( !directory ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): no directory "
						"specified after DIR keyword\n", dagFile, lineNum );
			debug_printf( DEBUG_QUIET, "%s\n", expectedSyntax.Value() );
			return false;
		}

		MyString errMsg;
		if ( !nodeDir.Cd2TmpDir(directory, errMsg) ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: can't change to directory %s: %s\n",
						directory, errMsg.Value() );
			return false;
		}
		doneKey = strtok( NULL, DELIMITERS );
	} else {
		doneKey = nextTok;
	}

		// anything else is garbage
	char *garbage = strtok( 0, DELIMITERS );
	if( doneKey ) {
		if( strcasecmp( doneKey, "DONE" ) != 0 ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", dagFile, lineNum, doneKey );
			debug_printf( DEBUG_QUIET, "%s\n", expectedSyntax.Value() );
			return false;
		}
		done = true;
	}
	if( garbage ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", dagFile, lineNum, garbage );
			debug_printf( DEBUG_QUIET, "%s\n", expectedSyntax.Value() );
			return false;
	}

	// check to see if this node name is also a splice name for this dag.
	if (dag->LookupSplice(MyString(nodeName), tmp) == 0) {
		debug_printf( DEBUG_QUIET, 
			  "%s (line %d): "
			  "Node name '%s' must not also be a splice name.\n",
			  dagFile, lineNum, nodeName );
		return false;
	}

	// If this is a "SUBDAG" line, generate the real submit file name.
	MyString nestedDagFile("");
	MyString dagSubmitFile(""); // must be outside if so it stays in scope
	if ( !strcasecmp( nodeTypeKeyword, "SUBDAG" ) ) {
			// Save original DAG file name (needed for rescue DAG).
		nestedDagFile = submitFile;

			// Generate the "real" submit file name (append ".condor.sub"
			// to the DAG file name).
		dagSubmitFile = submitFile;
		dagSubmitFile += ".condor.sub";
		submitFile = dagSubmitFile.Value();
	}

	// looks ok, so add it
	if( !AddNode( dag, nodeType, nodeName, directory,
				submitFile, NULL, NULL, done, whynot ) )
	{
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): %s\n",
					  dagFile, lineNum, whynot.Value() );
		debug_printf( DEBUG_QUIET, "%s\n", expectedSyntax.Value() );
		return false;
	}

	if ( nestedDagFile != "" ) {
		if ( !SetNodeDagFile( dag, nodeName, nestedDagFile.Value(),
					whynot ) ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): %s\n",
					  	dagFile, lineNum, whynot.Value() );
			return false;
		}
	}

	MyString errMsg;
	if ( !nodeDir.Cd2MainDir(errMsg) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: can't change to original directory: %s\n",
					errMsg.Value() );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//
// Function: parse_script
// Purpose:  Parse a line of the format:
//             SCRIPT (PRE|POST) JobName ScriptName Args ...
//
//-----------------------------------------------------------------------------
static bool 
parse_script(
	const char *endline,
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "SCRIPT (PRE|POST) JobName Script Args ...";
	Job * job = NULL;
	MyString whynot;

	//
	// Second keyword is either PRE or POST
	//
	bool   post;
	char * prepost = strtok (NULL, DELIMITERS);
	if (prepost == NULL) goto MISSING_PREPOST;
	else if (!strcasecmp (prepost, "PRE" )) post = false;
	else if (!strcasecmp (prepost, "POST")) post = true;
	else {
	MISSING_PREPOST:
		debug_printf( DEBUG_QUIET, "%s (line %d): "
					  "After specifying \"SCRIPT\", you must "
					  "indicate if you want \"PRE\" or \"POST\"\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}
	
	//
	// Third token is the JobName
	//
	const char *jobName = strtok(NULL, DELIMITERS);
	const char *jobNameOrig = jobName; // for error output
	const char * rest = jobName; // For subsequent tokens
	if (jobName == NULL) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else if (isReservedWord(jobName)) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): JobName cannot be a reserved word\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else {
		debug_printf(DEBUG_DEBUG_1, "jobName: %s\n", jobName);
		MyString tmpJobName = munge_job_name(jobName);
		jobName = tmpJobName.Value();

		job = dag->FindNodeByName( jobName );
		if (job == NULL) {
			debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobNameOrig );
			return false;
		}
	}
	
	//
	// The rest of the line is the script and args
	//
	
	// first, skip over the token we already read...
	while (*rest != '\0') rest++;
	
	// if we're not at the end of the line, move forward
	// one character so we're looking at the rest of the
	// line...
	if( rest < endline ) {
		rest++;
	} else {
		// if we're already at the end of the line, they
		// must not have given us any path to the script,
		// arguments, etc.  
		debug_printf( DEBUG_QUIET, "%s (line %d): "
					  "You named a %s script for node %s but "
					  "didn't provide a script filename\n",
					  filename, lineNumber, post ? "POST" : "PRE", 
					  jobNameOrig );
		exampleSyntax( example );
		return false;
	}
	
	// quick hack to get this working for extra
	// whitespace: make sure the "rest" of the line isn't
	// starting with any delimiter...
	while( rest[0] && isDelimiter(rest[0]) ) {
		rest++;
	}
	
	if( ! rest[0] ) {
		// this means we only saw whitespace after the
		// script.  however, because of how getline()
		// works and our comparison to endline above, we
		// should never hit this case.
		debug_printf( DEBUG_QUIET, "%s (line %d): "
					  "You named a %s script for node %s but "
					  "didn't provide a script filename\n",
					  filename, lineNumber, post ? "POST" : "PRE", 
					  jobNameOrig );
		exampleSyntax( example );
		return false;
	}
	
	if( !job->AddScript( post, rest, whynot ) ) {
		debug_printf( DEBUG_SILENT, "ERROR: %s (line %d): "
					  "failed to add %s script to node %s: %s\n",
					  filename, lineNumber, post ? "POST" : "PRE",
					  jobNameOrig, whynot.Value() );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_parent
// Purpose:  parse a line of the format PARENT node-name+ CHILD node-name+
//           where there can be one or more parent nodes and one or more
//           children nodes.
//
//-----------------------------------------------------------------------------
static bool 
parse_parent(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "PARENT p1 p2 p3 CHILD c1 c2 c3";
	Dag *splice_dag;
	
	List<Job> parents;
	ExtArray<Job*> *splice_initial;
	ExtArray<Job*> *splice_final;
	int i;
	Job *job;
	
	const char *jobName;
	
	// get the job objects for the parents
	while ((jobName = strtok (NULL, DELIMITERS)) != NULL &&
		   strcasecmp (jobName, "CHILD") != 0) {
		const char *jobNameOrig = jobName; // for error output
		MyString tmpJobName = munge_job_name(jobName);
		const char *jobName2 = tmpJobName.Value();

		// if splice name then deal with that first...
		if (dag->LookupSplice(jobName2, splice_dag) == 0) {

			// grab all of the final nodes of the splice and make them parents
			// for this job.
			splice_final = splice_dag->FinalRecordedNodes();

			// now add each final node as a parent
			for (i = 0; i < splice_final->length(); i++) {
				job = (*splice_final)[i];
				parents.Append(job);
			}

		} else {

			// orig code
			// if the name is not a splice, then see if it is a true node name.
			job = dag->FindNodeByName( jobName2 );
			if (job == NULL) {
				// oops, it was neither a splice nor a parent name, bail
				debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobNameOrig );
				return false;
			}
			parents.Append (job);
		}
	}
	
	// There must be one or more parent job names before
	// the CHILD token
	if (parents.Number() < 1) {
		debug_printf( DEBUG_QUIET, "%s (line %d): "
					  "Missing Parent Job names\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}
	
	if (jobName == NULL) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Expected CHILD token\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}
	
	List<Job> children;
	
	// get the job objects for the children
	while ((jobName = strtok (NULL, DELIMITERS)) != NULL) {
		const char *jobNameOrig = jobName; // for error output
		MyString tmpJobName = munge_job_name(jobName);
		const char *jobName2 = tmpJobName.Value();

		// if splice name then deal with that first...
		if (dag->LookupSplice(jobName2, splice_dag) == 0) {
			// grab all of the initial nodes of the splice and make them 
			// children for this job.

			debug_printf( DEBUG_DEBUG_1, "%s (line %d): "
				"Detected splice %s as a child....\n", filename, lineNumber,
					jobName2);

			splice_initial = splice_dag->InitialRecordedNodes();
			debug_printf( DEBUG_DEBUG_1, "Adding %d initial nodes\n", 
				splice_initial->length());

			// now add each initial node as a child
			for (i = 0; i < splice_initial->length(); i++) {
				job = (*splice_initial)[i];

				children.Append(job);
			}

		} else {

			// orig code
			// if the name is not a splice, then see if it is a true node name.
			job = dag->FindNodeByName( jobName2 );
			if (job == NULL) {
				// oops, it was neither a splice nor a child name, bail
				debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobNameOrig );
				return false;
			}
			children.Append (job);
		}
	}
	
	if (children.Number() < 1) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing Child Job names\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}
	
	//
	// Now add all the dependencies
	//
	
	Job *parent;
	parents.Rewind();
	while ((parent = parents.Next()) != NULL) {
		Job *child;
		children.Rewind();
		while ((child = children.Next()) != NULL) {
			if (!dag->AddDependency (parent, child)) {
				debug_printf( DEBUG_QUIET,
							  "ERROR: %s (line %d) failed to add dependency between "
							  "parent node \"%s\" and child node \"%s\"\n",
							  filename, lineNumber,
							  parent->GetJobName(), child->GetJobName() );
				return false;
			}
			debug_printf( DEBUG_DEBUG_3,
						  "Added Dependency PARENT: %s  CHILD: %s\n",
						  parent->GetJobName(), child->GetJobName() );
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_retry
// Purpose:  Parse a line of the format "Retry jobname num-times [UNLESS-EXIT 42]"
// 
//-----------------------------------------------------------------------------
static bool 
parse_retry(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char *example = "Retry JobName 3 [UNLESS-EXIT 42]";
	
	const char *jobName = strtok( NULL, DELIMITERS );
	const char *jobNameOrig = jobName; // for error output
	if( jobName == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	MyString tmpJobName = munge_job_name(jobName);
	jobName = tmpJobName.Value();
	
	Job *job = dag->FindNodeByName( jobName );
	if( job == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Unknown Job %s\n",
					  filename, lineNumber, jobNameOrig );
		return false;
	}
	
	char *s = strtok( NULL, DELIMITERS );
	if( s == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing Retry value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	
	char *tmp;
	job->retry_max = (int)strtol( s, &tmp, 10 );
	if( tmp == s ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Invalid Retry value \"%s\"\n",
					  filename, lineNumber, s );
		exampleSyntax( example );
		return false;
	}

    // Check for optional retry-abort value
    s = strtok( NULL, DELIMITERS );
    if ( s != NULL ) {
        if ( strcasecmp ( s, "UNLESS-EXIT" ) != 0 ) {
            debug_printf( DEBUG_QUIET, "%s (line %d) Invalid retry option: %s\n", 
                          filename, lineNumber, s);
            exampleSyntax( example );
            return false;
        }
        else {
            s = strtok( NULL, DELIMITERS );
            if ( s == NULL ) {
                debug_printf( DEBUG_QUIET, "%s (line %d) Missing parameter for UNLESS-EXIT\n",
                              filename, lineNumber);
                exampleSyntax( example );
                return false;
            } 
            char *unless_exit_end;
            int unless_exit = strtol(s, &unless_exit_end, 10);
            if (*unless_exit_end != 0) {
                debug_printf( DEBUG_QUIET, "%s (line %d) Bad parameter for UNLESS-EXIT: %s\n",
                              filename, lineNumber, s);
                exampleSyntax( example );
                return false;
            }
            job->have_retry_abort_val = true;
            job->retry_abort_val = unless_exit;
            debug_printf( DEBUG_DEBUG_1, "Retry Abort Value for %s is %d\n", 
                          jobName, job->retry_abort_val );
        }
    }
	
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_abort
// Purpose:  Parse a line of the format
// "ABORT-DAG-ON jobname node_exit_value [RETURN dag_return_value]"
// 
//-----------------------------------------------------------------------------
static bool 
parse_abort(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char *example = "ABORT-DAG-ON JobName 3 [RETURN 1]";
	
		// Job name.
	const char *jobName = strtok( NULL, DELIMITERS );
	const char *jobNameOrig = jobName; // for error output
	if( jobName == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	MyString tmpJobName = munge_job_name(jobName);
	jobName = tmpJobName.Value();
	
	Job *job = dag->FindNodeByName( jobName );
	if( job == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Unknown Job %s\n",
					  filename, lineNumber, jobNameOrig );
		return false;
	}
	
		// Node abort value.
	char *abortValStr = strtok( NULL, DELIMITERS );
	if( abortValStr == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing ABORT-ON node value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	
	int abortVal;
	char *tmp;
	abortVal = (int)strtol( abortValStr, &tmp, 10 );
	if( tmp == abortValStr ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Invalid ABORT-ON node value \"%s\"\n",
					  filename, lineNumber, abortValStr );
		exampleSyntax( example );
		return false;
	}

		// RETURN keyword.
	bool haveReturnVal = false;
	int returnVal = 9999; // assign value to avoid compiler warning
	const char *nextWord = strtok( NULL, DELIMITERS );
	if ( nextWord != NULL ) {
		if ( strcasecmp ( nextWord, "RETURN" ) != 0 ) {
			debug_printf( DEBUG_QUIET,
						"%s (line %d) Invalid ABORT-ON option: %s\n",
						filename, lineNumber, nextWord);
			exampleSyntax( example );
			return false;
		} else {

				// DAG return value.
			haveReturnVal = true;
			nextWord = strtok( NULL, DELIMITERS );
			if ( nextWord == NULL ) {
				debug_printf( DEBUG_QUIET,
							"%s (line %d) Missing parameter for ABORT-ON\n",
							filename, lineNumber);
				exampleSyntax( example );
				return false;
			}

			returnVal = strtol(nextWord, &tmp, 10);
			if ( tmp == nextWord ) {
				debug_printf( DEBUG_QUIET,
							"%s (line %d) Bad parameter for ABORT_ON: %s\n",
							filename, lineNumber, nextWord);
				exampleSyntax( example );
				return false;
			} else if ( (returnVal < 0) || (returnVal > 255) ) {
				debug_printf( DEBUG_QUIET,
							"%s (line %d) Bad return value for ABORT_ON "
							"(must be between 0 and 255): %s\n",
							filename, lineNumber, nextWord);
				return false;
			}
		}
	}

	job->abort_dag_val = abortVal;
	job->have_abort_dag_val = true;

	job->abort_dag_return_val = returnVal;
	job->have_abort_dag_return_val = haveReturnVal;

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_dot
// Purpose:  Parse a line of the format:
//             Dot dotfile [UPDATE | DONT-UPDATE] [OVERWRITE | DONT-OVERWRITE]
//                 [INCLUDE dot-file-header]
//           This command will tell DAGMan to generate dot files the show the
//           state of the DAG. 
// 
//-----------------------------------------------------------------------------
static bool parse_dot(Dag *dag, const char *filename, int lineNumber)
{
	const char *example = "Dot dotfile [UPDATE | DONT-UPDATE] "
		                  "[OVERWRITE | DONT-OVERWRITE] "
		                  "[INCLUDE <dot-file-header>]";
	
	char *dot_file_name = strtok(NULL, DELIMITERS);
	if (dot_file_name == NULL) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Missing dot file name,\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	char *token;
	while ((token = strtok(NULL, DELIMITERS)) != NULL) {
		if (strcasecmp(token, "UPDATE") == 0) {
			dag->SetDotFileUpdate(true);
		} else if (strcasecmp(token, "DONT-UPDATE") == 0) {
			dag->SetDotFileUpdate(false);
		} else if (strcasecmp(token, "OVERWRITE") == 0) {
			dag->SetDotFileOverwrite(true);
		} else if (strcasecmp(token, "DONT-OVERWRITE") == 0) {
			dag->SetDotFileOverwrite(false);
		} else if (strcasecmp(token, "INCLUDE") == 0) {
			token = strtok(NULL, DELIMITERS);
			if (token == NULL) {
				debug_printf(DEBUG_QUIET, "%s (line %d): Missing include"
							 " file name.\n", filename, lineNumber);
				exampleSyntax(example);
				return false;
			} else {
				dag->SetDotIncludeFileName(token);
			}
		}
	}

	dag->SetDotFileName(dot_file_name);
	return true;
}


//-----------------------------------------------------------------------------
// 
// Function: parse_vars
// Purpose:  Parses a line of named variables that will be made available to a job's
//           submit description file
//           The format of this line must be
//           Vars JobName VarName1="value1" VarName2="value2" etc
//           Whitespace surrounding the = sign is permissible
//-----------------------------------------------------------------------------
static bool parse_vars(Dag *dag, const char *filename, int lineNumber) {
	const char* example = "Vars JobName VarName1=\"value1\" VarName2=\"value2\"";
	MyString varName;
	MyString varValue;

	const char *jobName = strtok( NULL, DELIMITERS );
	const char *jobNameOrig = jobName; // for error output
	if(jobName == NULL) {
		debug_printf(DEBUG_QUIET, "%s (line %d): Missing job name\n", filename, lineNumber);
		exampleSyntax(example);
		return false;
	}

	MyString tmpJobName = munge_job_name(jobName);
	jobName = tmpJobName.Value();

	Job *job = dag->FindNodeByName( jobName );
	if(job == NULL) {
		debug_printf(DEBUG_QUIET, "%s (line %d): Unknown Job %s\n",
					filename, lineNumber, jobNameOrig);
		return false;
	}

	char *str = strtok(NULL, "\n"); // just get all the rest -- we'll be doing this by hand

	int numPairs;
	for(numPairs = 0; ; numPairs++) {  // for each name="value" pair

			// Fix PR 854 (multiple macronames per VARS line don't work).
		varName = "";
		varValue = "";

		if(str == NULL) // this happens when the above strtok returns NULL
			break;
		while(isspace(*str))
			str++;
		if(*str == '\0') {
			break;
		}

		// copy name char-by-char until we hit a symbol or whitespace
		// names are limited to alphanumerics and underscores
		while( isalnum(*str) || *str == '_' ) {
			varName += *str++;
		}

		if(varName.Length() == '\0') { // no alphanumeric symbols at all were written into name,
		                      // just something weird, which we'll print
			debug_printf(DEBUG_QUIET, "%s (line %d): Unexpected symbol: \"%c\"\n", filename,
				lineNumber, *str);
			return false;
		}
		
		// burn through any whitespace there may be afterwards
		while(isspace(*str))
			str++;
		if(*str != '=') {
			debug_printf(DEBUG_QUIET, "%s (line %d): No \"=\" for \"%s\"\n", filename,
				lineNumber, varName.Value());
			return false;
		}
		str++;
		while(isspace(*str))
			str++;
		
		if(*str != '"') {
			debug_printf(DEBUG_QUIET, "%s (line %d): %s's value must be quoted\n", filename,
				lineNumber, varName.Value());
			return false;
		}

		// now it's time to read in all the data until the next double quote, while handling
		// the two escape sequences: \\ and \"
		bool stillInQuotes = true;
		bool escaped       = false;
		do {
			varValue += *(++str);
			
			if(*str == '\0') {
				debug_printf(DEBUG_QUIET, "%s (line %d): Missing end quote\n", filename,
					lineNumber);
				return false;
			}
			
			if(!escaped) {
				if(*str == '"') {
					// we don't want that last " in the string
					varValue.setChar( varValue.Length() - 1, '\0' );
					stillInQuotes = false;
				} else if(*str == '\\') {
					// on the next pass it will be filled in appropriately
					varValue.setChar( varValue.Length() - 1, '\0' );
					escaped = true;
					continue;
				} else if(*str == '\'') { // these would mess the command line up 
				                          // later on
					debug_printf(DEBUG_QUIET,
						"%s (line %d): sinqle quotes are not allowed in values.\n",
						filename, lineNumber);
					return false;
				}
			} else {
				if(*str != '\\' && *str != '"') {
					debug_printf(DEBUG_QUIET, "%s (line %d): Unknown escape sequence "
						"\"\\%c\"\n", filename, lineNumber, *str);
					return false;
				}
				escaped = false; // being escaped only lasts for one character
			}
		} while(stillInQuotes);

		*str++;

			// Check for illegal variable name.
		MyString tmpName(varName);
		tmpName.lower_case();
		if ( tmpName.find( "queue" ) == 0 ) {
			debug_printf(DEBUG_QUIET, "Illegal variable name: %s; variable "
						"names cannot begin with \"queue\"\n", varName.Value() );
			return false;
		}
		debug_printf(DEBUG_DEBUG_1, "Argument added, Name=\"%s\"\tValue=\"%s\"\n", varName.Value(), varValue.Value());
		job->varNamesFromDag->Append(new MyString(varName));
		job->varValsFromDag->Append(new MyString(varValue));
	}

	if(numPairs == 0) {
		debug_printf(DEBUG_QUIET, "%s (line %d): No valid name-value pairs\n", filename, lineNumber);
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_priority
// Purpose:  Parses a line specifying the priority of a node
//           The format of this line must be
//           Priority <JobName> <Value>
//-----------------------------------------------------------------------------
static bool 
parse_priority(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "PRIORITY JobName Value";
	Job * job = NULL;

	//
	// Next token is the JobName
	//
	const char *jobName = strtok(NULL, DELIMITERS);
	const char *jobNameOrig = jobName; // for error output
	if (jobName == NULL) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else if (isReservedWord(jobName)) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): JobName cannot be a reserved word\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else {
		debug_printf(DEBUG_DEBUG_1, "jobName: %s\n", jobName);
		MyString tmpJobName = munge_job_name(jobName);
		jobName = tmpJobName.Value();

		job = dag->FindNodeByName( jobName );
		if (job == NULL) {
			debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobNameOrig );
			return false;
		}
	}

	//
	// Next token is the priority value.
	//
	const char *valueStr = strtok(NULL, DELIMITERS);
	if ( valueStr == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing PRIORITY value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int priorityVal;
	char *tmp;
	priorityVal = (int)strtol( valueStr, &tmp, 10 );
	if( tmp == valueStr ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Invalid PRIORITY value \"%s\"\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	valueStr = strtok(NULL, DELIMITERS);
	if ( valueStr != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Extra token (%s) on PRIORITY line\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}

	if ( job->_hasNodePriority && job->_nodePriority != priorityVal ) {
		debug_printf( DEBUG_NORMAL, "Warning: new priority %d for node %s "
					"overrides old value %d\n", priorityVal,
					job->GetJobName(), job->_nodePriority );
	}
	job->_hasNodePriority = true;
	job->_nodePriority = priorityVal;

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_category
// Purpose:  Parses a line specifying the type of a node
//           The format of this line must be
//           Category <JobName> <Category>
//			 No whitespace is allowed in the category name
//-----------------------------------------------------------------------------
static bool 
parse_category(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "CATEGORY JobName TypeName";
	Job * job = NULL;

	//
	// Next token is the JobName
	//
	const char *jobName = strtok(NULL, DELIMITERS);
	const char *jobNameOrig = jobName; // for error output
	if (jobName == NULL) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else if (isReservedWord(jobName)) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): JobName cannot be a reserved word\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else {
		debug_printf(DEBUG_DEBUG_1, "jobName: %s\n", jobName);
		MyString tmpJobName = munge_job_name(jobName);
		jobName = tmpJobName.Value();

		job = dag->FindNodeByName( jobName );
		if (job == NULL) {
			debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobNameOrig );
			return false;
		}
	}

	//
	// Next token is the category name.
	//
	const char *categoryName = strtok(NULL, DELIMITERS);
	if ( categoryName == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing CATEGORY name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	const char *tmpStr = strtok(NULL, DELIMITERS);
	if ( tmpStr != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Extra token (%s) on CATEGORY line\n",
					  filename, lineNumber, tmpStr );
		exampleSyntax( example );
		return false;
	}

	job->SetCategory( categoryName, dag->_catThrottles );

	return true;
}

static bool
parse_splice(
	Dag *dag,
	const char *filename,
	int lineNumber)
{
	const char *example = "SPLICE SpliceName SpliceFileName [DIR directory]";
	Dag *splice_dag = NULL;
	MyString spliceName, spliceFile;
	MyString errMsg;

	//
	// Next token is the splice name
	// 
	spliceName = strtok(NULL, DELIMITERS);
	if ( spliceName == "" ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing SPLICE name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	// Check to make sure we don't already have a node with the name of the
	// splice.
	if (dag->NodeExists(spliceName.Value()) == true) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): "
					  " Splice name '%s' must not also be a node name.\n",
					  filename, lineNumber, spliceName.Value() );
		return false;
	}

	/* "push" it onto the scoping "stack" which will be used later to
		munge the names of the nodes to have the splice name in them so
		the same splice dag file with different splice names don't conflict.
	*/
	_spliceScope.add(strdup(munge_job_name(spliceName.Value()).Value()));

	//
	// Next token is the splice file name
	// 
	spliceFile = strtok(NULL, DELIMITERS);
	if ( spliceFile == "" ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing SPLICE file name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	//
	// next token (if any) is "DIR"
	//
	TmpDir spliceDir;
	MyString dirTok = strtok( NULL, DELIMITERS );
	MyString directory = ".";

	dirTok.upper_case();
	if ( dirTok == "DIR" ) {
		// parse the directory name
		directory = strtok( NULL, DELIMITERS );
		if ( directory == "" ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): DIR requires a directory "
						"specification\n", filename, lineNumber);
			debug_printf( DEBUG_QUIET, "%s\n", example );
			return false;
		}

	}

	// 
	// anything else is garbage
	//
	MyString garbage = strtok( 0, DELIMITERS );
	if( garbage != "" ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", filename, lineNumber, 
						  garbage.Value() );
			debug_printf( DEBUG_QUIET, "%s\n", example );
			return false;
	}

	/* make a new dag to put everything into */

	/* parse increments this number, however, we want the splice nodes to
		be munged into the numeric identification of the invoking dag, so
		decrement it here so when it is incremented, nothing happened. */
	--_thisDagNum;

	// This "copy" is tailored to be correct according to Dag::~Dag()
	splice_dag = new Dag(	dag->DagFiles(),
							dag->MaxJobsSubmitted(),
							dag->MaxPreScripts(),
							dag->MaxPostScripts(),
							dag->AllowLogError(),
							dag->UseDagDir(),
							dag->MaxIdleJobProcs(),
							dag->RetrySubmitFirst(),
							dag->RetryNodeFirst(),
							dag->CondorRmExe(),
							dag->StorkRmExe(),
							dag->DAGManJobId(),
							dag->ProhibitMultiJobs(),
							dag->SubmitDepthFirst(),
							false);
	
	// initialize whatever the DIR line was, or defaults to, here.
	splice_dag->SetDirectory(directory);

	debug_printf(DEBUG_VERBOSE, "Parsing Splice %s in directory %s with "
		"file %s\n", spliceName.Value(), directory.Value(),
		spliceFile.Value());

	// CD into the DIR directory so we can continue parsing.
	// This must be done AFTER the DAG is created due to the DAG object
	// doing its own chdir'ing while looking for log files.
	if ( !spliceDir.Cd2TmpDir(directory.Value(), errMsg) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: can't change to directory %s: %s\n",
					directory.Value(), errMsg.Value() );
		return false;
	}

	// parse the splice file into a separate dag.
	if (!parse(splice_dag, spliceFile.Value(), _useDagDir)) {
		debug_error(1, DEBUG_QUIET, "Failed to parse splice %s in file %s\n",
			spliceName.Value(), spliceFile.Value());
		return false;
	}

	// CD back to main dir to keep processing.
	if ( !spliceDir.Cd2MainDir(errMsg) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: can't change to original directory: %s\n",
					errMsg.Value() );
		return false;
	}

	// munge the splice name
	spliceName = munge_job_name(spliceName.Value());

	// XXX I'm not sure this goes here quite yet....
	debug_printf(DEBUG_DEBUG_1, "Splice scope is: %s\n", 
		current_splice_scope().Value());
	splice_dag->PrefixAllNodeNames(MyString(current_splice_scope()));
	splice_dag->_catThrottles.PrefixAllCategoryNames(
				MyString(current_splice_scope()));

	// Print out a useful piece of debugging...
	if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) ) {
		splice_dag->PrintJobList();
	}

	// associate the splice_dag with its name in _this_ dag, later I'll merge
	// the nodes from this splice into _this_ dag.
	if (dag->InsertSplice(spliceName, splice_dag) == -1) {
		debug_printf( DEBUG_QUIET, "Splice name '%s' used for multiple "
			"splices. Splice names must be unique per dag file.\n", 
			spliceName.Value());
		return false;
	}

	// For now, we only keep track of the splice levels just below this dag.
	dag->LiftChildSplices();

	debug_printf(DEBUG_DEBUG_1, "Done parsing splice %s\n", spliceName.Value());

	// pop the just pushed value off of the end of the ext array
	free(_spliceScope[_spliceScope.getlast()]);
	_spliceScope.truncate(_spliceScope.getlast() - 1);

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_maxjobs
// Purpose:  Parses a line specifying the maximum number of jobs for
//           a given node category.
//           The format of this line must be
//           MaxJobs <Category> <Value>
//			 No whitespace is allowed in the category name
//-----------------------------------------------------------------------------
static bool 
parse_maxjobs(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "MAXJOBS TypeName Value";

	//
	// Next token is the category name.
	//
	const char *categoryName = strtok(NULL, DELIMITERS);
	if ( categoryName == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing MAXJOBS category name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	//
	// Next token is the maxjobs value.
	//
	const char *valueStr = strtok(NULL, DELIMITERS);
	if ( valueStr == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Missing MAXJOBS value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int maxJobsVal;
	char *tmp;
	maxJobsVal = (int)strtol( valueStr, &tmp, 10 );
	if( tmp == valueStr ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Invalid MAXJOBS value \"%s\"\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}
	if ( maxJobsVal < 0 ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): MAXJOBS value must be non-negative\n",
					  filename, lineNumber );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	valueStr = strtok(NULL, DELIMITERS);
	if ( valueStr != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Extra token (%s) on MAXJOBS line\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}

	MyString	tmpName( categoryName );
	dag->_catThrottles.SetThrottle( &tmpName, maxJobsVal );

	return true;
}

static MyString munge_job_name(const char *jobName)
{
		//
		// Munge the node name if necessary.
		//
	MyString newName;

	if ( _mungeNames ) {
		newName = MyString(_thisDagNum) + "." + jobName;
	} else {
		newName = jobName;
	}

	return newName;
}

static MyString current_splice_scope(void)
{
	int i;
	MyString scope;
	MyString tmp;

	for (i = 0; i < _spliceScope.length(); i++)
	{
		tmp = _spliceScope[i];
		// While a natural choice might have been : as a splice scoping
		// separator, this character was chosen because it is a valid character
		// on all the file systems we use (whereas : can't be a file system
		// character on windows). The plus, and really anything other than :,
		// isn't the best choice. Sorry.
		scope += tmp + "+";
	}

	return scope;
}



