/* 
** Copyright 1997 by the Condor Design Team
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the Condor Design Team not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the Condor
** Design Team make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE CONDOR DESIGN TEAM DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE CONDOR DESIGN TEAM BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
*/ 

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
