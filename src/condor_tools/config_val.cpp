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


 

/***********************************************************************
*
* Print declarations from the condor config files, condor_config and
* condor_config.local.  Declarations in files specified by
* LOCAL_CONFIG_FILE override those in the global config file, so this
* prints the same declarations found by condor programs using the
* config() routine.
*
* In addition, the configuration of remote machines can be queried.
* You can specify either a name or sinful string you want to connect
* to, what kind of daemon you want to query, and what pool to query to
* find the specified daemon.
*
***********************************************************************/

#include "condor_common.h"
#include "condor_config.h"
#include "condor_io.h"
#include "condor_uid.h"
#include "match_prefix.h"
#include "string_list.h"
#include "condor_string.h"
#include "get_daemon_name.h"
#include "daemon.h"
#include "dc_collector.h"
#include "daemon_types.h"
#include "internet.h"
#include "condor_distribution.h"
#include "string_list.h"
#include "simplelist.h"
#include "subsystem_info.h"
#include "Regex.h"

#include <sstream>
#include <algorithm> // for std::sort

const char * MyName;

StringList params;
daemon_t dt = DT_MASTER;
bool mixedcase = false;
bool diagnostic = false;

// The pure-tools (PureCoverage, Purify, etc) spit out a bunch of
// stuff to stderr, which is where we normally put our error
// messages.  To enable config_val.test to produce easily readable
// output, even with pure-tools, we just write everything to stdout.  
#if defined( PURE_DEBUG ) 
#	define stderr stdout
#endif

enum PrintType {CONDOR_OWNER, CONDOR_TILDE, CONDOR_NONE};
enum ModeType {CONDOR_QUERY, CONDOR_SET, CONDOR_UNSET,
			   CONDOR_RUNTIME_SET, CONDOR_RUNTIME_UNSET};


// On some systems, the output from config_val sometimes doesn't show
// up unless we explicitly flush before we exit.
void PREFAST_NORETURN
my_exit( int status )
{
	fflush( stdout );
	fflush( stderr );
	exit( status );
}


void
usage(int retval = 1)
{
	fprintf( stderr, "Usage: %s [options] variable [variable] ...\n", MyName );
	fprintf( stderr,
			 "   or: %s [options] -set string [string] ...\n",
			 MyName );
	fprintf( stderr,
			 "   or: %s [options] -rset string [string] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -unset variable [variable] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -runset variable [variable] ...\n",
			 MyName );
	fprintf( stderr, "   or: %s [options] -tilde\n", MyName );
	fprintf( stderr, "   or: %s [options] -owner\n", MyName );
	fprintf( stderr, "   or: %s -dump [-verbose] [-expand]\n", MyName );
	fprintf( stderr, "\n   Valid options are:\n" );
	fprintf( stderr, "   -name daemon_name\t(query the specified daemon for its configuration)\n" );
	fprintf( stderr, "   -pool hostname\t(use the given central manager to find daemons)\n" );
	fprintf( stderr, "   -address <ip:port>\t(connect to the given ip/port)\n" );
	fprintf( stderr, "   -set\t\t\t(set a persistent config file expression)\n" );
	fprintf( stderr, "   -rset\t\t(set a runtime config file expression\n" );

	fprintf( stderr, "   -unset\t\t(unset a persistent config file expression)\n" );
	fprintf( stderr, "   -runset\t\t(unset a runtime config file expression)\n" );

	fprintf( stderr, "   -master\t\t(query the master)\n" );
	fprintf( stderr, "   -schedd\t\t(query the schedd)\n" );
	fprintf( stderr, "   -startd\t\t(query the startd)\n" );
	fprintf( stderr, "   -collector\t\t(query the collector)\n" );
	fprintf( stderr, "   -negotiator\t\t(query the negotiator)\n" );
	fprintf( stderr, "   -tilde\t\t(return the path to the Condor home directory)\n" );
	fprintf( stderr, "   -owner\t\t(return the owner of the condor_config_val process)\n" );
	fprintf( stderr, "   -local-name name\t(Specify a local name for use with the config system)\n" );
	fprintf( stderr, "   -verbose\t\t(print information about where variables are defined)\n" );
	fprintf( stderr, "   -dump\t\t(print locally defined variables)\n" );
	fprintf( stderr, "   -expand\t\t(with -dump, expand macros from config files)\n" );
	fprintf( stderr, "   -evaluate\t\t(when querying <daemon>, evaluate param with respect to classad from <daemon>)\n" );
	fprintf( stderr, "   -config\t\t(print the locations of found config files)\n" );
	fprintf( stderr, "   -debug[:<flags>]\t\t(dprintf to stderr, optionally overiding the TOOL_DEBUG flags)\n" );
	my_exit(retval);
}


char* GetRemoteParam( Daemon*, char* );
char* GetRemoteParamRaw(Daemon*, const char* name, bool & raw_supported, MyString & raw_value, MyString & file_and_line, MyString & def_value, MyString & usage_report);
int   GetRemoteParamNamesMatching(Daemon*, const char* name, std::vector<std::string> & names);
void  SetRemoteParam( Daemon*, char*, ModeType );
static void PrintConfigSources(void);


char* EvaluatedValue(char const* value, ClassAd const* ad) {
    classad::Value res;
    ClassAd empty;
    bool rc = (ad) ? ad->EvaluateExpr(value, res) : empty.EvaluateExpr(value, res);
    if (!rc) return NULL;
    std::stringstream ss;
    ss << res;
    return strdup(ss.str().c_str());
}

// consume the next command line argument, otherwise return an error
// note that this function MAY change the value of i
//
static const char * use_next_arg(const char * arg, const char * argv[], int & i)
{
	if (argv[i+1]) {
		return argv[++i];
	}

	fprintf(stderr, "-%s requires an argument\n", arg);
	//usage();
	my_exit(1);
	return NULL;
}

// remote queries return raw values as
// EFFECTIVE_NAME = value
// this function returns a pointer to the value part or
// it returns the input string if there is no = in the string.
//
static const char * RemoteRawValuePart(MyString & raw)
{
	const char * praw = raw.Value();
	while (*praw) {
		if (*praw == '=') {
			++praw;
			while (*praw == ' ') ++praw;
			return praw;
		}
		++praw;
	}
	return raw.Value();
}

int
main( int argc, const char* argv[] )
{
	char	*value = NULL, *tmp = NULL;
	const char * pcolon;
	const char *host = NULL;
	const char *addr = NULL;
	const char *name = NULL;
	const char *pool = NULL;
	const char *local_name = NULL;
	const char *subsys = "TOOL";
	bool	ask_a_daemon = false;
	bool    verbose = false;
	bool    dash_usage = false;
	bool    dump_all_variables = false;
	bool    expand_dumped_variables = false;
	bool    show_by_usage = false;
	bool    show_by_usage_unused = false;
	bool    evaluate_daemon_vars = false;
	bool    print_config_sources = false;
	bool	write_config = false;
	bool    dash_debug = false;
	bool    dash_raw = false;
	bool    dash_default = false;
	const char * debug_flags = NULL;
	
	PrintType pt = CONDOR_NONE;
	ModeType mt = CONDOR_QUERY;

	MyName = argv[0];
	myDistro->Init( argc, argv );

	for (int i = 1; i < argc; ++i) {
#if 1
		// arguments that don't begin with "-" are params to be looked up.
		if (*argv[i] != '-') {
			params.append(strdup(argv[i]));
			continue;
		}

		// if we get here, the arg begins with "-",
		const char * arg = argv[i]+1;
		if (is_arg_prefix(arg, "host", 1)) { // as of 8/27/2013 -host is a secret option used by condor_init
			host = use_next_arg("host", argv, i);
		} else if (is_arg_prefix(arg, "name", 1)) {
			const char * tmp = use_next_arg("name", argv, i);
			name = get_daemon_name(tmp);
			if ( ! name) {
				fprintf(stderr, "%s: unknown host %s\n", MyName, get_host_part(tmp));
				my_exit(1);
			}
		} else if (is_arg_prefix(arg, "address", 1)) {
			addr = use_next_arg("address", argv, i);
			if ( ! is_valid_sinful(addr)) {
				fprintf(stderr, "%s: invalid address %s\n"
						"Address must be of the form \"<111.222.333.444:555>\n"
						"   where 111.222.333.444 is the ip address and 555 is the port\n"
						"   you wish to connect to (the punctuation is important).\n", 
						MyName, addr);
			}
		} else if (is_arg_prefix(arg, "pool", 1)) {
			pool = use_next_arg("pool", argv, i);
		} else if (is_arg_prefix(arg, "local-name", 2)) {
			local_name = use_next_arg("local_name", argv, i);
		} else if (is_arg_prefix(arg, "subsystem", 3)) {
			subsys = use_next_arg("subsystem", argv, i);
		} else if (is_arg_prefix(arg, "owner", 1)) {
			pt = CONDOR_OWNER;
		} else if (is_arg_prefix(arg, "tilde", 1)) {
			pt = CONDOR_TILDE;
		} else if (is_arg_prefix(arg, "master", 2)) {
			dt = DT_MASTER;
			ask_a_daemon = true;
		} else if (is_arg_prefix(arg, "schedd", 2)) {
			dt = DT_SCHEDD;
		} else if (is_arg_prefix(arg, "startd", 2)) {
			dt = DT_STARTD;
		} else if (is_arg_prefix(arg, "collector", 3)) {
			dt = DT_COLLECTOR;
		} else if (is_arg_prefix(arg, "negotiator", 3)) {
			dt = DT_NEGOTIATOR;
		} else if (is_arg_prefix(arg, "set", 2)) {
			mt = CONDOR_SET;
		} else if (is_arg_prefix(arg, "unset", 3)) {
			mt = CONDOR_UNSET;
		} else if (is_arg_prefix(arg, "rset", 3)) {
			mt = CONDOR_RUNTIME_SET;
		} else if (is_arg_prefix(arg, "runset", 5)) {
			mt = CONDOR_RUNTIME_UNSET;
		} else if (is_arg_prefix(arg, "mixedcase", 2)) {
			mixedcase = true;
		} else if (is_arg_prefix(arg, "diagnostic", 4)) {
			diagnostic = true;
		} else if (is_arg_prefix(arg, "raw", 3)) {
			dash_raw = true;
		} else if (is_arg_prefix(arg, "default", 3)) {
			dash_default = true;
		} else if (is_arg_prefix(arg, "config", 2)) {
			print_config_sources = true;
		} else if (is_arg_colon_prefix(arg, "verbose", &pcolon, 1)) {
			verbose = true;
			if (pcolon) {
				++pcolon;
				if (is_arg_prefix(pcolon, "usage", 2) || 
					is_arg_prefix(pcolon, "use", -1)) {
					dash_usage = true;
				}
			}
		} else if (is_arg_prefix(arg, "dump", 1)) {
			dump_all_variables = true;
		} else if (is_arg_prefix(arg, "expanded", 2)) {
			expand_dumped_variables = true;
		} else if (is_arg_prefix(arg, "evaluate", 2)) {
			evaluate_daemon_vars = true;
		} else if (is_arg_prefix(arg, "unused", 4)) {
			show_by_usage = true;
			show_by_usage_unused = true;
		} else if (is_arg_prefix(arg, "used", 2)) {
			show_by_usage = true;
			show_by_usage_unused = false;
			//dash_usage = true;
		} else if (is_arg_prefix(arg, "writeconfig", 2)) {
			write_config = true;
		} else if (is_arg_colon_prefix(arg, "debug", &pcolon, 2)) {
				// dprintf to console
			dash_debug = true;
			if (pcolon) { 
				debug_flags = ++pcolon;
				if (*debug_flags == '"') ++debug_flags;
			}
		} else if (is_arg_prefix(arg, "help", 2)) {
			usage();
		} else {
			fprintf(stderr, "%s is not valid argument\n", argv[i]);
			usage();
		}
#else
		if( match_prefix( argv[i], "-host" ) ) {
			if( argv[i + 1] ) {
				host = strdup( argv[++i] );
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-name" ) ) {
			if( argv[i + 1] ) {
				i++;
				name = get_daemon_name( argv[i] );
				if( ! name ) {
					fprintf( stderr, "%s: unknown host %s\n", MyName, 
							 get_host_part(argv[i]) );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-address" ) ) {
			if( argv[i + 1] ) {
				i++;
				if( is_valid_sinful(argv[i]) ) {
					addr = strdup( argv[i] );
				} else {
					fprintf( stderr, "%s: invalid address %s\n"
						 "Address must be of the form \"<111.222.333.444:555>\n"
						 "   where 111.222.333.444 is the ip address and 555 is the port\n"
						 "   you wish to connect to (the punctuation is important).\n", 
						 MyName, argv[i] );
					my_exit( 1 );
				}
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-pool" ) ) {
			if( argv[i + 1] ) {
				i++;
				pool = argv[i];
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-local-name" ) ) {
			if( argv[i + 1] ) {
				i++;
				local_name = argv[i];
			} else {
				usage();
			}
		} else if( match_prefix( argv[i], "-owner" ) ) {
			pt = CONDOR_OWNER;
		} else if( match_prefix( argv[i], "-tilde" ) ) {
			pt = CONDOR_TILDE;
		} else if( match_prefix( argv[i], "-master" ) ) {
			dt = DT_MASTER;
			ask_a_daemon = true;
		} else if( match_prefix( argv[i], "-schedd" ) ) {
			dt = DT_SCHEDD;
		} else if( match_prefix( argv[i], "-startd" ) ) {
			dt = DT_STARTD;
		} else if( match_prefix( argv[i], "-collector" ) ) {
			dt = DT_COLLECTOR;
		} else if( match_prefix( argv[i], "-negotiator" ) ) {
			dt = DT_NEGOTIATOR;
		} else if( match_prefix( argv[i], "-set" ) ) {
			mt = CONDOR_SET;
		} else if( match_prefix( argv[i], "-unset" ) ) {
			mt = CONDOR_UNSET;
		} else if( match_prefix( argv[i], "-rset" ) ) {
			mt = CONDOR_RUNTIME_SET;
		} else if( match_prefix( argv[i], "-runset" ) ) {
			mt = CONDOR_RUNTIME_UNSET;
		} else if( match_prefix( argv[i], "-mixedcase" ) ) {
			mixedcase = true;
		} else if( match_prefix( argv[i], "-config" ) ) {
			print_config_sources = true;
		} else if( match_prefix( argv[i], "-verbose" ) ) {
			verbose = true;
		} else if( match_prefix( argv[i], "-dump" ) ) {
			dump_all_variables = true;
		} else if( match_prefix( argv[i], "-expand" ) ) {
			expand_dumped_variables = true;
		} else if( match_prefix( argv[i], "-evaluate" ) ) {
			evaluate_daemon_vars = true;
		} else if( match_prefix( argv[i], "-writeconfig" ) ) {
			write_config = true;
		} else if( match_prefix( argv[i], "-debug" ) ) {
				// dprintf to console
			debug = true;
		} else if( match_prefix( argv[i], "-" ) ) {
			usage();
		} else {
			MyString str;
			str = argv[i];
			// remove any case sensitivity, this is done mostly so output
			// later can look nice. The param() subsystem inherently assumes
			// case insensitivity, so this is perfectly fine to do here.
			str.upper_case();
			params.append( strdup( str.Value() ) ) ;
		}
#endif
	}

	// Set subsystem to tool, and subsystem name to either "TOOL" or what was 
	// specified on the command line.
	set_mySubSystem(subsys, SUBSYSTEM_TYPE_TOOL);

		// Honor any local name we might want to use for the param system
		// while looking up variables.
	if( local_name != NULL ) {
		get_mySubSystem()->setLocalName( local_name );
	}

		// We need to do this before we call config().  A) we don't
		// need config() to find this out and B) config() will fail if
		// all the config sources aren't setup yet, and we use
		// "condor_config_val -owner" for condor_init.  Derek 9/23/99 
	if( pt == CONDOR_OWNER ) {
		printf( "%s\n", get_condor_username() );
		my_exit( 0 );
	}

		// We need to handle -tilde before we call config() for the
		// same reasons listed above for -owner. -Derek 2/16/00
	if( pt == CONDOR_TILDE ) {
		if( (tmp = get_tilde()) ) {
			printf( "%s\n", tmp );
			my_exit( 0 );
		} else {
			fprintf( stderr, "Error: Specified -tilde but can't find " 
					 "condor's home directory\n" );
			my_exit( 1 );
		}
	}		

		// Want to do this before we try to find the address of a
		// remote daemon, since if there's no -pool option, we need to
		// param() for the COLLECTOR_HOST to contact.
	if( host ) {
		config_host( host );
	} else {
		config( 0, true );
		if (print_config_sources) {
			PrintConfigSources();
		}
	}

	if (dash_debug) {
		dprintf_set_tool_debug("TOOL", 0);
		if (debug_flags) set_debug_flags(debug_flags, 0);
	}

	// temporary, to get rid of build warning.
	if (dash_default) { fprintf(stderr, "-default not (yet) supported\n"); }

	if(write_config == true) {
		write_config_file("static_condor_config");
	}
	
	if( pool && ! name ) {
		fprintf( stderr, "Error: you must specify -name with -pool\n" );
		my_exit( 1 );
	}

	if( name || addr || mt != CONDOR_QUERY || dt != DT_MASTER ) {
		ask_a_daemon = true;
	}

	if (dump_all_variables && ! ask_a_daemon) {
		/* XXX -dump only currently spits out variables found through the
			configuration file(s) available to this tool. Environment overloads
			would be reflected. */

		char *source = NULL;
		char * hostname = param("FULL_HOSTNAME");
		if (hostname == NULL) {
			hostname = strdup("<unknown hostname>");
		}

		fprintf(stdout, 
			"# Showing macros from configuration files(s) only.\n");
		fprintf(stdout, 
			"# Environment overloads were honored.\n\n");

		fprintf(stdout,
			"# Configuration from machine: %s\n",
			hostname);

		fprintf(stdout, 
			"# Contributing configuration file(s):\n");

		// dump all the files I found.
		if (global_config_source.Length() > 0) {
			fprintf( stdout, "#\t%s\n", global_config_source.Value() );
		}
		local_config_sources.rewind();
		while ( (source = local_config_sources.next()) != NULL ) {
			fprintf( stdout, "#\t%s\n", source );
		}

		params.rewind();
		if (params.number()) {
			while ((tmp = params.next()) != NULL) {
				fprintf(stdout, "\n# Parameters with names that match %s:\n", tmp);
				std::vector<std::string> names;
				Regex re; int err = 0; const char * pszMsg = 0;
				if (re.compile(tmp, &pszMsg, &err, PCRE_CASELESS)) {
					if (param_names_matching(re, names)) {
						for (int ii = 0; ii < (int)names.size(); ++ii) {
							const char * name = names[ii].c_str();
							MyString name_used, filename, def_value;
							int line_number, use_count, ref_count;
							const char * rawval = param_get_info(name, subsys, local_name,
															name_used, use_count, ref_count,
															filename, line_number);
							if ( ! rawval)
								continue;
							if (show_by_usage) {
								if (show_by_usage_unused && use_count+ref_count > 0)
									continue;
								if ( ! show_by_usage_unused && use_count+ref_count <= 0)
									continue;
							}
							if (expand_dumped_variables) {
								MyString upname = name; upname.upper_case();
								fprintf(stdout, "%s = %s\n", upname.Value(), param(name));
							} else {
								MyString upname = name_used;
								if (upname.IsEmpty()) upname = name;
								upname.upper_case();
								fprintf(stdout, "%s = %s\n", upname.Value(), rawval);
							}
							if (verbose) {
								if (line_number < 0) {
									fprintf(stdout, " # at: %s\n", filename.Value());
								} else {
									fprintf(stdout, " # at: %s, Line %d\n", filename.Value(), line_number);
									const char * def_val = param_default_string(name, subsys);
									if (def_val) { fprintf(stdout, " # default: %s\n", def_val); }
								}
								if (expand_dumped_variables) {
									fprintf(stdout, " # raw: %s\n", rawval);
								} else {
									fprintf(stdout, " # expanded: %s\n", param(name));
								}
								if (dash_usage) {
									if (ref_count) fprintf(stdout, " # use_count: %d / %d\n", use_count, ref_count);
									else fprintf(stdout, " # use_count: %d\n", use_count);
								}
							}
						}
					}
				}
			}
		} else {

			fprintf( stdout, "\n");

			ExtArray<ParamValue> *pvs = NULL;
			ParamValue pv;
			MyString upname;
			int j;
			pvs = param_all();

			// dump the configuration file attributes.
			for (j = 0; j < pvs->getlast() + 1; j++) {
				pv = (*pvs)[j];
				upname = pv.name;
				upname.upper_case();

				if (expand_dumped_variables) {
					fprintf(stdout, "%s = %s\n", upname.Value(), param(upname.Value()));
				} else {
					fprintf(stdout, "%s = %s\n", upname.Value(), pv.value.Value());
				}
				if (verbose) {
					if (pv.lnum < 0) {
						fprintf(stdout, " # %s\n", pv.filename.Value());
					} else {
						fprintf(stdout, " # at: %s, Line %d\n", pv.filename.Value(), pv.lnum);
						const char * def_val = param_default_string(pv.name.Value(), subsys);
						if (def_val) { fprintf(stdout, " # default: %s\n", def_val); }
					}
					if (expand_dumped_variables) {
						fprintf(stdout, " # raw: %s\n", pv.value.Value());
					} else {
						fprintf(stdout, " # expanded: %s\n", param(upname.Value()));
					}
				}
			}
			delete pvs;
		}

		fflush( stdout );

		if (hostname != NULL) {
			free(hostname);
			hostname = NULL;
		}

		my_exit( 0 );

	}

	params.rewind();
	if( ! params.number() && !print_config_sources) {
		if (dump_all_variables) {
			params.append(strdup(""));
			params.rewind();
			//if (diagnostic) fprintf(stderr, "querying all\n");
		} else {
			usage();
		}
	}

	Daemon* target = NULL;
	if( ask_a_daemon ) {
		if( addr ) {
			target = new Daemon( dt, addr, NULL );
		} else {
			char* collector_addr = NULL;
			if( pool ) {
				collector_addr = strdup(pool);
			} else { 
				CollectorList * collectors = CollectorList::create();
				Daemon * collector = NULL;
				ReliSock sock;
				while (collectors->next (collector)) {
					if (collector->locate() &&
					    sock.connect((char*) collector->addr(), 0)) {
						// Do something with the connection, 
						// such that we won't end up with 
						// noise in the collector log
						collector->startCommand( DC_NOP, &sock, 30 );
						sock.encode();
						sock.end_of_message();
						// If we can connect to the
						// collector, then we accept
						// it as valid
						break;
					}
				}
				if( (!collector) || (!collector->addr()) ) {
					fprintf( stderr, 
							 "%s, Unable to locate a collector\n", 
							 MyName);
					my_exit( 1 );
				}
				collector_addr = strdup(collector->addr());
				delete collectors;
			}
			target = new Daemon( dt, name, collector_addr );
			free( collector_addr );
		}
		if( ! target->locate() ) {
			fprintf( stderr, "Can't find address for this %s\n", 
					 daemonString(dt) );
			fprintf( stderr, "Perhaps you need to query another pool.\n" );
			my_exit( 1 );
		}
	}

    if (target && evaluate_daemon_vars && !target->daemonAd()) {
        // NULL is one possible return value from this method, for example startd
        fprintf(stderr, "Warning: Classad for %s daemon '%s' is null, will evaluate expressions against empty classad\n", 
                daemonString(target->type()), target->name());
    }

	while( (tmp = params.next()) ) {
		if( mt == CONDOR_SET || mt == CONDOR_RUNTIME_SET ||
			mt == CONDOR_UNSET || mt == CONDOR_RUNTIME_UNSET ) {
			SetRemoteParam( target, tmp, mt );
		} else {
			MyString raw_value, file_and_line, def_value, usage_report;
			bool raw_supported = false;
			//fprintf(stderr, "param = %s\n", tmp);
			if( target ) {
				//fprintf(stderr, "dump = %d\n", dump_all_variables);
				if (dump_all_variables) {
					value = NULL;
					std::vector<std::string> names;
					//MyString pat = tmp;
					//if ( ! strstr(tmp, "*")) { pat.formatstr(".*%s.*", tmp); }
					//int iret = GetRemoteParamNamesMatching(target, pat.Value(), names);
					int iret = GetRemoteParamNamesMatching(target, tmp, names);
					if (iret == -2) {
						fprintf(stderr, "-dump is not supported by the remote version of HTCondor\n");
						my_exit(1);
					}
					if (iret > 0) {
						for (int ii = 0; ii < (int)names.size(); ++ii) {
							value = GetRemoteParamRaw(target, names[ii].c_str(), raw_supported, raw_value, file_and_line, def_value, usage_report);
							if (show_by_usage && ! usage_report.IsEmpty()) {
								if (show_by_usage_unused && usage_report != "0")
									continue;
								if ( ! show_by_usage_unused && usage_report == "0")
									continue;
							}
							MyString ucname = names[ii];
							ucname.upper_case();
							if (expand_dumped_variables || ! raw_supported) {
								printf("%s = %s\n", ucname.Value(), value);
							} else {
								printf("%s = %s\n", ucname.Value(), RemoteRawValuePart(raw_value));
							}
							if ( ! raw_supported && (verbose || ! expand_dumped_variables)) {
								printf(" # remote HTCondor version does not support -verbose");
							}
							if (verbose) {
								if (expand_dumped_variables) {
									printf(" # from: %s\n", raw_value.Value());
								} else {
									printf(" # expanded: %s\n", value);
								}
								if ( ! file_and_line.IsEmpty()) {
									printf(" # at: %s\n", file_and_line.Value());
								}
								if ( ! def_value.IsEmpty()) {
									printf(" # default: %s\n", def_value.Value());
								}
								if (dash_usage && ! usage_report.IsEmpty()) {
									printf(" # use_count: %s\n", usage_report.Value());
								}
							}
							//free(value);
						}
						value = NULL;
					}
					continue;
				} else if (dash_raw || verbose) {
					value = GetRemoteParamRaw(target, tmp, raw_supported, raw_value, file_and_line, def_value, usage_report);
					if ( ! verbose && ! raw_value.IsEmpty()) {
						free(value);
						value = strdup(RemoteRawValuePart(raw_value));
					}
				} else {
					value = GetRemoteParam(target, tmp);
				}
                if (value && evaluate_daemon_vars) {
                    char* ev = EvaluatedValue(value, target->daemonAd());
                    if (ev != NULL) {
                        free(value);
                        value = ev;
                    } else {
                        fprintf(stderr, "Warning: Failed to evaluate '%s', returning it as config value\n", value);
                    }
                }
			} else {
				if (dash_raw || verbose) {
					MyString name_used;
					int line_number, use_count, ref_count;
					const char * val = param_get_info(tmp, subsys, local_name,
											name_used, use_count, ref_count,
											file_and_line, line_number);
					if (val) {
						raw_supported = true;
						if ( ! verbose) {
							value = strdup(val);
						} else {
							value = param(tmp);
							raw_value = name_used;
							raw_value.upper_case();
							raw_value += " = ";
							raw_value += val;
							if (line_number >= 0)
								file_and_line.formatstr_cat(", line %d", line_number);
							if (line_number != -2)
								def_value = param_exact_default_string(name_used.Value());
							if (ref_count) {
								usage_report.formatstr("%d / %d", use_count, ref_count);
							} else {
								usage_report.formatstr("%d", use_count);
							}
						}
					}
				} else {
					value = param(tmp);
					if (verbose) {
						int      line_number;
						param_get_location(tmp, file_and_line, line_number);
						if (file_and_line != "<Internal>") {
							const char * val = param_default_string(tmp, subsys);
							if (val)
								def_value = macro_expand(val);
						}
						if (line_number >= 0)
							file_and_line.formatstr_cat(", line %d", line_number);
					}
				}
			}
			if( value == NULL ) {
				fprintf(stderr, "Not defined: %s\n", tmp);
				my_exit( 1 );
			} else {
				if (verbose) {
					printf("%s: %s\n", tmp, value);
				} else {
					printf("%s\n", value);
				}
				free( value );
				if ( ! raw_supported && (dash_raw || verbose)) {
					printf(" # the remote HTCondor version does not support -raw or -verbose");
				}
				if (verbose) {
#if 1
					if ( ! raw_value.IsEmpty()) {
						printf(" # from: %s\n", raw_value.Value());
					}
					if ( ! file_and_line.IsEmpty()) {
						printf(" # at: %s\n", file_and_line.Value());
					}
					if ( ! def_value.IsEmpty()) {
						printf(" # default: %s\n", def_value.Value());
					}
					if (dash_usage && ! usage_report.IsEmpty()) {
						printf(" # use_count: %s\n", usage_report.Value());
					}
					printf("\n");
#else  // this is the old format, do we need to match this exactly?
					if (line_number == -1) {
						printf("  Defined in '%s'.\n\n", filename.Value());
					} else {
						printf("  Defined in '%s', line %d.\n\n",
							   filename.Value(), line_number);
					}
#endif
				}
			}
		}
	}
	my_exit( 0 );
	return 0;
}


char*
GetRemoteParam( Daemon* target, char* param_name ) 
{
    ReliSock s;
	s.timeout( 30 );
	char	*val = NULL;

		// note: printing these things out in each dprintf() is
		// stupid.  we should be using Daemon::idStr().  however,
		// since this code didn't used to use a Daemon object at all,
		// and since it's so hard for us to change the output of our
		// tools for fear that someone's ASCII parser will break, i'm
		// just cheating and being lazy here by replicating the old
		// behavior...
	char* addr;
	const char* name;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if( !name ) {
			name = "";
		}

		if( s.connect( addr, 0 ) ) {
			connect_error = false;
			break;
		}
	} while( target->nextValidCm() == true );

	if( connect_error == true ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemonString(dt), name, addr );
		my_exit(1);
	}

	target->startCommand( CONFIG_VAL, &s, 30 );

	s.encode();
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send request (%s)\n", param_name );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't send end of message\n" );
		return NULL;
	}

	s.decode();
	if( !s.code(val) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemonString(dt), name, addr );
		return NULL;
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		return NULL;
	}

	return val;
}

char*
GetRemoteParamRaw(
	Daemon* target,
	const char * param_name,
	bool & raw_supported,
	MyString & raw_value,
	MyString & file_and_line,
	MyString & def_value,
	MyString & usage_report)
{
	ReliSock s;
	s.timeout(30);
	char *val = NULL;

	raw_supported = false;
	raw_value = "";
	file_and_line = "";
	def_value = "";
	usage_report = "";

	char* addr = NULL;
	const char* name = NULL;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if (s.connect(addr, 0)) {
			connect_error = false;
			break;
		}
	} while(target->nextValidCm());

	if (connect_error) {
		fprintf (stderr, "Can't connect to %s on %s %s\n", daemonString(dt), name ? name : "", addr);
		my_exit(1);
	}

	target->startCommand(DC_CONFIG_VAL, &s, 30);
	s.encode();

	if (diagnostic) fprintf(stderr, "sending %s\n", param_name);
	if ( ! s.code(const_cast<char*&>(param_name))) {
		fprintf(stderr, "Can't send request (%s)\n", param_name);
		return NULL;
	}
	if ( ! s.end_of_message()) {
		fprintf (stderr, "Can't send end of message\n");
		return NULL;
	}

	s.decode();
	if ( ! s.code(val)) {
		fprintf(stderr, "Can't receive reply from %s on %s %s\n", daemonString(dt), name ? name : "", addr );
		return NULL;
	}
	if (diagnostic) fprintf(stderr, "result: '%s'\n", val);

	// daemons from 8.1.2 and later will return more than one string from this query.
	// after the param value, we expect to see
	//    <used_name> = <raw_value>\0
	//    <file>, line <num>\0
	//
	int ix = 0;
	while ( ! s.peek_end_of_message()) {
		MyString dummy;
		MyString * ps = &dummy;
		if (0 == ix) ps = &raw_value;
		else if (1 == ix) ps = &file_and_line;
		else if (2 == ix) ps = &def_value;
		else if (3 == ix) ps = &usage_report;

		if ( ! s.code(*ps)) { 
			fprintf(stderr, "Can't read line %d\n", ix); 
			break; 
		}
		if (diagnostic) fprintf(stderr, "result[%d]: '%s'\n", ix, ps->Value());
		++ix;
		// if we got more than one string in the reply, then the remote daemon supports -raw
		raw_supported = true;
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't receive end of message\n" );
		return NULL;
	}

	return val;
}

int
GetRemoteParamNamesMatching(Daemon* target, const char * param_pat, std::vector<std::string> & names)
{
	ReliSock s;
	s.timeout(30);

	char* addr = NULL;
	const char* name = NULL;
	bool connect_error = true;
	do {
		addr = target->addr();
		name = target->name();
		if (s.connect(addr, 0)) {
			connect_error = false;
			break;
		}
	} while(target->nextValidCm());

	if (connect_error) {
		fprintf (stderr, "Can't connect to %s on %s %s\n", daemonString(dt), name ? name : "", addr);
		my_exit(1);
	}

	target->startCommand(DC_CONFIG_VAL, &s, 30);
	s.encode();

	// if we do DC_CONFIG_VAL query of the param name "?names" we will either get
	// back a set of strings containing parameter names, or "Not defined" if the daemon 
	// is pre 8.1.2
	//
	MyString query("?names");
	//if (param_pat && param_pat[0] == '*' && ! param_pat[1]) {
	//	; // ?names without a qualifier implies a ".*" pattern
	//} else
	if (param_pat && param_pat[0]) {
		query += ":";
		query += param_pat;
	}

	if (diagnostic) fprintf(stderr, "sending %s\n", query.Value());
	if ( ! s.code(query)) {
		fprintf(stderr, "Can't send request for names matching %s'\n", param_pat);
		return 0;
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't send end of message\n" );
		return 0;
	}

	s.decode();
	std::string val;
	if ( ! s.code(val)) {
		fprintf(stderr, "Can't receive reply from %s on %s %s\n", daemonString(dt), name ? name : "", addr );
		return 0;
	}

	// daemons from 8.1.2 and later will return a set of strings
	// containing parameter names or a single string of the form
	// "!error:type:num: message"
	// daemons prior to 8.1.2 will return "Not Defined"
	//
	if (diagnostic) fprintf(stderr, "result: '%s'\n", val.c_str());
	if (val == "Not defined") {
		if ( ! s.end_of_message()) {
			fprintf(stderr, "Can't receive end of message\n");
		}
		return -2; // -2 for not supported.
	}

	// param names can't start with !, so if the first character is !
	// then the reply is an error message of the form "!error:type:num: message"
	if (val[0] == '!') {
		fprintf(stderr, "%s\n", val.substr(1).c_str());
		if ( ! s.end_of_message()) {
			fprintf(stderr, "Can't receive end of message\n");
		}
		return -1;
	}

	// an empty string means that nothing matched the query.
	if ( ! val.empty()) {
		names.push_back(val);
	}

	while ( ! s.peek_end_of_message()) {
		if ( ! s.code(val)) { 
			fprintf(stderr, "Can't read name\n"); 
			break; 
		}
		if (diagnostic) fprintf(stderr, "result: '%s'\n", val.c_str());
		names.push_back(val);
	}
	if ( ! s.end_of_message()) {
		fprintf( stderr, "Can't receive end of message\n" );
		return 0;
	}

	if (names.size() > 1) {
		std::sort(names.begin(), names.end(), sort_ascending_ignore_case);
	}
	return names.size();
}

void
SetRemoteParam( Daemon* target, char* param_value, ModeType mt )
{
	int cmd = DC_NOP, rval;
	ReliSock s;
	bool set = false;

		// note: printing these things out in each dprintf() is
		// stupid.  we should be using Daemon::idStr().  however,
		// since this code didn't used to use a Daemon object at all,
		// and since it's so hard for us to change the output of our
		// tools for fear that someone's ASCII parser will break, i'm
		// just cheating and being lazy here by replicating the old
		// behavior...
	char* addr;
	const char* name;
	bool connect_error = true;

		// We need to know two things: what command to send, and (for
		// error messages) if we're setting or unsetting.  Since our
		// bool "set" defaults to false, we only hit the "set = true"
		// statements when we want to, and fall through to also set
		// the right commands in those cases... Derek Wright 9/1/99
	switch (mt) {
	case CONDOR_SET:
		set = true;
	case CONDOR_UNSET:
		cmd = DC_CONFIG_PERSIST;
		break;
	case CONDOR_RUNTIME_SET:
		set = true;
	case CONDOR_RUNTIME_UNSET:
		cmd = DC_CONFIG_RUNTIME;
		break;
	default:
		fprintf( stderr, "Unknown command type %d\n", (int)mt );
		my_exit( 1 );
	}

		// Now, process our strings for sanity.
	char* param_name = strdup( param_value );
	char* tmp = NULL;

	if( set ) {
		tmp = strchr( param_name, ':' );
		if( ! tmp ) {
			tmp = strchr( param_name, '=' );
		}
		if( ! tmp ) {
			fprintf( stderr, "%s: Can't set configuration value (\"%s\")\n" 
					 "You must specify \"macro_name = value\" or " 
					 "\"expr_name : value\"\n", MyName, param_name );
			my_exit( 1 );
		}
			// If we're still here, we found a ':' or a '=', so, now,
			// chop off everything except the attribute name
			// (including spaces), so we can send that seperately. 
		do {
			*tmp = '\0';
			tmp--;
		} while( *tmp == ' ' );
	} else {
			// Want to do different sanity checking.
		if( (tmp = strchr(param_name, ':')) || 
			(tmp = strchr(param_name, '=')) ) {
			fprintf( stderr, "%s: Can't unset configuration value (\"%s\")\n" 
					 "To unset, you only specify the name of the attribute\n", 
					 MyName, param_name );
			my_exit( 1 );
		}
		tmp = strchr( param_name, ' ' );
		if( tmp ) {
			*tmp = '\0';
		}
	}

		// At this point, in either set or unset mode, param_name
		// should hold a valid name, so do a final check to make sure
		// there are no spaces.
	if( !is_valid_param_name(param_name) ) {
		fprintf( stderr, 
				 "%s: Error: Configuration variable name (%s) is not valid, alphanumeric and _ only\n",
				 MyName, param_name );
		my_exit( 1 );
	}

	if (!mixedcase) {
		strlwr(param_name);		// make the config name case insensitive
	}

		// We need a version with a newline at the end to make
		// everything cool at the other end.
	char* buf = (char*)malloc( strlen(param_value) + 2 );
	ASSERT( buf != NULL );
	sprintf( buf, "%s\n", param_value );

	s.timeout( 30 );
	do {
		addr = target->addr();
		name = target->name();
		if( !name ) {
			name = "";
		}

		if( s.connect( addr, 0 ) ) {
			connect_error = false;
			break;
		}
	} while( target->nextValidCm() == true );

	if( connect_error == true ) {
		fprintf( stderr, "Can't connect to %s on %s %s\n", 
				 daemonString(dt), name, addr );
		my_exit(1);
	}

	target->startCommand( cmd, &s );

	s.encode();
	if( !s.code(param_name) ) {
		fprintf( stderr, "Can't send config name (%s)\n", param_name );
		my_exit(1);
	}
	if( set ) {
		if( !s.code(param_value) ) {
			fprintf( stderr, "Can't send config setting (%s)\n", param_value );
			my_exit(1);
		}
	} else {
		if( !s.put("") ) {
			fprintf( stderr, "Can't send config setting\n" );
			my_exit(1);
		}
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't send end of message\n" );
		my_exit(1);
	}

	s.decode();
	if( !s.code(rval) ) {
		fprintf( stderr, "Can't receive reply from %s on %s %s\n",
				 daemonString(dt), name, addr );
		my_exit(1);
	}
	if( !s.end_of_message() ) {
		fprintf( stderr, "Can't receive end of message\n" );
		my_exit(1);
	}
	if (rval < 0) {
		if (set) {
			fprintf( stderr, "Attempt to set configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_value, daemonString(dt), name, addr );
		} else {
			fprintf( stderr, "Attempt to unset configuration \"%s\" on %s %s "
					 "%s failed.\n",
					 param_value, daemonString(dt), name, addr );
		}
		my_exit(1);
	}
	if (set) {
		fprintf( stdout, "Successfully set configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_value, daemonString(dt), name, addr );
	} else {
		fprintf( stdout, "Successfully unset configuration \"%s\" on %s %s "
				 "%s.\n",
				 param_value, daemonString(dt), name, addr );
	}

	free( buf );
	free( param_name );
}

static void PrintConfigSources(void)
{
		// print descriptive lines to stderr and config source names to
		// stdout, so that the output can be cleanly piped into
		// something like xargs...

	if (global_config_source.Length() > 0) {
		fprintf( stderr, "Configuration source:\n" );
		fflush( stderr );
		fprintf( stdout, "\t%s\n", global_config_source.Value() );
		fflush( stdout );
	} else {
		fprintf( stderr, "Can't find the configuration source.\n" );
	}

	unsigned int numSources = local_config_sources.number();
	if (numSources > 0) {
		if (numSources == 1) {
			fprintf( stderr, "Local configuration source:\n" );
		} else {
			fprintf( stderr, "Local configuration sources:\n" );
		}
		fflush( stderr );

		char *source;
		local_config_sources.rewind();
		while ( (source = local_config_sources.next()) != NULL ) {
			fprintf( stdout, "\t%s\n", source );
			fflush( stdout );
		}

		fprintf(stderr, "\n");
	}

	return;
}
