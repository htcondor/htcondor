#ifndef _JOB_QUEUE_H
#define _JOB_QUEUE_H


#include "_condor_fix_resource.h"
#include "condor_xdr.h"
#include <ndbm.h>


#if defined(__cplusplus)
extern "C" {
#endif

#include "proc.h"

#if defined(__STDC__) || defined(__cplusplus) /* ANSI style prototypes */

XDR * OpenHistory ( char *file, XDR *xdrs, int *fd );
void CloseHistory ( XDR *H );
void AppendHistory ( XDR *H, PROC *p );
int ScanHistory ( XDR *H, void (*func)(PROC *) );
int LockHistory ( XDR *H, int op );
DBM * OpenJobQueue ( char *path, int flags, int mode );
int CloseJobQueue ( DBM *Q );
int LockJobQueue ( DBM *Q, int op );
int CreateCluster ( DBM *Q );
int StoreProc ( DBM *Q, PROC *proc );
int FetchProc ( DBM *Q, PROC *proc );
int TerminateCluster ( DBM *Q, int cluster, int status );
int TerminateProc ( DBM *Q, PROC_ID *pid, int status );
#if defined(__cplusplus)
int ScanJobQueue ( DBM *Q, void (*func)(PROC *) );
int ScanCluster ( DBM *Q, int cluster, void (*func)() );
#else
int ScanJobQueue ( DBM *Q, int (*func)() );
int ScanCluster ( DBM *Q, int cluster, int (*func)() );
#endif

#else	/* Non-ANSI style prototypes */

XDR * OpenHistory();
void CloseHistory();
void AppendHistory();
int ScanHistory();
int LockHistory();
DBM * OpenJobQueue();
int CloseJobQueue();
int LockJobQueue();
int CreateCluster();
int StoreProc();
int FetchProc();
int TerminateCluster();
int TerminateProc();
int ScanJobQueue();
int ScanCluster();

#endif /* non-ANSI prototypes */


#if defined(__cplusplus)
}
#endif

#endif /* _JOB_QUEUE_H */
