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

#include "job.h"
#include "parse.h"
#include "util.h"
#include "debug.h"
#include "list.h"
#include "util_lib_proto.h"
#include "dagman_commands.h"

static const char   COMMENT    = '#';
static const char * DELIMITERS = " \t";
static const int    MAX_LENGTH = 255;

static bool parse_node( Job::job_type_t nodeType, char* nodeTypeKeyword,
						Dag* dag, char* dagFile, int lineNum );

static bool parse_script(char *endline, Dag *dag, char *filename, int lineNumber);
static bool parse_parent(Dag *dag, char *filename, int lineNumber);
static bool parse_retry(Dag *dag, char *filename, int lineNumber);
static bool parse_dot(Dag *dag, char *filename, int lineNumber);
static bool parse_vars(Dag *dag, char *filename, int lineNumber);


void exampleSyntax (const char * example) {
    debug_printf( DEBUG_QUIET, "Example syntax is: %s\n", example);
}


bool
isKeyWord( const char *token )
{
    static const char * keywords[] = {
        "JOB", "PARENT", "CHILD", "PRE", "POST", "DONE", "Retry", "SCRIPT",
		"DOT", "DAP"
    };
    static const unsigned int numKeyWords = sizeof(keywords) / 
		                                    sizeof(const char *);

    for (unsigned int i = 0 ; i < numKeyWords ; i++) {
        if (!strcasecmp (token, keywords[i])) return true;
    }
    return false;
}


bool
isDelimiter( char c ) {
	char* tmp = strchr( DELIMITERS, (int)c );
	return tmp ? true : false;
}


//-----------------------------------------------------------------------------
bool parse (char *filename, Dag *dag) {
	ASSERT( dag != NULL );

	FILE *fp = fopen(filename, "r");
	if(fp == NULL) {
		if(DEBUG_LEVEL(DEBUG_QUIET)) {
			debug_printf( DEBUG_QUIET, "Could not open file %s for input\n", filename);
			return false;
		}
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

		char *token = strtok(line, DELIMITERS);
		bool parsed_line_successfully;

		// Handle a Job spec
		// Example Syntax is:  JOB j1 j1.condor [DONE]
		//
		if(strcasecmp(token, "JOB") == 0) {
			parsed_line_successfully = parse_node( Job::TYPE_CONDOR, token,
												   dag, filename, lineNumber );
		}

		// Handle a DaP spec
		// Example Syntax is:  DAP j1 j1.dapsubmit [DONE]
		//
		else if (strcasecmp(token, "DAP") == 0) {
			parsed_line_successfully = parse_node( Job::TYPE_STORK, token,
												   dag, filename, lineNumber );
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
		
		// None of the above means that there was bad input.
		else {
			debug_printf( DEBUG_QUIET, "%s (line %d): "
				"Expected JOB, SCRIPT, or PARENT token\n",
				filename, lineNumber );
			parsed_line_successfully = false;
		}
		
		if (!parsed_line_successfully) {
			fclose(fp);
			return false;
		}
	}
	return true;
}


static bool 
parse_node( Job::job_type_t nodeType, char* nodeTypeKeyword, Dag* dag,
			char* dagFile, int lineNum )
{
	MyString example;
	MyString whynot;
	bool done = false;

	MyString expectedSyntax;
	expectedSyntax.sprintf( "Expected syntax: %s nodename submitfile [DONE]",
							nodeTypeKeyword );

		// NOTE: fear not -- any missing tokens resulting in NULL
		// strings will be error-handled correctly by AddNode()

		// first token is the node name
	char *nodeName = strtok( NULL, DELIMITERS );
		// next token is the submit file name
	char *submitFile = strtok( NULL, DELIMITERS );
		// last (optional) token is DONE marker
	char *doneKey = strtok( 0, DELIMITERS );
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
	if( !AddNode( nodeType, nodeName, submitFile, NULL, NULL, done, whynot ) )
	{
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): %s\n",
					  dagFile, lineNum, whynot.Value() );
		debug_printf( DEBUG_QUIET, "%s\n", expectedSyntax.Value() );
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
	char *endline,
	Dag  *dag, 
	char *filename, 
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
	char *jobName = strtok(NULL, DELIMITERS);
	if (jobName == NULL) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else if (isKeyWord(jobName)) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): JobName cannot be a keyword\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	} else {
		debug_printf(DEBUG_QUIET, "jobName: %s\n", jobName);
		job = dag->GetJob(jobName);
		if (job == NULL) {
			debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobName );
			return false;
		}
	}
	
	//
	// The rest of the line is the script and args
	//
	char * rest = jobName;
	
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
					  jobName );
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
					  jobName );
		exampleSyntax( example );
		return false;
	}
	
	if( !job->AddScript( post, rest, whynot ) ) {
		debug_printf( DEBUG_SILENT, "ERROR: %s (line %d): "
					  "failed to add %s script to node %s: %s\n",
					  filename, lineNumber, post ? "POST" : "PRE",
					  jobName, whynot.Value() );
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
	char *filename, 
	int  lineNumber)
{
	const char * example = "PARENT p1 p2 p3 CHILD c1 c2 c3";
	
	List<Job> parents;
	
	char *jobName;
	
	while ((jobName = strtok (NULL, DELIMITERS)) != NULL &&
		   strcasecmp (jobName, "CHILD") != 0) {
		Job * job = dag->GetJob(jobName);
		if (job == NULL) {
			debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobName );
			return false;
		}
		parents.Append (job);
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
	
	while ((jobName = strtok (NULL, DELIMITERS)) != NULL) {
		Job * job = dag->GetJob(jobName);
		if (job == NULL) {
			debug_printf( DEBUG_QUIET, 
						  "%s (line %d): Unknown Job %s\n",
						  filename, lineNumber, jobName );
			return false;
		}
		children.Append (job);
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
							  "ERROR: failed to add dependency between "
							  "parent node \"%s\" and child node \"%s\"\n",
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
	char *filename, 
	int  lineNumber)
{
	const char *example = "Retry JobName 3 [UNLESS-EXIT 42]";
	
	char *jobName = strtok( NULL, DELIMITERS );
	if( jobName == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "%s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	
	Job *job = dag->GetJob( jobName );
	if( job == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "%s (line %d): Unknown Job %s\n",
					  filename, lineNumber, jobName );
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
            debug_printf( DEBUG_NORMAL, "%s (line %d) Invalid retry option: %s\n", 
                          filename, lineNumber, s);
            exampleSyntax( example );
            return false;
        }
        else {
            s = strtok( NULL, DELIMITERS );
            if ( s == NULL ) {
                debug_printf( DEBUG_NORMAL, "%s (line %d) Missing parameter for UNLESS-EXIT\n",
                              filename, lineNumber);
                exampleSyntax( example );
                return false;
            } 
            char *unless_exit_end;
            int unless_exit = strtol(s, &unless_exit_end, 10);
            if (*unless_exit_end != 0) {
                debug_printf( DEBUG_NORMAL, "%s (line %d) Bad parameter for UNLESS-EXIT: %s\n",
                              filename, lineNumber, s);
                exampleSyntax( example );
                return false;
            }
            job->have_retry_abort_val = true;
            job->retry_abort_val = unless_exit;
            debug_printf( DEBUG_QUIET, "Retry Abort Value for %s is %d\n", 
                          jobName, job->retry_abort_val );
        }
    }
	
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
static bool parse_dot(Dag *dag, char *filename, int lineNumber)
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
static bool parse_vars(Dag *dag, char *filename, int lineNumber) {
	const char* example = "Vars JobName VarName1=\"value1\" VarName2=\"value2\"";
	const int maxLen = 5096;
	char name[maxLen];
	char value[maxLen];

	char *jobName = strtok( NULL, DELIMITERS );
	if(jobName == NULL) {
		debug_printf(DEBUG_QUIET, "%s (line %d): Missing job name\n", filename, lineNumber);
		exampleSyntax(example);
		return false;
	}

	Job *job = dag->GetJob(jobName);
	if(job == NULL) {
		debug_printf(DEBUG_QUIET, "%s (line %d): Unknown Job %s\n", filename, lineNumber, jobName);
		return false;
	}

	char *str = strtok(NULL, "\n"); // just get all the rest -- we'll be doing this by hand

	int numPairs;
	for(numPairs = 0; ; numPairs++) {  // for each name="value" pair
		if(str == NULL) // this happens when the above strtok returns NULL
			break;
		while(isspace(*str))
			str++;
		if(*str == '\0') {
			break;
		}

		// copy name char-by-char until we hit a symbol or whitespace
		char* nameWriter = name;
		while(isalpha(*str) && nameWriter-name < maxLen)
			*nameWriter++ = *str++;
		*nameWriter = '\0';

		if(name[0] == '\0') { // no alphanumeric symbols at all were written into name,
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
				lineNumber, name);
			return false;
		}
		str++;
		while(isspace(*str))
			str++;
		
		if(*str != '"') {
			debug_printf(DEBUG_QUIET, "%s (line %d): %s's value must be quoted\n", filename,
				lineNumber, name);
			return false;
		}

		// now it's time to read in all the data until the next double quote, while handling
		// the two escape sequences: \\ and \"
		bool stillInQuotes = true;
		bool escaped       = false;
		int i = 0;
		do {
			value[i++] = *(++str);
			if(i >= maxLen) {
				debug_printf(DEBUG_QUIET, "%s (line %d): Sorry, value lengths are restricted "
					"to %d characters maximum\n", filename, lineNumber, maxLen);
				return false;
			}
			
			if(*str == '\0') {
				debug_printf(DEBUG_QUIET, "%s (line %d): Missing end quote\n", filename,
					lineNumber);
				return false;
			}
			
			if(!escaped) {
				if(*str == '"') {
					i--; // we don't want that last " in the string
					stillInQuotes = false;
				} else if(*str == '\\') {
					i--; // on the next pass it will be filled in appropriately
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
		value[i] = '\0';

		*str++;
		
		debug_printf(DEBUG_VERBOSE, "Argument added, Name=\"%s\"\tValue=\"%s\"\n", name, value);
		job->varNamesFromDag->Append(new MyString(name));
		job->varValsFromDag->Append(new MyString(value));
	}

	if(numPairs == 0) {
		debug_printf(DEBUG_QUIET, "%s (line %d): No valid name-value pairs\n", filename, lineNumber);
		return false;
	}

	return true;
}

