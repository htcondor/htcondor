
#include "DagMan.h"
#include "util.h"
#include "RecoveryLog.h"
#include "RecoveryLogConstants.h"

extern int DEFAULT_SLEEP;

static int
CondorIDCompare(CondorID a, CondorID b)
{
    int result;
    result = IntCompare(a.cluster, b.cluster);
    if (result == 0)
	result = IntCompare(a.proc, b.proc);
    if (result == 0)
	result = IntCompare(a.subproc, b.subproc);
    return result;
}

static int
CondorIDHash(CondorID a, int numBuckets)
{
    return BinaryHash(&a, sizeof(CondorID), numBuckets);
}

DagStatus::DagStatus(DagStatus::Code c)
{
    code = c;
    extra = NULL;
}
    
DagStatus::DagStatus()
{
    code = Ok;
    extra = NULL;
}

bool DagStatus::IsOK()
{
    return (code == Ok);
}

const char *DagStatus::GetString()
{
    switch (code)
    {
      case Ok:
	return "OK";
	break;
      case JobNotFound:
	return "Job not found";
	break;
      case BadJobState:
	return "Invalid job state";
	break;
      case AttrNotFound:
	return "Attribute not found";
	break;
      case LogInitError:
	return "Log initialization error";
	break;
      case LogReadError:
	return "Log read error";
	break;
      case LogWriteError:
	return "Log write error";
	break;
      case SubmitFailed:
	return "Condor submit failed";
	break;
      case OutOfMemory:
	return "Out of memory";
	break;
      default:
	return "Unknown error";
	break;
    }
}

void DagStatus::PrintError(FILE *fp)
{
    if (extra == NULL)
    {
	fprintf(fp, "%s", GetString());
    }
    else
    {
	fprintf(stderr, "%s: %s", GetString(), extra);
    }
}

bool operator ! (const DagStatus &s)
{
    return (s.code != DagStatus::Ok);
}

DagMan::DagMan()
{
    _condorLogName = NULL;
    _dagLogName = NULL;
    _recoveryLogName = NULL;
    _condorLogInitialized = false;
    _nJobs = 0;
    _nJobsNotReady = 0;
    _nJobsReady = 0;
    _nJobsRunning = 0;
    _nJobsDone = 0;
    _jobHT = new HashTable<CondorID, JobID>
	(101, CondorIDHash, CondorIDCompare);
}

DagStatus
DagMan::Init(const char *condorLog, const char *dagLog,
	     const char *recoveryLog, const char *username)
{
    DagStatus status;
    
    delete [] _condorLogName;
    delete [] _dagLogName;
    delete [] _recoveryLogName;
    
    _condorLogName = new char [strlen(condorLog) + 1];
    strcpy(_condorLogName, condorLog);
    _dagLogName = new char [strlen(dagLog) + 1];
    strcpy(_dagLogName, dagLog);
    _recoveryLogName = new char [strlen(recoveryLog) + 1];
    strcpy(_recoveryLogName, recoveryLog);
    
    //
    // Intialize the DAG log
    //
    unlink(_dagLogName);
    _dagLog.initialize(username, _dagLogName, 0, getpid(), 0);
    
    //
    // Hack. My condor simulation doesn't look inside the command files
    // to see the name of the log file. I have to explicitly send it
    // the name of the log file I want it to use.
    //
    // ** This code should be removed once submits are done with condor.
    //
#ifdef FAKE_SUBMIT
    int condorstatus;
    char *error;
    condorstatus = SetDefaultCondorLog(DEFAULT_CONDOR_SERVER,
				       DEFAULT_CONDOR_PORT,
				       _condorLogName, error);
    if (condorstatus != 0)
    {
	fprintf(stderr, "Error setting default condor log: %s\n", error);
	exit(1);
    }
#endif
    
    status.code = DagStatus::Ok;
    return status;
    
}

DagMan::~DagMan()
{
    delete [] _condorLogName;
    delete [] _dagLogName;
    delete [] _recoveryLogName;
}

DagStatus
DagMan::GetJobPtr(JobID id, Ship_ComplexObj *&job)
{
    DagStatus status;
    int index;

    index = (int) id;
    if (index < 0 || index >= _list.nobjects())
    {
	status.code = DagStatus::JobNotFound;
	status.extra = IntToString(index);
	return status;
    }
    
    job = (Ship_ComplexObj *) _list[index];
    if (job == NULL || strcmp("Job", job->GetType()))
    {
	status.code = DagStatus::JobNotFound;
	status.extra = IntToString(index);
	return status;
    }

    status.code = DagStatus::Ok;
    return status;
    
}

DagStatus
DagMan::SetJobAttr(JobID id, const char *attrName, const char *attrValue)
{
    DagStatus status;
    bool found;
    Ship_ComplexObj *job;

    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    found = SetObjAttr(job, attrName, attrValue);
    if (!found)
    {
	status.code = DagStatus::AttrNotFound;
	status.extra = strdup(attrName);
	return status;
    }
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::SetJobState(JobID id, const char *state)
{
    return SetJobAttr(id, "state", state);
}

DagStatus
DagMan::GetJobState(JobID id, char *&state)
{
    return GetJobAttr(id, "state", state);
}

DagStatus
DagMan::GetJobAttr(JobID id, const char *attrName, char *&attrValue)
{
    DagStatus status;
    bool found;
    Ship_ComplexObj *job;

    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    found = GetObjAttr(job, attrName, attrValue);
    if (!found)
    {
	status.code = DagStatus::AttrNotFound;
	status.extra = strdup(attrName);
	return status;
    }
    
    status.code = DagStatus::Ok;
    return status;

}

#ifndef NEW_SHIP_REP

//
// Before new shipping representation...
//
DagStatus
DagMan::AddJob(JobInfo *info, JobID &id)
{
    Ship_ComplexObj *job;
    Ship_Obj *attr;
    DagStatus status;
    
    job = new Ship_ComplexObj(&_list);
    if (job == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    job->SetType("Job");
    id = job->getid();

    status = SetJobName(id, info->name);
    if (!status)
    {
	return status;
    }
    
    attr = CreateObjAttr(job, "state", "string", "Ready");
    if (attr == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }

    attr = CreateObjAttr(job, "cmdfile", "string", info->cmdfile);
    if (attr == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    _nJobs++;
    _nJobsReady++;
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::SetJobName(JobID id, const char *name)
{
    DagStatus status;
    Ship_ComplexObj *job;
    
    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    job->SetQual((char *) name);
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::GetJobName(JobID id, char *&name)
{
    DagStatus status;
    bool found;
    Ship_ComplexObj *job;

    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    name = job->GetQual();
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::AddDependency(JobID parent, JobID child)
{
    DagStatus status;
    Ship_ComplexObj *pParent, *pChild;
    char *oldState;
    bool parentWasReady;
    
    parentWasReady = false;
    
    status = GetJobPtr(parent, pParent);
    if (!status)
    {
	return status;
    }
    
    status = GetJobPtr(child, pChild);
    if (!status)
    {
	return status;
    }
    
    status = GetJobState(parent, oldState);
    if (!status)
    {
	return status;
    }
    if (!strcmp(oldState, "Ready"))
    {
	parentWasReady = true;
    }
    
    SetJobState(parent, "NotReady");
    pParent->Create_and_set("Children", pChild);
    pChild->Create_and_set("Parents", pParent);

    if (parentWasReady)
    {
	_nJobsNotReady++;
	_nJobsReady--;
    }
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::GetChildList(JobID id, List<OBJECTINDEX> *&children)
{
    DagStatus status;
    Ship_ComplexObj *job;
    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    children = job->relatedobjs("Children");
    status.code = DagStatus::Ok;
    return status;
}

DagStatus
DagMan::GetParentList(JobID id, List<OBJECTINDEX> *&parents)
{
    DagStatus status;
    Ship_ComplexObj *job;
    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    parents = job->relatedobjs("Parents");
    status.code = DagStatus::Ok;
    return status;
}

#else

//
// Versions for new shipping representation
//
DagStatus
DagMan::AddJob(JobInfo *info, JobID &id)
{
    Ship_ComplexObj *job;
    Ship_Obj *attr;
    DagStatus status;
    
    job = new Ship_ComplexObj(&_list);
    if (job == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    job->SetType("Job");
    id = job->getid();

    attr = CreateObjAttr(job, "name", "string", info->name);
    if (attr == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }

    attr = CreateObjAttr(job, "state", "string", "Ready");
    if (attr == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }

    attr = CreateObjAttr(job, "cmdfile", "string", info->cmdfile);
    if (attr == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    _nJobs++;
    _nJobsReady++;
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::AddDependency(JobID parent, JobID child)
{
    DagStatus status;
    Ship_ComplexObj *pParent, *pChild, *pParentSet, *pChildSet;
    char *oldState;
    bool parentWasReady;
    
    parentWasReady = false;
    
    status = GetJobPtr(parent, pParent);
    if (!status)
    {
	return status;
    }
    status = GetJobPtr(child, pChild);
    if (!status)
    {
	return status;
    }
    
    //
    // Get pointer to the child set of the parent, and the parent
    // set of the child. Create the set objects if necessary.
    //
    status = GetChildSet(parent, pChildSet);
    if (!status)
    {
	return status;
    }
    if (pChildSet == NULL)
    {
	pChildSet = (Ship_ComplexObj *)
	    CreateObjAttr(pParent, "child_set", "JobSet", "");
	if (pChildSet == NULL)
	{
	    status.code = DagStatus::OutOfMemory;
	    return status;
	}
    }
    
    status = GetParentSet(child, pParentSet);
    if (!status)
    {
	return status;
    }
    if (pParentSet == NULL)
    {
	pParentSet = (Ship_ComplexObj *)
	    CreateObjAttr(pChild, "parent_set", "JobSet", "");
	if (pParentSet == NULL)
	{
	    status.code = DagStatus::OutOfMemory;
	    return status;
	}
    }
    
    //
    // Look at the current state of the parent. If it is Ready, then
    // we'll eventually have to change it to NotReady and update the
    // counts of jobs in each state.
    //
    status = GetJobState(parent, oldState);
    if (!status)
    {
	return status;
    }
    if (!strcmp(oldState, "Ready"))
    {
	parentWasReady = true;
    }
    SetJobState(parent, "NotReady");
    
    //
    // Add the child to the parent's child set
    //
    pChildSet->Create_and_set("members", pChild);
    
    //
    // Add the parent to the child's parent set
    //
    pParentSet->Create_and_set("members", pParent);
    
    if (parentWasReady)
    {
	// Update job counts
	_nJobsNotReady++;
	_nJobsReady--;
    }
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::SetJobName(JobID id, const char *name)
{
    return SetJobAttr(id, "name", name);
}

DagStatus
DagMan::GetJobName(JobID id, char *&name)
{
    return GetJobAttr(id, "name", name);
}

DagStatus
DagMan::GetChildSet(JobID id, Ship_ComplexObj *&pChildSet)
{
    DagStatus status;
    Ship_ComplexObj *job;
    List<OBJECTINDEX> *list;
    Node<OBJECTINDEX> *node;

    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    if ((list = job->relatedobjs("child_set")) == NULL)
    {
	pChildSet = NULL;
	status.code = DagStatus::Ok;
	return status;
    }
    
    if ((node = list->getheader()) == NULL)
    {
	pChildSet = NULL;
	status.code = DagStatus::Ok;
	return status;
    }

    pChildSet = (Ship_ComplexObj *) _list[node->getvalue()];
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::GetParentSet(JobID id, Ship_ComplexObj *&pParentSet)
{
    DagStatus status;
    Ship_ComplexObj *job;
    List<OBJECTINDEX> *list;
    Node<OBJECTINDEX> *node;

    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    if ((list = job->relatedobjs("parent_set")) == NULL)
    {
	pParentSet = NULL;
	status.code = DagStatus::Ok;
	return status;
    }
    
    if ((node = list->getheader()) == NULL)
    {
	pParentSet = NULL;
	status.code = DagStatus::Ok;
	return status;
    }

    pParentSet = (Ship_ComplexObj *) _list[node->getvalue()];
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::GetChildList(JobID id, List<OBJECTINDEX> *&list)
{
    DagStatus status;
    Ship_ComplexObj *obj;
    status = GetChildSet(id, obj);
    if (!status)
    {
	return status;
    }
    if (obj == NULL)
    {
	list = NULL;
	status.code = DagStatus::Ok;
	return status;
    }
    list = obj->relatedobjs("members");
    status.code = DagStatus::Ok;
    return status;
}

DagStatus
DagMan::GetParentList(JobID id, List<OBJECTINDEX> *&list)
{
    DagStatus status;
    Ship_ComplexObj *obj;
    status = GetParentSet(id, obj);
    if (!status)
    {
	return status;
    }
    if (obj == NULL)
    {
	list = NULL;
	status.code = DagStatus::Ok;
	return status;
    }
    list = obj->relatedobjs("members");
    status.code = DagStatus::Ok;
    return status;
}

#endif // NEW_SHIP_REP

DagStatus
DagMan::SetCondorLogFile(char *logfile)
{
    DagStatus status;
    delete [] _condorLogName;
    _condorLogName = new char[strlen(logfile) + 1];
    if (_condorLogName == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    strcpy(_condorLogName, logfile);    
    status.code = DagStatus::Ok;
    return status;
}

DagStatus
DagMan::SetDagLogFile(char *logfile)
{
    DagStatus status;
    delete [] _dagLogName;
    _dagLogName = new char[strlen(logfile) + 1];
    if (_dagLogName == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    strcpy(_dagLogName, logfile);    
    status.code = DagStatus::Ok;
    return status;
}

DagStatus
DagMan::RunReadyJobs(int &nJobsSubmitted)
{
    DagStatus status;
    OBJECTINDEX i;
    Ship_ComplexObj *job;
    char *state;
    
    nJobsSubmitted = 0;
    
    for (i = _list.SearchType(0, "Job");
	 i >= 0;
	 i = _list.SearchType(i + 1, "Job"), nJobsSubmitted++)
    {
	job = (Ship_ComplexObj *) _list[i];

	status = GetJobState(i, state);
	if (!status)
	{
	    return status;
	}
	
	if (strcmp(state, "Ready"))
	{
	    continue;
	}
	
	status = SubmitJob(i);
	if (!status)
	{
	    return status;
	}
	
    }
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::WriteLogEvent(const char *info)
{
    DagStatus status;
    GenericEvent e;
    strncpy(e.info, info, MAX_EVENT_STRING);
    e.info[MAX_EVENT_STRING - 1] = '\0';
    if (_dagLog.writeEvent(&e) != 1)
    {
	status.code = DagStatus::LogWriteError;
	return status;
    }
    status.code = DagStatus::Ok;
    return status;
}

DagStatus
DagMan::ProcessLogEvents()
{
    DagStatus status;
    ULogEvent *e;
    ULogEventOutcome outcome;
    bool done;
    bool found;
    JobID jobID;
    CondorID condorID;
    
    if (!_condorLogInitialized)
    {
	_condorLog.initialize(_condorLogName);
	_condorLogInitialized = true;
    }

    done = false;
    
    while (!done)
    {
	outcome = _condorLog.readEvent(e);
	switch (outcome)
	{
	  case ULOG_OK:
	    condorID.cluster = e->cluster;
	    condorID.proc = e->proc;
	    condorID.subproc = e->subproc;
	    //printf("Read an event. Type %d\n", e->eventNumber);
	    //
	    // See if the event is a TERM event. If so, conclude
	    // that the job has terminated.
	    //
	    if (e->eventNumber == ULOG_JOB_TERMINATED)
	    {
		found = _jobHT->Lookup(condorID, jobID);
		if (!found)
		{
		    fprintf(stderr,
			    "WARNING: TERM event for unknown job: %d.%d.%d\n",
			    e->cluster, e->proc, e->subproc);
		}
		else
		{
#ifdef DEBUG
		    printf("Received TERM event for job %d (%d.%d.%d)\n",
			   jobID, e->cluster, e->proc, e->subproc);
#endif

		    char *jobname;
		    GetJobName(jobID,jobname);
		    LogCondorJob *log;
		    FILE *logfp;

		    logfp = fopen(_recoveryLogName,"a");
		    log = new LogCondorJob(CONDOR_JobTerminate,e->cluster,e->proc,e->subproc,jobID,jobname);
		    log->Write(logfp);

		    fclose(logfp);
		    delete log;


		    //
		    // What if the exit status is bad? Should parent
		    // jobs still run?
		    //
		    status = TerminateJob(jobID);
		    if (!status)
		    {
			return status;
		    }
		}
		
	    }
	    break;
	    
	  case ULOG_NO_EVENT:
	    //printf("No events in condor log\n");
	    done = true;
	    break;
	    
	  case ULOG_RD_ERROR:
	  case ULOG_UNK_ERROR:
	    if (_condorLog.synchronize() != 1)
	    {
		fprintf(stderr,
			"WARNING: attempt to synchronize condor log failed\n");
	    }
	    break;
	    
	}
    }
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::SubmitJob(JobID id)
{
    DagStatus status;
    int condorstatus;
    char info[MAX_EVENT_STRING];
    Ship_ComplexObj *job;
    char *cmdfile;
    int cluster, proc, subproc;
    bool found;
    char *condorError;
    char *jobname;
    LogCondorJob *log;
    
    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    status = GetJobName(id, jobname);
    if (!status)
    {
	return status;
    }
    
    sprintf(info, "SUBMIT %s", jobname);
    status = WriteLogEvent(info); 
    if (!status)
    {
	return status;
    }
    printf("Submitting job %s to condor\n", jobname);
    fflush(stdout);
    
    found = GetObjAttr(job, "cmdfile", cmdfile);
    if (!found)
    {
	status.code = DagStatus::AttrNotFound;
	status.extra = strdup("cmdfile");
	return status;
    }

#ifdef FAKE_SUBMIT
    condorstatus = CondorSubmit_FAKE(DEFAULT_CONDOR_SERVER,
				     DEFAULT_CONDOR_PORT,
				     cmdfile, condorError,
				     cluster, proc, subproc);
#else
    condorstatus = CondorSubmit(cmdfile, condorError,
				cluster, proc, subproc);
#endif

    if (condorstatus != 0)
    {
	status.code = DagStatus::SubmitFailed;
	status.extra = strdup(condorError);
	return status;
    }
    
    sprintf(info, "CONDOR_ID %s %d.%d.%d", jobname,
	    cluster, proc, subproc);
    
    FILE *logfp;

    logfp = fopen(_recoveryLogName,"a");
    log = new LogCondorJob(CONDOR_SubmitJob,cluster,proc,subproc,id,jobname);
    log->Write(logfp);

    fclose(logfp);
    delete log;

    status = WriteLogEvent(info); 
    if (!status)
    {
	return status;
    }
    printf("Job %s has been assigned Condor ID %d.%d.%d\n", jobname,
	   cluster, proc, subproc);
    fflush(stdout);
    
    if (CreateObjAttr(job, "Cluster", cluster) == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    if (CreateObjAttr(job, "Proc", proc) == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    if (CreateObjAttr(job, "Subproc", subproc) == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    found = SetObjAttr(job, "state", "Running");
    if (!found)
    {
	status.code = DagStatus::AttrNotFound;
	status.extra = strdup("state");
	return status;
    }

    CondorID condorID = {cluster, proc, subproc};
    _jobHT->Insert(condorID, id);
    _nJobsReady--;
    _nJobsRunning++;
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::RecoverySubmitJob(JobID jobID, CondorID condorID)
{
    DagStatus status;
    Ship_ComplexObj *job;
    bool found;
    char *jobname;

    status = GetJobName(jobID, jobname);
    if (!status)
    {
	return status;
    }
    printf("Job to be submitted: %s\n",jobname);

    status = GetJobPtr(jobID, job);
    if (!status)
    {
      return status;
    }    
    if (CreateObjAttr(job, "Cluster", condorID.cluster) == NULL)
    {
      status.code = DagStatus::OutOfMemory;
      return status;
    }
    
    if (CreateObjAttr(job, "Proc", condorID.proc) == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    if (CreateObjAttr(job, "Subproc", condorID.subproc) == NULL)
    {
	status.code = DagStatus::OutOfMemory;
	return status;
    }
    
    found = SetObjAttr(job, "state", "Running");
    if (!found)
    {
	status.code = DagStatus::AttrNotFound;
	status.extra = strdup("state");
	return status;
    }

    _jobHT->Insert(condorID,jobID);
    _nJobsReady--;
    _nJobsRunning++;

    status.code = DagStatus::Ok;
    return status;
}

DagStatus
DagMan::ReportChildTermination(JobID parentID, JobID childID)
{
    DagStatus status;
    List<OBJECTINDEX> *children;
    Node<OBJECTINDEX> *node;
    Ship_ComplexObj *pParent;
    char *childState;
    char *oldState;

    //
    // If parent state is not NotReady, then there's nothing to do
    //
    status = GetJobState(parentID, oldState);
    if (!status)
    {
	return status;
    }
    if (strcmp(oldState, "NotReady") != 0)
    {
	status.code = DagStatus::Ok;
	return status;
    }
        
    //
    // Get a pointer to the parent shipping object
    //
    status = GetJobPtr(parentID, pParent);
    if (!status)
    {
	status.code = DagStatus::Ok;
	return status;
    }
    
    //
    // See if the parent has any children. (It should, or else this
    // function would never be called. But we won't consider it an 
    // error if there are no children.)
    //
    status = GetChildList(parentID, children);
    if (!status)
    {
	return status;
    }
    
    if (children == NULL)
    {
	status.code = DagStatus::Ok;
	return status;
    }

    //
    // Inspect the state of each child. If any child's state is not
    // "Done", then the parent cannot run yet. 
    //
    for (node = children->getheader();
	 node != NULL;
	 node = node->successor())
    {
	status = GetJobState(node->getvalue(), childState);
	if (!status)
	{
	    return status;
	}
	if (strcmp(childState, "Done"))
	{
	    //
	    // Some child is still not done. Can't run the parent yet.
	    //
	    status.code = DagStatus::Ok;
	    return status;
	}
    }
    
    //
    // All children have terminated. Move parent state to Ready
    //
    status = SetJobState(parentID, "Ready");
    if (!status)
    {
	return status;
    }
    _nJobsReady++;
    _nJobsNotReady--;

    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::TerminateJob(JobID id)
{
    Ship_ComplexObj *job;
    DagStatus status;
    char info[MAX_EVENT_STRING];
    List<OBJECTINDEX> *parents;
    Node<OBJECTINDEX> *node;
    char *oldState;
    char *jobname;

    status = GetJobName(id, jobname);
    if (!status)
    {
	return status;
    }
    
    //
    // Make sure job is currently running
    //
    status = GetJobState(id, oldState);
    if (!status)
    {
	return status;
    }
    if (strcmp(oldState, "Running"))
    {
	fprintf(stderr, "WARNING: TERM event for a non-running job: %s\n",
		jobname);
	fprintf(stderr, "  The event will be ignored\n");
	status.code = DagStatus::Ok;
	return status;
    }
        
    //
    // Get a pointer into the shipping object list
    //
    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    //
    // Write a TERM event into the DagMan log
    //
    sprintf(info, "TERM %s", jobname);
    status = WriteLogEvent(info);
    if (!status)
    {
	return status;
    }
    printf("Job %s has terminated\n", jobname);
    fflush(stdout);
    
    //
    // Set job state to Done
    //
    status = SetJobState(id, "Done");
    if (!status)
    {
	return status;
    }
    _nJobsRunning--;
    _nJobsDone++;
    
    //
    // Report termination to all parent jobs
    //
    status = GetParentList(id, parents);
    if (!status)
    {
	return status;
    }
    
    if (parents == NULL)
    {
	status.code = DagStatus::Ok;
	return status;
    }
    
    for (node = parents->getheader();
	 node != NULL;
	 node = node->successor())
    {
	status = ReportChildTermination(node->getvalue(), id);
	if (!status)
	{
	    return status;
	}
    }
    
    status.code = DagStatus::Ok;
    return status;

}

DagStatus
DagMan::RecoveryTerminateJob(JobID id)
{
    Ship_ComplexObj *job;
    DagStatus status;
    char info[MAX_EVENT_STRING];
    List<OBJECTINDEX> *parents;
    Node<OBJECTINDEX> *node;
    bool found;
    char *jobname;
    
    status = GetJobName(id, jobname);
    if (!status)
    {
	return status;
    }

    printf("Job to be terminated: %s",jobname);
    //
    // Get a pointer into the shipping object list
    //
    status = GetJobPtr(id, job);
    if (!status)
    {
	return status;
    }
    
    //
    // Set job state to Done
    //
    found = SetObjAttr(job,"state", "Done");
    if (!found)
    {
      status.code = DagStatus::AttrNotFound;
      status.extra = "state";
      return status;
    }
    _nJobsRunning--;
    _nJobsDone++;
    
    //
    // Report termination to all parent jobs
    //
    status = GetParentList(id, parents);
    if (!status)
    {
	return status;
    }
    
    if (parents == NULL)
    {
	status.code = DagStatus::Ok;
	return status;
    }
    
    for (node = parents->getheader();
	 node != NULL;
	 node = node->successor())
    {
	status = ReportChildTermination(node->getvalue(), id);
	if (!status)
	{
	    return status;
	}
    }
    
    status.code = DagStatus::Ok;
    return status;

}


int DagMan::NumJobs()
{
    return _nJobs;
}

int DagMan::NumJobsNotReady()
{
    return _nJobsNotReady;
}

int DagMan::NumJobsReady()
{
    return _nJobsReady;
}

int DagMan::NumJobsRunning()
{
    return _nJobsRunning;
}

int DagMan::NumJobsCompleted()
{
    return _nJobsDone;
}

Ship_ObjList *DagMan::GetShipObjList()
{
    return &_list;
}

void DagMan::Dump()
{
    printf("BEGIN DagMan\n");
    printf(" %d jobs. %d not ready, %d ready, %d running, %d done\n",
	   _nJobs, _nJobsNotReady, _nJobsReady, _nJobsRunning, _nJobsDone);
    printf("END DagMan\n");
}

bool DagMan::WaitForLogEvents()
{
    fd_set fdset;
    int nReady;
    int DEFAULT_SLEEP = 2;
    DagStatus status;
    int fd;
    
    if (!_condorLogInitialized)
    {
	_condorLog.initialize(_condorLogName);
	_condorLogInitialized = true;
    }

    //
    // Wait for new events to be written into the condor log
    //
    fd = _condorLog.getfd();
    while (fd < 0 || fd >= FD_SETSIZE)
    {
	//
	// Wake up periodically to see if fd becomes valid
	//
	fprintf(stderr, "WARNING: invalid condor log fd: %d\n", 
		fd);
	fprintf(stderr,
		"Sleeping for %d seconds to see if fd becomes valid...",
		DEFAULT_SLEEP);
	fflush(stdout);
	sleep(DEFAULT_SLEEP);
	printf("done\n");
	fflush(stdout);
	fd = _condorLog.getfd();
    }
    do
    {
	FD_ZERO(&fdset);
	FD_SET(fd, &fdset);
	fprintf(stderr, "Calling select...");
	fflush(stderr);
	nReady = select(fd + 1, &fdset, NULL, NULL, NULL);
	fprintf(stderr, "return value = %d\n", nReady);
	sleep(2);
	fflush(stderr);
    } while (nReady != 1);

    return true;
    
}

int
DagMan::RunRecovery()
{
  LogRecord *log_rec;
  LogCondorJob *condor_rec;
  CondorID  condorID;
  JobID jobID;
  int status;
  FILE *recoverfp;
  int rval;

  recoverfp = fopen(_recoveryLogName,"r");
  
  //DagManInitialize Log
  log_rec = ReadLogEntry(recoverfp);
  delete log_rec;
  
  //DagManDoneInitialize Log
  log_rec= ReadLogEntry(recoverfp);
  delete log_rec;
  

  printf("Reading the remaining logs... \n");
  /*Read all the remaining log records*/

  condor_rec = new LogCondorJob(-1,-1,-1,-1,-1,"");
  rval = condor_rec->ReadHeader(recoverfp);
  if(rval<0){
    printf("rval is less than 0.. \n");
    fclose(recoverfp);
    return 1;
  }
  condor_rec->ReadBody(recoverfp);
  condor_rec->ReadTail(recoverfp);

  printf("Entering while loop..\n");
  while(condor_rec != 0){
    
    status = condor_rec->Play(&condorID,jobID);
    printf("In while: status: %d\n",status);
    if(status == CONDOR_SubmitJob){
      RecoverySubmitJob(jobID,condorID);
      printf("Called Recovery Submit Job\n");
    }
    else{
      if(status == CONDOR_JobTerminate){
	RecoveryTerminateJob(jobID);
	printf("Called Recovery terminate job\n");
      }
      
      else{
	fclose(recoverfp);
	return 1;
      }
    }
    delete condor_rec;

    condor_rec = new LogCondorJob(-1,-1,-1,-1,-1,"");
    rval = condor_rec->ReadHeader(recoverfp);
    if(rval<0){
      printf("rval is less than 0.. \n");
      fclose(recoverfp);
      return 1;
    }
    condor_rec->ReadBody(recoverfp);
    condor_rec->ReadTail(recoverfp);
  }
  fclose(recoverfp);
  return 1;
}

