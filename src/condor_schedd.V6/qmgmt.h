#if !defined(_QMGMT_H)
#define _QMGMT_H

#include "condor_classad.h"
#include "condor_io.h"

#define NEW_BORN	1
#define DEATHS_DOOR	2

class Cluster;

class Job
{
public:
	Job(Cluster *, int);
	~Job();
	Job *get_next();
	int  get_proc() { return proc; }
	int	 SetAttribute(const char *, char *);
	int  DeleteAttribute(const char *);
	int  GetAttribute(const char *, float *);
	int  GetAttribute(const char *, int *);
	int  GetAttribute(const char *, char *);
	int  GetAttributeExpr(const char *, char *);
	int  FirstAttributeName(char *);
	int  NextAttributeName(char *);
	int  OwnerCheck(char *);
	void DeleteSelf() { state |= DEATHS_DOOR; }
	void Commit();
	int  Apply(const char*);
	void Rollback();
	void print();

private:
	int EvalAttribute(const char *, EvalResult &);

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
class Service;

#if defined(__cplusplus)
extern "C" {
#endif
int ReadLog(char *);
int handle_q(Service *, int, Stream *sock);
#if defined(__cplusplus)
}
#endif

#endif /* _QMGMT_H */
