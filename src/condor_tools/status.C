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
** Dream Team make will have no altercations over the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION (EVEN THE WORST KIND), ARISING OUT OF OR IN 
** CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include <iostream.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <string.h>
#include <time.h>


extern "C" {
#include "condor_types.h"
}
#include "manager.h"
#include "expr.h"
#include "debug.h"
#include "except.h"
#include "trace.h"
#include "sched.h"
#include "clib.h"
#include "util_lib_proto.h"
#include "status_aggregates.h"


extern "C" {

void SetSyscalls(){}  

void      dprintf ( int flags, char *fmt, ... );
int       xdr_mach_rec( XDR *xdrs, MACH_REC *ptr );
int       print_header ( FILE *fp );
int       display_status_line ( STATUS_LINE *line, FILE *fp );
char*     ltrunc ( register char *str );
CONTEXT*  create_context ( void );
char*     param ( char *name );
void      display_elem ( ELEM *elem, FILE *log_fp );
ELEM*     eval ( char *name, CONTEXT *cont1, CONTEXT *cont2 );
void      display_context ( CONTEXT *context );
int       xdr_status_line(XDR *xdrs, STATUS_LINE *line );
XDR*      xdr_Init( int *sock, XDR *xdrs );

#if defined(OSF1) || defined(SUNOS41)
void*     memset( void *,int, long unsigned int );  
#else
void*     memset( void *,int, unsigned int );  
#endif
char*     strdup();              
void       printTimeAndColl();
}


static char *_FileName_ = __FILE__;	     

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
int             Run;   
int             Job;       
char	*RosterFile;           

int		SubmittorDisplay;
int		ServerDisplay;
int		MachineUpdateInterval;
time_t	        Now;                      

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

typedef struct {
	char	*arch;
	char	*op_sys;
	int		machines;
	int		jobs;
	int		running;
	int     condor;
	int     sub_machs;
} SUMMARY;

SUMMARY		*Summary[50];
int			N_Summaries;
SUMMARY		Total;                  //used to store the summary statistics.


int        prio_comp_simple( STATUS_LINE **ptr1, STATUS_LINE **ptr2 );
int        prio_comp_arch( STATUS_LINE **ptr1, STATUS_LINE **ptr2 );
void       get_job_status( int argc, char *argv[], XDR *xdrs );
void       get_status_lines(int argc,char *argv[], XDR *xdrs );
void       get_status( int argc, char *argv[], XDR *xdrs );
int        selected( char *name, int argc, char *argv[] );
void       display_virt_mem( MACH_REC *ptr );
void       display_verbose( MACH_REC *ptr );
int        find_prefix(char *str, ENTRY *tab );
int        name_comp( STATUS_LINE **ptr1, STATUS_LINE **ptr2 );
int        prio_comp_simple( STATUS_LINE **ptr1, STATUS_LINE **ptr2 );
int        prio_comp_arch( STATUS_LINE **ptr1, STATUS_LINE **ptr2 );
void       inc_summary( STATUS_LINE *line );
SUMMARY*   get_summary( char *arch, char *op_sys );
void       display_summaries();
void       display_summary( SUMMARY *s );
void       read_roster_file();
void       add_roster_elem( char *line );
void       display_unknowns();
void       display_absent();
void       mark_present( char *name );
char       first_non_blank( char *str );
int        find_prefix(char *str, ENTRY *tab );
void       help();
void       init_params();
void       job_hash_insert( MACH_REC *rec );


#define VERBOSE			1  
#define SORT_PRIO		2
#define SERVERS			3
#define COLLECTOR		4
#define SUBMITTORS		5
#define AVAIL			6
#define HELP			7
#define ROLL_CALL		8
#define TOTAL_ONLY		9
#define RUN                    10
#define JOB                    11
#define NO_VAL	-1


ENTRY	ArgTab [] = {
	{"",			NO_VAL},           
	{"avail",		AVAIL},
	{"collector",	        COLLECTOR},
	{"help",		HELP},
	{"host",		COLLECTOR},
	{"long",		VERBOSE},
	{"pool",		COLLECTOR},
	{"prio",		SORT_PRIO},
	{"roll",		ROLL_CALL},
	{"servers",		SERVERS},
	{"submittors",	        SUBMITTORS},
	{"verbose",		VERBOSE},
	{"total",		TOTAL_ONLY},
        {"run",                 RUN},
        {"job",                 JOB},
	{"\077",		NO_VAL},
};

#define TAB_SIZE	15


/* 
main

condor_status has several options:
to see them at the screen type condor_status -h 

verbose		Print all known info on each machine
                will set the Verbose flag and end up calling get_status function.
prio		Display machines in priority order
                set the SortByPrio and call get_status_lines function.
server		Display free disk and swap space
pool <hostname>	Print info from condor pool served by <hostname>
avail		Print info on \"available\" machines only
run             Print info on which machines are running jobs.
submittors	Display only machines with jobs in the pool
roll		Display roll call info (not available on all machines)
total		Don't display individual machine info (totals only)
help		Print this message 

according to the arguments supplied we will set the appropriate
global flags and call the corresponding functions.
*/
main(int argc, char *argv[])
{
	int			cmd;
	int			sock = -1;
	XDR			xdr, *xdrs = NULL;
	char		*override_host_name = NULL;

	/* Set up buffering for standard error.  Output for Verbose output */
	/* defaults to stderr, which normally has no buffering. */
	setbuf(stderr, MyStderrBuf);

	MyName = argv[0];  /* sets MyName to function name: "condor_status" */

	for( argv++,argc--; *argv && **argv == '-'; argv++,argc-- ) {
		cmd = find_prefix( (*argv)+1, ArgTab );
		switch( cmd ) {
			case VERBOSE:
				Verbose++;  /* set the verbose flag,             */ 
				break;      /* later will call get_status funct. */
			case SORT_PRIO:
				SortByPrio = 1;  /* set the SortByPrio flag    */
				break;           /* will call get_status_lines */
			case SERVERS:
				ServerDisplay++;   /* calls get_status */
				break;
			case COLLECTOR:            /* calls get_status_lines*/
				override_host_name = *(++argv);
				argc--;
				break;
			case SUBMITTORS:            /* calls get_status */
				SubmittorsOnly++;  
				SubmittorDisplay++;
				break;
			case AVAIL:                /* calls get_status_lines */
				AvailOnly++;
				ServerDisplay++;
				break;
			case HELP:                 /* calls help */
				help();            /* which prints options */
				exit( 0 );
				break;
			case ROLL_CALL:            /* calls get_status_lines */
				RollCall++;        /* then display_unknowns()*/
				break;             /* and absent() from main */
			case TOTAL_ONLY:      /* calls get_status_lines */
				TotalOnly++;
				break;
                        case RUN:                     /* calls get_status */
                                Run++;
                                break;
                        case JOB:
                                Job++;
                                break;
			case NO_VAL:            /* calls help which prints options. */
				printf( "Unknown Option \"%s\"\n", *argv );
				help();      
				exit( 1 );
				break;
		}
	}


	if( argc == 0 ) {              /* if we just called 'condor_status' */
		DisplayTotal = TRUE;   /* then we want info on all machines. */
	} else {
		SubmittorsOnly = FALSE;  /* otherwise... */
		DisplayTotal = FALSE;    /* do not display all... */
	}                              


	config( 0 );    
	init_params();                 /* first record the time of this call. */
	Terse = TRUE;
	Silent = TRUE;

	if( override_host_name ) {
		CollectorHost = override_host_name;
	}

	if( RollCall ) {
		read_roster_file();
	}
		/* Connect to the collector */
	if( (sock = do_connect(CollectorHost, "condor_collector", COLLECTOR_PORT)) < 0 ) {
		dprintf( D_ALWAYS, "Can't connect to Condor Collector on \"%s\"\n", CollectorHost );
		exit( 1 );
	}


	xdrs = xdr_Init( &sock, &xdr );
	xdrs->x_op = XDR_ENCODE;

        /* if -verbose, -server, -submittor, or -run  call get_status */
        /* else -prio, -pool, -avail, -roll, or -total call get_status_lines */
        /*  if the call is simply condor_status then we will call get_status_lines */
	if( Verbose || ServerDisplay || SubmittorDisplay  || Run || Job ) { 
		get_status( argc, argv, xdrs );
	} else {                 
		get_status_lines( argc, argv, xdrs );
		if( RollCall ) {
			display_unknowns();
			display_absent();
		}
	}
}


void get_status_lines(int argc, char *argv[], XDR *xdrs )
{
	int			i;
	int			cmd;
	STATUS_LINE	        *line;
      
	cmd = GIVE_STATUS_LINES;
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );

	xdrs->x_op = XDR_DECODE;

	for(;;) {

		line = (STATUS_LINE *)CALLOC( 1, sizeof(STATUS_LINE) );
                  
		if( !xdr_status_line( xdrs, line ) ) {
			EXCEPT( "Can't read status line from Condor Collector" );
		}

                /* if there is no name then we will break from 
                   the for loop since it is done looking at all the machines. */
		if( line->name == NULL || line->name[0] == '\0' ) {
			break;
		}

                /* if for some reason we could not get the 
                   state of the machine, it has been
                   standard in all the code to place empty 
                   strings for no values instead of 
                   null pointers.  keeping with the standard. */
		if( line->state == NULL ) {  
			line->state = "";
		}

                /* MATCH is defined as the equality compare for
                   strcmp.  so if it matches then it is equal. */
		if( strcmp(line->state,"(DOWN)") == MATCH ) {
			continue;
		}


		if( SubmittorsOnly && line->tot == 0 ) {
			continue;
		}

                /* increment statistics */
		inc_summary( line );

		if( RollCall ) {
			mark_present( line->name );
		}

		if( !selected(strdup(line->name),argc,argv) ) {
			continue;
		}
                /* store the status_line into the array. */
		StatusLines[ N_StatusLines++ ] = line;
	}


	if( SortByPrio ) {
		if( SubmittorsOnly ) {
			qsort( (char *)StatusLines, N_StatusLines, sizeof(StatusLines[0]),
			       (void*)prio_comp_simple );
		} else {
			qsort( (char *)StatusLines, N_StatusLines, sizeof(StatusLines[0]),				    (void *)prio_comp_arch );
		}
	} else {
		qsort( (char *)StatusLines, N_StatusLines, sizeof(StatusLines[0]),
																(void *)name_comp );
	}


	if( !TotalOnly ) {
		print_header( stdout );            
		for( i=0; i<N_StatusLines; i++ ) {
                   display_status_line( StatusLines[i], stdout );        
                 }
		(void)putchar( '\n' );
	}

     
	if( DisplayTotal )  {
		display_summaries();
	}
}



//returns a 1 if it is in a running state.
int runningstate(CONTEXT *context)
{

        char            *c;

	//if we do not know what state it is in then we fail.
        if (evaluate_string("State",&c,context,NIL ) < 0) {
            return 0;
        }

        if ( strcmp("Running", c) == MATCH || strcmp("Suspended", c) == MATCH ||
             strcmp("Checkpoint", c) == MATCH ) 
            return 1;
        else 
            return 0;
}

/* failruncondition: this will return a 1 if the record is */
/* not valid for the query.  otherwise, if it is a rec     */
/* that is in a run state, and if it is an argument if any */
/* then it will be valid, otherwise it is not valid.       */
int failruncondition(MACH_REC *rec, int argc, char* argv[])
{
        CONTEXT         *context;
        char            *c;
        int             running = runningstate(rec->machine_context);

        if (!running) 
            return 0;

        context = rec->machine_context;

        if (evaluate_string("ClientMachine", &c, context, NIL ) < 0) {
            return 1;
        }

        if (!selected(strdup(c),argc,argv) ) {  //not selected means it is selected.
            return 1;
        }else
	    return 0;

}


void process_get_status_server(serv_rec_list* &servtot, MACH_REC* recin, int argc, char *argv[])
{
    if (servtot == NULL)
	servtot = new serv_rec_list(AvailOnly, TotalOnly, DisplayTotal,
				    MachineUpdateInterval, Now, SortByPrio);

    if ( !selected(strdup(recin->name),argc,argv) ) 
        return;

    servtot->build_server_rec(recin);

}
/***************************************************************************************
    get_status_sub

    called by get_status which will retrieve records for all machines on the pool.
    if we called condor_status -submittors then this function will first determine if
    the record has been chosen by the user. ie: condor_status -sub golan
    if so then it will store the record until all records are examined,
    then in get_status_display they will be displayed.
****************************************************************************************/
void process_get_status_sub(sub_rec_list* &subtot, MACH_REC* recin, int argc, char *argv[])
{
    if (subtot == NULL) 
	subtot = new sub_rec_list(AvailOnly, TotalOnly, DisplayTotal,
				  MachineUpdateInterval, Now, SortByPrio);
   
    if ( runningstate(recin->machine_context) ){
        subtot->hashinsert(recin);
    }

    if ( !selected(strdup(recin->name), argc, argv) ){
	return;
    }

    subtot->build_submittor_rec(recin);
}


/***************************************************************************************
    get_status_run

    called by get_status which will retrieve records for all machines on the pool.
    if we called condor_status -run then this function will first determiine if it
    is a record for a machine that is running.  if so then it will store the record
    until all records are examined.  then in get_status_display they will be displayed.
****************************************************************************************/
void process_get_status_run(run_rec_list* &runtotal, MACH_REC* recin, int argc, char *argv[])
{
    if (runtotal == NULL)
	runtotal = new run_rec_list(AvailOnly, TotalOnly, DisplayTotal,
				    MachineUpdateInterval, Now, SortByPrio);

    if (failruncondition(recin, argc, argv))   
        return;

    runtotal->build_run_rec(recin);

}


/* this function will be changed to include the job functions. */
/***************************************************************************************
   get_status_display 

   if run called displays all that are running.
   if servers called displays all that are servers.
   if submittors called displays all that are submittors.
   if jobs called, displayed all that are jobs.
****************************************************************************************/
void get_status_display(job_rec_list* jobtot, serv_rec_list* servtot, 
                        sub_rec_list *subtot, run_rec_list *runtot)
{
    if (servtot != NULL)
        servtot->display_servers(stdout);	
    else if (subtot != NULL)
        subtot->display_submittors(stdout);
    else if (runtot != NULL)
        runtot->display_run(stdout);
    else if (jobtot != NULL)
        jobtot->display_job(stdout);
}


int j_condition(MACH_REC *rec, int argc, char* argv[])
{
        CONTEXT         *context;
        char            *c;
        int             running = runningstate(rec->machine_context);

        if (!running) 
            return 0;

        context = rec->machine_context;

        if (evaluate_string("RemoteUser", &c, context, NIL ) < 0) {
            return 0;
        }

        if (!selected(strdup(c),argc,argv) ) {  //not selected means it is selected.
            return 1;
        }else
	    return 0;

}


//there are two different ways that we return a 0
//1. running && it is one in the list.
//2. or state is running && it is one in the list.
int failjobcondition(job_rec_list *job_tot, MACH_REC *rec, int argc, char* argv[])
{
        CONTEXT         *context;

        context = rec->machine_context;

        if (j_condition(rec, argc, argv)){  
	    return 1;                            
	}
 
        job_tot->job_hash_insert(rec);

	return 0;
}


void process_get_status_job(job_rec_list *&job_tot, MACH_REC* recin, int argc, char *argv[])
{
    if ( job_tot == NULL )
        job_tot = new job_rec_list(AvailOnly, TotalOnly, DisplayTotal,
				    MachineUpdateInterval, Now, SortByPrio);
    
    if (failjobcondition(job_tot, recin, argc, argv))
        return;
   
    job_tot->build_job_rec(recin);

}


void get_status( int argc, char *argv[], XDR *xdrs )
{
	int			cmd;
	MACH_REC	*rec;
        sub_rec_list*   sub_tot  = NULL;
        serv_rec_list*  serv_tot = NULL;
        run_rec_list*   run_tot  = NULL;
        job_rec_list*   job_tot  = NULL;
        CONTEXT         *context;
        char            *c;

	cmd = GIVE_STATUS;           
	ASSERT( xdr_int(xdrs, &cmd) );
	ASSERT( xdrrec_endofrecord(xdrs,TRUE) );


	xdrs->x_op = XDR_DECODE;
	for(;;) {
       
		rec = (MACH_REC *) MALLOC( sizeof(MACH_REC) );
		if( rec == NULL ) {
			EXCEPT("Out of memory");
		}

		memset( (char *)rec,0, sizeof(MACH_REC) );
		rec->machine_context = create_context();
		ASSERT( xdr_mach_rec(xdrs, rec) );
		if( !rec->name || !rec->name[0] ) {
			break;
		}

                if (Run){
                    process_get_status_run(run_tot, rec, argc, argv);
		} else if (ServerDisplay){
                    process_get_status_server(serv_tot, rec, argc, argv);
		} else if (SubmittorDisplay){
                    process_get_status_sub(sub_tot, rec, argc, argv);
                } else if (Job){
		    process_get_status_job(job_tot, rec, argc, argv);  
		} else {
                    if ( !selected(strdup(rec->name), argc, argv) )
	                continue;     
		    display_verbose(rec);
                }

	}  /* this  is the end of the for loop. */

        //currently have this if.. this will disseapear.
	get_status_display(job_tot, serv_tot, sub_tot, run_tot);	
}




int selected( char *name, int argc, char *argv[] )
{
	int		i;
	char	     *ptr;

	if( argc == 0 ) {
		return 1;
	}

	for( i=0; i<argc; i++ ) {
		if( ptr=strchr(name,'.') ) {
			*ptr = '\0';
		}
		if( strcmp(argv[i],name) == MATCH ) {
			return 1;
		}
	}
	return 0;
}


void display_virt_mem( MACH_REC *ptr )
{
	int		i;
	ELEM	*v_mem, *arch, *opsys;
	char	*p;


	v_mem = eval( "VirtualMemory", ptr->machine_context, (CONTEXT *)0 );
	arch = eval( "Arch", ptr->machine_context, (CONTEXT *)0 );
	opsys = eval( "OpSys", ptr->machine_context, (CONTEXT *)0 );

	if( p=strchr(ptr->name,'.') ) {
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



void display_verbose( MACH_REC *ptr )
{
	printf( "name: \"%s\"\n", ptr->name );
	printf( "machine_context:\n" );
	display_context( ptr->machine_context ); 
	printf( "time_stamp: %s", ctime( (time_t *)&ptr->time_stamp) );
	printf( "prio: %d\n", ptr->prio );                              
	printf( "\n" );
}




void init_params()
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



int name_comp( STATUS_LINE **ptr1, STATUS_LINE **ptr2 )
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



int prio_comp_simple( STATUS_LINE **ptr1, STATUS_LINE **ptr2 )
{
	int		status;

	if( status = (*ptr2)->prio - (*ptr1)->prio ) {
		return status;
	}

	return strcmp((*ptr1)->name, (*ptr2)->name);
}

int prio_comp_arch( STATUS_LINE **ptr1, STATUS_LINE **ptr2 )
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


extern "C" {
void printTimeAndColl()
{
  time_t tmp;
  time(&tmp);

  printf("\nCollector : %s\n", CollectorHost); 
  printf("Time      : %s\n", ctime(&tmp));
}
}


void inc_summary( STATUS_LINE *line )
{
	SUMMARY	*s;                    
                                     
	if(!line->arch)
	{
		line->arch = "Unknown";
	}
	if(!line->op_sys)
	{
		line->op_sys = "Unknown";
	}
	s = get_summary( line->arch, line->op_sys );
	s->machines += 1;
	s->jobs += line->tot;

	Total.machines += 1;
	Total.jobs += line->tot;

	if(line->tot > 0) {
	   s->sub_machs++;
	   Total.sub_machs++;
	 }
	s->running += line->run;
	Total.running += line->run;
	
	if(( strncmp(line->state, "Run", 3) == 0 )
	   ||(strncmp(line->state, "Susp", 4)==0)) {
	  s->condor += 1;
	  Total.condor += 1;
	}
}




SUMMARY* get_summary( char *arch, char *op_sys )
{
	int		i;
	SUMMARY	*new_summary;

	for( i=0; i<N_Summaries; i++ ) {
		if( strcmp(Summary[i]->arch,arch) == MATCH &&
						strcmp(Summary[i]->op_sys,op_sys) == MATCH ) {
			return Summary[i];
		}
	}

	new_summary = (SUMMARY *)CALLOC( 1, sizeof(SUMMARY) );
	new_summary->arch = arch;
	new_summary->op_sys = op_sys;
	Summary[ N_Summaries++ ] = new_summary;
	return new_summary;
}

void display_summaries()
{
	int		i;

	printTimeAndColl();

	printf("----------------------------------------------------------\n");
	printf("ARCH/OS           machines |condor| Machs/jobs  exporting |\n");
	printf("----------------------------------------------------------|\n");
	for( i=0; i<N_Summaries; i++ ) {
		display_summary( Summary[i] );
	}
	printf("----------------------------------------------------------|\n");
	display_summary( &Total );
	printf("----------------------------------------------------------\n");
}

void display_summary( SUMMARY *s )
{
	char	tmp[256];

	if( s->arch ) {
		(void)sprintf( tmp, "%s/%s", s->arch, s->op_sys );
	} else {
		strcpy(tmp,"Total");
	}

	printf( "%-20s  %3d  | %3d  |  %3d /%3d    %3d      | \n",
		       tmp, s->machines, s->condor, s->sub_machs, s->jobs, s->running );
}



void read_roster_file()
{
	FILE	*fp;       
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

void add_roster_elem( char *line )
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

void display_unknowns()
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

void display_absent()
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

void mark_present( char *name )
{
	int		i;
	char	*ptr;

	if( ptr=strchr(name,'.') ) {
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


char first_non_blank( char *str )
{
	char	*p;

	for( p=str; *p; p++ ) {
		if( !isspace(*p) ) {
			return *p;
		}
	}
	return '\0';
}


int find_prefix(char *str, ENTRY *tab )
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
substr( char *sub, char *string )
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
"    -run                Print info on which machines are running jobs.",
"    -job                Print info on users using which machines.",
"    -submittors		Display only machines with jobs in the pool",
"    -roll		Display roll call info (not available on all machines)",
"    -total		Don't display individual machine info (totals only)",
"    -help		Print this message",
"",
};


void help()
{
	char	**ptr;

	fprintf( stderr, "Usage: %s [-option]... [hostname]...\n", MyName );
	for( ptr = HelpTab; **ptr; ptr++ ) {
		fprintf( stderr, "%s\n", *ptr );
	}
}

















