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


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include "condor_types.h"
#include "expr.h"
#include "manager.h"
#include "except.h"
#include "debug.h"

typedef struct {
	char	*name;
	float	swap;
	float	disk;
	char	*state;
	float	load_avg;
	int		kbd_idle;
	char	*arch;
	char	*op_sys;
	int		idle;
	int    bench_mips;       
	int    kflops;    
	int    tot_jobs;  /* jobs submitted */
	int    run;  /* jobs exported */
} SERVER_REC;

SERVER_REC	Servers[1024];
int			N_ServerRecs;

typedef struct {
	char	*name;
	int		run;
	int		tot;
	int		max;
	int		users;
	int		prio;
	float	swap;
	char	*arch;
	char	*op_sys;
	char    *state;
} SUBMITTOR_REC;

SUBMITTOR_REC	Submittors[1024];
int				N_Submittors;
	
	

#ifndef LINT
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */
#endif LINT

char	*index();

#define NIL	(CONTEXT *)0
#define MATCH 0

CONTEXT		*GenericJob, *create_context();
extern int	AvailOnly;
extern int	TotalOnly;
extern int	DisplayTotal;
extern int	MachineUpdateInterval;
extern int	Now;
extern int	SortByPrio;

build_submittor_rec( mach )
MACH_REC	*mach;
{
	SUBMITTOR_REC	*rec;
	CONTEXT	*context;
	char	*shorten();
	char	*ptr;
	char	*format_seconds();
	int		swap;
	int		disk;
	int		idle;
	int		running;

	
	if( Now - mach->time_stamp > MachineUpdateInterval ) {
		return;
	}

	context = mach->machine_context;

	if( evaluate_int("Running",&running,context,NIL ) < 0 ) {
		running = -1;
	}

	if( evaluate_int("Idle",&idle,context,NIL ) < 0 ) {
		idle = -1;
	}

	if( running <= 0 && idle <= 0 ) {
		return;
	}

	rec = &Submittors[N_Submittors++];

	rec->run = running;
	rec->tot = running + idle;


	if( evaluate_int("MAX_JOBS_RUNNING",&rec->max,context,NIL ) < 0 ) {
		rec->max = -1;
	}

	rec->prio = mach->prio;

	if( evaluate_int("VirtualMemory",&swap,context,NIL ) < 0 ) {
		rec->swap = -1.0;
	} else {
		rec->swap = (float)swap / 1024.0;
	}

	if( evaluate_int("Users",&rec->users,context,NIL ) < 0 ) {
		rec->users = -1;
	}

	if( ptr=index(mach->name,'.') ) {
		*ptr = '\0';
	}

	rec->name = mach->name;

	if( evaluate_string("Arch",&rec->arch,context,NIL ) < 0 ) {
		rec->arch = "(?)";
	}

	if( evaluate_string("OpSys",&rec->op_sys,context,NIL ) < 0 ) {
		rec->op_sys = "(?)";
	}

	if( evaluate_string("State",&rec->state,context,NIL ) < 0 ) {
		rec->state = "?";
	} else {
		rec->state = shorten(rec->state);
	}

	inc_submit_sum( rec );

}

build_server_rec( mach )
MACH_REC	*mach;
{
	SERVER_REC	my_rec, *rec = &my_rec;
	char	*shorten();
	char	*ptr;
	char	*format_seconds();
	CONTEXT	*context;
	int	swap;
	int	disk;
	int     idle_jobs;

	if( Now - mach->time_stamp > MachineUpdateInterval ) {
		return;
	}

	context = mach->machine_context;

	if( evaluate_bool("START",&rec->idle,context,GenericJob) < 0 ) {
		rec->idle = 0;
	}

	if( evaluate_string("State",&rec->state,context,NIL ) < 0 ) {
		rec->state = "?";
	} else {
		rec->state = shorten(rec->state);
	}

	if( ptr=index(mach->name,'.') ) {
		*ptr = '\0';
	}
	rec->name = mach->name;


	if( evaluate_int("Running",&rec->run,context,NIL ) < 0 ) {
		rec->run = 0;
	}

	if( evaluate_int("Idle",&idle_jobs,context,NIL ) < 0 ) {
		idle_jobs = 0;
	}

	rec->tot_jobs = rec->run + idle_jobs;

	if( evaluate_int("VirtualMemory",&swap,context,NIL ) < 0 ) {
		rec->swap = -1.0;
	} else {
		rec->swap = (float)swap / 1024.0;
	}

	if( evaluate_int("Disk",&disk,context,NIL ) < 0 ) {
		rec->disk = -1.0;
	} else {
		rec->disk = (float)disk / 1024.0;
	}

	if( evaluate_float("LoadAvg",&rec->load_avg,context,NIL ) < 0 ) {
		rec->load_avg = -1.0;
	}

	if( evaluate_int("KeyboardIdle",&rec->kbd_idle,context,NIL ) < 0 ) {
		rec->kbd_idle = -1;
	}

	if( evaluate_string("Arch",&rec->arch,context,NIL ) < 0 ) {
		rec->arch = "(?)";
	}

	if( evaluate_string("OpSys",&rec->op_sys,context,NIL ) < 0 ) {
		rec->op_sys = "(?)";
	}

	if( evaluate_int("KFLOPS",&rec->kflops,context,NIL ) < 0 ) {
		rec->kflops = 0;
	}

	if( evaluate_int("MIPS",&rec->bench_mips,context,NIL ) < 0 ) {
		rec->bench_mips = 0;
	}

	inc_serv_sum( rec );

	if( !AvailOnly || rec->idle ) {
		Servers[N_ServerRecs++] = my_rec;
	}
}

display_submittor( rec, fp )
SUBMITTOR_REC	*rec;
FILE		*fp;
{
	char	*format_seconds();

	
	fprintf( fp, "%-14s ", rec->name );
	fprintf( fp, "%-4d ", rec->run );
	fprintf( fp, "%-4d ", rec->tot );
	fprintf( fp, "%-4d ", rec->max );
	fprintf( fp, "%-5d ", rec->users );

	fprintf( fp, "%-9d ", rec->prio );

	if( rec->swap < 0.0 ) {
		fprintf( fp, "%-6s ", "?" );
	} else {
		fprintf( fp, "%-6.1f ", rec->swap );
	}

	fprintf( fp, "%-7s ", rec->arch );
	fprintf( fp, "%-8s ", rec->op_sys );

	fprintf( fp, "\n" );
}

display_submittors( fp )
FILE	*fp;
{
	int		i;
	int		submittor_comp();

	if( !TotalOnly ) {
		qsort( (char *)Submittors, N_Submittors, sizeof(Submittors[0]),
															submittor_comp );

		print_header_submittors( stdout );
		for( i=0; i<N_Submittors; i++ ) {
			display_submittor( &Submittors[i], fp );
		}
		fprintf( fp, "\n" );
	}
	if( DisplayTotal ) {
		display_submit_summaries();
	}
}

display_servers( fp )
FILE	*fp;
{
	int		i;
	int		server_comp();

	if( !TotalOnly ) {
		qsort( (char *)Servers, N_ServerRecs, sizeof(Servers[0]), server_comp );

		print_header_servers( stdout );
		for( i=0; i<N_ServerRecs; i++ ) {
			display_server( &Servers[i], fp );
		}
		fprintf( fp, "\n" );
	}
	if( DisplayTotal ) {
		display_serv_summaries();
	}
}

display_server( rec, fp )
SERVER_REC	*rec;
FILE		*fp;
{
	char	*format_seconds();

	
	fprintf( fp, "%-14s", rec->name );

 	if(!AvailOnly)
	{
	  if( rec->idle ) {
		fprintf( fp, "%-5s ", "True" );
	  } else {
		fprintf( fp, "%-5s ", "False" );
	  }
   	}

	if( rec->swap < 0.0 ) {
		fprintf( fp, "%-6s ", "?" );
	} else {
		fprintf( fp, "%-6.1f ", rec->swap );
	}

	if( rec->disk < 0.0 ) {
		fprintf( fp, "%-6s ", "?" );
	} else {
		fprintf( fp, "%-6.1f ", rec->disk );
	}

	if(!AvailOnly)
	  fprintf( fp, "%-6s ", rec->state );

	if( rec->load_avg < 0.0 ) {
		fprintf( fp, "%-6s", "?" );
	} else {
		fprintf( fp, "%-6.2f", rec->load_avg );
	}

	if( rec->kbd_idle < 0 ) {
		fprintf( fp, "%-12s ", "?" );
	} else {
		fprintf( fp, "%12s ", format_seconds(rec->kbd_idle) );
	}
	
	if(AvailOnly) {
	  if( rec->bench_mips <= 0 ) {
	      fprintf( fp, "%3s  ", "?" );
	  } else {
	      fprintf( fp, "%3d ", rec->bench_mips );
	  }
	  
	  if( rec->kflops <= 0 ) {
	       fprintf( fp, "%-5s ", "?" );
	  } else {
	       fprintf( fp, "%-6.2f ", (float)rec->kflops/1000 );
	  }
	}

	fprintf( fp, "%-7s", rec->arch );
	fprintf( fp, "%-8s ", rec->op_sys );

	fprintf( fp, "\n" );
}


print_header_submittors( fp )
FILE	*fp;
{

	fprintf( fp, "%-14s ", "Name" );
	fprintf( fp, "%-4s ", "Run" );
	fprintf( fp, "%-4s ", "Tot" );
	fprintf( fp, "%-4s ", "Max" );
	fprintf( fp, "%-5s ", "Users" );
	fprintf( fp, "%-9s ", "Prio" );
	fprintf( fp, "%-6s ", "Swap" );
	fprintf( fp, "%-7s ", "Arch" );
	fprintf( fp, "%-8s ", "OpSys" );
	fprintf( fp, "\n" );
}

print_header_servers( fp )
FILE	*fp;
{

	fprintf( fp, "%-14s ", "Name" );
	if(!AvailOnly)
	   fprintf( fp, "%-4s ", "Avail" );
	fprintf( fp, "%-6s", "Swap" );
	fprintf( fp, "%-6s ", "Disk" );
	if(!AvailOnly)
	  fprintf( fp, "%-6s ", "State" );
	fprintf( fp, "%-6s ", "LdAvg" );
	fprintf( fp, "%8s ", "Idle" );
	if(AvailOnly) {
	  fprintf( fp, "%6s ", "MIPS" );
	  fprintf( fp, "%6s ", "MFLOPS" );
	  fprintf( fp, "%-7s ", "Arch" );
	}
	else
	  fprintf( fp, "%-8s ", "  Arch");
	fprintf( fp, "%-8s ", "OpSys" );
	fprintf( fp, "\n" );
}

submittor_comp( ptr1, ptr2 )
SUBMITTOR_REC	*ptr1, *ptr2;
{
	int		status;

	if( SortByPrio ) {
		if( status = ptr2->prio - ptr1->prio ) {
			return status;
		}

		return strcmp( ptr1->name, ptr2->name );
	} else {

		if( status = strcmp( ptr1->arch, ptr2->arch ) ) {
			return status;
		}

		if( status = strcmp( ptr1->op_sys, ptr2->op_sys ) ) {
			return status;
		}

		return strcmp( ptr1->name, ptr2->name );
	}
}

server_comp( ptr1, ptr2 )
SERVER_REC	*ptr1, *ptr2;
{
	int		status;

	if( status = strcmp( ptr1->arch, ptr2->arch ) ) {
		return status;
	}

	if( status = strcmp( ptr1->op_sys, ptr2->op_sys ) ) {
		return status;
	}

	return strcmp( ptr1->name, ptr2->name );
}

init_generic_job()
{
	GenericJob = create_context();
	store_stmt( scan("Owner = \"nobody\""), GenericJob );
}

typedef struct {
	char	*arch;
	char	*op_sys;
	int		machines;
	int		avail;
	int		condor;
	int		user;
	int		down;
	int    run;
	int    tot_jobs;
	int    sub_machs;   /* machines with a job queue */
	int    bench_mips;
	int    kflops;
} SERV_SUMMARY;

SERV_SUMMARY	*ServSum[50];
int				N_ServSum;
SERV_SUMMARY	ServTot;

inc_serv_sum( rec )
SERVER_REC	*rec;
{
	SERV_SUMMARY *s, *get_serv_sum();

	s = get_serv_sum( rec->arch, rec->op_sys );

	s->machines += 1;
	ServTot.machines += 1;

	s->run += rec->run;
	ServTot.run += rec->run;

	s->bench_mips += rec->bench_mips;
	ServTot.bench_mips += rec->bench_mips;

	s->kflops += rec->kflops;
	ServTot.kflops += rec->kflops;

	s->tot_jobs += rec->tot_jobs;
	ServTot.tot_jobs+= rec->tot_jobs;
	
	if(rec->tot_jobs > 0) {
	  s->sub_machs++;
	  ServTot.sub_machs++;
	}

	if( rec->idle ) {
		s->avail += 1;
		ServTot.avail += 1;
	} else if( strcmp(rec->state,"NoJob") == MATCH ) {
		s->user += 1;
		ServTot.user += 1;
	} else {
		s->condor += 1;
		ServTot.condor += 1;
	}
}

SERV_SUMMARY *
get_serv_sum( arch, op_sys )
char	*arch;
char	*op_sys;
{
	int		i;
	SERV_SUMMARY	*new;

	for( i=0; i<N_ServSum; i++ ) {
		if( strcmp(ServSum[i]->arch,arch) == MATCH &&
						strcmp(ServSum[i]->op_sys,op_sys) == MATCH ) {
			return ServSum[i];
		}
	}

	new = (SERV_SUMMARY *)CALLOC( 1, sizeof(SERV_SUMMARY) );
	new->arch = arch;
	new->op_sys = op_sys;
	ServSum[ N_ServSum++ ] = new;
	return new;
}

display_serv_summaries()
{
	int		i;

	printTimeAndColl();

	if(AvailOnly)
	     printf("------------------------------------------------\n");
	else
	  printf("-------------------------------------------------------------------------\n");

	if(AvailOnly) {
	        printf("ARCH/OS           machines  avail Mflops  mips  |\n");
	        printf("------------------------------------------------|\n");
	} else {
	        printf("ARCH/OS           machines | avail   user  condor | Machs/jobs exporting |\n");
	        printf("-------------------------------------------------------------------------|\n");
	}

	for( i=0; i<N_ServSum; i++ ) {
		display_serv_sum( ServSum[i] );
	}


	if(AvailOnly) {
	        printf("------------------------------------------------|\n");
	} else {
	        printf("-------------------------------------------------------------------------|\n");
	}

	display_serv_sum( &ServTot );

	if(AvailOnly)
	     printf("------------------------------------------------\n");
	else
	  printf("-------------------------------------------------------------------------\n");

}

display_serv_sum( s )
SERV_SUMMARY		*s;
{
	char	tmp[256];

	if( s->arch ) {
		(void)sprintf( tmp, "%s/%s", s->arch, s->op_sys );
	} else {
		strcpy(tmp, "Total");
	}

	if( AvailOnly ) {
		printf( "%-20s %3d   %3d   %6.2f  %5d  |\n",
			tmp, s->machines, s->avail, (float)s->kflops/1000,s->bench_mips );
	} else {
		printf( "%-20s %3d   | %3d    %3d     %3d   | %3d /%3d     %3d     |\n",
		  	  tmp, s->machines, s->avail, s->user, s->condor, s->sub_machs, s->tot_jobs, s->run );
	}
}

typedef struct {
	char	*arch;
	char	*op_sys;
	int		machines;
	int		jobs;
	int		run;
} SUBMIT_SUMMARY;

SUBMIT_SUMMARY	*SubmitSum[50];
int				N_SubmitSum;
SUBMIT_SUMMARY	SubmitTot;

inc_submit_sum( rec )
SUBMITTOR_REC	*rec;
{
	SUBMIT_SUMMARY *s, *get_submit_sum();

	s = get_submit_sum( rec->arch, rec->op_sys );

	s->machines += 1;
	SubmitTot.machines += 1;

	s->jobs += rec->tot;
	s->run += rec->run;
	SubmitTot.jobs += rec->tot;
	SubmitTot.run += rec->run;
	
}

SUBMIT_SUMMARY *
get_submit_sum( arch, op_sys )
char	*arch;
char	*op_sys;
{
	int		i;
	SUBMIT_SUMMARY	*new;

	for( i=0; i<N_SubmitSum; i++ ) {
		if( strcmp(SubmitSum[i]->arch,arch) == MATCH &&
						strcmp(SubmitSum[i]->op_sys,op_sys) == MATCH ) {
			return SubmitSum[i];
		}
	}

	new = (SUBMIT_SUMMARY *)CALLOC( 1, sizeof(SUBMIT_SUMMARY) );
	new->arch = arch;
	new->op_sys = op_sys;
	SubmitSum[ N_SubmitSum++ ] = new;
	return new;
}

display_submit_summaries()
{
	int		i;
	
	printTimeAndColl();

	printf("------------------------------------------\n");
	printf("ARCH/OS           machines  jobs exporting|\n");
	printf("------------------------------------------|\n");
	for( i=0; i<N_SubmitSum; i++ ) {
		display_submit_sum( SubmitSum[i] );
	}
	printf("------------------------------------------|\n");
	display_submit_sum( &SubmitTot );
	printf("------------------------------------------\n");
}

display_submit_sum( s )
SUBMIT_SUMMARY		*s;
{
	char	tmp[256];

	if( s->arch ) {
		(void)sprintf( tmp, "%s/%s", s->arch, s->op_sys );
	} else {
		strcpy(tmp, "Total");
	}

	printf( "%-20s %3d    %3d   %3d     |\n",
		   tmp, s->machines, s->jobs, s->run );
}
