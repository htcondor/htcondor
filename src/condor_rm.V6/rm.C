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

#ifdef __GNUG__
#pragma implementation "list.h"
#endif

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_network.h"
#include "condor_io.h"
#include "sched.h"
#include "alloc.h"
#include "get_daemon_addr.h"
#include "internet.h"
#include "condor_attributes.h"
#include "match_prefix.h"
#include  "list.h"
#include "sig_install.h"
#include "condor_version.h"
#include "daemon.h"

#include "condor_qmgr.h"

template class List<PROC_ID>;
template class Item<PROC_ID>;

char	*MyName;
BOOLEAN	TroubleReported;
BOOLEAN All = FALSE;
int nToProcess = 0;
int mode;
int error_occurred = 0;
List<PROC_ID> ToProcess;
Daemon* schedd = NULL;

	// Prototypes of local interest
void handle_constraint(const char *);
void ProcArg(const char*);
void notify_schedd();
void usage();
void handle_all();




void
usage()
{
	char word[10];
	switch( mode ) {
	case IDLE:
		sprintf( word, "Releases" );
		break;
	case HELD:
		sprintf( word, "Holds" );
		break;
	case REMOVED:
		sprintf( word, "Removes" );
		break;
	default:
		fprintf( stderr, "ERROR: Unknown mode: %d\n", mode );
		exit( 1 );
		break;
	}
	fprintf( stderr, "Usage: %s [options] [constraints]\n", MyName );
	fprintf( stderr, " where [options] is zero or more of:\n" );
	fprintf( stderr, "  -help               Display this message and exit\n" );
	fprintf( stderr, "  -version            Display version information and exit\n" );
	fprintf( stderr, "  -name schedd_name   Connect to the given schedd\n" );
	fprintf( stderr, "  -pool hostname      Use the given central manager to find daemons\n" );
	fprintf( stderr, "  -addr <ip:port>     Connect directly to the given \"sinful string\"\n" );
	fprintf( stderr, " and where [constraints] is one or more of:\n" );
	fprintf( stderr, "  cluster.proc        %s the given job\n", word );
	fprintf( stderr, "  cluster             %s the given cluster of jobs\n", word );
	fprintf( stderr, "  user                %s all jobs owned by user\n", word );
	fprintf( stderr, "  -all                %s all jobs "
			 "(Cannot be used with other constraints)\n", word );
	exit( 1 );
}


void
version( void )
{
	printf( "%s\n%s\n", CondorVersion(), CondorPlatform() );
	exit( 0 );
}


int
main( int argc, char *argv[] )
{
	char	*arg;
	char	**args = (char **)malloc(sizeof(char *)*(argc - 1)); // args 
	int					nArgs = 0;				// number of args 
	int					i;
	Qmgr_connection*	q;
	char*	cmd_str;
	char* pool = NULL;
	char* scheddName = NULL;
	char* scheddAddr = NULL;

	MyName = strrchr( argv[0], DIR_DELIM_CHAR );
	if( !MyName ) {
		MyName = argv[0];
	} else {
		MyName++;
	}

	cmd_str = strchr( MyName, '_');
	if (cmd_str && strcmp(cmd_str, "_hold") == MATCH) {
		mode = HELD;
	} else if( cmd_str && strcmp( cmd_str, "_release" ) == MATCH ) {
		mode = IDLE;
	} else {
		mode = REMOVED;
	}

	config();


	if( argc < 2 ) {
		usage();
	}

#if !defined(WIN32)
	install_sig_handler(SIGPIPE, SIG_IGN );
#endif

	for( argv++; arg = *argv; argv++ ) {
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
						fprintf( stderr, 
								 "Should be of the form <ip.address.here:port>.\n" );
						fprintf( stderr, 
								 "For example: <123.456.789.123:6789>\n" );
						exit( 1 );
					}
					break;
				}
				All = TRUE;
				break;
			case 'n': 
				// use the given name as the schedd name to connect to
				argv++;
				if( ! *argv ) {
					fprintf( stderr, "%s: -name requires another argument\n", 
							 MyName);
					exit(1);
				}				
				if( !(scheddName = get_daemon_name(*argv)) ) { 
					fprintf( stderr, "%s: unknown host %s\n", 
							 MyName, get_host_part(*argv) );
					exit(1);
				}
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
			default:
					// This gets hit for "-h", too.
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
			// We got no indication of what to remove
		usage();
	}

		// We're done parsing args, now make sure we know how to
		// contact the schedd. 
	if( ! scheddAddr ) {
			// This will always do the right thing, even if either or
			// both of scheddName or pool are NULL.
		schedd = new Daemon( DT_SCHEDD, scheddName, pool );
	} else {
		schedd = new Daemon( DT_SCHEDD, scheddAddr );
	}
	if( ! schedd->locate() ) {
		fprintf( stderr, "%s: %s\n", MyName, schedd->error() ); 
		exit( 1 );
	}

		// Open job queue
	q = ConnectQ( schedd->addr() );
	if( !q ) {
		fprintf( stderr, "Failed to connect to %s\n", schedd->idStr() );
		exit(1);
	}

	if( All ) {
		handle_all();
	} else {
			// Set status of requested jobs to REMOVED/HELD/RELEASED
		for(i = 0; i < nArgs; i++) {
			if( match_prefix( args[i], "-constraint" ) ) {
				i++;
				handle_constraint( args[i] );
			} else {
				ProcArg(args[i]);
			}
		}
	}

	// Close job queue
	DisconnectQ(q);

	// If we did a condor_rm or a condor_hold, we need to tell the schedd what 
	// we did.  We send the schedd the
	// KILL_FRGN_JOB command.  We pass the number of jobs to
	// remove/hold _unless_ one or more of the jobs are to be
	// removed/held via cluster or via user name, in which case we say
	// "-1" jobs to remove/hold.  Telling the schedd there are "-1"
	// jobs to remove forces the schedd to scan the queue looking for
	// jobs which have a REMOVED/HELD status.  If all jobs to be removed/held
	// are via a cluster.proc, we then send the schedd all the
	// cluster.proc numbers to save the schedd from having to scan the
	// entire queue.
	if ( mode != IDLE && nToProcess != 0 )
		notify_schedd();

#if defined(ALLOC_DEBUG)
	print_alloc_stats();
#endif

	return error_occurred;
}


extern "C" int SetSyscalls( int foo ) { return foo; }


void
notify_schedd()
{
	int			cmd;
	PROC_ID		*job_id;
	int 		i;

	Sock* sock;

	if (!(sock = schedd->startCommand (KILL_FRGN_JOB, Stream::reli_sock, 0))) {
		if( !TroubleReported ) {
			fprintf( stderr, "%s: can't connect to %s\n", MyName, 
					 schedd->idStr() );
			error_occurred = 1;
			TroubleReported = 1;
		}
		return;
	}

	sock->encode();

	if( !sock->code(nToProcess) ) {
		fprintf( stderr,
			"Warning: can't send num jobs to process to schedd (%d)\n",
			nToProcess );
		return;
	}

	ToProcess.Rewind();
	for (i=0;i<nToProcess;i++) {
		job_id = ToProcess.Next();
		if( !sock->code(*job_id) ) {
			fprintf( stderr,
				"Error: can't send a proc_id to condor scheduler\n" );
			error_occurred = 1;
			return;
		}
	}

	if( !sock->end_of_message() ) {
		fprintf( stderr,
			"Warning: can't send end of message to condor scheduler\n" );
		return;
	}

	sock->close();
	delete sock;
}


void ProcArg(const char* arg)
{
	int		c, p;								// cluster/proc #
	char*	tmp;
	PROC_ID *id;

	if(isdigit(*arg))
	// process by cluster/proc #
	{
		c = strtol(arg, &tmp, 10);
		if(c <= 0)
		{
			fprintf(stderr, "Invalid cluster # from %s.\n", arg);
			error_occurred = 1;
			return;
		}
		if(*tmp == '\0')
		// delete the cluster
		{
			char constraint[ATTRLIST_MAX_EXPRESSION];

			if( mode == IDLE ) {
				sprintf( constraint, "%s==%d && %s==%d", ATTR_JOB_STATUS, HELD,
					ATTR_CLUSTER_ID, c );
			} else {
				sprintf(constraint, "%s == %d", ATTR_CLUSTER_ID, c);
			}

			if(SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,mode)<0)
			{
				fprintf( stderr, "Couldn't find/%s all jobs in cluster %d.\n",
					 (mode==REMOVED)?"remove":(mode==HELD)?"hold":"release", c);
				error_occurred = 1;
			} else {
				fprintf(stderr, "Cluster %d %s.\n", c,
					(mode==REMOVED)?"has been marked for removal":
						(mode==HELD)?"held":"released");
				nToProcess = -1;
			}
			return;
		}
		if(*tmp == '.')
		{
			p = strtol(tmp + 1, &tmp, 10);
			if(p < 0)
			{
				fprintf( stderr, "Invalid proc # from %s.\n", arg);
				error_occurred = 1;
				return;
			}
			if(*tmp == '\0')
			// process a proc
			{
				if( mode==IDLE ) {
					int status;
					if( GetAttributeInt(c,p,ATTR_JOB_STATUS,&status) < 0 ) {
						fprintf(stderr,"Couldn't access job queue for %d.%d\n", 
							c, p );
						error_occurred = 1;
						return;
					}
					if( status != HELD ) {
						fprintf(stderr,"Job %d.%d not held to be released\n",
							c, p);
						error_occurred = 1;
						return;
					}
				}
					
				if(SetAttributeInt(c, p, ATTR_JOB_STATUS, mode ) < 0)
				{
					fprintf( stderr, "Couldn't find/%s job %d.%d.\n",
					 (mode==REMOVED)?"remove":(mode==HELD)?"hold":"release", 
					 c, p);
					error_occurred = 1;
				} else {
					fprintf(stdout, "Job %d.%d %s.\n", c, p,
						(mode==REMOVED)?"has been marked for removal":
							(mode==HELD)?"held":"released");
					if ( mode != IDLE && nToProcess != -1 ) {
						nToProcess++;
						id = new PROC_ID;
						id->proc = p;
						id->cluster = c;
						ToProcess.Append(id);
					}
				}
				return;
			}
			fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
			return;
		}
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
	}
	else if(isalpha(*arg))
	// process by user name
	{
		char	constraint[ATTRLIST_MAX_EXPRESSION];

		if( mode == IDLE ) {
			sprintf( constraint, "%s==%d && %s==\"%s\"", ATTR_JOB_STATUS, HELD,
				ATTR_OWNER, arg );
		} else {
			sprintf(constraint, "Owner == \"%s\"", arg);
		}
		if(SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,mode) < 0)
		{
			fprintf( stderr, "Couldn't find/%s all of user %s's job(s).\n",
				 (mode==REMOVED)?"remove":(mode==HELD)?"hold":"release",arg );
			error_occurred = 1;
		} else {
			fprintf(stdout, "User %s's job(s) %s.\n", arg,
				(mode==REMOVED)?"have been marked for removal":
					(mode==HELD)?"held":"released");
			nToProcess = -1;
		}
	}
	else 
	{
		fprintf( stderr, "Warning: unrecognized \"%s\" skipped.\n", arg );
	}
}

void
handle_constraint( const char *constraint )
{
	char expr[ATTRLIST_MAX_EXPRESSION];
	if( mode == IDLE ) {
		sprintf( expr, "%s==%d && %s", ATTR_JOB_STATUS, HELD, constraint );
	} else {
		strcpy( expr, constraint );
	}
	if( SetAttributeIntByConstraint( constraint, ATTR_JOB_STATUS, mode ) < 0 ) {
		fprintf( stderr, "Couldn't find/%s all jobs matching constraint %s\n",
				 (mode==REMOVED)?"remove":(mode==HELD)?"hold":"release",
				 constraint);
		error_occurred = 1;
	} else {
		fprintf( stdout, "Jobs matching constraint %s %s.\n", constraint,
				(mode==REMOVED)?"have been marked for removal":
					(mode==HELD)?"held":"released");
		nToProcess = -1;
	}
}


void
handle_all()
{
	char	constraint[1000];

		// Remove/Hold/Release all jobs... let queue management code decide
		// which ones we can and can not remove.
	if( mode == IDLE ) {
			// in the case of "release", make sure we only release HELD jobs
		sprintf( constraint, "%s==%d && %s>=%d", ATTR_JOB_STATUS, HELD,
			ATTR_CLUSTER_ID, 0 );
	} else {
		sprintf(constraint, "%s >= %d", ATTR_CLUSTER_ID, 0 );
	}
	if( SetAttributeIntByConstraint(constraint,ATTR_JOB_STATUS,mode) < 0 ) {
		fprintf( stdout, "Could not %s all jobs.\n",
				 (mode==REMOVED)?"remove":
				 (mode==HELD)?"hold":"release" );
		error_occurred = 1;
	} else {
		fprintf( stdout, "All jobs %s.\n",
				 (mode==REMOVED)?"marked for removal":
				 (mode==HELD)?"held":"released" );
	}
	nToProcess = -1;
}
