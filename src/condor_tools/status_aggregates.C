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
** 
*/ 



#include <iostream.h>
#include "status_aggregates.h"
#include "hashtable.h"


      //hash tables for the summaries.
      h_table<runhashsummary*>         HRUN_S;
      h_table<jobsum*>                 JHtab;
      h_table<int*>                     HSUB_S;


extern "C" {
void  printTimeAndColl();
}


sub_rec_list::sub_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                            int MACHINEUPDATEINTERVAL, int NOW, 
                            int SORTBYPRIO )
{
           AvailOnly = AVAILONLY; 
           TotalOnly = TOTALONLY; 
           DisplayTotal = DISPLAYTOTAL;
           MachineUpdateInterval = MACHINEUPDATEINTERVAL; 
           Now = NOW;
           SortByPrio = SORTBYPRIO; 
      
}




void  sub_rec_list::build_submittor_rec( MACH_REC *mach )
{
	SUBMITTOR_REC	*rec;
	CONTEXT	*context;
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

 
      
void sub_rec_list::hashinsert(MACH_REC *recin)
{

   if (Now - recin->time_stamp > MachineUpdateInterval ) {
      return;
   }

   char *ptr ;
   char *ptr2 ;

   if (evaluate_string("ClientMachine", &ptr2, recin->machine_context, NIL) < 0 ){
       ptr2 = "(?)";
   }
   if ( ptr=index(ptr2,'.') ) {
       *ptr = '\0';
   }

   int success ;
   int *tmp = HSUB_S.exists( ptr2, success ) ;
   if ( success ){
       (*tmp)++ ;
   } else {
       int *p = new int;
       *p = 1;
       HSUB_S.insert( ptr2, p ) ;
   }

}



int sub_rec_list::submittor_comp( SUBMITTOR_REC *ptr1, SUBMITTOR_REC *ptr2 )
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



static sub_rec_list *list;

int subc( const void *ptr1, const void *ptr2 ) 
{
       
	return list->submittor_comp((SUBMITTOR_REC*)ptr1, (SUBMITTOR_REC*)ptr2);

}



void sub_rec_list::display_submittors( FILE *fp )
{
	int		i;
        int             success;
        int             *count;

	if( !TotalOnly ) {    
                list = this;
            
 		qsort( Submittors, N_Submittors, sizeof(Submittors[0]), subc );

		print_header_submittors( stdout );
		for( i=0; i<N_Submittors; i++ ) {
                        count = HSUB_S.exists( Submittors[i].name, success );
                        if ( success )
                            Submittors[i].print_rec( fp, *count );
                        else 
                            Submittors[i].print_rec( fp, 0 );
		}
		fprintf( fp, "\n" );
	}
	if( DisplayTotal ) {
		display_submit_summaries();
	}
}




/* start of the private functions. */



/*
print_header_submittors

prints the header for condor_status -submittor
*/
void sub_rec_list::print_header_submittors( FILE *fp )
{

	fprintf( fp, "%-14s ", "Name" );   //name of machine.
        fprintf( fp, "%-4s ", "Run" );  //number out running
	fprintf( fp, "%-4s ", "Exp" );     
	fprintf( fp, "%-4s ", "Tot" );
	fprintf( fp, "%-4s ", "Max" );
	fprintf( fp, "%-5s ", "Users" );
	fprintf( fp, "%-9s ", "Prio" );
	fprintf( fp, "%-6s ", "Swap" );
	fprintf( fp, "%-7s ", "Arch" );
	fprintf( fp, "%-8s ", "OpSys" );
	fprintf( fp, "\n" );

}


void sub_rec_list::inc_submit_sum( SUBMITTOR_REC *rec )
{

	SUBMIT_SUMMARY *s;

	s = get_submit_sum( rec->arch, rec->op_sys );

	s->machines += 1;
	SubmitTot.machines += 1;

	s->jobs += rec->tot;
	s->run += rec->run;
	SubmitTot.jobs += rec->tot;
	SubmitTot.run += rec->run;
	
}


SUBMIT_SUMMARY * sub_rec_list::get_submit_sum( char *arch, char *op_sys )
{
	int		i;
	SUBMIT_SUMMARY	*new_summary;

	for( i=0; i<N_SubmitSum; i++ ) {
		if( strcmp(SubmitSum[i]->arch,arch) == MATCH &&
		    strcmp(SubmitSum[i]->op_sys,op_sys) == MATCH ) {
			return SubmitSum[i];
		}
	}

	new_summary = (SUBMIT_SUMMARY *)CALLOC( 1, sizeof(SUBMIT_SUMMARY) );
	new_summary->arch = arch;
	new_summary->op_sys = op_sys;
	SubmitSum[ N_SubmitSum++ ] = new_summary;
	return new_summary;
}

void sub_rec_list::display_submit_summaries()
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

void sub_rec_list::display_submit_sum( SUBMIT_SUMMARY *s )
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


serv_rec_list::serv_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                            int MACHINEUPDATEINTERVAL, int NOW, 
                            int SORTBYPRIO )
{
           AvailOnly = AVAILONLY; 
           TotalOnly = TOTALONLY; 
           DisplayTotal = DISPLAYTOTAL;
           MachineUpdateInterval = MACHINEUPDATEINTERVAL; 
           Now = NOW;
           SortByPrio = SORTBYPRIO; 
    
	   GenericJob = create_context();
	   store_stmt( scan("Owner = \"nobody\""), GenericJob );

  
}


void serv_rec_list::build_server_rec( MACH_REC *mach )
{
	SERVER_REC	my_rec, *rec = &my_rec;
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




static serv_rec_list *sl;

int servc( const void *ptr1, const void *ptr2 ) 
{
       
	return sl->server_comp((SERVER_REC*)ptr1, (SERVER_REC*)ptr2);

}

     

void serv_rec_list::display_servers( FILE *fp )
{
	int		i;


	if( !TotalOnly ) {
             sl = this;

	     qsort( (char *)Servers, N_ServerRecs, sizeof(Servers[0]), servc );

		print_header_servers( stdout );
		for( i=0; i<N_ServerRecs; i++ ) {
                        Servers[i].print_rec( fp );
		}
		fprintf( fp, "\n" );
	}
	if( DisplayTotal ) {
		display_serv_summaries();
	}
}






/*
print_header_server

prints the header for condor_status -server
*/
void serv_rec_list::print_header_servers( FILE *fp )
{

	fprintf( fp, "%-14s ", "Name" );      //name of machine
	if(!AvailOnly)
	   fprintf( fp, "%-4s ", "Avail" );
	fprintf( fp, "%-6s", "Swap" );
	fprintf( fp, "%-6s ", "Disk" );
	if(!AvailOnly)
	  fprintf( fp, "%-6s ", "State" );    //state of machine
	fprintf( fp, "%-6s ", "LdAvg" );      //the load average
	fprintf( fp, "%8s ", "Idle" );
	if(AvailOnly) {
	  fprintf( fp, "%6s ", "MIPS" );      
	  fprintf( fp, "%6s ", "MFLOPS" );
	  fprintf( fp, "%-7s ", "Arch" );     //what arch is machine
	}
	else
	  fprintf( fp, "%-8s ", "  Arch");
	fprintf( fp, "%-8s ", "OpSys" );
	fprintf( fp, "\n" );
}



int serv_rec_list::server_comp( SERVER_REC *ptr1, SERVER_REC *ptr2 )
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


void serv_rec_list::inc_serv_sum( SERVER_REC *rec )
{
	SERV_SUMMARY *s;

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


SERV_SUMMARY* serv_rec_list::get_serv_sum( char *arch, char *op_sys)
{
	int		i;
	SERV_SUMMARY	*new_summary;

	for( i=0; i<N_ServSum; i++ ) {
		if( strcmp(ServSum[i]->arch,arch) == MATCH &&
		    strcmp(ServSum[i]->op_sys,op_sys) == MATCH ) {
			return ServSum[i];
		}
	}

	new_summary = (SERV_SUMMARY *)CALLOC( 1, sizeof(SERV_SUMMARY) );
	new_summary->arch = arch;
	new_summary->op_sys = op_sys;
	ServSum[ N_ServSum++ ] = new_summary;
	return new_summary;
}




void serv_rec_list::display_serv_summaries()
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

void serv_rec_list::display_serv_sum( SERV_SUMMARY *s )
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



/** run_rec_list functions **/


run_rec_list::run_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                            int MACHINEUPDATEINTERVAL, int NOW, 
                            int SORTBYPRIO )
{
           AvailOnly = AVAILONLY; 
           TotalOnly = TOTALONLY; 
           DisplayTotal = DISPLAYTOTAL;
           MachineUpdateInterval = MACHINEUPDATEINTERVAL; 
           Now = NOW;
           SortByPrio = SORTBYPRIO; 
    
	   GenericJob = create_context();
	   store_stmt( scan("Owner = \"nobody\""), GenericJob );

  
}


void run_rec_list::build_run_rec( MACH_REC *mach )
{

        if (Now - mach->time_stamp > MachineUpdateInterval ) {
            return;
        }


    RUN_REC  my_rec, *rec = &my_rec;
    char        *ptr;
    CONTEXT  *context;
    time_t   current_time;
    int      success;

        context = mach->machine_context;
        if (evaluate_string("ClientMachine", &rec->client, context, NIL) < 0 ){
            rec->client = "(?)";
        }
        if ( ptr=index(rec->client,'.') ) {
                *ptr = '\0';
        }

    runhashsummary   *holder = HRUN_S.exists(rec->client, success);
    runhashsummary   *holder2 = new runhashsummary;

        if (success) holder->total++;

        holder2->mach_name = rec->client;
        holder2->running = 0;          //set the hash table summary to zero.
        holder2->suspended = 0;          
        holder2->checkpointed = 0;
        holder2->total = 1;

	if (evaluate_string("State",&rec->state,context,NIL ) < 0) {
            return;
        }
  
        if ( !strcmp("Running", rec->state) ) {
            rec->state = "condor(r)";
            if (success)
                holder->running++;
            else
                holder2->running++;
        }
        else if ( !strcmp("Suspended", rec->state) ) {
            rec->state = "condor(s)";
            if (success)
                holder->suspended++;
            else
                holder2->suspended++;
        }
        else if ( !strcmp("Checkpoint", rec->state) ) {
            rec->state = "condor(c)";
            if (success)
                holder->checkpointed++;
            else
                holder2->checkpointed++;
        }
        else //it was not in a runing state so return.
            return; 

	if( ptr=index(mach->name,'.') ) {
		*ptr = '\0';
	}
	rec->name = mach->name;

        if (evaluate_string("RemoteUser", &rec->remote_user, context, NIL) < 0 ){
                rec->remote_user = "(?)";
        }
        
	if( evaluate_string("OpSys",&rec->op_sys,context,NIL ) < 0 ) {
		rec->op_sys = "(?)";
	}

	if( evaluate_string("Arch",&rec->arch,context,NIL ) < 0 ) {
		rec->arch = "(?)";
	}

	if( evaluate_float("LoadAvg",&rec->load_avg,context,NIL ) < 0 ) {
		rec->load_avg = -1.0;
	}

        if (evaluate_int("JobStart", (int*)&rec->start_time, context, NIL) < 0 ) {
                rec->start_time = 0;
                rec->running = 0;     //if there was no start time, there's no
        }                             //way to find how long it has been running.
        else {  //otherwise it had a start time listed so we will...
                time(&current_time);
                rec->running = current_time - rec->start_time;
        }

    	if( evaluate_int("KFLOPS", &rec->kflops,context,NIL ) < 0 ) {
		rec->kflops = 0;
	}

	if( evaluate_int("MIPS",&rec->bench_mips,context,NIL ) < 0 ) {
		rec->bench_mips = 0;
	}

        if (evaluate_int("EnteredCurrentState", (int*)&rec->entered_state, context, NIL) < 0 ) {
                rec->entered_state = 0;
        }   
       
        if (!success)
            HRUN_S.insert(rec->client, holder2);
 
	inc_run_sum( rec );

        Runners[N_Runners++] = my_rec;
    
}

static run_rec_list *runl;

int runc( const void *ptr1, const void *ptr2 ) 
{
       
	return runl->run_comp((RUN_REC*)ptr1, (RUN_REC*)ptr2);

}

void run_rec_list::print_hash_run_header( FILE *fp )
{

        fprintf( fp, "client           total running suspended checkpointed\n");

}

void run_rec_list::run_rec_hash_print( FILE *fp )
{
runhashsummary* ptr;

        fprintf( fp, "\n\n" );
        print_hash_run_header(fp);
        if (!HRUN_S.returnfirst(ptr) )
            return;
        ptr->print(fp);
        while ( HRUN_S.returnnext(ptr) )
            ptr->print(fp);
} 

void run_rec_list::display_run( FILE *fp )
{
        int i;
	
        runl = this;

        qsort( (char *)Runners, N_Runners, sizeof(Runners[0]), runc );

	print_run_header( fp );
	for( i=0; i < N_Runners; i++ ) {
                Runners[i].print_rec( fp );
	}
	fprintf( fp, "\n" );
	
        
        run_rec_hash_print(fp);

        fprintf( fp, "\n" );

	if( DisplayTotal ) {
		display_run_summaries();
	}
}


/* 
print_run_header

prints the header for the condor_status -run.
*/
void run_rec_list::print_run_header( FILE *fp )
{

        fprintf( fp, "%-13s ", "USER" );          //user who sent out the job
	fprintf( fp, "%-13s ", "NAME" );          //name of machine running jobs.
        fprintf( fp, "%-11s ", "STATE" );         //state that machine is in.
	fprintf( fp, "%-6s ", "LdAvg" );          //what sort of load is on the machine.
	fprintf( fp, "%-10s ", "CLIENT" );         //whose jobs is it running.
        fprintf( fp, "%-7s ", "ARCH" );           //what architecture is the machine
        fprintf( fp, "%-13s ", "OP_SYS" );        //what OS is on the machine.
        fprintf( fp, "%-14s ", "RUNNING" );       //what time this job was started.
	fprintf( fp, "\n" );

}



/*
run_comp

comparison function for the condor_status -run used to 
pass to qsort so all the run recs will print out according 
to architecture, then os, and finally, sort by name of the machine.
*/
int run_rec_list::run_comp( RUN_REC *ptr1, RUN_REC *ptr2 )
{
	int		status;

	if( status = strcmp( ptr1->arch, ptr2->arch ) ) {
		return status;
	}

	if( status = strcmp( ptr1->op_sys, ptr2->op_sys ) ) {
		return status;
	}

        if ( status = strcmp( ptr1->remote_user, ptr2->remote_user ) ) {
                return status;
        }

	if ( status = strcmp( ptr1->client, ptr2->client ) ) {
                return status;
        }

	if ( status = strcmp( ptr1->name, ptr2->name ) ) {
                return status;
        }

        return strcmp( ptr1->state, ptr2->state );
}



void run_rec_list::display_run_sum( RUN_SUMMARY *s )
{
        char    tmp[256];
        double  totkflops = ((float)s->kflops/1000) * (s->load_total/s->machines);
        double  totmips   = ((float)s->bench_mips)  * (s->load_total/s->machines);

        if( s->arch ) {
                (void)sprintf( tmp, "%s/%s", s->arch, s->op_sys );
        } else {
                strcpy(tmp, "Total");
        }

        printf( "%-20s %3d   | %6.0f   %12.0f       %8.2f           |\n",
                tmp, s->machines, totmips, totkflops, s->load_total/s->machines );
    
}


void run_rec_list::display_run_summaries()
{
	int		i;

	printTimeAndColl();
       
	printf("----------------------------------------------------------------------------\n");
	printf("ARCH/OS           machines | TOTAL MIPS   TOTAL MFLOPS    AVG LOAD AVERAGE |\n");
	printf("----------------------------------------------------------------------------|\n");
	

	for( i=0; i<N_RunSum; i++ ) {
		display_run_sum( RunSum[i] );
	}

	printf("----------------------------------------------------------------------------|\n");	
	display_run_sum( &RunTot );
	printf("----------------------------------------------------------------------------\n");

}


RUN_SUMMARY* run_rec_list::get_run_sum( char *arch, char *op_sys)
{
	int		i;
	RUN_SUMMARY	*new_summary;

	for( i=0; i<N_RunSum; i++ ) {
		if( strcmp(RunSum[i]->arch,arch) == MATCH &&
						strcmp(RunSum[i]->op_sys,op_sys) == MATCH ) {
			return RunSum[i];
		}
	}

	new_summary = (RUN_SUMMARY *)CALLOC( 1, sizeof(RUN_SUMMARY) );
	new_summary->arch = arch;
	new_summary->op_sys = op_sys;
	RunSum[ N_RunSum++ ] = new_summary;
	return new_summary;
}


void run_rec_list::inc_run_sum( RUN_REC *rec )
{
	RUN_SUMMARY *s;

	s = get_run_sum( rec->arch, rec->op_sys );       

	s->machines += 1;
	RunTot.machines += 1;

	s->run += rec->run;
	RunTot.run += rec->run;

	s->bench_mips += rec->bench_mips;
	RunTot.bench_mips += rec->bench_mips;

	s->kflops += rec->kflops;
	RunTot.kflops += rec->kflops;

	s->tot_jobs += rec->tot_jobs;
	RunTot.tot_jobs+= rec->tot_jobs;

        /* HERE'S WHERE THE CHANGE IS FOR THE LOAD AVERAGE. */
        /* HERE I ONLY ADD 1.0.                             */
        if ( rec->load_avg > 1.0 )
            {
            s->load_total += 1.0;
            RunTot.load_total += 1.0;
            }
        else
            {
            s->load_total += rec->load_avg;
            RunTot.load_total += rec->load_avg;
            }

	if( rec->idle ) {
		s->avail += 1;
		RunTot.avail += 1;
	} else if( strcmp(rec->state,"NoJob") == MATCH ) {
		s->user += 1;
		RunTot.user += 1;
	} else {
		s->condor += 1;
		RunTot.condor += 1;
	}

}









job_rec_list::job_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                            int MACHINEUPDATEINTERVAL, int NOW, 
                            int SORTBYPRIO )
{
           AvailOnly = AVAILONLY; 
           TotalOnly = TOTALONLY; 
           DisplayTotal = DISPLAYTOTAL;
           MachineUpdateInterval = MACHINEUPDATEINTERVAL; 
           Now = NOW;
           SortByPrio = SORTBYPRIO; 
    
	   GenericJob = create_context();
	   store_stmt( scan("Owner = \"nobody\""), GenericJob );
}

void job_rec_list::build_job_rec( MACH_REC *mach )
{
    JOB_REC  my_rec, *rec = &my_rec;
    char     *ptr;
    CONTEXT  *context = mach->machine_context;
    time_t   current_time;
    int      k;

        if (Now - mach->time_stamp > MachineUpdateInterval ) {
            return;
        }

        if (evaluate_int("Running",&k,context,NIL) < 0) {
            return;
        }

	//if this is a client then insert into hash and exit.
        if (k>0) return;  

        if (evaluate_string("State",&rec->state,context,NIL ) < 0) {
	    return;
        }

	if ( !strcmp("Running", rec->state) ) {
            rec->state = "condor(r)";
	}
	else if ( !strcmp("Suspended", rec->state) ) {
	    rec->state = "condor(s)";
        }
        else if ( !strcmp("Checkpoint", rec->state) ) {
            rec->state = "condor(c)";
	}
	else //it was not in a runing state so return.
    	    return;

        if( ptr=index(mach->name,'.') ) {
            *ptr = '\0';
	}
	rec->name = mach->name;

        if (evaluate_string("ClientMachine", &rec->client, context, NIL) < 0 ){
	    rec->client = "(?)";
        }

	if ( ptr=index(rec->client,'.') ) {
	    *ptr = '\0';
	}

        if (evaluate_string("RemoteUser", &rec->remote_user, context, NIL) < 0){
            rec->remote_user = "(?)";
        }

        Jobbers[N_Jobbers++] = my_rec;    
}

void job_rec_list::job_hash_insert( MACH_REC *mach )
{
    char        *tmp;
    char        *ptr;
    CONTEXT  *context;
    time_t   current_time;
    jobsum   *sumblock = new jobsum;
    jobsum   *smptr;
    int      success;

        if (Now - mach->time_stamp > MachineUpdateInterval ) {
            return;
        }

        context = mach->machine_context;

	if (evaluate_string("State",&tmp,context,NIL ) < 0) {
            return;
        }
  
        if ( !strcmp("Running", tmp) ) {
        }
        else if ( !strcmp("Suspended", tmp) ) {
        }
        else if ( !strcmp("Checkpoint", tmp) ) {
        }
        else return;

        if (evaluate_string("RemoteUser", &sumblock->remoteuser, context, NIL) < 0){
            sumblock->remoteuser = "(?)";
        }
        
        smptr = JHtab.exists(sumblock->remoteuser, success);
        if (success)
          {
          smptr->number++;
          }
        else 
          {
          //rec->client is the machine that is sending out the jobs.
          //want to add how many jobs it thinks it has out...
          sumblock->number = 1;
          JHtab.insert(sumblock->remoteuser, sumblock);
          }
}

void job_rec_list::print_job_header( FILE *fp )
{
    fprintf( fp, "%-14s", "owner");
    fprintf( fp, "%-11s", "client");
    fprintf( fp, "%-14s\n", "server");
}


//this will change to sort on the name of the user.
int job_rec_list::job_comp( JOB_REC *ptr1, JOB_REC *ptr2 )
{
	int             status;

	if ( status = strcmp( ptr1->remote_user, ptr2->remote_user ) )
	    return status;        

	if ( status = strcmp( ptr1->client, ptr2->client ) )
	    return status;

	return strcmp( ptr1->name, ptr2->name ); 

}

void job_rec_list::job_hash_header( FILE *fp )
{

        fprintf( fp, "user    number of jobs\n");

}

void job_rec_list::display_job_summary( FILE *fp )
{
jobsum *ptr;

        fprintf( fp, "\n" );
        job_hash_header(fp);
        if (!JHtab.returnfirst(ptr) )
            return;
        ptr->print(fp);
        while ( JHtab.returnnext(ptr) )
            ptr->print(fp);
        fprintf( fp, "\n\n" );

}

static job_rec_list *jl;

int jobc( const void *ptr1, const void *ptr2 ) 
{
       
	return jl->job_comp((JOB_REC*)ptr1, (JOB_REC*)ptr2);

}


void job_rec_list::display_job( FILE *fp )
{
        int i;

        jl = this;
	qsort( (char *)Jobbers, N_Jobbers, sizeof(Jobbers[0]), 
					    jobc );

	print_job_header( fp );
	for( i=0; i < N_Jobbers; i++ ) {
                Jobbers[i].print_rec( fp );    
	}
	fprintf( fp, "\n" );
        fprintf( fp, "\n" );
        display_job_summary(fp);
}








