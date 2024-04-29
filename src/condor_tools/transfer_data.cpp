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
#include "condor_config.h"
#include "match_prefix.h"
#include "sig_install.h"
#include "condor_attributes.h"
#include "condor_version.h"
#include "condor_ver_info.h"
#include "dc_schedd.h"
#include "condor_distribution.h"
#include "basename.h"
#include "internet.h"
#include "condor_classad.h"


const char	*MyName = NULL;
std::string global_constraint;
bool had_error = false;
DCSchedd* schedd = NULL;
bool All = false;

void PREFAST_NORETURN usage(int iExitCode=1);
void procArg(const char*);
void addConstraint(const char *);
void handleAll();

void usage(int iExitCode)
{
	fprintf( stderr, "Usage: %s [options] [constraints]\n", MyName );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );

// i'm not sure we want -debug documented.  if we change our minds, we
// should just uncomment the next line
//	fprintf( stderr, "  -debug              Display debugging information while running\n" );

	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -addr <ip:port>     Connect directly to the given \"sinful string\"\n" );
	fprintf( stderr, " and where [constraints] is one or more of:\n" );
	fprintf( stderr, "  cluster.proc        transfer data for the given job\n");
	fprintf( stderr, "  cluster             transfer data for the given cluster of jobs\n");
	fprintf( stderr, "  user                transfer data for all jobs owned by user\n" );
	fprintf( stderr, "  -constraint expr    transfer data for all jobs matching the boolean expression\n" );
	fprintf( stderr, "  -all                transfer data for all jobs "
			 "(cannot be used with other constraints)\n" );
	exit( iExitCode );
}

void
version( void )
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}


void
procArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;

	std::string constraint;

	if(isdigit(*arg))
	// process by cluster/proc #
	{
		c = strtol(arg, &tmp, 10);
		if(c <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s.\n", arg);
			had_error = true;
			return;
		}
		if(*tmp == '\0')
		// delete the cluster
		{
			formatstr( constraint, "%s==%d", ATTR_CLUSTER_ID, c );
			addConstraint(constraint.c_str());
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				had_error = 1;
				return;
			}
			if(*tmp == '\0')
			// process a proc
			{
				formatstr( constraint, "(%s==%d && %s==%d)", 
					ATTR_CLUSTER_ID, c,
					ATTR_PROC_ID, p);
				addConstraint(constraint.c_str());
				return;
			}
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
		return;
	}
	else if(isalpha(*arg))
	// process by user name
	{
		formatstr( constraint, "%s == \"%s\"", ATTR_OWNER, arg );
		addConstraint(constraint.c_str());
	} else {
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped\n", arg );
	}
}


void
addConstraint( const char *constraint )
{
	if ( global_constraint.length() > 0 ) {
		global_constraint += " || (";
	} else {
		global_constraint += "(";
	}
	global_constraint += constraint;
	global_constraint += ")";
}


void
handleAll()
{
	char constraint[128];
	snprintf( constraint, sizeof(constraint), "%s >= 0", ATTR_CLUSTER_ID );

	addConstraint(constraint);
}




int
main(int argc, char *argv[])
{
	char	*arg;
	int		nArgs = 0;				// number of args 
	int	 i, result;
	char* pool = NULL;
	char* scheddName = NULL;
	char* scheddAddr = NULL;
	const char * pcolon;

	MyName = condor_basename(argv[0]);
	set_priv_initialize(); // allow uid switching if root
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	char **args = (char **)malloc(sizeof(char *) * argc); // args 
	if ( ! args) exit(2);

	// parse the arguments.
	for( argv++; (arg = *argv); argv++ ) {
		if( arg[0] == '-' ) {
			if( ! arg[1] ) {
				usage();
			}
			switch( arg[1] ) {
			case 'd':
				// dprintf to console
				if (is_dash_arg_colon_prefix(arg, "debug", &pcolon, 1)) {
					dprintf_set_tool_debug("TOOL", (pcolon && pcolon[1]) ? pcolon+1 : nullptr);
				} else {
					fprintf(stderr, "%s: unknown argument %s\n", MyName, arg);
					exit(1);
				}
				break;
			case 'c':
				args[nArgs] = arg;
				nArgs++;
				argv++;
				if( ! *argv ) {
					fprintf( stderr, 
							 "%s: -constraint requires another argument\n", 
							 MyName);
					exit(1);
				}				
				args[nArgs] = *argv;
				nArgs++;
				break;
			case 'a':
				if( arg[2] && arg[2] == 'd' ) {
					argv++;
					if( ! *argv ) {
						fprintf( stderr, 
								 "%s: -addr requires another argument\n", 
								 MyName);
						exit(1);
					}				
					if( is_valid_sinful(*argv) ) {
						scheddAddr = strdup(*argv);
						if( ! scheddAddr ) {
							fprintf( stderr, "Out of Memory!\n" );
							exit(1);
						}
					} else {
						fprintf( stderr, 
								 "%s: \"%s\" is not a valid address\n",
								 MyName, *argv );
						fprintf( stderr, "Should be of the form "
								 "<ip.address.here:port>\n" );
						fprintf( stderr, 
								 "For example: <123.456.789.123:6789>\n" );
						exit( 1 );
					}
					break;
				} else {
					All = true;
				}
				break;
			case 'n': 
				// use the given name as the schedd name to connect to
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -name requires another argument\n", 
							 MyName);
					exit(1);
				}			
				if ( scheddName ) free(scheddName);
				scheddName = strdup(*argv);
				break;
			case 'p':
				// use the given name as the central manager to query
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -pool requires another argument\n", 
							 MyName);
					exit(1);
				}				
				if( pool ) {
					free( pool );
				}
				pool = strdup( *argv );
				break;
			case 'v':
				version();
				break;
			case 'h':
				usage(0);
				break;
			default:
				fprintf( stderr, "Unrecognized option: %s\n", arg ); 
				usage();
				break;
			}
		} else {
			if( All ) {
					// If -all is set, there should be no other
					// constraint arguments.
				usage();
			}
			args[nArgs] = arg;
			nArgs++;
		}
	}

	if( ! (All || nArgs) ) {
			// We got no indication of what to act on


		fprintf( stderr, "You did not specify any jobs\n" ); 
		usage();
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if( ! scheddAddr ) {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new DCSchedd( scheddName, pool );
	} else {
		schedd = new DCSchedd( scheddAddr );
	}
	if( ! schedd->locate() ) {
		fprintf( stderr, "%s: %s\n", MyName, schedd->error() ); 
		exit( 1 );
	}

		// Process the args.
	if( All ) {
		handleAll();
	} else {
		for(i = 0; i < nArgs; i++) {
			if( match_prefix( args[i], "-constraint" ) ) {
				i++;
				addConstraint( args[i] );
			} else {
				procArg(args[i]);
			}
		}
	}
	free(args);

		// Sanity check: make certain we now have a constraint
	if ( global_constraint.length() <= 0 ) {
		fprintf( stderr, "Unable to create a job constraint!\n");
		exit(1);
	}

	fprintf(stdout,"Fetching data files...\n");

	CondorError errstack;
	result = schedd->receiveJobSandbox(global_constraint.c_str(), &errstack);
	if ( !result ) {
		fprintf( stderr, "\n%s\n", errstack.getFullText(true).c_str() );
		fprintf( stderr, "ERROR: Failed to spool job files.\n" );
		exit(1);
	}

	// All done
	return 0;
}

