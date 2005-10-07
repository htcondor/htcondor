#ifndef INCLUDE_SUBMIT_JOB_H
#define INCLUDE_SUBMIT_JOB_H

namespace classad { class ClassAd; }
class ClassAd;

/* 
- src - The classad to submit.  The ad will be modified based on the needs of
  the submission.  This will include adding QDate, ClusterId and ProcId as well as putting the job on hold (for spooling)

- schedd_name - Name of the schedd to send the job to, as specified in the Name
  attribute of the schedd's classad.  Can be NULL to indicate "local schedd"

- pool_name - hostname (and possible port) of collector where schedd_name can
  be found.  Can be NULL to indicate "whatever COLLECTOR_HOST in my
  condor_config file claims".

*/
bool submit_job( ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out = 0, int * proc_out = 0 );


/* 
- src - The classad to submit.  Will _not_ be modified.

- schedd_name - Name of the schedd to send the job to, as specified in the Name
  attribute of the schedd's classad.  Can be NULL to indicate "local schedd"

- pool_name - hostname (and possible port) of collector where schedd_name can
  be found.  Can be NULL to indicate "whatever COLLECTOR_HOST in my
  condor_config file claims".

*/
bool submit_job( classad::ClassAd & src, const char * schedd_name, const char * pool_name, int * cluster_out = 0, int * proc_out = 0 );


bool finalize_job(int cluster, int proc, const char * schedd_name, const char * pool_name);


#endif /* INCLUDE_SUBMIT_JOB_H*/
