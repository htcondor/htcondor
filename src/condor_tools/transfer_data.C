/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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
#include "MyString.h"


char	*MyName = NULL;
MyString global_constraint;
bool had_error = false;
DCSchedd* schedd = NULL;
bool All = false;

void usage();
void procArg(const char*);
void addConstraint(const char *);
void handleAll();

void
usage()
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
	exit( 1 );
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

	char constraint[ATTRLIST_MAX_EXPRESSION];

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
			sprintf( constraint, "%s==%d", ATTR_CLUSTER_ID, c );
			addConstraint(constraint);
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
				sprintf( constraint, "(%s==%d && %s==%d)", 
					ATTR_CLUSTER_ID, c,
					ATTR_PROC_ID, p);
				addConstraint(constraint);
				return;
			}
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
		return;
	}
	else if(isalpha(*arg))
	// process by user name
	{
		sprintf( constraint, "%s == \"%s\"", ATTR_OWNER, arg );
		addConstraint(constraint);
	} else {
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped\n", arg );
	}
}


void
addConstraint( const char *constraint )
{
	if ( global_constraint.Length() > 0 ) {
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
	sprintf( constraint, "%s >= 0", ATTR_CLUSTER_ID );

	addConstraint(constraint);
}




int
main(int argc, char *argv[])
{
	char	*arg;
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1)); // args 
	int		nArgs = 0;				// number of args 
	int	 i, result;
	char* pool = NULL;
	char* scheddName = NULL;
	char* scheddAddr = NULL;

	myDistro->Init( argc, argv );
	MyName = basename(argv[0]);
	config();

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( argv++; (arg = *argv); argv++ ) {
		if( arg[0] == '-' ) {
			if( ! arg[1] ) {
				usage();
			}
			switch( arg[1] ) {
			case 'd':
				// dprintf to console
				Termlog = 1;
				dprintf_config ("TOOL", 2 );
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
				}
				All = true;
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
				usage();
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

		// If this schedd doesn't support the new protocol, give a
		// useful error message.
	CondorVersionInfo ver_info( schedd->version(), "SCHEDD" );
	if( ! ver_info.built_since_version(6, 7, 0) ) {
		fprintf( stderr, "The version of the condor_schedd you want to "
				 "communicate with is:\n%s\n", schedd->version() );
		fprintf( stderr, "It is too old to support this version of "
				 "%s:\n%s\n", MyName, CondorVersion() );
		fprintf( stderr, "To use this version of %s you must upgrade "
				 "the\n%s to at least version 6.7.1.\nAborting.\n",
				 MyName, schedd->idStr() ); 
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

		// Sanity check: make certain we now have a constraint
	if ( global_constraint.Length() <= 0 ) {			
		fprintf( stderr, "Unable to create a job constraint!\n");
		exit(1);
	}

		// And now, do the work.
	fprintf(stdout,"Fetching data files...\n");
	CondorError errstack;
	result = schedd->receiveJobSandbox(global_constraint.Value(),&errstack);
	if ( !result ) {
		fprintf( stderr, "\n%s\n", errstack.getFullText(true) );
		fprintf( stderr, "ERROR: Failed to spool job files.\n" );
		exit(1);
	}


	return 0;
}

/************************************
	The following are dummy stubs for the DaemonCore class to allow
	tools using DCSchedd to link.  DaemonCore is brought in
	because of the FileTransfer object.
	These stub functions will become obsoluete once we start linking
	in the real DaemonCore library with the tools, -or- once
	FileTransfer is broken down.
*************************************/
#include "../condor_daemon_core.V6/condor_daemon_core.h"
	DaemonCore* daemonCore = NULL;
	int DaemonCore::Kill_Thread(int) { return 0; }
//	char * DaemonCore::InfoCommandSinfulString(int) { return NULL; }
	int DaemonCore::Register_Command(int,char*,CommandHandler,char*,Service*,
		DCpermission,int) {return 0;}
	int DaemonCore::Register_Reaper(char*,ReaperHandler,
		char*,Service*) {return 0;}
	int DaemonCore::Create_Thread(ThreadStartFunc,void*,Stream*,
		int) {return 0;}
	int DaemonCore::Suspend_Thread(int) {return 0;}
	int DaemonCore::Continue_Thread(int) {return 0;}
//	int DaemonCore::Register_Reaper(int,char*,ReaperHandler,ReaperHandlercpp,
//		char*,Service*,int) {return 0;}
//	int DaemonCore::Register_Reaper(char*,ReaperHandlercpp,
//		char*,Service*) {return 0;}
//	int DaemonCore::Register_Command(int,char*,CommandHandlercpp,char*,Service*,
//		DCpermission,int) {return 0;}
/**************************************/
