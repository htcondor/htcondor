/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h" 
#include "condor_debug.h"
#include "condor_classad.h"
#include "condor_debug.h"
#include "proc.h"
#include "read_user_log.h"
#include "condor_event.h"
#include "condor_id.h"
#include "userlog_to_classads.h"

typedef long condor_time_t;
static condor_time_t getEventTime(const ULogEvent *event)
{
  return condor_time_t( event->GetEventclock() );
}

static double tmval2double(struct timeval &t)
{
  return t.tv_sec+t.tv_usec*1.0e-6;
}

static bool isSuperset(CondorID &myID, CondorID &testID)
{
  // -1 means in testID all
  if (testID._cluster==-1) {
    return true;
  } else if (myID._cluster!=testID._cluster) {
    return false;
  } else if (testID._proc==-1) {
    return true;
  } else if (myID._proc!=testID._proc) {
    return false;
  } else if (testID._subproc==-1) {
    return true;
  } else {
    return (myID._subproc==testID._subproc);
  }
}

bool userlog_to_classads(const char *filename,
	bool (*pfnProcess)(void* pv, ClassAd* ad), void* pvProcess,
	CondorID* JobIds, int cJobIds, ExprTree *constraintExpr)
{
  std::map<CondorID,ClassAd *> cmap;   // classad map ... current classad for job
  std::map<CondorID,ULogEvent *> emap; // event map ... previous event for job
  ReadUserLog userlog;
  ULogEventOutcome res;
  ULogEvent *event;

  if (userlog.initialize(filename)!=true) {
    return false;
  }
  
  for (res=userlog.readEvent(event); res==ULOG_OK; res=userlog.readEvent(event)) {
    CondorID jobid(event->cluster,event->proc,event->subproc);
    ULogEventNumber eventNumber=event->eventNumber;
    if (eventNumber!=ULOG_SUBMIT) {
      // make sure we have seen the submit event
      // else we just throw it away
      if (cmap.find(jobid)==cmap.end()) {
        delete event;
        continue;
      }
    } else if (cJobIds > 0) {
      bool keep_it = false;
      for (int ix = 0; ix < cJobIds; ++ix) {
         if (isSuperset(jobid, JobIds[ix])) { keep_it = true; break; }
      }
      if ( ! keep_it) {
        // not somtething the user is interested in... skip
        // as a side effect, all events for this ID will be ignored
        delete event;
        continue;
      }
    }
     
    switch(eventNumber) {
    case ULOG_SUBMIT:
      {
	SubmitEvent *submit_event=dynamic_cast<SubmitEvent*>(event);
	ClassAd *jobClassAd = new ClassAd();

	// Standard header of the job
	jobClassAd->InsertAttr("MyType","Job");
	jobClassAd->InsertAttr("TargetType","Machine");
	// All jusbs must have a Cluster And Proc IDs
	// SubprocID is not propagated
	jobClassAd->InsertAttr("ClusterId",event->cluster);
	jobClassAd->InsertAttr("ProcId",event->proc);

	// Since we read from the log, let's propagate this info
	jobClassAd->InsertAttr("UserLog",filename);

	// Add the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("QDate",eventTime);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);

	  // Create a reasonable looking global ID
	  char globalid[256];
	  sprintf(globalid,"localhost#%i.%i#%li",event->cluster,event->proc,long(eventTime));
	  jobClassAd->InsertAttr("GlobalJobId",globalid);
	}

	{
	  condor_time_t now = condor_time_t(time(NULL));
	  jobClassAd->InsertAttr("ServerTime",now);
	}

	// SubmitHost is usually not available with condor_q
	// but seems a waste not to propagate the info
	jobClassAd->InsertAttr("SubmitHost",submit_event->getSubmitHost());

	// Since this is the first event, we assume we are idle
	jobClassAd->InsertAttr("JobStatus",IDLE);
	jobClassAd->InsertAttr("LastJobStatus",0);

	// Make up a few attributes needed by condor_q
	// Take educated guesses wherever possible
        jobClassAd->InsertAttr("Owner","???"); // start with an obviously fake default
#ifndef WIN32
	{
	  // Assuming that the current user is also the Owner
	  // is the best guess we can make.
	  // Seems reasonable to assume only owners would read the log file
          struct passwd *upass = getpwuid(getuid());
          if (upass!=NULL) {
            jobClassAd->InsertAttr("Owner",upass->pw_name);
          }
	}
#endif

	jobClassAd->InsertAttr("JobPrio",0);
	jobClassAd->InsertAttr("ImageSize",0);

	jobClassAd->InsertAttr("Cmd","???"); // no good default, make it obvious it is fake

	// Add a few default values that are expected in all Job ClassAds
	// and will be updated later on
	jobClassAd->InsertAttr("LastSuspensionTime",0);
	jobClassAd->InsertAttr("CompletionDate",0);

	jobClassAd->InsertAttr("NumJobStarts",0);
	jobClassAd->InsertAttr("TotalSuspensions",0);
	jobClassAd->InsertAttr("NumSystemHolds",0);

	jobClassAd->InsertAttr("LocalUserCpu",0.0);
	jobClassAd->InsertAttr("RemoteUserCpu",0.0);
	jobClassAd->InsertAttr("LocalSysCpu",0.0);
	jobClassAd->InsertAttr("RemoteSysCpu",0.0);
	jobClassAd->InsertAttr("RemoteWallClockTime",0.0);

	// ignoring file transfer for now
	// ignoring checkpointing for now

	cmap[jobid]=jobClassAd;
      }
      break;

    case ULOG_EXECUTE:
      {
	ExecuteEvent *start_event=dynamic_cast<ExecuteEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",RUNNING);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	  jobClassAd->InsertAttr("JobCurrentStartDate",eventTime);
	}

	{
	  classad::Value v;
	  int i = 0;
	  if (jobClassAd->EvaluateExpr("NumJobStarts+1",v) && v.IsIntegerValue(i)) {
	    jobClassAd->InsertAttr("NumJobStarts",i);
	  } else {
	    jobClassAd->InsertAttr("NumJobStarts",1);
	  }
	}

	jobClassAd->InsertAttr("StartdIpAddr",start_event->getExecuteHost());
	if (start_event->getRemoteName()!=NULL) {
	  jobClassAd->InsertAttr("RemoteHost",start_event->getRemoteName());
	} else {
	  // the execute host is the 2nd best we have
	  jobClassAd->InsertAttr("RemoteHost",start_event->getExecuteHost());
	}
      }
      break;


    case ULOG_JOB_TERMINATED:
      {
	JobTerminatedEvent *term_event=dynamic_cast<JobTerminatedEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",COMPLETED);
	}

	{
	  ExprTree *oldhost=jobClassAd->Remove("RemoteHost");
	  jobClassAd->Insert("LastRemoteHost",oldhost);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	  jobClassAd->InsertAttr("CompletionDate",eventTime);
	}

	if (term_event->normal) {
	  // normal termination
	  jobClassAd->InsertAttr("ExitBySignal",false);
	  jobClassAd->InsertAttr("ExitCode",term_event->returnValue);
	} else {
	  jobClassAd->InsertAttr("ExitBySignal",true);
	  jobClassAd->InsertAttr("ExitSignal",term_event->signalNumber);
	}

	jobClassAd->InsertAttr("LastLocalUserCpu",tmval2double(term_event->run_local_rusage.ru_utime));
	jobClassAd->InsertAttr("LastRemoteUserCpu",tmval2double(term_event->run_remote_rusage.ru_utime));
	jobClassAd->InsertAttr("LastLocalSysCpu",tmval2double(term_event->run_local_rusage.ru_stime));
	jobClassAd->InsertAttr("LastRemoteSysCpu",tmval2double(term_event->run_remote_rusage.ru_stime));
	{
	  classad::Value v;
	  if (jobClassAd->EvaluateExpr("real(EnteredCurrentStatus-JobCurrentStartDate)",v))
	    {
	      double t;
	      if (v.IsRealValue(t)) {
		jobClassAd->InsertAttr("LastRemoteWallClockTime",t);
	      }
	    }
	}

	jobClassAd->InsertAttr("LocalUserCpu",tmval2double(term_event->total_local_rusage.ru_utime));
	jobClassAd->InsertAttr("RemoteUserCpu",tmval2double(term_event->total_remote_rusage.ru_utime));
	jobClassAd->InsertAttr("LocalSysCpu",tmval2double(term_event->total_local_rusage.ru_stime));
	jobClassAd->InsertAttr("RemoteSysCpu",tmval2double(term_event->total_remote_rusage.ru_stime));
	{
	  classad::Value v;
	  if (jobClassAd->EvaluateExpr("RemoteWallClockTime+LastRemoteWallClockTime",v))
	    {
	      double t;
	      if (v.IsRealValue(t)) {
		jobClassAd->InsertAttr("RemoteWallClockTime",t);
	      }
	    }
	}
	// ignoring file transfer for now

      }
      break;

    case ULOG_JOB_SUSPENDED:
      {
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",SUSPENDED);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	  jobClassAd->InsertAttr("LastSuspensionTime",eventTime);
	}
	{
	  classad::Value v;
	  int i = 0;
	  if (jobClassAd->EvaluateExpr("TotalSuspensions+1",v) && v.IsIntegerValue(i)) {
	    jobClassAd->InsertAttr("TotalSuspensions",i);
	  } else {
	    jobClassAd->InsertAttr("TotalSuspensions",1);
	  }
	}
      }
      break;

    case ULOG_JOB_UNSUSPENDED:
      {
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",RUNNING);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	}
      }
      break;

    case ULOG_JOB_EVICTED:
      {
	JobEvictedEvent *evict_event=dynamic_cast<JobEvictedEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",IDLE);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	  jobClassAd->InsertAttr("LastVacateTime",eventTime);
	}
	
	{
	  
	  const char * reason=evict_event->getReason();
	  if (reason!=NULL) {
	    jobClassAd->InsertAttr("LastVacateReason",reason);
	  }
	}

	// ignoring checkpointing and requeuing for now
	// ignoring file transfer for now

      }
      break;

    case ULOG_SHADOW_EXCEPTION:
      {
	ShadowExceptionEvent *evict_event=dynamic_cast<ShadowExceptionEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",IDLE);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	  if (evict_event->began_execution)
	    jobClassAd->InsertAttr("LastVacateTime",eventTime);
	}

	jobClassAd->InsertAttr("LastVacateReason",evict_event->message);

	// ignore file trasfer for now
      }
      break;

    case ULOG_JOB_DISCONNECTED:
    case ULOG_JOB_RECONNECTED:
      // these are temporary problems, ignore
      break;

    case ULOG_JOB_RECONNECT_FAILED:
      {
	JobReconnectFailedEvent *evict_event=dynamic_cast<JobReconnectFailedEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",IDLE);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	  jobClassAd->InsertAttr("LastVacateTime",eventTime);
	}

	{
	  const char * reason=evict_event->getReason();
	  if (reason!=NULL) {
	    jobClassAd->InsertAttr("LastVacateReason",reason);
	  }
	}
      }
      break;
      
    case ULOG_JOB_HELD:
      {
	JobHeldEvent *hold_event=dynamic_cast<JobHeldEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	int prev_status=IDLE;
	{
	  classad::Value v;
	  jobClassAd->EvaluateExpr("JobStatus",v) && v.IsIntegerValue(prev_status);
	}

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",HELD);
	}

	// Update the time attributes
	{
	  const char * reason=hold_event->getReason();

	  {
	    condor_time_t eventTime = getEventTime(event);
	    jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	    if (prev_status!=IDLE) {
	      jobClassAd->InsertAttr("LastVacateTime",eventTime);
	      if (reason!=NULL) {
		jobClassAd->InsertAttr("LastVacateReason",reason);
	      }
	    }
	  }

	  if (reason!=NULL) {
	    jobClassAd->InsertAttr("HoldReason",reason);
	  }
	}
	jobClassAd->InsertAttr("HoldReasonCode",hold_event->getReasonCode());
      	jobClassAd->InsertAttr("HoldReasonSubCode",hold_event->getReasonSubCode());
	if (hold_event->getReasonCode()!=1) {
	  classad::Value v;
	  int i = 0;
	  if (jobClassAd->EvaluateExpr("NumSystemHolds+1",v) && v.IsIntegerValue(i)) {
	    jobClassAd->InsertAttr("NumSystemHolds",i);
	  } else {
	    jobClassAd->InsertAttr("NumSystemHolds",1);
	  }
	}
      }
      break;
      
    case ULOG_JOB_RELEASED:
      {
	JobReleasedEvent *rel_event=dynamic_cast<JobReleasedEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",IDLE);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);

	}

	{
	  const char * reason = rel_event->getReason();
	  if (reason!=NULL) {
	    jobClassAd->InsertAttr("ReleaseReason",reason);
	  }
	}
      }
      break;

    case ULOG_JOB_ABORTED:
      {
	JobAbortedEvent *abort_event=dynamic_cast<JobAbortedEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	// Change status, preserving the old one
        {
	  ExprTree *oldstatus=jobClassAd->Remove("JobStatus");
	  jobClassAd->Insert("LastJobStatus",oldstatus);
	  jobClassAd->InsertAttr("JobStatus",REMOVED);
	}

	// Update the time attributes
	{
	  condor_time_t eventTime = getEventTime(event);
	  jobClassAd->InsertAttr("EnteredCurrentStatus",eventTime);
	}

	{
	  const char * reason = abort_event->getReason();
	  if (reason!=NULL) {
	    jobClassAd->InsertAttr("RemoveReason",reason);
	  }
	}
      }
      break;

      
    case ULOG_IMAGE_SIZE:
      {
	JobImageSizeEvent *img_event=dynamic_cast<JobImageSizeEvent*>(event);
	ClassAd *jobClassAd = cmap[jobid];

	jobClassAd->InsertAttr("ImageSize",img_event->image_size_kb);
	jobClassAd->InsertAttr("ResidentSetSize",img_event->resident_set_size_kb);
	jobClassAd->InsertAttr("ProportionalSetSize",img_event->proportional_set_size_kb);
      }
      break;


    case ULOG_JOB_AD_INFORMATION:
      {
	// The JobAdInformation ClassAd contains both
	//   the interesting attributes and
	//   the classad attributes of the previous event
	// So I need to do a diff to get only the interesting ones

	ClassAd *eventClassAd=event->toClassAd(false);
	ClassAd *prevEventClassAd=emap[jobid]->toClassAd(false);
	ClassAd *jobClassAd = cmap[jobid];

	// delete attributes from the previous event
	for (ClassAd::iterator it=prevEventClassAd->begin();
	     it!=prevEventClassAd->end();
	     ++it) {
	  eventClassAd->Delete(it->first);
	}

	// plus, delete attributes that were added by JobAdEvent itself
	eventClassAd->Delete("MyType");
	eventClassAd->Delete("Cluster");
	eventClassAd->Delete("Proc");
	eventClassAd->Delete("Subproc");
	eventClassAd->Delete("EventTypeNumber");
	eventClassAd->Delete("TriggerEventTypeNumber");
	eventClassAd->Delete("TriggerEventTypeName");
	
	// merge the rest
	jobClassAd->Update(*eventClassAd);

	
	delete eventClassAd; 
	delete prevEventClassAd;
      }
      break;


    case ULOG_EXECUTABLE_ERROR:
      // Don't do anything here, because we seem to always
      // also get an ABORTED event when we get an
      // EXECUTABLE_ERROR event.  (Not doing anything
    case ULOG_NODE_EXECUTE:
    case ULOG_NODE_TERMINATED:
    case ULOG_POST_SCRIPT_TERMINATED:
    case ULOG_GENERIC:
    // ignoring Grid events for now
    // ignoring Checkpointing events for now
    default:
      break;
    }
    if (eventNumber!=ULOG_SUBMIT) {
      // SUMBIT is the first to touch emap
      delete emap[jobid];
    }
    emap[jobid]=event;
  }

  for (std::map<CondorID,ClassAd *>::iterator it=cmap.begin(); it!=cmap.end();++it) {
    // delete all old events
    /// can use the cmap index because the instertion happens at the same time
    delete emap[it->first];

    ClassAd *classad = it->second;

    bool include_classad=true;
    // Check if it passed the constraint
    if (constraintExpr) {
      include_classad=false;
      classad::Value result;
      if (classad->EvaluateExpr(constraintExpr,result)) {
        if ( ! result.IsBooleanValueEquiv(include_classad)) {
          include_classad=false;
        }
      }
    }

    // if the processing function was called, and it returns false
    // then it took ownership of the ad, otherwise we need to delete it.
    if ( ! include_classad || pfnProcess(pvProcess, classad)) {
      delete classad;
    }
  }

  return true;
}
