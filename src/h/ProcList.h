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

 

#include <stdio.h>
#include <proc.h>
#include <rcclass.h>
#include <carmi_ops.h>

#include <sched.h>
#include <condor_network.h>
#include <condor_qmgr.h>

#include <condor_xdr.h>
#include <xdr_lib.h>


typedef struct NLIST
{
	HostId host_id;
	struct NLIST *next;
}	NotifyLIST;


typedef struct PLIST
{
	PROC proc;
	int  procId;
	char CkptName[ 255  ];
	char ICkptName[ 255  ];  
	char RCkptName[ 255  ]; 
	char TmpCkptName[ 255 ]; 
	char hostname[255];
	int client_log;
	int rsc_sock;
	XDR *xdr_RSC1;

	/* No longer need this because of polling */
	/*
	NotifyLIST *DelNotifyList;
	NotifyLIST *ResNotifyList;
	NotifyLIST *SusNotifyList;
	*/

	struct PLIST* next;	
}	ProcLIST;


typedef struct ACPTLIST
{
	HostId id;
	int Cluster;
	int Proc;
	struct ACPTLIST *next;
}	AcptLIST;

typedef struct CLIST
{
	rcclass res_class;
	AcptLIST* hlist;
	struct CLIST* next;
}	ClassLIST;

typedef struct HLIST
{
	char hostname[255];
	HostId host_id;
	int 	status;		/* ADDED RECENTLY */
		/* Contains: Added, Deleted, Suspended,
			Resumed --> Changed to this when state changes from
				Suspended */
	struct HLIST* next;
}	HostLIST;

typedef struct SCHEDDLIST
{
        int Cluster;
	int Proc;
	char classname[20];
	struct SCHEDDLIST* next;
}       ScheddLIST;

typedef struct SPAWNLIST
{
	pid_t pid;
	char *hostname;
	char Cl_str[5];
	char Pr_str[5];
	struct SPAWNLIST *next;
}	SpawnLIST;
/* Note:
Structure of RCClass:

        int class_num;
        char *class_name;
        char *requirements;
        int min_needed;
        int max_needed;
        int orig_max_needed;
        int current;

*/
