#include "CondorPendingJobs.h"
#include "condor_debug.h"
#include "condor_io.h"

#include <string.h>

CondorPendingJobs::PendingJob* CondorPendingJobs::Jobs[MAX_ALLOWED];
numJobs_t CondorPendingJobs::JobCount = 0;

CondorPendingJobs::PendingJob::PendingJob()
{
  rid = NO_RID;
  capab = 0;
  sock = 0;
  from = 0;
}

void CondorPendingJobs::PendingJob::SetSock(Sock* s)
{
  sock = s;
}

void CondorPendingJobs::PendingJob::SetJobAd(ClassAd* job)
{
  jobAd = job;
}

void CondorPendingJobs::PendingJob::SetClient(const char* Cl)
{
  strcpy(Client,Cl);
}

void CondorPendingJobs::PendingJob::SetClientMachine(const char* ClM)
{
  strcpy(ClientMachine,ClM);
}

void CondorPendingJobs::PendingJob::SetAliveInterval(int AlI)
{
  AliveInterval = AlI;
}

Sock* CondorPendingJobs::PendingJob::RetSock()
{
  return sock;
}

struct sockaddr_in* CondorPendingJobs::PendingJob::RetRespondAddress()
{
  return from;
}

void CondorPendingJobs::PendingJob::SetRespondAddress(struct sockaddr_in* From)
{
  from = From;
}

void CondorPendingJobs::PendingJob::RetClient(char* client)
{
  client = strdup(Client);
}

void CondorPendingJobs::PendingJob::RetClientMachine(char* client)
{
  client = strdup(ClientMachine);
}

void CondorPendingJobs::PendingJob::ScheddInterval(int& interval)
{
  interval = AliveInterval;
}

ClassAd* CondorPendingJobs::PendingJob::JobAd()
{
  return jobAd;
}

resource_id_t CondorPendingJobs::PendingJob::RetResourceId()
{
  return rid;
}

char* CondorPendingJobs::PendingJob::RetCapabString()
{
  return capab;
}

CondorPendingJobs::PendingJob::PendingJob(resource_id_t Rid, char* cap)
{
  rid = Rid;
  capab = cap;
  sock = 0;
  from = 0;
}

CondorPendingJobs::PendingJob* CondorPendingJobs::IdentifyJob(resource_id_t rid)
{
  CondorPendingJobs::PendingJob* retval = 0;
  for(int i=0; i<JobCount; i++)
    if(Jobs[i]->RetResourceId()==rid) retval=Jobs[i];
  return retval;
}

CondorPendingJobs::Status CondorPendingJobs::AddJob(resource_id_t rid, 
						    char* cap)
{
  if(JobCount>MAX_ALLOWED) return eFAIL;
  Jobs[JobCount++] = new PendingJob(rid,cap);
  return (Jobs[JobCount])?eOK:eFAIL;
}

bool CondorPendingJobs::AreTherePendingJobs(resource_id_t rid)
{
  return IdentifyJob(rid);
}

CondorPendingJobs::Status 
CondorPendingJobs::SetClient(resource_id_t rid, 
			     const char* Client) 
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    pJob->SetClient(Client);
    return eOK;
  }
  return eFAIL;
}

CondorPendingJobs::Status 
CondorPendingJobs::SetClientMachine(resource_id_t rid, 
				    const char* ClientMC) 
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    pJob->SetClientMachine(ClientMC);
    return eOK;
  }
  return eFAIL;
}

CondorPendingJobs::Status 
CondorPendingJobs::SetAliveInterval(resource_id_t rid,
				    int SchedInterval)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
      pJob->SetAliveInterval(SchedInterval);
      return eOK;
  }
  return eFAIL;
}

CondorPendingJobs::Status 
CondorPendingJobs::StoreRespondInfo(Sock* sock, 
				    struct sockaddr_in* from,
				    resource_id_t rid)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
      pJob->SetRespondAddress(from);
      pJob->SetSock(sock);
      dprintf(D_ALWAYS,"StoredRespondInfo successfully\n");
      return eOK;
  }
  dprintf(D_ALWAYS,"StoreRespondInfo() failed\n");
  return eFAIL;
}

CondorPendingJobs::Status CondorPendingJobs::DequeueJob()
{
  if(!(JobCount>0))
  {
    dprintf(D_ALWAYS,"Couldnt dequeue job: no jobs on record\n");
    return eFAIL;
  }
  JobCount--;
  return eOK;
}

CondorPendingJobs::Status 
CondorPendingJobs::Client(resource_id_t rid, char* client)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    pJob->RetClient(client);
    return eOK;
  }
  return eFAIL;
}

CondorPendingJobs::Status 
CondorPendingJobs::ClientMachine(resource_id_t rid, char* client)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    pJob->RetClientMachine(client);
    return eOK;
  }
  return eFAIL;
}

CondorPendingJobs::Status 
CondorPendingJobs::RecordJob(resource_id_t rid, ClassAd* job)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    pJob->SetJobAd(job);
    return eOK;
  }
  return eFAIL;
}

CondorPendingJobs::Status 
CondorPendingJobs::ScheddInterval(resource_id_t rid, int& interval)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    pJob->ScheddInterval(interval);
    return eOK;
  }
  return eFAIL;
}

ClassAd* CondorPendingJobs::JobAd(resource_id_t rid)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    return pJob->JobAd();
  }
  return 0;
}

Sock* CondorPendingJobs::RetSock(resource_id_t rid)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    return pJob->RetSock();
  }
  return 0;
}

char* CondorPendingJobs::CapabString(resource_id_t rid)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    return pJob->RetCapabString();
  }
  return 0;
}

struct sockaddr_in* CondorPendingJobs::RespondAddr(resource_id_t rid)
{
  CondorPendingJobs::PendingJob* pJob;
  if(pJob=IdentifyJob(rid))
  {
    return pJob->RetRespondAddress();
  }
  return 0;
}

