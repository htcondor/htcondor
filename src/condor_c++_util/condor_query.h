#ifndef CONDOR_QUERY_H
#define CONDOR_QUERY_H

// this include file should probably be in condor_includes
#include "condor_classad.h"
#include "condor_parser.h"
#include "list.h"
#include "intlist.h"


// job constraint categories which may be easily manipulated
enum job_str_category
{
    JOB_OWNER,
    JOB_ARCH,
    JOB_OPSYS,
    JOB_COMMAND,

    JOB_STRING_THRESHOLD__,
};

enum job_int_category
{
    CLUSTER_ID,           // integer based values
    PROCESS_ID,
    JOB_PRIORITY,
    JOB_STATUS,

    JOB_INTEGER_THRESHOLD__
};

enum job_cust_category
{
    JOB_CUSTOM,         // the custom category

    JOB_CUSTOM_THRESHOLD__
};


// machine constraint categories which may be easily manipulated
enum machine_str_category 
{
    HOST_NAME,
    MACHINE_ARCH,
    MACHINE_OPSYS,

    MACHINE_STRING_THRESHOLD__
};

enum machine_int_category
{
    MACHINE_USERS,
    MACHINE_RUNNING,
    MACHINE_IDLE,
    MACHINE_PRIO,
  
    MACHINE_INTEGER_THRESHOLD__
};

enum machine_cust_category
{
    MACHINE_CUSTOM,

    MACHINE_CUSTOM_THRESHOLD__
};


// error numbers
enum query_result
{
    OK = 0,
    Q_MEMORY_ERROR,
    Q_PARSE_MACH_REQS,
    Q_PARSE_JOB_REQS,
	Q_COMMUNICATION_ERROR
};

class Condor_query
{
    public:

    // ctors and dtor
    Condor_query ();
    Condor_query (Condor_query &);
    ~Condor_query ();

    // clear job categories
    void clear (const job_str_category);
    void clear (const job_int_category);
    void clear (const job_cust_category);

    // clear machine categories
    void clear (const machine_str_category);
    void clear (const machine_int_category);
    void clear (const machine_cust_category);

    // add constraint to job category
    void add (const job_str_category, const char *);
    void add (const job_int_category, const int);
    void add (const job_cust_category, const char *);

    // add constraints to machine category
    void add (const machine_str_category, const char *);
    void add (const machine_int_category, const int);
    void add (const machine_cust_category, const char *);

    // get results of requests
    // - argument 1 is 'out': the list of ads 
    //    - machine type ads for condor_status
    //    - job type ads for condor_queue
    // - argument 2 is default 'in': machine on which the collector resides
    int condor_status (ClassAdList &, const char pool_name [] = "");
    int condor_queue  (ClassAdList &, const char pool_name [] = "");

    // get job queues from given list of machine type class ads
    // - argument 1 is 'in' : list of machine ads
    // - argument 2 is 'out': list of job ads
    int condor_queue  (ClassAdList &, ClassAdList &);

    // filter functions --- use constraints to filter given class ad list
    // - argument 1 is 'in' : given list of machine/job ads
    // - argument 2 is 'out': filtered list of machine/job ads
    int filter_machines (ClassAdList &, ClassAdList &);
    int filter_jobs     (ClassAdList &, ClassAdList &);

    // overloaded display
    friend ostream & operator<< (ostream &, Condor_query &);

    // overloaded assignment --- makes deep copies
    Condor_query & operator= (Condor_query &);

    // private:

    // room to store info
    List<char> job_string_constraints  [JOB_STRING_THRESHOLD__ ];
    IntList    job_integer_constraints [JOB_INTEGER_THRESHOLD__];
    List<char> job_custom_constraints;

    List<char> machine_string_constraints  [MACHINE_STRING_THRESHOLD__ ];
    IntList    machine_integer_constraints [MACHINE_INTEGER_THRESHOLD__];
    List<char> machine_custom_constraints;

    // private functions to obtain the query type classads
    int make_job_query_ad (ClassAd &);
    int make_machine_query_ad (ClassAd &);

    // private functions to obtain the requirement expression from the 
    // expressed constraints
    char *get_machine_expr_string (void);
    char *get_job_expr_string (void);

    // generic clear functions
    void clear_str_category(List<char> &);
    void clear_int_category(IntList    &);

    // generic copy functions
    void copy_str_category(List<char> &, List<char> &);
    void copy_int_category(IntList    &, IntList    &);
};  


#endif
