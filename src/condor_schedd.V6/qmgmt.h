#if !defined(_QMGMT_H)
#define _QMGMT_H

#include "condor_xdr.h"

#include "condor_classad.h"

#define NEW_BORN	1
#define DEATHS_DOOR	2

class Cluster;

class Job
{
public:
	Job(Cluster *, int);
	~Job();
	Job	*get_next();
	int get_proc() { return proc; }
	int	 SetAttribute(char *, char *);
	int  DeleteAttribute(char *);
	int  GetAttribute(char *, float *);
	int  GetAttribute(char *, int *);
	int  GetAttribute(char *, char *);
	int  GetAttributeExpr(char *, char *);
	int  FirstAttributeName(char *);
	int  NextAttributeName(char *);
	int  OwnerCheck(char *);
	void DeleteSelf() { state |= DEATHS_DOOR; }
	void Commit();
	int  Apply(const char*);
	void Rollback();
	void print();

private:
	int EvalAttribute(char *, EvalResult &);

	Job	*next;
	Job	*prev;
	ClassAd	*ad;
	ClassAd *hold_ad;
	Cluster	*cluster;
	int		proc;
	int 	state;
};


class Cluster
{
public:
	Cluster(int new_cluster_num = -1);
	~Cluster();
	int get_cluster_num() { return cluster_num; }
	Job	*get_job_list() { return job_list; }
	Cluster *get_next() { return next_cluster; }
	void SetAttribute(char *, char *);
	Job *new_job(int proc_num = -1);
	Job *FindJob(int);
	Job *FirstJob();
	void print();
	void Commit();
	int  ApplyToCluster(const char*);
	int  OwnerCheck(char*);
	void Rollback();
	void DeleteSelf() { state |= DEATHS_DOOR; }

	void clear_lookup_cache(Job *);

private:
	Job		*job_list;
	// Cache the most recently found job to speed up searches later
	Job		*last_lookup;
	Cluster	*next_cluster;
	ClassAd	*ad;
	int		cluster_num;
	int		next_proc;
	int 	state;
};

int SetNextClusterNum(int);
Cluster *FirstCluster();
Cluster *FindCluster(int);
Job *FindJob(int, int);
void PrintQ();

#if defined(__cplusplus)
extern "C" {
#endif
int ReadLog(char *);
int handle_q(XDR *, struct sockaddr_in*);
#if defined(__cplusplus)
}
#endif


#endif
