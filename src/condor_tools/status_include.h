#include <time.h>
#include <iostream.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>   //char* index(char* char);
#include <stdlib.h>     //contains qsort(...)

#include "condor_types.h" 
#include "except.h"
#include "debug.h"
#include "util_lib_proto.h"




extern int AvailOnly;
extern int SortByPrio;



class base {

    public:
         
        char* name;
        char* arch;
        char* op_sys;
        char* state;

        int   run;          //jobs exported.

};



class SERVER_REC : public base
{

    public:
     
        float swap;
        float disk;
        float load_avg;
        int   kbd_idle;
        int   idle;
        int   bench_mips;
        int   kflops;
        int   tot_jobs;         //jobs submitted.

        virtual void  print_rec( FILE *fp );      

};






class  SUBMITTOR_REC  
{

    public: 
 
        //start of the base class
        char* name;
        char* arch;
        char* op_sys;
        char* state;

        int   run;          //jobs exported.
        //end of the base class
 

        int   tot;    //total should be the number queued.
        int   max;
        int   users;
        int   prio;
        float swap;

        void  print_rec( FILE *fp, int counted );
       
        int   sub_comp( SUBMITTOR_REC *ptr2 );

};




class  RUN_REC 
{

    public:

        //comes from the base class
        char* name;
        char* arch;
        char* op_sys;
        char* state;
        char* owner;

        char* remote_user;

        int   run;          //jobs exported.
        //end of the base class.

        //start of the server rec 
        float swap;
        float disk;
        float load_avg;
        int   kbd_idle;
        int   idle;
        int   bench_mips;
        int   kflops;
        int   tot_jobs;         //jobs submitted.
        //end of the server rec

        char* client;    //machine that sent the server the job.   
        
        long  start_time;
        long  entered_state;
        long  running;

        void  print_rec( FILE *fp );

};
	



class  JOB_REC 
{

    public:

        //comes from the base class
        char* remote_user;
        char* name;
        char* arch;
        char* op_sys;
        char* state;

        int   run;          //jobs exported.
        //end of the base class.

        //start of the server rec 
        float swap;
        float disk;
        float load_avg;
        int   kbd_idle;
        int   idle;
        int   bench_mips;
        int   kflops;
        int   tot_jobs;         //jobs submitted.
        //end of the server rec

        char* client;    //machine that sent the server the job.   
        
        long  start_time;
        long  entered_state;
        long  running;

        void  print_rec( FILE *fp );

};
	














/*******************************/
/* here's where the summary    */
/* records start.              */


class  summary_base
{

    public:

        char* arch;        //all summaries record the architecture
        char* op_sys;      //all summaries record the operating system.
        int   machines;    
        
        int   run;

};


class SERV_SUMMARY : public summary_base
{

    public:

        int avail;
        int condor;
        int user;
        int down;
       
        int tot_jobs;
        int sub_machs;  //machines with a job queue
        int bench_mips;
        int kflops;

};


class SUBMIT_SUMMARY : public summary_base 
{

     public:

        int jobs;

}; 



/*
we want the opsystem

the number of machines, what operating system,
the total mips, the total kflops, and the 
average load average.
*/
class RUN_SUMMARY : public summary_base
{

     public:

         int    total_mips;
         int    total_kflops;

         //this and the number of machines will 
         //give us the average load average.
         //XXX if the load average is over 1.0, we only
         //will only add 1.0
         double load_total;
     
        //these here for now to transition code.    
        int avail;
        int condor;
        int user;
        int down;
       
        int tot_jobs;
        int sub_machs;  //machines with a job queue
        int bench_mips;
        int kflops;



};



class JOB_SUMMARY : public summary_base
{

     public:

         int    total_mips;
         int    total_kflops;

         //this and the number of machines will 
         //give us the average load average.
         //XXX if the load average is over 1.0, we only
         //will only add 1.0
         double load_total;
     
        //these here for now to transition code.    
        int avail;
        int condor;
        int user;
        int down;
       
        int tot_jobs;
        int sub_machs;  //machines with a job queue
        int bench_mips;
        int kflops;



};




/****************hash table classes for the summaries******************/


class runhashsummary
{
  public:
    char *mach_name;
    int  total;
    int  running;
    int  suspended;
    int  checkpointed;

    void print( FILE *fp )
      {
      fprintf( fp, "%-14s", this->mach_name );
      fprintf( fp, "%8d",   this->total );
      fprintf( fp, "%8d",   this->running );
      fprintf( fp, "%10d",   this->suspended );
      fprintf( fp, "%13d\n", this->checkpointed );
      }

};




class jobsum
{
  public:
    char* remoteuser;
    int   number; 

    print( FILE *fp )
      {
      fprintf( fp, "%-14s", this->remoteuser );
      fprintf( fp, "%5d\n",   this->number );
      }
 
};

