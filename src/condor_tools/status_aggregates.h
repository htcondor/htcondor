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

#if !defined(STATUS_AGGREGATES_H)
#define STATUS_AGGREGATES_H


#include <time.h>
#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>    
#include "condor_types.h"   
#include "expr.h"
#include "manager.h"
#include "except.h"
#include "debug.h"
#include "util_lib_proto.h"
#include "status_include.h"


#define NIL 0
#define MATCH 0

extern "C" {

    CONTEXT *create_context();
  }


class sub_rec_list
{

         private:

           SUBMIT_SUMMARY   *SubmitSum[50];
           int              N_SubmitSum;
           SUBMIT_SUMMARY   SubmitTot;

           SUBMITTOR_REC    Submittors[1024];
           int              N_Submittors;

           int AvailOnly; 
           int TotalOnly; 
           int DisplayTotal;
           int MachineUpdateInterval; 
           int Now;
           int SortByPrio; 
       
           void           print_header_submittors( FILE *fp );
           void           inc_submit_sum( SUBMITTOR_REC  *rec);
           SUBMIT_SUMMARY *get_submit_sum( char* arch, char* opsys );
           void           display_submit_summaries();
           void           display_submit_sum( SUBMIT_SUMMARY *s );

         public:
           int            submittor_comp( SUBMITTOR_REC *ptr1, SUBMITTOR_REC *ptr2 );

           void           build_submittor_rec( MACH_REC *mach );
           void           display_submittors( FILE *fp );

           sub_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                         int MACHINEUPDATEINTERVAL, int NOW, 
                         int SORTBYPRIO );

           void hashinsert(MACH_REC *rec);

};






class serv_rec_list
{

         private:

           SERV_SUMMARY	    *ServSum[50];
           int		    N_ServSum;
           SERV_SUMMARY	    ServTot;

           SERVER_REC	    Servers[1024];        
           int		    N_ServerRecs; 
 
           CONTEXT	    *GenericJob;

           int AvailOnly; 
           int TotalOnly; 
           int DisplayTotal;
           int MachineUpdateInterval; 
           int Now;
           int SortByPrio; 
       
           void          print_header_servers( FILE *fp );
           void          inc_serv_sum( SERVER_REC *rec );
           SERV_SUMMARY* get_serv_sum( char *arch, char *op_sys);
           void          display_serv_summaries();
           void          display_serv_sum( SERV_SUMMARY *s );

         /* functions */
         public:
           void build_server_rec( MACH_REC *mach );
           void display_servers( FILE *fp );

           int           server_comp( SERVER_REC *ptr1, SERVER_REC *ptr2 );

           serv_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                         int MACHINEUPDATEINTERVAL, int NOW, 
                         int SORTBYPRIO );

};



class run_rec_list
{
         private:

           RUN_REC         Runners[1024];  
           int             N_Runners;      

           RUN_SUMMARY    *RunSum[50];
           int             N_RunSum;
           RUN_SUMMARY     RunTot;

           // test to see if we need.
           CONTEXT	    *GenericJob;

           int AvailOnly; 
           int TotalOnly; 
           int DisplayTotal;
           int MachineUpdateInterval; 
           int Now;
           int SortByPrio; 
       
           void          print_run_header( FILE *fp );
           void          inc_run_sum( RUN_REC *rec );
           RUN_SUMMARY*  get_run_sum( char *arch, char *op_sys);
           void          display_run_summaries();
           void          display_run_sum( RUN_SUMMARY *s );
           void          print_hash_run_header( FILE *fp );

         public:
           void build_run_rec( MACH_REC *mach );
           void display_run( FILE *fp );
           void run_rec_hash_print( FILE *fp );

           int           run_comp( RUN_REC *ptr1, RUN_REC *ptr2 );

           run_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                         int MACHINEUPDATEINTERVAL, int NOW, 
                         int SORTBYPRIO );

};
          

class job_rec_list
{
         private:
           JOB_REC         Jobbers[1024];  
           int             N_Jobbers;      

           JOB_SUMMARY    *JobSum[50];
           int             N_JobSum;
           JOB_SUMMARY     JobTot;

           // test to see if we need.
           CONTEXT	    *GenericJob;

           int AvailOnly; 
           int TotalOnly; 
           int DisplayTotal;
           int MachineUpdateInterval; 
           int Now;
           int SortByPrio; 
       


         public:

           void build_job_rec( MACH_REC *mach );
           void job_hash_insert( MACH_REC *mach );
           int  job_comp( JOB_REC *ptr1, JOB_REC *ptr2 );
           void display_job_summary( FILE *fp );
           void display_job( FILE *fp );
           void print_job_header( FILE *fp );
           void job_hash_header( FILE *fp );
          
           job_rec_list( int AVAILONLY, int TOTALONLY, int DISPLAYTOTAL,
                         int MACHINEUPDATEINTERVAL, int NOW, 
                         int SORTBYPRIO );

};


#endif
