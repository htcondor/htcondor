/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
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

int InitializeConnection(const char * );
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

Qmgr_connection *ConnectQ(char *qmgr_location );
bool DisconnectQ(Qmgr_connection *);
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
