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
