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

#include "node.h"
#include "parse.h"
#include "debug.h"
#include "dagman_commands.h"
#include "tmp_dir.h"
#include "basename.h"
#include "condor_getcwd.h"

namespace deep = DagmanDeepOptions;

static const char   COMMENT    = '#';
static const char * DELIMITERS = " \t";
static const char * ILLEGAL_CHARS = "+";

static const std::set<istring> NODE_KEYWORDS = {
	"JOB",
	"FINAL",
	"PROVISIONER",
	"SERVICE",
};

static std::vector<char*> _spliceScope;
static bool _useDagDir = false;
static bool _useDirectSubmit = true;
static bool _appendVars = false;

// _thisDagNum will be incremented for each DAG specified on the
// condor_submit_dag command line.
static int _thisDagNum = -1;
static bool _mungeNames = true;

// DAGMan global schedd object. Only used here to hand off to a splice DAG.
DCSchedd *_schedd = NULL;

static bool parse_subdag( Dag *dag,
						const char* nodeTypeKeyword,
						const char* dagFile, int lineNum,
						const char *directory);

static bool parse_node( Dag *dag, const char * nodeName, 
						const char * submitFileOrSubmitDesc,
						const char* nodeTypeKeyword,
						const char* dagFile, int lineNum,
						const char *directory, const char *inlineOrExt,
						const char *submitOrDagFile);

static bool pre_parse_node(std::string & nodename, const char * &submitFile);

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
static bool parse_splice(const Dagman& dm, Dag *dag, const char *filename, int lineNumber);
static bool parse_node_status_file(Dag  *dag, const char *filename,
		int  lineNumber);
static bool parse_save_point_file(Dag *dag, const char* filename, int lineNumber);
static bool parse_reject(Dag  *dag, const char *filename,
		int  lineNumber);
static bool parse_jobstate_log(Dag  *dag, const char *filename,
		int  lineNumber);
static bool parse_pre_skip(Dag *dag, const char* filename,
		int lineNumber);
static bool parse_done(Dag  *dag, const char *filename, int  lineNumber);
static bool parse_connect( Dag  *dag, const char *filename, int  lineNumber );
static bool parse_pin_in_out( Dag  *dag, const char *filename,
			int  lineNumber, bool isPinIn );
static bool parse_include(const Dagman& dm, Dag  *dag, const char *filename, int  lineNumber );
static std::string munge_node_name(const char *nodeName);

static std::string current_splice_scope(void);

static bool get_next_var( const char *filename, int lineNumber, char *&str,
			std::string &varName, std::string &varValue );

void exampleSyntax (const char * example) {
    debug_printf( DEBUG_QUIET, "Example syntax is: %s\n", example);
}

bool
isReservedWord( const char *token )
{
    static const char * keywords[] = { "PARENT", "CHILD", DAG::ALL_NODES };
    for (auto & keyword : keywords) {
        if (!strcasecmp (token, keyword)) {
    		debug_printf( DEBUG_QUIET,
						"ERROR: token (%s) is a reserved word\n", token );
			return true;
		}
    }
    return false;
}

bool
isDelimiter( char c ) {
	char const* tmp = strchr( DELIMITERS, (int)c );
	return tmp ? true : false;
}

std::string
makeString(char *s) { 
	return std::string(s ? s : "");
}

//-----------------------------------------------------------------------------
void parseSetDoNameMunge(bool doit)
{
	_mungeNames = doit;
}

static bool get_inline_desc_end(const char* desc, std::string& end) {
	if ( ! desc) { return false; }
	else if (*desc == '{') { end = "}"; return true; }

	std::string start(desc);
	if (starts_with(start, "@=")) {
		end = start.length() > 2 ? "@" + start.substr(start.find("=") + 1) : "";
		return true;
	}

	return false;
}

static std::string parse_inline_desc(MacroStreamYourFile& ms, int gl_opts, const std::string& end, std::string& error, char* endline) {
	std::string desc;
	char* line;
	bool found_end = false;
	bool add_queue = true;

	if (end.empty()) {
		error = "No inline description closing token specified (end=TOKEN)";
		return desc;
	}

	while ((line=ms.getline(gl_opts)) != nullptr) {
		if (line[0] == 0)       continue;  // Ignore blank lines
		if (line[0] == COMMENT) continue;  // Ignore comments
		if (starts_with(line, "queue")) { add_queue = false; }
		if (starts_with(line, end)) {
			endline = line;
			found_end = true;
			break;
		}
		desc += std::string(line) + "\n";
	}

	debug_printf(DEBUG_DEBUG_1, "Inline Description Parse End Line<%s>\n", endline);

	if ( ! found_end) {
		error = "Missing inline description closing token: " + end;
	} else if (add_queue) { desc += "queue\n"; }

	return desc;
}

//-----------------------------------------------------------------------------
bool parse(const Dagman& dm, Dag *dag, const char * filename, bool incrementDagNum)
{
	ASSERT( dag != NULL );

	if ( incrementDagNum ) {
		++_thisDagNum;
	}

	_useDagDir = dm.options[deep::b::UseDagDir];
	_useDirectSubmit = dm.options[deep::i::SubmitMethod] == 1;
	_appendVars = dm.doAppendVars;
	_schedd = dm._schedd;

		//
		// If useDagDir is true, we have to cd into the directory so we can
		// parse the submit files correctly.
		// 
	std::string		tmpDirectory;
	const char *	tmpFilename = filename;
	TmpDir		dagDir;

	if ( _useDagDir ) {
		tmpDirectory = condor_dirname( filename );

		std::string	errMsg;
		if ( !dagDir.Cd2TmpDir( tmpDirectory.c_str(), errMsg ) ) {
			debug_printf( DEBUG_QUIET,
					"ERROR: Could not change to DAG directory %s: %s\n",
					tmpDirectory.c_str(), errMsg.c_str() );
			return false;
		}
		tmpFilename = condor_basename( filename );
	}
	std::string tmpcwd;
	condor_getcwd( tmpcwd );

	FILE *fp = safe_fopen_wrapper_follow(tmpFilename, "r");
	if(fp == NULL) {
		std::string cwd;
		condor_getcwd( cwd );
		debug_printf( DEBUG_QUIET, "ERROR: Could not open file %s for input "
					"(cwd %s) (errno %d, %s)\n", tmpFilename,
					cwd.c_str(), errno, strerror(errno));
		return false;
   	}

	char *line;
	//int lineNumber = 0;

	MACRO_SOURCE src = { false, false, 0, 0, 0, 0 };
	MacroStreamYourFile ms(fp, src);
	src.line = 0;
	src.id = 4; // index into macro_set.sources. 4 is the first index after the pre-defined ones
	int gl_opts = 3; // CONFIG_GETLINE_OPT_COMMENT_DOESNT_CONTINUE | CONFIG_GETLINE_OPT_CONTINUE_MAY_BE_COMMENTED_OUT;

	//
	// We now parse in two passes, so that commands such as PARENT..CHILD
	// can come before the relevant nodes are defined (see gittrac #5732).
	// The first pass parses commands that define nodes (JOB, DATA, FINAL,
	// SPLICE, SUBDAG); the second pass parses everything else.
	//

	//
	// PASS 1.
	// This loop will read every line of the input file
	//
	while ( ((line=ms.getline(gl_opts)) != NULL) ) {
		//std::string varline(line);
		int lineNumber = src.line;

		//
		// Find the terminating '\0'
		//
		//char * endline = line;
		//while (*endline != '\0') endline++;


		// Note that getline will truncate leading spaces (as defined by isspace())
		// so we don't need to do that before checking for empty lines or comments.
		if (line[0] == 0)       continue;  // Ignore blank lines
		if (line[0] == COMMENT) continue;  // Ignore comments

		debug_printf( DEBUG_DEBUG_3, "Parsing line <%s>\n", line );

		char *token = strtok(line, DELIMITERS);
		if ( !token ) continue; // so Coverity is happy

		bool parsed_line_successfully = false;

		// JOB, SERVICE, PROVISIONER, FINAL are types of nodes...
		// parse them the same way with possibility of inline submit descriptions
		if (NODE_KEYWORDS.contains(token)) {
			std::string keyword(token);
			std::string nodename;
			const char * subfile = NULL;
			pre_parse_node(nodename, subfile);
			bool inline_parsed = true;
			std::string inline_end;
			if (get_inline_desc_end(subfile, inline_end)) {
				std::string err;
				std::string desc = parse_inline_desc(ms, gl_opts, inline_end, err, line);
				if ( ! err.empty()) {
					debug_printf(DEBUG_NORMAL, "ERROR (line %d): %s\n", lineNumber, err.c_str());
					inline_parsed = false;
				} else {
					dag->InlineDescriptions.insert(std::make_pair(nodename, desc));
				}

				subfile = "InlineSubmitDesc"; // Set a pseudo filename
				token = strtok(line, DELIMITERS); // Continue tokenizing after end token
			}

			if (inline_parsed) {
				parsed_line_successfully = parse_node(dag, nodename.c_str(), subfile, keyword.c_str(),
				                                      filename, lineNumber, tmpDirectory.c_str(),
				                                      "", "submitfile");
				std::string temp_nodename = munge_node_name(nodename.c_str());
				Node *node = dag->FindAllNodesByName(temp_nodename.c_str(), "", filename, lineNumber);
				if (node) {
					if (dag->InlineDescriptions.contains(nodename)) {
						node->inline_desc = dag->InlineDescriptions[nodename];
					}
				} else {
					debug_printf(DEBUG_NORMAL, "Error: unable to find node %s in our DAG structure, aborting.\n",
					             nodename.c_str());
					parsed_line_successfully = false;
				}
			}
		}

		// Handle a SUBDAG spec
		else if	(strcasecmp(token, "SUBDAG") == 0) {
			parsed_line_successfully = parse_subdag( dag, 
						token, filename, lineNumber, tmpDirectory.c_str() );
		}

		// Handle a Splice spec
		else if(strcasecmp(token, "SPLICE") == 0) {
			parsed_line_successfully = parse_splice(dm, dag, filename,
						lineNumber);
		}

		// Handle a SUBMIT-DESCRIPTION spec
		else if(strcasecmp(token, "SUBMIT-DESCRIPTION") == 0) {
			std::string descName;
			const char* desc;
			pre_parse_node(descName, desc);
			std::string desc_end;
			if (get_inline_desc_end(desc, desc_end)) {
				std::string err;
				std::string data = parse_inline_desc(ms, gl_opts, desc_end, err, line);
				if ( ! err.empty()) {
					debug_printf(DEBUG_NORMAL, "ERROR (line %d): %s\n", lineNumber, err.c_str());
				} else {
					dag->InlineDescriptions.insert(std::make_pair(descName, data));
					parsed_line_successfully = true;
				}
			}
		}

		else {
				// Just accept everything for now -- we'll detect unknown
				// command names in the second pass.
			parsed_line_successfully = true;
		}

		if (!parsed_line_successfully) {
			fclose(fp);
			return false;
		}
	}

	//
	// PASS 2.
	// Seek back to the beginning of the DAG file.
	//
	if ( fseek( fp, 0, SEEK_SET ) != 0 ) {
		debug_printf( DEBUG_QUIET,
					"Error (%d, %s) seeking back to beginning of DAG file\n",
					errno, strerror( errno ) );
		fclose(fp);
		return false;
	}

	// reset line number because we seeked back to the start of the file.
	//lineNumber = 0;
	src.line = 0;

	//
	// This loop will read every line of the input file
	//
	while ( ((line=ms.getline(gl_opts)) != NULL) ) {
		//std::string varline(line);
		int lineNumber = src.line;
		//
		// Find the terminating '\0'
		//
		char * endline = line;
		while (*endline != '\0') endline++;


		// Note that getline will truncate leading spaces (as defined by isspace())
		// so we don't need to do that before checking for empty lines or comments.
		if (line[0] == 0)       continue;  // Ignore blank lines
		if (line[0] == COMMENT) continue;  // Ignore comments

		debug_printf( DEBUG_DEBUG_3, "Parsing line <%s>\n", line );

		char *token = strtok(line, DELIMITERS);
		if ( !token ) continue; // so Coverity is happy

		bool parsed_line_successfully;

		// JOB, SERVICE, PROVISIONER, FINAL already parsed... slurp up any inline submit lines
		if (NODE_KEYWORDS.contains(token)) {
			parsed_line_successfully = true;
			std::string nodename;
			const char * subfile = NULL;
			pre_parse_node(nodename, subfile);
			std::string inline_end;
			if (get_inline_desc_end(subfile, inline_end)) {
				std::string err;
				(void)parse_inline_desc(ms, gl_opts, inline_end, err, line);
			}
		}

		// Handle a SUBDAG spec
		else if	(strcasecmp(token, "SUBDAG") == 0) {
				// Parsed in first pass.
			parsed_line_successfully = true;
		}

		// Handle a SCRIPT spec
		// Example Syntax is:  SCRIPT (PRE|POST|HOLD) [DEFER status time] NodeName ScriptName Args ...
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
		// Example Syntax is:  Retry NodeName 3 UNLESS-EXIT 42
		else if( strcasecmp( token, "RETRY" ) == 0 ) {
			parsed_line_successfully = parse_retry(dag, filename, lineNumber);
		} 

		// Handle an Abort spec
		// Example Syntax is:  ABORT-DAG-ON NodeName 2
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
		// Example syntax is: Vars NodeName var1="val1" var2="val2"
		else if(strcasecmp(token, "VARS") == 0) {
			parsed_line_successfully = parse_vars(dag, filename, lineNumber);
		}

		// Handle a Priority spec
		// Example syntax is: Priority NodeName 2
		else if(strcasecmp(token, "PRIORITY") == 0) {
			parsed_line_successfully = parse_priority(dag, filename,
						lineNumber);
		}

		// Handle a Category spec
		// Example syntax is: Category NodeName Simulation
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

		// Allow a SET_JOB_ATTR spec, but ignore it here because it
		// is actually parsed by condor_submit_dag.
		else if (strcasecmp( token, "SET_JOB_ATTR" ) == 0 ) {
			parsed_line_successfully = true;
		}

		// Allow ENV spec, but ignore it as it is parsed in dagman utils
		else if (strcasecmp(token, "ENV") == MATCH) {
			parsed_line_successfully = true;
		}

		// Handle a Splice spec
		else if(strcasecmp(token, "SPLICE") == 0) {
				// Parsed in first pass.
			parsed_line_successfully = true;
		}

		// Handle a NODE_STATUS_FILE spec
		else if(strcasecmp(token, "NODE_STATUS_FILE") == 0) {
			parsed_line_successfully = parse_node_status_file(dag,
						filename, lineNumber);
		}

		// Handle a SAVE_POINT_FILE spec
		else if(strcasecmp(token, "SAVE_POINT_FILE") == MATCH) {
			parsed_line_successfully = parse_save_point_file(dag, filename, lineNumber);
		}

		// Handle a REJECT spec
		else if(strcasecmp(token, "REJECT") == 0) {
			parsed_line_successfully = parse_reject(dag,
						filename, lineNumber);
		}
		
		// Handle a JOBSTATE_LOG spec
		else if(strcasecmp(token, "JOBSTATE_LOG") == 0) {
			parsed_line_successfully = parse_jobstate_log(dag,
				filename, lineNumber);
		}
		
		// Handle a PRE_SKIP
		else if(strcasecmp(token, "PRE_SKIP") == 0) {
			parsed_line_successfully = parse_pre_skip(dag,
				filename, lineNumber);
		}

		// Handle a DONE spec
		else if(strcasecmp(token, "DONE") == 0) {
			parsed_line_successfully = parse_done(dag,
						filename, lineNumber);
		}

		// Handle a CONNECT spec
		else if(strcasecmp(token, "CONNECT") == 0) {
			parsed_line_successfully = parse_connect( dag,
						filename, lineNumber );
		}

		// Handle a PIN_IN spec
		else if(strcasecmp(token, "PIN_IN") == 0) {
			parsed_line_successfully = parse_pin_in_out( dag,
						filename, lineNumber, true );
		}

		// Handle a PIN_OUT spec
		else if(strcasecmp(token, "PIN_OUT") == 0) {
			parsed_line_successfully = parse_pin_in_out( dag,
						filename, lineNumber, false );
		}

		// Handle a INCLUDE spec
		else if(strcasecmp(token, "INCLUDE") == 0) {
			parsed_line_successfully = parse_include(dm, dag,
						filename, lineNumber );
		}

		// Handle a SUBMIT-DESCRIPTION spec
		else if(strcasecmp(token, "SUBMIT-DESCRIPTION") == 0) {
				// Parsed in first pass.
				// However we still need to advance the line parser.
			parsed_line_successfully = true;
			std::string descName;
			const char *desc = NULL;
			pre_parse_node(descName, desc);
			std::string desc_end;
			if (get_inline_desc_end(desc, desc_end)) {
				std::string err;
				(void)parse_inline_desc(ms, gl_opts, desc_end, err, line);
			}
		}

		// None of the above means that there was bad input.
		else {
			debug_printf( DEBUG_QUIET, "%s (line %d): "
				"ERROR: expected JOB, DATA, SUBDAG, FINAL, SCRIPT, PARENT, "
				"RETRY, ABORT-DAG-ON, DOT, VARS, PRIORITY, CATEGORY, "
				"MAXJOBS, CONFIG, SET_JOB_ATTR, SPLICE, PROVISIONER, SERVICE, "
				"NODE_STATUS_FILE, REJECT, JOBSTATE_LOG, PRE_SKIP, DONE, "
				"CONNECT, PIN_IN, PIN_OUT, INCLUDE, SAVE_POINT_FILE or"
				"SUBMIT-DESCRIPTION token "
				"(found %s)\n",
				filename, lineNumber, token );
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
	dag->LiftSplices(SELF);
	dag->RecordInitialAndTerminalNodes();
	
	if ( _useDagDir ) {
		std::string	errMsg;
		if ( !dagDir.Cd2MainDir( errMsg ) ) {
			debug_printf( DEBUG_QUIET,
					"ERROR: Could not change to original directory: %s\n",
					errMsg.c_str() );
			return false;
		}
	}

	return true;
}

static bool 
parse_subdag( Dag *dag, 
			const char* nodeTypeKeyword,
			const char* dagFile, int lineNum, const char *directory )
{
	const char *inlineOrExt = strtok( NULL, DELIMITERS );
	if ( !inlineOrExt ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): SUBDAG needs "
					"EXTERNAL keyword\n", dagFile, lineNum);
		return false;
	}
	if ( !strcasecmp( inlineOrExt, "EXTERNAL" ) ) {
		std::string nodename;
		const char * subfile;
		pre_parse_node(nodename, subfile);
		return parse_node( dag, nodename.c_str(), subfile, nodeTypeKeyword, dagFile,
					lineNum, directory, " EXTERNAL", "dagfile" );
	}

	debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): only SUBDAG "
				"EXTERNAL is supported at this time\n", dagFile, lineNum);
	return false;
}

static const char* next_possibly_quoted_token( void )
{
	char *remainder = strtok( NULL, "" );
	if ( !remainder ) {
		return NULL;
	}
	while ( remainder[0] == ' ' || remainder[0] == '\t' )
		remainder++;
	if ( remainder[0] == '"' )
		return strtok( ++remainder, "\"" );
	else
		return strtok( remainder, DELIMITERS );
}

//-----------------------------------------------------------------------------
// parse just enough of the line to know if this is an inline submit file or a submit filename
static bool
pre_parse_node(std::string & nodename, const char * &submitFile)
{
	nodename.clear();
	submitFile = NULL;

	// first token is the node name
	const char *nodeName = strtok(NULL, DELIMITERS);
	if (nodeName) {
		nodename = nodeName;
	}

	// next token is the either the submit file name,
	// or an inline submit description
	submitFile = next_possibly_quoted_token();
	if (!submitFile) {
		return false;
	}

	// return success for pre-parse. we have two tokens, we don't yet know if they have valid values though!!!
	return true;
}

//-----------------------------------------------------------------------------
static bool 
parse_node( Dag *dag, const char * nodeName, const char * submitFileOrSubmitDesc,
			const char* nodeTypeKeyword,
			const char* dagFile, int lineNum, const char *directory,
			const char *inlineOrExt, const char *submitOrDagFile)
{
	std::string example = std::string(nodeTypeKeyword) + 
		std::string(inlineOrExt) + " <nodename> " +
		submitOrDagFile + " [DIR directory] [NOOP] [DONE]";
	std::string whynot;
	bool done = false;

	NodeType type = NodeType::JOB;
	if ( strcasecmp ( nodeTypeKeyword, "FINAL" ) == 0 ) type = NodeType::FINAL;
	if ( strcasecmp ( nodeTypeKeyword, "PROVISIONER" ) == 0 ) type = NodeType::PROVISIONER;
	if ( strcasecmp ( nodeTypeKeyword, "SERVICE" ) == 0 ) type = NodeType::SERVICE;

		// NOTE: fear not -- any missing tokens resulting in NULL
		// strings will be error-handled correctly by AddNode()

		// first token is the node name
	if ( !nodeName ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): no node name "
					"specified\n", dagFile, lineNum );
		exampleSyntax( example.c_str() );
		return false;
	}

	if ( isReservedWord( nodeName ) ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): NodeName cannot be a reserved word\n",
					  dagFile, lineNum );
		exampleSyntax( example.c_str() );
		return false;
	}

	bool allowIllegalChars = param_boolean("DAGMAN_ALLOW_ANY_NODE_NAME_CHARACTERS", false );
	if ( !allowIllegalChars && ( strcspn ( nodeName, ILLEGAL_CHARS ) < strlen ( nodeName ) ) ) {
        std::string errorMessage = "ERROR: " + std::string(dagFile) + 
			" (line " + std::to_string(lineNum) + "): NodeName " +
			std::string(nodeName) + " contains one or more illegal characters (";
        for( unsigned int i = 0; i < strlen( ILLEGAL_CHARS ); i++ ) {
            errorMessage += "'";
            errorMessage += ILLEGAL_CHARS[i];
            errorMessage += "'";         
            if( i < strlen( ILLEGAL_CHARS ) - 1 ) {
                errorMessage += ", ";
            }
        }
        errorMessage += ")\n";
        debug_error( 1, DEBUG_QUIET, "%s", errorMessage.c_str() );
        return false;
    }

	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();

		// next token is the either the submit file name,
		// or an inline submit description
	if ( !submitFileOrSubmitDesc ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): no submit file or "
					"submit description specified\n", dagFile, lineNum );
		exampleSyntax( example.c_str() );
		return false;
	}

		// next token (if any) is "DIR" "NOOP", or "DONE" (in that order)
	TmpDir nodeDir;
	const char* nextTok = strtok( NULL, DELIMITERS );
	if ( nextTok ) {
		if (strcasecmp(nextTok, "DIR") == 0) {
			if ( strcmp(directory, "") ) {
				debug_printf( DEBUG_QUIET, "ERROR: DIR specification in node "
							"lines not allowed with -UseDagDir command-line "
							"argument\n");
				return false;
			}

			directory = next_possibly_quoted_token();
			if ( !directory ) {
				debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): no directory "
							"specified after DIR keyword\n", dagFile, lineNum );
				exampleSyntax( example.c_str() );
				return false;
			}

			std::string errMsg;
			if ( !nodeDir.Cd2TmpDir(directory, errMsg) ) {
				debug_printf( DEBUG_QUIET,
							"ERROR: can't change to directory %s: %s\n",
							directory, errMsg.c_str() );
				return false;
			}
			nextTok = strtok( NULL, DELIMITERS );
		} else {
			// Fall through to check for NOOP.
		}
	}

	bool noop = false;

	if ( nextTok ) {
		if ( strcasecmp( nextTok, "NOOP" ) == 0 ) {
			noop = true;
			nextTok = strtok( NULL, DELIMITERS );
		} else {
			// Fall through to check for DONE.
		}
	}

	if ( nextTok ) {
		if ( strcasecmp( nextTok, "DONE" ) == 0 ) {
			done = true;
		} else {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", dagFile, lineNum, nextTok );
			exampleSyntax( example.c_str() );
			return false;
		}
		nextTok = strtok( NULL, DELIMITERS );
	}

		// anything else is garbage
	if ( nextTok ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", dagFile, lineNum, nextTok );
			exampleSyntax( example.c_str() );
			return false;
	}

	// check to see if this node name is also a splice name for this dag.
	if (dag->LookupSplice(nodeName)) {
		debug_printf( DEBUG_QUIET, 
			  "ERROR: %s (line %d): "
			  "Node name '%s' must not also be a splice name.\n",
			  dagFile, lineNum, nodeName );
		return false;
	}

	// If this is a "SUBDAG" line, generate the real submit file name.
	// This can only be done with real submit files, not inline descriptions.
	std::string nestedDagFile("");
	std::string dagSubmitFile(""); // must be outside if so it stays in scope
	if ( submitFileOrSubmitDesc ) {
		if ( strcasecmp( nodeTypeKeyword, "SUBDAG" ) == MATCH ) {
				// Save original DAG file name (needed for rescue DAG).
			nestedDagFile = submitFileOrSubmitDesc;

				// Generate the "real" submit file name (append ".condor.sub"
				// to the DAG file name).
			dagSubmitFile = submitFileOrSubmitDesc;
			dagSubmitFile += DAG_SUBMIT_FILE_SUFFIX;
			submitFileOrSubmitDesc = dagSubmitFile.c_str();

		} else if ( strstr( submitFileOrSubmitDesc, DAG_SUBMIT_FILE_SUFFIX) ) {
			// Assume JOB commands submit file ending with '.condor.sub' is a DAG
			// submit file. This is no longer allowed for subdags
			debug_printf(DEBUG_NORMAL, "Error: The use of the JOB keyword for nested DAGs is prohibited.\n");
			return false;
		}
	}

	// looks ok, so add it
	if( !AddNode( dag, nodeName, directory,
				submitFileOrSubmitDesc, noop, done, type, whynot ) )
	{
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): %s\n",
					dagFile, lineNum, whynot.c_str() );
		exampleSyntax( example.c_str() );
		return false;
	}

	if ( nestedDagFile != "" ) {
		if ( !SetNodeDagFile( dag, nodeName, nestedDagFile.c_str(),
					whynot ) ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): %s\n",
					  	dagFile, lineNum, whynot.c_str() );
			return false;
		}
	}

	std::string errMsg;
	if ( !nodeDir.Cd2MainDir(errMsg) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: can't change to original directory: %s\n",
					errMsg.c_str() );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
//
// Function: parse_script
// Purpose:  Parse a line of the format:
//             SCRIPT [DEFER status time] (PRE|POST|HOLD) NodeName ScriptName Args ...
//
//-----------------------------------------------------------------------------
static bool 
parse_script(
	const char *endline,
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "SCRIPT [DEFER status time] [DEBUG file (STDOUT|STDERR|ALL)] (PRE|POST|HOLD) NodeName Script Args ...";

	//
	// Second keyword is either PRE, POST or DEFER
	//
	char * type_name = strtok( NULL, DELIMITERS );
	if ( !type_name ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: %s (line %d): Missing PRE, POST, HOLD, or DEFER\n",
					filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int defer_status = SCRIPT_DEFER_STATUS_NONE;
	int defer_time = 0;
	if ( !strcasecmp( type_name, "DEFER" ) ) {
			// Our script has a defer statement.
		char *token = strtok( NULL, DELIMITERS );
		if ( token == NULL ) {
			debug_printf( DEBUG_QUIET,
					"ERROR: %s (line %d): Missing DEFER status value\n",
					filename, lineNumber );
			exampleSyntax( example );
			return false;
		}
		char *tmp;
		defer_status = (int)strtol( token, &tmp, 10 );
		if ( tmp == token || defer_status <= 0 ) {
			debug_printf( DEBUG_QUIET,
				"ERROR: %s (line %d): Invalid DEFER status value \"%s\"\n",
				filename, lineNumber, token );
			exampleSyntax( example );
			return false;
		}

		token = strtok( NULL, DELIMITERS );
		if ( token == NULL ) {
			debug_printf( DEBUG_QUIET,
				"ERROR: %s (line %d): Missing DEFER time value\n",
				filename, lineNumber );
			exampleSyntax( example );
			return false;
		}
		defer_time = (int)strtol( token, &tmp, 10 );
		if ( tmp == token || defer_time < 0 ) {
			debug_printf( DEBUG_QUIET,
				"ERROR: %s (line %d): Invalid DEFER time value \"%s\"\n",
				filename, lineNumber, token );
			exampleSyntax( example );
			return false;
		}

			// The next token must be PRE, POST or HOLD.
		type_name = strtok( NULL, DELIMITERS );
		if ( !type_name ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): Missing PRE, POST, HOLD or optional DEBUG\n",
						filename, lineNumber );
			exampleSyntax( example );
			return false;
		}
	}

	DagScriptOutput debugType = DagScriptOutput::NONE;
	std::string debugFile;
	if (strcasecmp(type_name, "DEBUG") == MATCH) {
		const char* token = strtok(NULL, DELIMITERS);
		if ( ! token) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Script DEBUG missing filename\n",
			             filename, lineNumber);
			exampleSyntax(example);
			return false;
		}
		debugFile = token;
		token = strtok(NULL, DELIMITERS);
		if ( ! token) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Script DEBUG missing output stream type (STDOUT,STDERR,ALL)\n",
			             filename, lineNumber);
			exampleSyntax(example);
			return false;
		}
		if (strcasecmp(token, "STDOUT") == MATCH) {
			debugType = DagScriptOutput::STDOUT;
		} else if (strcasecmp(token, "STDERR") == MATCH) {
			debugType = DagScriptOutput::STDERR;
		} else if (strcasecmp(token, "ALL") == MATCH) {
			debugType = DagScriptOutput::ALL;
		} else {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Unknown Script output stream type (%s) expected STDOUT, STDERR, or ALL\n",
			            filename, lineNumber, token);
			exampleSyntax(example);
			return false;
		}
		type_name = strtok(NULL, DELIMITERS);
		if ( ! type_name) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Missing PRE, POST or HOLD\n",
						filename, lineNumber);
			exampleSyntax(example);
			return false;
		}
	}

	ScriptType scriptType;
	if ( !strcasecmp (type_name, "PRE" ) ) {
		scriptType = ScriptType::PRE;
	} else if ( !strcasecmp (type_name, "POST") ) {
		scriptType = ScriptType::POST;
	} else if ( !strcasecmp (type_name, "HOLD") ) {
		scriptType = ScriptType::HOLD;
	} else {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): "
					  "After specifying \"SCRIPT\", you must "
					  "indicate if you want \"PRE\", \"POST\", \"HOLD\" "
					  "(or DEFER)\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}

	//
	// Next token is the NodeName
	//
	const char *nodeName = strtok( NULL, DELIMITERS );
	if ( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing job name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	const char *rest = nodeName; // For subsequent tokens

	debug_printf(DEBUG_DEBUG_1, "nodeName: %s\n", nodeName);
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();

	//
	// The rest of the line is the script and args
	//
	
	// first, skip over the token we already read...
	// Why not just call strtok() again here? wenger 2016-10-13
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
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): "
					  "You named a %s script for node %s but "
					  "didn't provide a script filename\n",
					  filename, lineNumber, type_name, 
					  nodeNameOrig );
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
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): "
					  "You named a %s script for node %s but "
					  "didn't provide a script filename\n",
					  filename, lineNumber, type_name, 
					  nodeNameOrig );
		exampleSyntax( example );
		return false;
	}


	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_script(): skipping node %s because final nodes must have SCRIPT set explicitly (%s: %d)\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		Script* script = new Script(scriptType, rest, defer_status, defer_time);
		if(! script) {
			debug_printf(DEBUG_SILENT, "ERROR: Failed to make script object: out of memory!\n");
			return false;
		}
		if (debugType != DagScriptOutput::NONE)
			script->SetDebug(debugFile, debugType);
		if(! node->AddScript(script)) {
			debug_printf(DEBUG_SILENT, "ERROR: %s (line %d): failed to add %s script to node %s\n",
			             filename, lineNumber, type_name, node->GetNodeName());
			return false;
		}
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Unknown Node %s\n",
					  filename, lineNumber, nodeNameOrig );
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
	const char * example = "PARENT p1 [p2 p3 ...] CHILD c1 [c2 c3 ...]";
	std::string failReason = "";
	const char *nodeName;
	
	// get the node objects for the parents
	std::forward_list<Node*> parents;
	auto last_parent = parents.before_begin();
	while ((nodeName = strtok (NULL, DELIMITERS)) != NULL &&
		   strcasecmp (nodeName, "CHILD") != 0) {
		const char *nodeNameOrig = nodeName; // for error output
		std::string tmpNodeName = munge_node_name(nodeName);
		const char *nodeName2 = tmpNodeName.c_str();

		// if splice name then deal with that first...
		Dag* splice_dag = dag->LookupSplice(nodeName2);
		if (splice_dag) {

			// grab all of the final nodes of the splice and make them parents
			// for this node.
			std::vector<Node*> *splice_final;
			splice_final = splice_dag->FinalRecordedNodes();

			// now add each final node as a parent
			for (auto node : *splice_final) {
					last_parent = parents.insert_after(last_parent, node);
			}

		} else {

			// orig code
			// if the name is not a splice, then see if it is a true node name.
			Node *node = dag->FindNodeByName( nodeName2 );
			if (node == NULL) {
				// oops, it was neither a splice nor a parent name, bail
				debug_printf( DEBUG_QUIET, 
						  "ERROR: %s (line %d): Unknown Node %s\n",
						  filename, lineNumber, nodeNameOrig );
				return false;
			}
			last_parent = parents.insert_after(last_parent, node);
		}
	}
	
	// There must be one or more parent node names before
	// the CHILD token
	if (parents.empty()) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): "
					  "Missing Parent Node names\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}

	parents.sort(SortNodesById());
	parents.unique(EqualNodesById());
	
	if (nodeName == NULL) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Expected CHILD token\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}
	
	std::forward_list<Node*> children;
	auto last_child = children.before_begin();
	
	// get the node objects for the children
	while ((nodeName = strtok (NULL, DELIMITERS)) != NULL) {
		const char *nodeNameOrig = nodeName; // for error output
		std::string tmpNodeName = munge_node_name(nodeName);
		const char *nodeName2 = tmpNodeName.c_str();

		// if splice name then deal with that first...
		Dag *splice_dag = dag->LookupSplice(nodeName2);
		if (splice_dag) {
			// grab all of the initial nodes of the splice and make them 
			// children for this node.

			debug_printf( DEBUG_DEBUG_1, "%s (line %d): "
				"Detected splice %s as a child....\n", filename, lineNumber,
					nodeName2);

			std::vector<Node*> *splice_initial;
			splice_initial = splice_dag->InitialRecordedNodes();
			debug_printf( DEBUG_DEBUG_1, "Adding %zu initial nodes\n", 
				splice_initial->size());

			// now add each initial node as a child
			for (auto node : *splice_initial) {
					last_child = children.insert_after(last_child, node);
			}

		} else {

			// orig code
			// if the name is not a splice, then see if it is a true node name.
			Node *node = dag->FindNodeByName( nodeName2 );
			if (node == NULL) {
				// oops, it was neither a splice nor a child name, bail
				debug_printf( DEBUG_QUIET, 
						  "ERROR: %s (line %d): Unknown Node %s\n",
						  filename, lineNumber, nodeNameOrig );
				return false;
			}
			last_child = children.insert_after(last_child, node);
		}
	}
	
	if (children.empty()) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing Child Node names\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}
	
	children.sort(SortNodesById());
	children.unique(EqualNodesById());

	//
	// Now add all the dependencies
	//
	
	static int numJoinNodes = 0;
	bool useJoinNodes = param_boolean( "DAGMAN_USE_JOIN_NODES", true );
	const char * parent_type = "parent";


	// If this statement has multiple parent nodes and multiple child nodes, we
	// can optimize the dag structure by creating an intermediate "join node"
	// connecting the two sets.
	if (useJoinNodes && more_than_one(parents) && more_than_one(children)) {
		// First create the join node and add it
		std::string joinNodeName;
		formatstr(joinNodeName, "_condor_join_node%d", ++numJoinNodes);
		Node* joinNode = AddNode(dag, joinNodeName.c_str(), "", "noop.sub", true, 
			false, NodeType::JOB, failReason);
		if (!joinNode) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d) while attempting to"
				" add join node\n", failReason.c_str(), lineNumber);
		}
		// Now connect all parents and children to the join node
		for (auto parent : parents) {
				std::forward_list<Node*> lst = { joinNode };
			if (!parent->AddChildren(lst, failReason)) {
				debug_printf( DEBUG_QUIET, "ERROR: %s (line %d) failed"
					" to add dependency between parent"
					" node \"%s\" and join node \"%s\"\n",
					filename, lineNumber,
					parent->GetNodeName(), joinNode ? joinNode->GetNodeName() : "unknown");
				return false;
			}
		}
		// reset parent list to the join node and fall through to build the child edges
		parents.clear();
		parents.push_front(joinNode);
		parent_type = "join";
	}

	for (auto parent : parents) {
			if ( ! parent->AddChildren(children, failReason)) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d) failed"
				" to add dependencies between %s"
				" node \"%s\" and child nodes : %s\n",
				filename, lineNumber, parent_type,
				parent->GetNodeName(), failReason.c_str());
			return false;
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_retry
// Purpose:  Parse a line of the format "Retry nodename num-times [UNLESS-EXIT 42]"
// 
//-----------------------------------------------------------------------------
static bool 
parse_retry(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char *example = "Retry NodeName 3 [UNLESS-EXIT 42]";
	
	const char *nodeName = strtok( NULL, DELIMITERS );
	if( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing node name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();
	
	char *token = strtok( NULL, DELIMITERS );
	if( token == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Missing Retry value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	
	char *tmp;
	int retryMax = (int)strtol( token, &tmp, 10 );
	if( tmp == token ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Invalid Retry value \"%s\"\n",
					  filename, lineNumber, token );
		exampleSyntax( example );
		return false;
	}
	if ( retryMax < 0 ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Invalid Retry value \"%d\" "
					  "(cannot be negative)\n",
					  filename, lineNumber, retryMax );
		exampleSyntax( example );
		return false;
	}

    // Check for optional retry-abort value
	int unless_exit = 0;
    token = strtok( NULL, DELIMITERS );
    if ( token != NULL ) {
        if ( strcasecmp ( token, "UNLESS-EXIT" ) != 0 ) {
            debug_printf( DEBUG_QUIET, "ERROR: %s (line %d) Invalid retry option: %s\n", 
                          filename, lineNumber, token );
            exampleSyntax( example );
            return false;
        }
        else {
            token = strtok( NULL, DELIMITERS );
            if ( token == NULL ) {
                debug_printf( DEBUG_QUIET, "ERROR: %s (line %d) Missing parameter for UNLESS-EXIT\n",
                              filename, lineNumber);
                exampleSyntax( example );
                return false;
            } 
            char *unless_exit_end;
            unless_exit = strtol( token, &unless_exit_end, 10 );
            if (*unless_exit_end != 0) {
                debug_printf( DEBUG_QUIET, "ERROR: %s (line %d) Bad parameter for UNLESS-EXIT: %s\n",
                              filename, lineNumber, token );
                exampleSyntax( example );
                return false;
            }
        }
    }

	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_retry(): skipping node %s because final nodes cannot have RETRY specifications (%s: %d)\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		debug_printf( DEBUG_DEBUG_3, "parse_retry(): found node %s\n",
					node->GetNodeName() );

		if ( node->GetType() == NodeType::FINAL ) {
			debug_printf( DEBUG_QUIET, 
						"ERROR: %s (line %d): Final Node %s cannot have RETRY specification\n",
						filename, lineNumber, node->GetNodeName() );
			return false;
		}
		if ( node->GetType() == NodeType::SERVICE ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): SERVICE node %s cannot have RETRY specification\n",
						filename, lineNumber, node->GetNodeName() );
			return false;
		}

		node->retry_max = retryMax;
		if ( unless_exit != 0 ) {
           	node->have_retry_abort_val = true;
           	node->retry_abort_val = unless_exit;
		}
           debug_printf( DEBUG_DEBUG_1, "Retry Abort Value for %s is %d\n",
		   			node->GetNodeName(), node->retry_abort_val );
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Unknown Node %s\n",
					filename, lineNumber, nodeNameOrig );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_abort
// Purpose:  Parse a line of the format
// "ABORT-DAG-ON nodename node_exit_value [RETURN dag_return_value]"
// 
//-----------------------------------------------------------------------------
static bool 
parse_abort(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char *example = "ABORT-DAG-ON NodeName 3 [RETURN 1]";
	
		// Node name.
	const char *nodeName = strtok( NULL, DELIMITERS );
	if ( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing node name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();
	
		// Node abort value.
	char *abortValStr = strtok( NULL, DELIMITERS );
	if ( abortValStr == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Missing ABORT-ON node value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	
	int abortVal;
	char *tmp;
	abortVal = (int)strtol( abortValStr, &tmp, 10 );
	if( tmp == abortValStr ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Invalid ABORT-ON node value \"%s\"\n",
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
						"ERROR: %s (line %d) Invalid ABORT-ON option: %s\n",
						filename, lineNumber, nextWord);
			exampleSyntax( example );
			return false;
		} else {

				// DAG return value.
			haveReturnVal = true;
			nextWord = strtok( NULL, DELIMITERS );
			if ( nextWord == NULL ) {
				debug_printf( DEBUG_QUIET,
							"ERROR: %s (line %d) Missing parameter for ABORT-ON\n",
							filename, lineNumber);
				exampleSyntax( example );
				return false;
			}

			returnVal = strtol(nextWord, &tmp, 10);
			if ( tmp == nextWord ) {
				debug_printf( DEBUG_QUIET,
							"ERROR: %s (line %d) Bad parameter for ABORT_ON: %s\n",
							filename, lineNumber, nextWord);
				exampleSyntax( example );
				return false;
			} else if ( (returnVal < 0) || (returnVal > 255) ) {
				debug_printf( DEBUG_QUIET,
							"ERROR: %s (line %d) Bad return value for ABORT_ON "
							"(must be between 0 and 255): %s\n",
							filename, lineNumber, nextWord);
				return false;
			}
		}
	}

	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_abort(): skipping node %s because final nodes cannot have ABORT-DAG-ON specification (%s: %d)s\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		debug_printf( DEBUG_DEBUG_3, "parse_abort(): found node %s\n",
					node->GetNodeName() );

		if ( node->GetType() == NodeType::FINAL ) {
			debug_printf( DEBUG_QUIET, 
			  			"ERROR: %s (line %d): Final Node %s cannot have ABORT-DAG-ON specification\n",
			  			filename, lineNumber, node->GetNodeName() );
			return false;
		}

		node->abort_dag_val = abortVal;
		node->have_abort_dag_val = true;

		node->abort_dag_return_val = returnVal;
		node->have_abort_dag_return_val = haveReturnVal;
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Unknown Node %s\n",
					filename, lineNumber, nodeNameOrig );
		return false;
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
static bool parse_dot(Dag *dag, const char *filename, int lineNumber)
{
	const char *example = "Dot dotfile [UPDATE | DONT-UPDATE] "
		                  "[OVERWRITE | DONT-OVERWRITE] "
		                  "[INCLUDE <dot-file-header>]";
	
	char *dot_file_name = strtok( NULL, DELIMITERS );
	if ( dot_file_name == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing dot file name,\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	char *token;
	while ( (token = strtok( NULL, DELIMITERS ) ) != NULL ) {
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
				debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Missing include"
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
// Purpose:  Parses a line of named variables that will be made available to a node's
//           submit description file
//           The format of this line must be
//           Vars NodeName VarName1="value1" VarName2="value2" etc
//           Whitespace surrounding the = sign is permissible
//-----------------------------------------------------------------------------
static bool parse_vars(Dag *dag, const char *filename, int lineNumber)
{
	const char* example = "Vars NodeName [PREPEND | APPEND] VarName1=\"value1\" VarName2=\"value2\"";
	const char *nodeName = strtok( NULL, DELIMITERS );
	if ( nodeName == NULL ) {
		debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Missing node name\n",
					filename, lineNumber);
		exampleSyntax(example);
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();

	std::string varName;
	std::string varValue;
	bool prepend; //Bool for if variable is prepended or appended. Default is false

	char *varsStr = strtok( NULL, "\n" ); // just get all the rest and we will manually parse vars

/*	
*	Check to see if PREPEND or APPEND was specified before variables to be passed.
*	If option is found then we set prepend boolean appropriately and increment
*	the original varsStr pointer to point at char after option. -Cole Bollig
*/

	if (starts_with(varsStr,"PREPEND") ){//See if PREPEND is at beginning of token
		prepend = true;
		varsStr += 7;
	} else if ( starts_with(varsStr,"APPEND") ){//See if APPEND is at beginning of token
		prepend = false;
		varsStr += 6;
	} else {
		//If options aren't found then set to global knob
		// !append -> prepend
		prepend = !_appendVars;
	}

	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_vars(): skipping node %s because final nodes must have VARS set explicitly (%s: %d)\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		debug_printf( DEBUG_DEBUG_3, "parse_vars(): found node %s\n",
					node->GetNodeName() );

			// Note:  this re-parses most of the VARS line for every node...
			// wenger 2016-10-13
		char *str = varsStr;

		int numPairs;
		for ( numPairs = 0; ; numPairs++ ) {  // for each name="value" pair
			if ( str == NULL ) { // this happens when the above strtok returns NULL
				break;
			}

			varName.clear();
			varValue.clear();

			if ( !get_next_var( filename, lineNumber, str, varName,
						varValue ) ) {
				return false;
			}
			if ( varName.empty() ) {
				break;
			} else if (varName[0] == '+') {
				//convert + syntax vars to 'My.' syntax
				varName = "My." + varName.substr(1);
			}

			node->AddVar(varName.c_str(), varValue.c_str(), filename, lineNumber, prepend);
			debug_printf(DEBUG_DEBUG_1,
				"Argument added, Name=\"%s\"\tValue=\"%s\"\n",
				varName.c_str(), varValue.c_str());
		}
		node->ShrinkVars();

		if ( numPairs == 0 ) {
			debug_printf(DEBUG_QUIET,
						"ERROR: %s (line %d): No valid name-value pairs\n",
						filename, lineNumber);
			return false;
		}
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Unknown Node %s\n",
					filename, lineNumber, nodeNameOrig );
		return false;
	}
	
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_priority
// Purpose:  Parses a line specifying the priority of a node
//           The format of this line must be
//           Priority <NodeName> <Value>
//-----------------------------------------------------------------------------
static bool 
parse_priority(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "PRIORITY NodeName Value";

	//
	// Next token is the NodeName
	//
	const char *nodeName = strtok( NULL, DELIMITERS );
	if ( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing node name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}

	debug_printf(DEBUG_DEBUG_1, "nodeName: %s\n", nodeName);
	const char *nodeNameOrig = nodeName; // for error output
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();

	//
	// Next token is the priority value.
	//
	const char *valueStr = strtok(NULL, DELIMITERS);
	if ( valueStr == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Missing PRIORITY value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int priorityVal;
	char *tmp;
	priorityVal = (int)strtol( valueStr, &tmp, 10 );
	if( tmp == valueStr ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Invalid PRIORITY value \"%s\"\n",
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
					  "ERROR: %s (line %d): Extra token (%s) on PRIORITY line\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}

	//
	// Actually assign priorities to the relevant node(s).
	//
	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_priority(): skipping node %s because final nodes cannot have PRIORITY specifications (%s: %d)\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		debug_printf( DEBUG_DEBUG_3, "parse_priority(): found node %s\n",
					node->GetNodeName() );

		if ( node->GetType() == NodeType::FINAL ) {
			debug_printf( DEBUG_QUIET, 
			  			"ERROR: %s (line %d): Final Node %s cannot have PRIORITY specification\n",
			  			filename, lineNumber, node->GetNodeName() );
			return false;
		}
		if ( node->GetType() == NodeType::SERVICE ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): SERVICE node %s cannot have PRIORITY specification\n",
						filename, lineNumber, node->GetNodeName() );
			return false;
		}

		if ( ( node->_explicitPriority != 0 )
					&& ( node->_explicitPriority != priorityVal ) ) {
			debug_printf( DEBUG_NORMAL, "Warning: new priority %d for node %s "
						"overrides old value %d\n", priorityVal,
						node->GetNodeName(), node->_explicitPriority );
			check_warning_strictness( DAG_STRICT_2 );
		}

		node->_explicitPriority = priorityVal;
		node->_effectivePriority = priorityVal;
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Unknown Node %s\n",
					filename, lineNumber, nodeNameOrig );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_category
// Purpose:  Parses a line specifying the type of a node
//           The format of this line must be
//           Category <NodeName> <Category>
//			 No whitespace is allowed in the category name
//-----------------------------------------------------------------------------
static bool 
parse_category(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "CATEGORY NodeName TypeName";

	//
	// Next token is the NodeName
	//
	const char *nodeName = strtok(NULL, DELIMITERS);
	if ( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing node name\n",
					  filename, lineNumber );
		exampleSyntax (example);
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	debug_printf(DEBUG_DEBUG_1, "nodeName: %s\n", nodeName);
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();

	//
	// Next token is the category name.
	//
	const char *categoryName = strtok(NULL, DELIMITERS);
	if ( categoryName == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Missing CATEGORY name\n",
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
					  "ERROR: %s (line %d): Extra token (%s) on CATEGORY line\n",
					  filename, lineNumber, tmpStr );
		exampleSyntax( example );
		return false;
	}

	//
	// Actually assign categories to the relevant node(s).
	//
	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_category(): skipping node %s because final nodes cannot have CATEGORY specifications (%s: %d)\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		debug_printf( DEBUG_DEBUG_3, "parse_category(): found node %s\n",
					node->GetNodeName() );

		if ( node->GetType() == NodeType::FINAL ) {
			debug_printf( DEBUG_QUIET, 
			  			"ERROR: %s (line %d): Final node %s cannot have CATEGORY specification\n",
			  			filename, lineNumber, node->GetNodeName() );
			return false;
		}
		if ( node->GetType() == NodeType::SERVICE ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): SERVICE node %s cannot have CATEGORY specification\n",
						filename, lineNumber, node->GetNodeName() );
			return false;
		}

		node->SetCategory( categoryName, dag->_catThrottles );
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, "%s (line %d): Unknown Node %s\n",
					filename, lineNumber, nodeNameOrig );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_splice
// Purpose:  Parses a splice (subdag internal) line.
//           The format of this line must be
//           SPLICE <SpliceName> <SpliceFileName> [DIR <directory>]
//-----------------------------------------------------------------------------
static bool
parse_splice(
	const Dagman& dm,
	Dag *dag,
	const char *filename,
	int lineNumber)
{
	const char *example = "SPLICE SpliceName SpliceFileName [DIR directory]";
	Dag *splice_dag = NULL;
	std::string errMsg;

	//
	// Next token is the splice name
	// 
	std::string spliceName = strtok( NULL, DELIMITERS );
	// Note: this if is true if strtok() returns NULL. wenger 2014-10-07
	if ( spliceName.empty() ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Missing SPLICE name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	// Check to make sure we don't already have a node with the name of the
	// splice.
	if ( dag->NodeExists(spliceName.c_str()) ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): "
					  " Splice name '%s' must not also be a node name.\n",
					  filename, lineNumber, spliceName.c_str() );
		return false;
	}

	/* "push" it onto the scoping "stack" which will be used later to
		munge the names of the nodes to have the splice name in them so
		the same splice dag file with different splice names don't conflict.
	*/
	_spliceScope.push_back(strdup(munge_node_name(spliceName.c_str()).c_str()));

	//
	// Next token is the splice file nameMyStrstding
	// 
	std::string spliceFile = makeString(strtok( NULL, DELIMITERS ));
	// Note: this if is true if strtok() returns NULL. wenger 2014-10-07
	if ( spliceFile.empty() ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Missing SPLICE file name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	//
	// next token (if any) is "DIR"
	//
	TmpDir spliceDir;
	std::string dirTok = makeString( strtok( NULL, DELIMITERS ) );
	std::string directory = ".";

	std::string dirTokUpper = dirTok;
	upper_case(dirTokUpper);

	// Note: this if is okay even if strtok() returns NULL. wenger 2014-10-07
	if ( dirTokUpper == "DIR" ) {
		// parse the directory name
		directory = strtok( NULL, DELIMITERS );
		// Note: this if is true if strtok() returns NULL. wenger 2014-10-07
		if ( directory == "" ) {
			debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): DIR requires a directory "
						"specification\n", filename, lineNumber);
			exampleSyntax( example );
			return false;
		}
	} else if ( dirTok != "" ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: %s (line %d): illegal token (%s) on SPLICE line\n",
					filename, lineNumber, dirTok.c_str() );
		exampleSyntax( example );
		return false;
	}

	// 
	// anything else is garbage
	//
	std::string garbage = makeString( strtok( 0, DELIMITERS ) );
	// Note: this if is true if strtok() returns NULL. wenger 2014-10-07
	if (garbage != "") {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", filename, lineNumber, 
						  garbage.c_str() );
			exampleSyntax( example );
			return false;
	}

	/* make a new dag to put everything into */

	// This "copy" is tailored to be correct according to Dag::~Dag()
	// We can pass in NULL for submitDagOpts because the splice DAG
	// object will never actually do a submit.  wenger 2010-03-25
	splice_dag = new Dag(dm, true, current_splice_scope());
	
	// initialize whatever the DIR line was, or defaults to, here.
	splice_dag->SetDirectory(directory);

	debug_printf(DEBUG_VERBOSE, "Parsing Splice %s in directory %s with "
		"file %s\n", spliceName.c_str(), directory.c_str(),
		spliceFile.c_str());

	// CD into the DIR directory so we can continue parsing.
	// This must be done AFTER the DAG is created due to the DAG object
	// doing its own chdir'ing while looking for log files.
	if ( !spliceDir.Cd2TmpDir(directory.c_str(), errMsg) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: can't change to directory %s: %s\n",
					directory.c_str(), errMsg.c_str() );
		delete splice_dag;
		return false;
	}

	// parse the splice file into a separate dag.
	if (!parse(dm, splice_dag, spliceFile.c_str(), false)) {
		debug_error(1, DEBUG_QUIET, "ERROR: Failed to parse splice %s in file %s\n",
			spliceName.c_str(), spliceFile.c_str());
		delete splice_dag;
		return false;
	}

	// CD back to main dir to keep processing.
	if ( !spliceDir.Cd2MainDir(errMsg) ) {
		debug_printf( DEBUG_QUIET,
					"ERROR: can't change to original directory: %s\n",
					errMsg.c_str() );
		delete splice_dag;
		return false;
	}

	// Splices cannot have final nodes.
	if ( splice_dag->HasFinalNode() ) {
		debug_printf( DEBUG_QUIET, "ERROR: splice %s has a final node; "
					"splices cannot have final nodes\n", spliceName.c_str() );
		delete splice_dag;
		return false;
	}

	// munge the splice name
	spliceName = munge_node_name(spliceName.c_str());

	// XXX I'm not sure this goes here quite yet....
	debug_printf(DEBUG_DEBUG_1, "Splice scope is: %s\n", 
		current_splice_scope().c_str());
	splice_dag->PrefixAllNodeNames(current_splice_scope());
	splice_dag->_catThrottles.PrefixAllCategoryNames(current_splice_scope());

	// Print out a useful piece of debugging...
	if( DEBUG_LEVEL( DEBUG_DEBUG_1 ) ) {
		splice_dag->PrintNodeList();
	}

	// associate the splice_dag with its name in _this_ dag, later I'll merge
	// the nodes from this splice into _this_ dag.
	if (!dag->InsertSplice(spliceName, splice_dag)) {
		debug_printf( DEBUG_QUIET, "ERROR: Splice name '%s' used for multiple "
			"splices. Splice names must be unique per dag file.\n", 
			spliceName.c_str());
		delete splice_dag;
		return false;
	}
	debug_printf(DEBUG_DEBUG_1, "Done parsing splice %s\n", spliceName.c_str());

	// pop the just pushed value off of the end of the vector and free it
	char *tmp = _spliceScope.back();
	_spliceScope.pop_back();
	free(tmp);

	debug_printf(DEBUG_DEBUG_1, "_spliceScope has length %zu\n", _spliceScope.size());

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
					  "ERROR: %s (line %d): Missing MAXJOBS category name\n",
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
					  "ERROR: %s (line %d): Missing MAXJOBS value\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int maxJobsVal;
	char *tmp;
	maxJobsVal = (int)strtol( valueStr, &tmp, 10 );
	if( tmp == valueStr ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Invalid MAXJOBS value \"%s\"\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}
	if ( maxJobsVal < 0 ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): MAXJOBS value must be non-negative\n",
					  filename, lineNumber );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	valueStr = strtok(NULL, DELIMITERS);
	if ( valueStr != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on MAXJOBS line\n",
					  filename, lineNumber, valueStr );
		exampleSyntax( example );
		return false;
	}

	std::string tmpName( categoryName );
	dag->_catThrottles.SetThrottle( &tmpName, maxJobsVal );

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_node_status_file
// Purpose:  Parses a line specifying the a node_status_file for the DAG.
//           The format of this line must be
//           NODE_STATUS_FILE <filename> [min update time]
//			 No whitespace is allowed in the file name
//-----------------------------------------------------------------------------

// Defined in seconds...
#define DEFAULT_MIN_NODE_STATUS_UPDATE_TIME 60

static bool 
parse_node_status_file(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "NODE_STATUS_FILE StatusFile [min update time] [ALWAYS-UPDATE]";

	char *statusFileName = strtok(NULL, DELIMITERS);
	if (statusFileName == NULL) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing node status file name,\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int minUpdateTime = DEFAULT_MIN_NODE_STATUS_UPDATE_TIME;
	char *minUpdateStr = strtok(NULL, DELIMITERS);
	if ( minUpdateStr != NULL ) {
		char *tmp;
		minUpdateTime = (int)strtol( minUpdateStr, &tmp, 10 );
		if ( tmp == minUpdateStr ) {
			debug_printf( DEBUG_QUIET,
					  	"ERROR: %s (line %d): Invalid min update time value \"%s\"\n",
					  	filename, lineNumber, minUpdateStr );
			exampleSyntax( example );
			return false;
		}
	}

	bool alwaysUpdate = false;
	char *alwaysUpdateStr = strtok( NULL, DELIMITERS );
	if ( alwaysUpdateStr != NULL ) {
		if ( strcasecmp( alwaysUpdateStr, "ALWAYS-UPDATE" ) == 0) {
			alwaysUpdate = true;
		} else {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
						  "parameter \"%s\"\n", filename, lineNumber,
						  alwaysUpdateStr );
			exampleSyntax( example );
			return false;
		}
	}

	//
	// Check for illegal extra tokens.
	//
	char *token = strtok( NULL, DELIMITERS );
	if ( token != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on NODE_STATUS_FILE line\n",
					  filename, lineNumber, token );
		exampleSyntax( example );
		return false;
	}

	dag->SetNodeStatusFileName( statusFileName, minUpdateTime, alwaysUpdate );
	return true;
}

//-----------------------------------------------------------------------------
// Function: parse_save_point_file
// Purpose:  Parses a line specifying a node to write a save point file. The
//           save point file will be written first time we start a node. This
//           while will be written out as a partial rescue. Users can optionally
//           designate a filename for the file to be written as. If filename
//           includes a path then we respect that. Otherwise, we write save file
//           to save_files subdir created near all other DAG files.
//-----------------------------------------------------------------------------
static bool
parse_save_point_file(Dag *dag, const char* filename, int lineNumber)
{
	std::string example = "SAVE_POINT_FILE NodeName [Filename]";
	//Get node name to apply save file info to
	const char* nodeName = strtok(NULL, DELIMITERS);
	if (nodeName == NULL) {
		debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): No Node specified for SAVE_POINT_FILE.\n",
					 filename, lineNumber);
		exampleSyntax(example.c_str());
		return false;
	} else if (strcasecmp(nodeName, DAG::ALL_NODES) == MATCH) { //It is redundant to use all nodes for save files
		debug_printf(DEBUG_NORMAL, "ERROR: %s (line %d): SAVE_POINT_FILE does not allow ALL_NODES option.\n",
					 filename, lineNumber);
		exampleSyntax(example.c_str());
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	std::string tmpNodeName = munge_node_name(nodeName);
	nodeName = tmpNodeName.c_str();

	//Get save file name/path&name
	const char* p = strtok(NULL, DELIMITERS);
	std::string saveFile = p ? p : "";
	if (!saveFile.empty()) { //Name was specified
		//Check for any extra invalid tokens
		char* token = strtok(NULL, DELIMITERS);
		if (token != NULL) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Extra token (%s) found in SAVE_POINT_FILE declaration.\n",
						 filename, lineNumber, token);
			exampleSyntax(example.c_str());
			return false;
		}
	} else { //No name given so create one using node name and associated dag filename
		formatstr(saveFile,"%s-%s.save",nodeNameOrig,condor_basename(filename));
	}

	//Find node and set save file
	Node *node = dag->FindNodeByName(nodeName);
	if (node == NULL) {
		debug_printf(DEBUG_QUIET, "Warning: %s (line %d): Unknown Node %s\n",
								  filename, lineNumber, nodeNameOrig);
		return !check_warning_strictness(DAG_STRICT_1, false);
	} else {
		if (node->GetType() == NodeType::SERVICE || node->GetType() == NodeType::PROVISIONER) {
			debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): SAVE_POINT_FILE can not be declared with neither SERVICE nor PROVISIONER nodes.\n",
									  filename, lineNumber);
			return false;
		}
		node->SetSaveFile(saveFile);
	}
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_reject
// Purpose:  Parses a line specifying the REJECT directive for a DAG.
//           The format of this line must be
//           REJECT
//-----------------------------------------------------------------------------
static bool 
parse_reject(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "REJECT";

	char *token = strtok(NULL, DELIMITERS);
	if ( token != NULL ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): REJECT should have "
					"no additional tokens.\n",
					filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	std::string location;
	formatstr( location, "%s (line %d)", filename, lineNumber );
	debug_printf( DEBUG_QUIET, "REJECT specification at %s "
				"will cause this DAG to fail\n", location.c_str() );

	dag->SetReject( location );
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_jobstate_log
// Purpose:  Parses a line specifying the a jobstate.log for the DAG.
//           The format of this line must be
//           JOBSTATE_LOG <filename>
//			 No whitespace is allowed in the file name
//-----------------------------------------------------------------------------
static bool 
parse_jobstate_log(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "JOBSTATE_LOG JobstateLogFile";

	char *logFileName = strtok(NULL, DELIMITERS);
	if (logFileName == NULL) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing jobstate log file name,\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	char *extraTok = strtok( NULL, DELIMITERS );
	if ( extraTok != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on JOBSTATE_LOG line\n",
					  filename, lineNumber, extraTok );
		exampleSyntax( example );
		return false;
	}

	dag->SetJobstateLogFileName( logFileName );
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_pre_skip
// Purpose:  Tell dagman to skip execution if the PRE script exits with a
//           a certain code
//-----------------------------------------------------------------------------
bool 
parse_pre_skip( Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char * example = "PRE_SKIP NodeName Exitcode";
	std::string whynot;

		//
		// second token is the NodeName
		//
	const char *nodeName = strtok( NULL, DELIMITERS );
	if ( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing node name\n",
				filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	debug_printf( DEBUG_DEBUG_1, "nodeName: %s\n", nodeName );
	std::string tmpNodeName = munge_node_name( nodeName );
	nodeName = tmpNodeName.c_str();

		//
		// The rest of the line consists of the exitcode
		//
	const char *exitCodeStr = strtok( NULL, DELIMITERS );
	if ( exitCodeStr == NULL ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing exit code\n",
				filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	char *tmp;
	int exitCode = (int)strtol( exitCodeStr, &tmp, 10 );
	if ( tmp == exitCodeStr ) {
		debug_printf( DEBUG_QUIET,
				"ERROR: %s (line %d): Invalid exit code \"%s\"\n",
				filename, lineNumber, exitCodeStr );
		exampleSyntax( example );
		return false;
	}

		//
		// Anything else is garbage
		//
	const char *nextTok = strtok( NULL, DELIMITERS );
	if ( nextTok ) {
		debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): invalid "
				"parameter \"%s\"\n", filename, lineNumber, nextTok );
		exampleSyntax( example );
		return false;
	}

	Node *node;
	while ( ( node = dag->FindAllNodesByName( nodeName,
				"In parse_pre_skip(): skipping node %s because final nodes must have PRE_SKIP set explicitly (%s: %d)\n",
				filename, lineNumber ) ) ) {
		nodeName = NULL;

		std::string whynot;
		if( !node->AddPreSkip( exitCode, whynot ) ) {
			debug_printf( DEBUG_SILENT, "ERROR: %s (line %d): "
					  	"failed to add PRE_SKIP to node %s: %s\n",
					  	filename, lineNumber, node->GetNodeName(),
						whynot.c_str() );
			return false;
		}
	}

	if ( nodeName ) {
		debug_printf( DEBUG_QUIET, 
					  "ERROR: %s (line %d): Unknown Node %s\n",
					  filename, lineNumber, nodeNameOrig );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_done
// Purpose:  Parse a line of the format "Done nodename"
// 
//-----------------------------------------------------------------------------
static bool 
parse_done(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber)
{
	const char *example = "Done NodeName";
	
	const char *nodeName = strtok( NULL, DELIMITERS );
	if ( nodeName == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing node name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	const char *nodeNameOrig = nodeName; // for error output
	std::string tmpNodeName = munge_node_name( nodeName );
	nodeName = tmpNodeName.c_str();

	//
	// Check for illegal extra tokens.
	//
	char *extraTok = strtok( NULL, DELIMITERS );
	if ( extraTok != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on DONE line\n",
					  filename, lineNumber, extraTok );
		exampleSyntax( example );
		return false;
	}

	Node *node = dag->FindNodeByName( nodeName );
	if( node == NULL ) {
		debug_printf( DEBUG_QUIET, 
					  "Warning: %s (line %d): Unknown Node %s\n",
					  filename, lineNumber, nodeNameOrig );
		return !check_warning_strictness( DAG_STRICT_1, false );
	}

	if ( node->GetType() == NodeType::FINAL ) {
		debug_printf( DEBUG_QUIET, 
					"Warning: %s (line %d): FINAL Node %s cannot be set to DONE\n",
					filename, lineNumber, nodeNameOrig );
		return !check_warning_strictness( DAG_STRICT_1, false );
	}
	if ( node->GetType() == NodeType::SERVICE ) {
		debug_printf( DEBUG_QUIET,
					"Warning: %s (line %d): SERVICE node %s cannot be set to DONE\n",
					filename, lineNumber, nodeNameOrig );
		return !check_warning_strictness( DAG_STRICT_1, false );
	}

	node->SetStatus( Node::STATUS_DONE );
	
	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_connect
// Purpose:  Parse a line of the format "Connect splice1 splice2"
// 
//-----------------------------------------------------------------------------
static bool 
parse_connect(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber )
{
	const char *example = "CONNECT splice1 splice2";

	const char *splice1 = strtok( NULL, DELIMITERS );
	if ( splice1 == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing splice1 name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	std::string splice1Name = munge_node_name( splice1 );
	splice1 = splice1Name.c_str();

	const char *splice2 = strtok( NULL, DELIMITERS );
	if ( splice2 == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing splice2 name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	std::string splice2Name = munge_node_name( splice2 );
	splice2 = splice2Name.c_str();

	//
	// Check for illegal extra tokens.
	//
	char *extraTok = strtok( NULL, DELIMITERS );
	if ( extraTok != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on CONNECT line\n",
					  filename, lineNumber, extraTok );
		exampleSyntax( example );
		return false;
	}

	Dag *parentSplice = dag->LookupSplice( splice1 );
	if ( !parentSplice ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Splice %s not found!\n",
					  filename, lineNumber, splice1 );
		return false;
	}

	Dag *childSplice = dag->LookupSplice( splice2 );
	if ( !childSplice ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Splice %s not found!\n",
					  filename, lineNumber, splice2 );
		return false;
	}

	if ( !Dag::ConnectSplices( parentSplice, childSplice ) ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): (see previous line)\n",
					  filename, lineNumber );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_pin_in_out
// Purpose:  Parse a line of the format "Pin_in|pin_out node pin_num"
// 
//-----------------------------------------------------------------------------
static bool 
parse_pin_in_out(
	Dag  *dag, 
	const char *filename, 
	int  lineNumber, bool isPinIn )
{
	const char *example = "PIN_IN|PIN_OUT node pin_number";

	const char *node = strtok( NULL, DELIMITERS );
	if ( node == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing node name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}
	std::string nodeName = munge_node_name( node );
	node = nodeName.c_str();

	const char *pinNumber = strtok( NULL, DELIMITERS );
	if ( pinNumber == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing pin_number\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	int pinNum;
	char *tmp;
	pinNum = (int)strtol( pinNumber, &tmp, 10 );
	if ( tmp == pinNumber ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Invalid pin_number value \"%s\"\n",
					  filename, lineNumber, pinNumber );
		exampleSyntax( example );
		return false;
	}

	if ( pinNum < 1 ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): pin_number value must be positive\n",
					  filename, lineNumber );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	char *extraTok = strtok( NULL, DELIMITERS );
	if ( extraTok != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on PIN_IN/PIN_OUT line\n",
					  filename, lineNumber, extraTok );
		exampleSyntax( example );
		return false;
	}
	
	if ( !dag->SetPinInOut( isPinIn, node, pinNum ) ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): (see previous line)\n",
					  filename, lineNumber );
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// 
// Function: parse_include
// Purpose:  Parse a line of the format "Include filename"
// 
//-----------------------------------------------------------------------------
static bool 
parse_include(
	const Dagman& dm,
	Dag  *dag, 
	const char *filename, 
	int  lineNumber )
{
	const char *example = "INCLUDE filename";

	const char *includeFile = strtok( NULL, DELIMITERS );
	if ( includeFile == NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Missing include file name\n",
					  filename, lineNumber );
		exampleSyntax( example );
		return false;
	}

	//
	// Check for illegal extra tokens.
	//
	char *extraTok = strtok( NULL, DELIMITERS );
	if ( extraTok != NULL ) {
		debug_printf( DEBUG_QUIET,
					  "ERROR: %s (line %d): Extra token (%s) on INCLUDE line\n",
					  filename, lineNumber, extraTok );
		exampleSyntax( example );
		return false;
	}

		// Note:  we save the filename here because otherwise it gets
		// goofed up by the tokenizing in parse().
	std::string tmpFilename( includeFile );
		// Note:  false here for useDagDir argument means that the
		// include file path is always relative to the submit directory,
		// *not* relative to the DAG file's directory, even if
		// 'condor_submit -usedagdir' is specified.
	return parse(dm, dag, tmpFilename.c_str(), false);
}

static std::string munge_node_name(const char *nodeName)
{
		//
		// Munge the node name if necessary.
		//
	std::string newName;

	if ( _mungeNames ) {
		newName = std::to_string(_thisDagNum) + "." + nodeName;
	} else {
		newName = nodeName;
	}

	return newName;
}

static std::string current_splice_scope(void)
{
	std::string tmp;
	std::string scope;
	if(_spliceScope.size() > 0) {
		// While a natural choice might have been : as a splice scoping
		// separator, this character was chosen because it is a valid character
		// on all the file systems we use (whereas : can't be a file system
		// character on windows). The plus, and really anything other than :,
		// isn't the best choice. Sorry.
		tmp = _spliceScope.back();
		scope = tmp + "+";
	}
	return scope;
}

/** Get the next variable name/value pair.
	@param filename the name of the file we're parsing (for error messages)
	@param lineNumber the line number we're parsing (for error messages)
	@param varName (returned) the name of the variable ("" means no
		more variables)
	@param varValue (returned) the value of the variable
	@return true means success; false means error
 */
static bool
get_next_var( const char *filename, int lineNumber, char *&str,
			std::string &varName, std::string &varValue ) {
	while ( isspace( *str ) ) {
		str++;
	}

	if ( *str == '\0' ) {
		return true;
	}

	// copy name char-by-char until we hit a symbol or whitespace
	// names are limited to alphanumerics and underscores (except
	// that '+' is legal as the first character)
	int varnamestate = 0; // 0 means not within a varname
	while ( isalnum( *str ) || *str == '_' || *str == '+' || *str == '.') {
		if ( *str == '+' ) {
			if ( varnamestate != 0 ) {
				debug_printf( DEBUG_QUIET,
						"ERROR: %s (line %d): '+' can only be first character of macroname (%s)\n",
						filename, lineNumber, varName.c_str() );
				return false;
			}
		}
		varnamestate = 1;
		varName += *str++;
	}

	if ( varName.length() == '\0' ) { // no alphanumeric symbols at all were written into name,
	                      // just something weird, which we'll print
		debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): Unexpected symbol: \"%c\"\n", filename,
			lineNumber, *str);
		return false;
	}

	if ( varName == "+" ) {
		debug_printf(DEBUG_QUIET,
			"ERROR: %s (line %d): macroname (%s) must contain at least one alphanumeric character\n",
			filename, lineNumber, varName.c_str() );
		return false;
	}
		
	// Burn through any whitespace between var name and "=".
	while ( isspace( *str ) ) {
		str++;
	}

	if ( *str != '=' ) {
		if ( varName.compare("PREPEND") == 0 || varName.compare("APPEND") == 0){
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): %s not declared before all passed variables.\n"
						,filename,lineNumber,varName.c_str());
		} else {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Illegal character (%c) in or after macroname %s\n", filename, lineNumber, *str, varName.c_str() );
		}
		return false;
	}
	str++;

	// Burn through any whitespace between "=" and var value.
	while ( isspace( *str ) ) {
		str++;
	}
	
	if ( *str != '"' ) {
		debug_printf(DEBUG_QUIET, "ERROR: %s (line %d): %s's value must be quoted\n", filename,
			lineNumber, varName.c_str());
		return false;
	}

	// now it's time to read in all the data until the next double quote, while handling
	// the two escape sequences: \\ and \"
	bool stillInQuotes = true;
	bool escaped       = false;
	do {
		varValue += *(++str);
		
		if ( *str == '\0' ) {
			debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Missing end quote\n", filename,
				lineNumber );
			return false;
		}
			
		if ( !escaped ) {
			if ( *str == '"' ) {
				// we don't want that last " in the string
				varValue = varValue.substr( 0, varValue.length() - 1 );
				stillInQuotes = false;
			} else if ( *str == '\\' ) {
				// on the next pass it will be filled in appropriately
				varValue = varValue.substr( 0, varValue.length() - 1 );
				escaped = true;
				continue;
			}
		} else {
			if ( *str != '\\' && *str != '"' ) {
				debug_printf( DEBUG_QUIET, "ERROR: %s (line %d): Unknown escape sequence "
					"\"\\%c\"\n", filename, lineNumber, *str );
				return false;
			}
			escaped = false; // being escaped only lasts for one character
		}
	} while ( stillInQuotes );

	str++;

		// Check for illegal variable name.
	std::string tmpName( varName );
	lower_case( tmpName );
	if ( tmpName.find( "queue" ) == 0 ) {
		debug_printf( DEBUG_QUIET, "ERROR: Illegal variable name: %s; variable "
					"names cannot begin with \"queue\"\n", varName.c_str() );
		return false;
	}

	return true;
}
