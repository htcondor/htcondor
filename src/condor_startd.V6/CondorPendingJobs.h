#ifndef CONDOR_PENDING_JOBS_H
#define CONDOR_PENDING_JOBS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include "condor_io.h"

#define MAX_ALLOWED 1

#include "resource.h"
#include "condor_expressions.h"
#include "condor_classad.h"

typedef int numJobs_t;

class CondorPendingJobs
{
  private:

  class PendingJob
  {
    private:
    char* capab; // capability of job as advertized by negotiatior
    // sock and from are typically shadow particulars
    char* Client;
    char* ClientMachine;
    Sock* sock;
    struct sockaddr_in* from; // who do we respond to when we clear this job?
    resource_id_t rid; // which resource did this job request
    int AliveInterval;
    ClassAd* jobAd;
    
    public:
    PendingJob(void);
    PendingJob(resource_id_t Rid, char* cap);

    void RetClient(char*);
    void RetClientMachine(char*);
    void ScheddInterval(int&);
    void SetSock(Sock*);
    void SetRespondAddress(struct sockaddr_in*);
    void SetAliveInterval(int);
    void SetClient(const char*);
    void SetClientMachine(const char*);
    void SetJobAd(ClassAd*);
    ClassAd* JobAd(void);
    struct sockaddr_in* RetRespondAddress(void);
    resource_id_t RetResourceId(void);
    char* RetCapabString(void);
    Sock* RetSock(void);
  };

  static PendingJob* Jobs[MAX_ALLOWED]; // currently a 1 element queue
  static numJobs_t JobCount;

  static PendingJob* IdentifyJob(resource_id_t);

  public:
  enum Status {eOK, eFAIL};
  // Add a job pending for resource rid, identified by the negotiator 
  // with a capability string cap 
  static Status AddJob(resource_id_t rid, char* cap);
  static bool AreTherePendingJobs(resource_id_t rid);
  static Status StoreRespondInfo(Sock* sock, struct sockaddr_in* from,
				 resource_id_t rid);
  static Status Client(resource_id_t rid, char* client);
  static Status ClientMachine(resource_id_t rid, char* client);
  static Status DequeueJob(void);
  static Sock* RetSock(resource_id_t rid);
  static Status ScheddInterval(resource_id_t rid, int& interval);
  static char* CapabString(resource_id_t rid);
  static struct sockaddr_in* RespondAddr(resource_id_t rid);
  static Status SetClient(resource_id_t rid, const char* Client);
  static Status SetClientMachine(resource_id_t rid, const char* ClientMC);
  static Status SetAliveInterval(resource_id_t rid, int SchedInterval);
  static Status RecordJob(resource_id_t rid, ClassAd* job);
  static ClassAd* JobAd(resource_id_t rid);

};

#endif

