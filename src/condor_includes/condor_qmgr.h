#if !defined(_LIBQMGR_H)
#define _LIBQMGR_H

#include "proc.h"
class ClassAd;


typedef struct {
	int		count;
	char	*rendevous_file;
} Qmgr_connection;

typedef int (*scan_func)(ClassAd *ad);


#if defined(__cplusplus)
extern "C" {
#endif

int InitializeConnection(char *, char *);
int NewCluster();
int NewProc( int );
int DestroyProc(int, int);
int DestroyCluster(int);
int DestroyClusterByConstraint(const char*); 
int SetAttributeByConstraint(const char *, const char *, char *);
int SetAttributeIntByConstraint(const char *, const char *, int);
int SetAttributeFloatByConstraint(const char *, const char *, float);
int SetAttributeStringByConstraint(const char *, const char *, char *);
int SetAttribute(int, int, const char *, char *);
int SetAttributeInt(int, int, const char *, int);
int SetAttributeFloat(int, int, const char *, float);
int SetAttributeString(int, int, const char *, char *);
int CloseConnection();

int GetAttributeFloat(int, int, const char *, float *);
int GetAttributeInt(int, int, const char *, int *);
int GetAttributeString(int, int, const char *, char *);
int GetAttributeExpr(int, int, const char *, char *);
int DeleteAttribute(int, int, const char *);

/* These functions return NULL on failure, and return the
   job ClassAd on success.  The caller MUST call FreeJobAd
   when the ad is no longer in use. */
ClassAd *GetJobAd(int cluster_id, int proc_id);
ClassAd *GetJobByConstraint(const char *constraint);
ClassAd *GetNextJob(int initScan);
ClassAd *GetNextJobByConstraint(const char *constraint, int initScan);
void FreeJobAd(ClassAd *&ad);

int SendSpoolFile(char *filename);		/* prepare for file xfer */
int SendSpoolFileBytes(char *filename); /* actually do file xfer */

Qmgr_connection *ConnectQ(char *qmgr_location);
void DisconnectQ(Qmgr_connection *);
void WalkJobQueue(scan_func);

void InitQmgmt();
void InitJobQueue(const char *job_queue_name);
void CleanJobQueue();

int rusage_to_float(struct rusage, float *, float *);
int float_to_rusage(float, float, struct rusage *);

/* These are here for compatibility with old code which uses the PROC
   structure to ease porting.  Use of these functions is discouraged! */
#if defined(NEW_PROC)
int SaveProc(PROC *);
int GetProc(int, int, PROC *);
#endif

#if defined(__cplusplus)
}
#endif

#define SetAttributeExpr(cl, pr, name, val) SetAttribute(cl, pr, name, val);
#define SetAttributeExprByConstraint(con, name, val) SetAttributeByConstraint(con, name, val);

#endif
