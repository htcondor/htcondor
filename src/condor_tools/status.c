/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Authors:  Allan Bricker and Michael J. Litzkow,
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

/*********************************************************************
* Get status information regarding all the machines in the pool from
* the central manager and print it in various ways.
*********************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include "condor_types.h"
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include "debug.h"
#include "except.h"
#include "trace.h"
#include "expr.h"
#include "sched.h"
#include "manager.h"
#include "clib.h"

static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char	*param(), *strdup();
char	first_non_blank();
XDR		*xdr_Init();
CONTEXT	*create_context();

extern int	Terse;
extern int	Silent;

char	*CollectorHost;
char	*MyName;
int		Verbose;
int		SubmittorsOnly;
int		AvailOnly;
int		TotalOnly;
int		DisplayTotal;
int		SortByPrio;
int		RollCall;
char	*RosterFile;

int		SubmittorDisplay;
int		ServerDisplay;
int		MachineUpdateInterval;
int		Now;

STATUS_LINE		*StatusLines[1024];
int				N_StatusLines;

typedef struct {
	char	*name;
	char	*comment;
	int		present;
} ROSTER;

ROSTER	Roster[1024];
int		RosterLen = 0;
int		Absent;

char	*Unknowns[1024];
int		N_Unknowns = 0;

/* Storage for buffering output to standard error. */
char MyStderrBuf[BUFSIZ];


typedef struct {
	char	*str;
	int		val;
} ENTRY;


#define VERBOSE			1
#define SORT_PRIO		2
#define SERVERS			3
#define COLLECTOR		4
#define SUBMITTORS		5
#define AVAIL			6
#define HELP			7
#define ROLL_CALL		8
#define TOTAL_ONLY		9

#define NO_VAL	-1

ENTRY	ArgTab [] = {
	{"",			NO_VAL},
	{"avail",		AVAIL},
	{"collector",	COLLECTOR},
	{"help",		HELP},
	{"host",		COLLECTOR},
	{"long",		VERBOSE},
	{"pool",		COLLECTOR},
	{"prio",		SORT_PRIO},
	{"roll",		ROLL_CALL},
	{"servers",		SERVERS},
	{"submittors",	SUBMITTORS},
	{"verbose",		VERBOSE},
	{"total",		TOTAL_ONLY},
	{"\077",		NO_VAL},
};
#define TAB_SIZE	14

usage()
{
	fprintf( stderr, "Usage: %s [-l] [-p] [-h hostname]  name ...\n", MyName );
	exit( 1 );
}

main( argc, argv )
int		argc;
char	*argv[];
{
	int			cmd;
	int			sock = -1;
	XDR			xdr, *xdrs = NULL;
	char		*override_host_name = NULL;

	/* Set up buffering for standard error.  Output for Verbose output */
	/* defaults to stderr, which normally has no buffering. */
	setbuf(stderr, MyStderrBuf);

	MyName = argv[0];

	for( argv++,argc--; *argv && **argv == '-'; argv++,argc-- ) {
		cmd = find_prefix( (*argv)+1, ArgTab );
		switch( cmd ) {
			case VERBOSE:
				Verbose++;
				break;
			case SORT_PRIO:
				SortByPrio = 1;
				break;
			case SERVERS:
				ServerDisplay++;
				break;
			case COLLECTOR:
				override_host_name = *(++argv);
				argc--;
				break;
			case SUBMITTORS:
				SubmittorsOnly++;
				SubmittorDisplay++;
				break;
			case AVAIL:
				AvailOnly++;
				ServerDisplay++;
				break;
			case HELP:
				help();
				exit( 0 );
				break;
			case ROLL_CALL:
				RollCall++;
				break;
			case TOTAL_ONLY:
				TotalOnly++;
				break;
			case NO_VAL:
				printf( "Unknown Option \"%s\"\n", *argv );
				help();
				exit( 1 );
				break;
		}
	}

	if( argc == 0 ) {
		DisplayTotal = TRUE;
	} else {
		SubmittorsOnly = FALSE;
		DisplayTotal = FALSE;
	}

		
	config( MyName, (CONTEXT *)0 );
	init_params();
	Terse = TRUE;
	Silent = TRUE;

	if( override_host_name ) {
		CollectorHost = override_host_name;
	}

	if( RollCall ) {
		read_roster_file();
	}

		/* Connect to the collector */
	if( (sock = do_connect(CollectorHost, "condor_collector",
												COLLECTOR_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to Condor Collector on \"%s\"\n",
														CollectorHost );
		exit( 1 );
	}
	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

	if( Verbose || ServerDisplay || SubmittorDisplay ) {
		get_status( argc, argv, xdrs );
	} else {
		get_status_lines( argc, argv, xdrs );
		/* If there are any args then we only want info for that machine */
		if( RollCall ) {
			display_unknowns();
			display_absent();
		}
	}
}

get_status_lines( argc, argv, xdrs )
int		argc;
char	*argv[];
XDR		*xdrs;
{
	int			i;
	int			cmd;
	STATUS_LINE	*line;
	int			name_comp(), prio_comp_simple(), prio_comp_arch();

	cmd = GIVE_STATUS_LINES;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

	xdrs->x_op = XDR_DECODE;

	for(;;) {
		line = (STATUS_LINE *)CALLOC( 1, sizeof(STATUS_LINE) );
		if( !xdr_status_line( xdrs, line ) ) {
			EXCEPT( "Can't read status line from Condor Collector" );
		}
		if( line->name == NULL || line->name[0] == '\0' ) {
			break;
		}

		if( line->state == NULL ) {
			line->state = "";
		}

		if( strcmp(line->state,"(DOWN)") == MATCH ) {
			continue;
		}

		if( SubmittorsOnly && line->tot == 0 ) {
			continue;
		}

		inc_summary( line );

		if( RollCall ) {
			mark_present( line->name );
		}

		if( !selected(strdup(line->name),argc,argv) ) {
			continue;
		}
		StatusLines[ N_StatusLines++ ] = line;
	}

	if( SortByPrio ) {
		if( SubmittorsOnly ) {
			qsort( (char *)StatusLines, N_StatusLines, sizeof(StatusLines[0]),
															prio_comp_simple );
		} else {
			qsort( (char *)StatusLines, N_StatusLines, sizeof(StatusLines[0]),
															prio_comp_arch );
		}
	} else {
		qsort( (char *)StatusLines, N_StatusLines, sizeof(StatusLines[0]),
																name_comp );
	}

	if( !TotalOnly ) {
		print_header( stdout );
		for( i=0; i<N_StatusLines; i++ ) {
			display_status_line( StatusLines[i], stdout );
		}
		(void)putchar( '\n' );
	}

		/* If there are any args then we only want info for that machine */
	if( DisplayTotal )  {
		display_summaries();
	}
}


get_status( argc, argv, xdrs )
int		argc;
char	*argv[];
XDR		*xdrs;
{
	int			cmd;
	MACH_REC	*rec;

	if( ServerDisplay ) {
		init_generic_job();
	}

	cmd = GIVE_STATUS;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

	xdrs->x_op = XDR_DECODE;
	for(;;) {
		rec = (MACH_REC *) MALLOC( sizeof(MACH_REC) );
		if( rec == NULL ) {
			EXCEPT("Out of memory");
		}

		bzero( (char *)rec, sizeof(MACH_REC) );
		rec->machine_context = create_context();
		ASSERT( xdr_mach_rec(xdrs,rec) );
		if( !rec->name || !rec->name[0] ) {
			break;
		}
		if( !selected(strdup(rec->name),argc,argv) ) {
			continue;
		}
		if( ServerDisplay ) {
			build_server_rec( rec );
		} else if(SubmittorDisplay ) {
			build_submittor_rec( rec );
		} else {
			display_verbose( rec );
		}

	}

	if( ServerDisplay ) {
		display_servers( stdout );
	}
	if( SubmittorDisplay ) {
		display_submittors( stdout );
	}
	
}


selected( name, argc, argv )
char	*name;
int		argc;
char	*argv[];
{
	int		i;
	char	*ptr, *index();

	if( argc == 0 ) {
		return 1;
	}

	for( i=0; i<argc; i++ ) {
		if( ptr=index(name,'.') ) {
			*ptr = '\0';
		}
		if( strcmp(argv[i],name) == MATCH ) {
			return 1;
		}
	}
	return 0;
}

display_virt_mem( ptr )
MACH_REC	*ptr;
{
	int		i;
	ELEM	*v_mem, *arch, *opsys, *eval();
	char	*p, *index();

	v_mem = eval( "VirtualMemory", ptr->machine_context, (CONTEXT *)0 );
	arch = eval( "Arch", ptr->machine_context, (CONTEXT *)0 );
	opsys = eval( "OpSys", ptr->machine_context, (CONTEXT *)0 );

	if( p=index(ptr->name,'.') ) {
		*p = '\0';
	}
	fprintf( stderr, "%-14s ", ptr->name );

	if( arch ) {
		fprintf( stderr, "%-8s", arch->s_val );
	} else {
		fprintf( stderr, "(unknown)" );
	}

	if( opsys ) {
		fprintf( stderr, "%-8s", opsys->s_val );
	} else {
		fprintf( stderr, "(unknown)" );
	}

	if( v_mem ) {
		display_elem( v_mem, stderr );
	} else {
		fprintf( stderr, "(unknown)" );
	}

	fprintf( stderr, "\n" );
}

display_verbose( ptr )
MACH_REC	*ptr;
{
	printf( "name: \"%s\"\n", ptr->name );
	printf( "machine_context:\n" );
	display_context( ptr->machine_context );
	printf( "time_stamp: %s", ctime( (time_t *)&ptr->time_stamp) );
	printf( "prio: %d\n", ptr->prio );
	printf( "\n" );
}


init_params()
{
	char	*tmp;

	time( &Now );

	if( (CollectorHost = param("COLLECTOR_HOST")) == NULL ) {
		EXCEPT( "COLLECTOR_HOST not specified in config file\n" );
	}

	RosterFile = param( "ROSTER_FILE" );
	if( RollCall && !RosterFile ) {
		fprintf( stderr, "Warning: No Roster File\n" );
	}

	tmp = param( "MACHINE_UPDATE_INTERVAL" );
    if( tmp == NULL ) {
        MachineUpdateInterval = 300;
    } else {
		MachineUpdateInterval = atoi( tmp );
	}

}


SetSyscalls(){}

name_comp( ptr1, ptr2 )
STATUS_LINE	**ptr1, **ptr2;
{
	int		status;

	if( status = strcmp( (*ptr1)->arch, (*ptr2)->arch ) ) {
		return status;
	}

	if( status = strcmp( (*ptr1)->op_sys, (*ptr2)->op_sys ) ) {
		return status;
	}

	return strcmp( (*ptr1)->name, (*ptr2)->name );
}

prio_comp_simple( ptr1, ptr2 )
STATUS_LINE	**ptr1, **ptr2;
{
	int		status;

	if( status = (*ptr2)->prio - (*ptr1)->prio ) {
		return status;
	}

	return strcmp( (*ptr1)->name, (*ptr2)->name );
}

prio_comp_arch( ptr1, ptr2 )
STATUS_LINE	**ptr1, **ptr2;
{
	int		status;

	if( status = strcmp( (*ptr1)->arch, (*ptr2)->arch ) ) {
		return status;
	}

	if( status = strcmp( (*ptr1)->op_sys, (*ptr2)->op_sys ) ) {
		return status;
	}

	if( status = (*ptr2)->prio - (*ptr1)->prio ) {
		return status;
	}

	return strcmp( (*ptr1)->name, (*ptr2)->name );
}

typedef struct {
	char	*arch;
	char	*op_sys;
	int		machines;
	int		jobs;
	int		running;
} SUMMARY;

SUMMARY		*Summary[50];
int			N_Summaries;
SUMMARY		Total;

inc_summary( line )
STATUS_LINE	*line;
{
	SUMMARY	*s, *get_summary();

	s = get_summary( line->arch, line->op_sys );
	s->machines += 1;
	s->jobs += line->tot;

	Total.machines += 1;
	Total.jobs += line->tot;

	if( SubmittorDisplay ) {
		s->running += line->run;
		Total.running += line->run;
	} else {
		if( strncmp(line->state, "Run", 3) == 0 ) {
			s->running += 1;
			Total.running += 1;
		}
	}
}

SUMMARY *
get_summary( arch, op_sys )
char	*arch;
char	*op_sys;
{
	int		i;
	SUMMARY	*new;

	for( i=0; i<N_Summaries; i++ ) {
		if( strcmp(Summary[i]->arch,arch) == MATCH &&
						strcmp(Summary[i]->op_sys,op_sys) == MATCH ) {
			return Summary[i];
		}
	}

	new = (SUMMARY *)CALLOC( 1, sizeof(SUMMARY) );
	new->arch = arch;
	new->op_sys = op_sys;
	Summary[ N_Summaries++ ] = new;
	return new;
}

display_summaries()
{
	int		i;

	for( i=0; i<N_Summaries; i++ ) {
		display_summary( Summary[i] );
	}
	display_summary( &Total );

}

display_summary( s )
SUMMARY		*s;
{
	char	tmp[256];

	if( s->arch ) {
		(void)sprintf( tmp, "%s/%s", s->arch, s->op_sys );
	} else {
		tmp[0] = '\0';
	}

	printf( "%-20s %3d machines %3d jobs %3d running\n",
					tmp, s->machines, s->jobs, s->running );
}


read_roster_file()
{
	FILE	*fp;
	char	*ltrunc();
	char	buf[1024];

	if( !RosterFile ) {
		fprintf( stderr, "\"ROSTER_FILE\" not specified in config file\n" );
		exit( 1 );
	}

	if( (fp=fopen(RosterFile,"r")) == NULL ) {
		perror( RosterFile );
		return;
	}

	while( fgets(buf,sizeof buf,fp) ) {
		if( buf[0] == '\n' ) {
			continue;
		}
		if( first_non_blank(buf) == '#' ) {
			continue;
		}
		add_roster_elem( ltrunc(buf) );
	}
	Absent = RosterLen;
}

add_roster_elem( line )
char	*line;
{
	char	*ptr;
	char	*comment;
	char	save;

	if( *line == '#' ) {
		return;
	}

	for( ptr=line; *ptr && !isspace(*ptr); ptr++ )
		;
	comment = ptr;
	save = *ptr;
	*ptr = '\0';
	Roster[RosterLen].name = strdup( line );
	*ptr = save;
	Roster[RosterLen].comment = strdup( comment );
	Roster[RosterLen].present = FALSE;
	RosterLen += 1;

/*
	if( *ptr ) {
		comment = ptr + 1;
		*ptr = '\0';
	} else {
		comment = ptr;
	}
	Roster[RosterLen].name = strdup( line );

	for( ptr=comment; *ptr && isspace(*ptr); ptr++ )
		;

	Roster[RosterLen].comment = strdup( ptr );
	Roster[RosterLen].present = FALSE;
	RosterLen += 1;
*/

}

display_unknowns()
{
	int		i;

	if( N_Unknowns == 0 ) {
		return;
	}

	printf( "\n" );
	if( N_Unknowns == 1 ) {
		printf(
		"The following  machine is present, but not on the roster...\n");
	} else {
		printf(
		"The following %d machines are present, but not on the roster...\n",
		N_Unknowns );
	}

	for( i=0; i<N_Unknowns; i++ ) {
		printf( "%s\n", Unknowns[i] );
	}
}

display_absent()
{
	int		i;

	printf( "\n" );
	if( Absent == 0 ) {
		printf( "All machines on the roster are present\n" );
		return;
	}
		
	if( Absent == 1 ) {
		printf( "The following machine is absent...\n" );
	} else {
		printf( "The following %d machines are absent...\n", Absent );
	}

	for( i=0; i<RosterLen; i++ ) {
		if( !Roster[i].present ) {
			/*
			printf( "%-15s %s\n", Roster[i].name, Roster[i].comment );
			*/
			printf( "%s%s\n", Roster[i].name, Roster[i].comment );
		}

	}
}

mark_present( name )
char	*name;
{
	int		i;
	char	*ptr;

	if( ptr=index(name,'.') ) {
		*ptr = '\0';
	}

	for( i=0; i<RosterLen; i++ ) {
		if( strcmp(name,Roster[i].name) == 0 ) {
			Roster[i].present = TRUE;
			Absent -= 1;
			return;
		}
	}
	Unknowns[N_Unknowns] = strdup( name );
	N_Unknowns += 1;
}

char
first_non_blank( str )
char	*str;
{
	char	*p;

	for( p=str; *p; p++ ) {
		if( !isspace(*p) ) {
			return *p;
		}
	}
	return '\0';
}

find_prefix( str, tab )
char	*str;
ENTRY	*tab;
{
	int	idx1, idx2;
	int	len;

	len = strlen( str );

	for( idx1 = 0; idx1 < TAB_SIZE; idx1++ ) {
		if( strncmp(str,tab[idx1].str,len) == MATCH ) {
			break;
		}
	}
	if( idx1 > TAB_SIZE ) {
		return NO_VAL;
	}

	for( idx2 = TAB_SIZE - 1; idx2 >= 0; idx2-- ) {
		if( strncmp(str,tab[idx2].str,len) == MATCH ) {
			break;
		}
	}
	if( idx2 < 0 ) {
		return NO_VAL;
	}

	if( idx1 == idx2 ) {
		return tab[idx1].val;
	} else {
		return NO_VAL;
	}
}

#if 0
substr( sub, string )
char	*sub;
char	*string;
{
	while( *sub && *string && *sub == *string ) {
		sub++;
		string++;
	}

	return *sub == '\0';
}
#endif

char	*HelpTab[] = {
"Options: (may be selected with any non-ambiguous prefix)",
"    -verbose		Print all known info on each machine",
"    -prio		Display machines in priority order",
"    -server		Display free disk and swap space",
"    -pool <hostname>	Print info from condor pool served by <hostname>",
"    -avail		Print info on \"available\" machines only",
"    -submittors		Display only machines with jobs in the pool",
"    -roll		Display roll call info (not available on all machines)",
"    -total		Don't display individual machine info (totals only)",
"    -help		Print this message",
"",
};

help()
{
	char	**ptr;

	fprintf( stderr, "Usage: %s [-option]... [hostname]...\n", MyName );
	for( ptr = HelpTab; **ptr; ptr++ ) {
		fprintf( stderr, "%s\n", *ptr );
	}
}
