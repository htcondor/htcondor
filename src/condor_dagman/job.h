/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#ifndef JOB_H
#define JOB_H

#include "condor_common.h"      /* for <stdio.h> */
#include "condor_constants.h"   /* from condor_includes/ directory */
#include "simplelist.h"         /* from condor_c++_util/ directory */

//
// Local DAGMan includes
//
#include "types.h"
#include "debug.h"
#include "script.h"

#define JOB_ERROR_TEXT_MAXLEN 128

/**  The job class represents a job in the DAG and it's state in the Condor
     system.  A job is given a name, a CondorID, and three queues.  The
     parents queue is a list of parent jobs that this one depends on.  That
     queue never changes once set.  The waiting queue is the same as the
     parents queue, but shrinks to emptiness has the parent jobs complete.
     The children queue is a list of child jobs that depend on this one.
     Once set, this queue never changes.<p>

     From DAGMan's point of view, a job has six basic states in the
     Condor system (refer to the Job::status_t enumeration).  It
     starts out as READY, meaning that it has not yet been submitted.
     If the job has a PRE script defined it changes to PRERUN while
     that script executes.  The job's state changes to SUBMITTED once
     in the Condor system.  If it completes successfully and the job
     has a POST script defined it changes to POSTRUN while that script
     executes, otherwise if it completes successfully it is DONE, or
     is in the ERROR state if it completes abnormally.<p>

     The DAG class will control the job by modifying and viewing its
     three queues.  Once the WAITING queue becomes empty, the job
     state either changes to PRERUN while the job's PRE script runs,
     or the job is submitted and its state changes immediately to
     SUBMITTED.  If the job completes successfully, its state is
     changed to DONE, and the children (listed in this jobs CHILDREN
     queue) are put on the DAG's ready list.  */
class Job {
  public:
  
    /** Enumeration for specifying which queue for Add() and Remove().
        If you change this enum, you *must* also update queue_t_names
    */
    enum queue_t {
        /** Parents of this job.  The list of parent jobs, which does not
            change while DAGMan is running.
        */
        Q_PARENTS,

        /** Parents of this job not finished.  The list of parents that
            have not yet finished.  Once this queue is empty, this job may
            run.
        */
        Q_WAITING,

        /** Children of this job.  The list of child jobs, which does not
            change while DAGMan is running.
        */
        Q_CHILDREN
    };

    /** The string names for the queue_t enumeration.  Use this for printing
        the names "Q_PARENTS", "Q_WAITING", or "Q_CHILDREN".  For example,
        to print out whether the children queue is empty:<p>
        <pre>
          Job * job;  // Assume this is assigned to a job
          printf ("The %s queue of job %s is %s empty\n",
                  Job::queue_t_names[Q_CHILDREN], job->GetJobName(),
                  job->IsEmpty(Q_CHILDREN) ? "", "not");
        </pre>
    */
    static const char *queue_t_names[];
  
    /** The Status of a Job
        If you update this enum, you *must* also update status_t_names
    */
    enum status_t {
        /** Job is ready for submission */ STATUS_READY,
        /** Job waiting for PRE script */  STATUS_PRERUN,
        /** Job has been submitted */      STATUS_SUBMITTED,
        /** Job waiting for POST script */ STATUS_POSTRUN,
        /** Job is done */                 STATUS_DONE,
        /** Job exited abnormally */       STATUS_ERROR,
    };

    /** The string names for the status_t enumeration.  Use this the same
        way you would use the queue_t_names array.
    */
    static const char * status_t_names[];

	// explanation text for errors
	char error_text[JOB_ERROR_TEXT_MAXLEN];
  
    /** Constructor
        @param jobName Name of job in dag file.  String is deep copied.
        @param cmdFile Path to condor cmd file.  String is deep copied.
    */
    Job (const char *jobName, const char *cmdFile);
  
    ///
    ~Job();
  
    // returns _jobName
	inline const char* GetJobName() { return _jobName; }

    /** */ inline char *  GetCmdFile () const { return _cmdFile; }
    /** */ inline JobID_t GetJobID   () const { return _jobID;   }

    Script * _scriptPre;
    Script * _scriptPost;

    ///
    inline SimpleList<JobID_t> & GetQueueRef (const queue_t queue) {
        return _queues[queue];
    }

    /** Add a job to one of the queues.  Adds the job with ID jobID to
        the parents, children, or waiting queue of this job.
        @param jobID ID of the job to be added.  Should not be this job's ID
        @param queue The queue to add the job to
        @return true: success, false: failure (lack of memory)
    */
    inline bool Add (const queue_t queue, const JobID_t jobID) {
        return _queues[queue].Append(jobID);
    }

    /** Returns true if this job is ready for submittion.
        @return true if job is submitable, false if not
    */
    inline bool CanSubmit () const {
        return (IsEmpty(Job::Q_WAITING) && _Status == STATUS_READY);
    }

    /** Remove a job from one of the queues.  Removes the job with ID
        jobID from the parents, children, or waiting queue of this job.
        @param jobID ID of the job to be removed.  Should not be this job's ID
        @param queue The queue to add the job to
        @return true: success, false: failure (jobID not found in queue)
    */
    bool Remove (const queue_t queue, const JobID_t jobID);

    /** Returns true if a queue is empty (has no jobs)
        @param queue Selects which queue to look at
        @return true: queue is empty, false: otherwise
    */
    inline bool IsEmpty (const queue_t queue) const {
        return _queues[queue].IsEmpty();
    }
 
    /** Dump the contents of this Job to stdout for debugging purposes.
        @param level Only do the dump if the current debug level is >= level
    */
    void Dump () const;
  
    /** Print the identification info for this Job to stdout.
        @param condorID If true, also print the job's CondorID
     */
    void Print (bool condorID = false) const;
  
    /** */ CondorID _CondorID;
    /** */ status_t _Status;

	// the return code of the job
	int retval;
  
  private:
  
    // filename of condor submit file
    char * _cmdFile;
  
    // name given to the job by the user
    char * _jobName;
  
    /*  Job queue's
      
        parents -> dependencies coming into the Job
        children -> dependencies going out of the Job
        waiting -> Jobs on which the current Job is waiting for output 
    */
    SimpleList<JobID_t> _queues[3];
  
    /*  The ID of this job.  This serves as a primary key for Job's, where each
        Job's ID is unique from all the rest
    */
    JobID_t _jobID;

    /*  Ensures that all jobID's are unique.  Starts at zero and increments
        by one for every Job object that is constructed
    */
    static JobID_t _jobID_counter;
};

/** A wrapper function for Job::Print which allows a NULL job pointer.
    @param job Pointer to job object, if NULL then "(UNKNOWN)" is printed
    @param condorID If true, also print the job's CondorID
*/
void job_print (Job * job, bool condorID = false);


#endif /* ifndef JOB_H */
