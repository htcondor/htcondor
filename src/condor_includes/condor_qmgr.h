#if !defined(_LIBQMGR_H)
#define _LIBQMGR_H

#include "proc.h"
class ClassAd;
class ClassAdList;


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
int SetAttribute(int, int, const char *, char *);
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
void FreeJobAd(ClassAd *ad);

int SendSpoolFile(char *filename);		/* prepare for file xfer */
int SendSpoolFileBytes(char *filename); /* actually do file xfer */

Qmgr_connection *ConnectQ(char *qmgr_location);
void DisconnectQ(Qmgr_connection *);
void WalkJobQueue(scan_func);

void InitJobQueue(const char *job_queue_name);

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

#define SetAttributeInt(cl, pr, name, val){\
										   char buf[100]; \
										   sprintf(buf, "%d", val); \
										   SetAttribute(cl, pr, name, buf);\
									   }
#define SetAttributeFloat(cl, pr, name, val){\
										   char buf[100]; \
										   sprintf(buf, "%f", val); \
										   SetAttribute(cl, pr, name, buf);\
									   }
#define SetAttributeString(cl, pr, name, val){\
										   char buf[1000]; \
										   sprintf(buf, "\"%s\"", val); \
										   SetAttribute(cl, pr, name, buf);\
									   }
#define SetAttributeExpr(cl, pr, name, val) SetAttribute(cl, pr, name, val);
#endif
