#ifndef JOB_H
#define JOB_H

#include <stdio.h>

#include "types.h"
#include "debug.h"
#include "condor_constants.h"   /* from condor_includes/ directory */
#include "simplelist.h"         /* from condor_c++_util/ directory */

/**
   A Job instance will be used to pass job attributes to the
   AddJob() function.
*/
class Job {
public:
  
  /** Enumeration for specifying which queue for Add() and Remove().
      If you change this enum, you *must* also update queue_t_names
  */
  enum queue_t { Q_INCOMING, Q_WAITING, Q_OUTGOING };

  /// The string names for the queue_t enumeration
  static const char *queue_t_names[];
  
  /** The Status of a Job
      If you update this enum, you *must* also update status_t_names
   */
  enum status_t {
    /** Job is ready */               STATUS_READY,
    /** Job has been submitted */     STATUS_SUBMITTED,
    /** Job is done */                STATUS_DONE,
  };

  /// The string names for the status_t enumeration
  static const char * status_t_names[];
  
  /** Constructor
      @param jobName Name of job in dag file.  String is deep copied.
      @param cmdFile Path to condor cmd file.  String is deep copied.
  */
  Job (const char *jobName, const char *cmdFile);
  
  ///
  ~Job();
  
  /** */ inline char *  GetJobName () const { return _jobName; }
  /** */ inline char *  GetCmdFile () const { return _cmdFile; }
  /** */ inline JobID_t GetJobID   () const { return _jobID;   }

  ///
  inline SimpleList<JobID_t> & GetQueueRef (const queue_t queue) {
    return _queues[queue];
  }

  /** Add a job to one of the queues.  Adds the job with ID jobID to
      the incoming, outgoing, or waiting queue of this job.
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

  /** Remove a job from one of the queues.  Removes the job with ID jobID from
      the incoming, outgoing, or waiting queue of this job.
      @param jobID ID of the job to be removed.  Should not be this job's ID
      @param queue The queue to add the job to
      @return true: success, false: failure (jobID not found in queue)
  */
  bool Remove (const queue_t queue, const JobID_t jobID);

  ///
  inline bool IsEmpty (const queue_t queue) const {
    return _queues[queue].IsEmpty();
  }
 
  /** Dump the contents of this Job to stdout for debugging purposes.
      @param level Only do the dump if the current debug level is >= level
  */
  void Dump () const;
  
  /** Print the identification info for this Job.
   */
  void Print (bool condorID = false) const;
  
  /** */ CondorID _CondorID;
  /** */ status_t _Status;
  
 private:
  
  /// filename of condor submit file
  char * _cmdFile;
  
  /// name given to the job by the user
  char * _jobName;
  
  /** Job queue's
      
      incoming -> dependencies coming into the Job
      outgoing -> dependencies going out of the Job
      waiting -> Jobs on which the current Job is waiting for output 
  */
  SimpleList<JobID_t> _queues[3];
  
  /** The ID of this job.  This serves as a primary key for Job's, where each
      Job's ID is unique from all the rest
  */
  JobID_t _jobID;

  /** Ensures that all jobID's are unique.  Starts at zero and increments
      by one for every Job object that is constructed
  */
  static JobID_t _jobID_counter;
};

void job_print (Job * job, bool condorID = false);


#endif /* ifndef JOB_H */
