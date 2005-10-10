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


/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes.
	Assumes the existance of an open qmgr connection (via ConnectQ).

	Likely usage:
	// original_vanilla_ad is the ad passed to VanillaToGrid
	//   (it can safely a more recent version pulled from the queue)
	// new_grid_ad is the resulting ad from VanillaToGrid, with any updates 
	//   (indeed, it should probably be the most recent version from the queue)
	original_vanilla_ad.ClearAllDirtyFlags();
	update_job_status(original_vanilla_ad, new_grid_ad);
	push_dirty_attributes(original_vanilla_ad);
*/
bool push_dirty_attributes(classad::ClassAd & src);

/*
	Push the dirty attributes in src into the queue.  Does _not_ clear
	the dirty attributes. 
	Establishes (and tears down) a qmgr connection.
	schedd_name and pool_name can be NULL to indicate "local".
*/
bool push_dirty_attributes(classad::ClassAd & src, const char * schedd_name, const char * pool_name);

/*

Pull a submit_job() job's results out of the sandbox and place them back where
they came from.

*/
bool finalize_job(int cluster, int proc, const char * schedd_name, const char * pool_name);


#endif /* INCLUDE_SUBMIT_JOB_H*/
