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

 

/*
This file has CARMI Ops on a non-PVM environment.
CARMI_PROJECT_CONFIG:  

1. pseudo_carmi_cancel_request	- 		No sense in non-PVM
2. pseudo_carmi_config		- 		Done & Checked
3. pseudo_carmi_get_class_ad	- Raman's work reqd.
4. pseudo_carmi_addhosts	-	 	Done & Checked
5. pseudo_carmi_delhosts	- 	Done  
6. pseudo_carmi_add_notify	- 		Done & Checked
7. pseudo_carmi_delete_notify	- 	Done
8. pseudo_carmi_suspend_notify	- 	Done
9. pseudo_carmi_resume_notify	- 	Done 
10.pseudo_carmi_class_spawn	- 		Done & Checked
11.pseudo_carmi_process_info	-		Done & Checked
12.pseudo_carmi_class_definitions		Done & Checked
13.pseudo_carmi_define_class	- 		Done & Checked
14.pseudo_carmi_remove_class	- 		Done & Checked
					Warning: Don't do before ClassSpawn
					completes (asynchronous one)
15.pseudo_carmi_ckpt		- TBD 
16.pseudo_carmi_restart		- TBD 
17.pseudo_carmi_migrate		- TBD

*/
#include <carmi_ops.h>
#include <ProcList.h> 
#include <assert.h>
#include <condor_debug.h>
#include "debug.h"
#include <fileno.h>
#include <errno.h>
#include "get_daemon_addr.h"

#define MAX_HOSTS 100

int ProcCount = 0;
int ClassCount = 0;
int HostCount = 0;

ProcLIST* ProcList = NULL;
ClassLIST* ClassList = NULL;
HostLIST* HostList = NULL;
ScheddLIST* ScheddList = NULL;
SpawnLIST *SpawnList = NULL;

extern XDR *xdr_syscall;

static char *_FileName_ = __FILE__;
int _mkckpt(char *, char *);
char* gen_ckpt_name(char *, int, int, int);

int readline(int fd, char *buf)
{
  int count = 0;
  int rval;
  
  for(;;) 
    {
      if ((rval = read(fd, buf, 1)) < 0) 
	{
	  return rval;
	}
      if (*buf != '\n') 
	{
	  count++;
	  buf++;
	} 
      else 
	{
	  break;
	}
    }
        return  count;
}


/************************************************/
int pseudo_carmi_cancel_request(RequestId id, ResponseTag resp_tag)
{
	/* 1 */
  dprintf(D_ALWAYS, "CARMI_CANCEL_REQUEST(%d, %d) \n", id, resp_tag);
  
  return resp_tag;
}

/************************************************/
int pseudo_carmi_config(ResponseTag resp_tag)
{
	/* Returns nhost, HostId[] */
	/* 2 */
  int nhost;
  int rval=1;
  HostLIST *temp;
#ifdef ACPT
  AcptLIST *acptemp;
#endif

  xdr_syscall->x_op = XDR_ENCODE;

  dprintf(D_ALWAYS, "CARMI_CONFIG(%d) \n", resp_tag);

  /* Find the number of hosts */
#ifdef ACPT
  for( nhost=0, acptemp=ProcList->ClassList->AcptList; acptemp != NULL; nhost++)
	acptemp = acptemp->next;
  for(; temp=HostList; temp != NULL; nhost++)
#endif
  for(nhost=0; temp=HostList; temp != NULL; nhost++)
	temp = temp->next;


  assert( xdr_int( xdr_syscall, &rval ) );
  assert( xdr_int( xdr_syscall, &nhost ));


#ifdef ACPT
  /* Send each HostId one by one. */
  for( acptemp=ProcList->ClassList->AcptList; acptemp != NULL; 
	acptemp = acptemp->next)
  {
	assert( xdr_int(xdr_syscall, &(acptemp->id)) );
  }
#endif

  for( temp=HostList; temp != NULL; temp=temp->next)
  {
	assert( xdr_int(xdr_syscall, &(temp->host_id)) );
  }

  return 1;
}


/************************************************/
int pseudo_carmi_get_class_ad(HostId id, ResponseTag resp_tag)
{
/*
carmi_get_class_ad(HostId id, ResponseTag resp_tag)
        returns(ClassAd machine_ad)
*/

	/* 3 */
  dprintf(D_ALWAYS, "CARMI_GET_CLASS_AD(%d, %d) \n", id, resp_tag); 

  /* From schedd, do a "GetAttributeString(cluster_id, proc_id, char*attr_name,
	char *val) */

  return resp_tag;
}

/************************************************/
int pseudo_carmi_addhosts(char *class_name, int count, ResponseTag resp_tag)
{
	/* 4 */
  ClassLIST *temp = ClassList;
  HostLIST *hostnode;
  int found = 0;
  int cl;
  int pr;
  char *scheddAddr;
  XDR xdr, *xdrs = NULL;
  int sock = -1;
  int cmd;
  PROC *p;
  int i, nbytes;
  char hostname[255];
  
  
/*  AcptLIST *hostInfo = (AcptLIST *) malloc(sizeof(AcptLIST));*/
  ScheddLIST *hostInfo = (ScheddLIST *) malloc(sizeof(ScheddLIST));

  dprintf(D_ALWAYS, "CARMI_ADDHOSTS(%s, %d, %d) \n", class_name, count, resp_tag);
  
  while ((temp != NULL) && (!found))
  {
     if (!strcmp(temp->res_class.class_name, class_name))
	 found = 1;
     else
	 temp = temp->next;
  }

  if (!found)
  {
     dprintf(D_ALWAYS, "Unable to find Class %s\n", class_name);
     free(hostInfo);
     return 0;
  }

  /* contacting the scheduler here */
  ConnectQ(NULL);
  cl = hostInfo->Cluster = ProcList->proc.id.cluster;
  pr = hostInfo->Proc = NewProc(hostInfo->Cluster);
  strcpy(hostInfo->classname, temp->res_class.class_name);
 
  p = &(ProcList->proc);
   
  SetAttributeInt(cl, pr, "Universe", p->universe);                           
  SetAttributeInt(cl, pr, "Checkpoint", p->checkpoint);  
  SetAttributeInt(cl, pr, "Remote_syscalls", p->remote_syscalls);  
  SetAttributeString(cl, pr, "Owner", p->owner);  
  SetAttributeInt(cl, pr, "Q_date", p->q_date);  
  SetAttributeInt(cl, pr, "Completion_date", p->completion_date);  
  SetAttributeInt(cl, pr, "Status", UNEXPANDED);  
  SetAttributeInt(cl, pr, "Prio", p->prio);  
  SetAttributeInt(cl, pr, "Notification", p->notification);  
  SetAttributeInt(cl, pr, "Image_size", p->image_size);  /* What to do about this */
  SetAttributeString(cl, pr, "Env", p->env);  
/* these will be later reset in the spawn process */ 
  SetAttributeString(cl, pr, "Cmd", p->cmd[0]);  
  SetAttributeString(cl, pr, "Args", p->args[0]);
/*------------------------------------------------*/
  
  SetAttributeString(cl, pr, "In", p->in[0]);                                             
  SetAttributeString(cl, pr, "Out", p->out[0]);  
  SetAttributeString(cl, pr, "Err", p->err[0]);  
  SetAttributeInt(cl, pr, "Exit_status", p->exit_status[0]); 
  SetAttributeInt(cl, pr, "CurrentHosts", (p->min_needed >> 16) & 0xffff);  
  SetAttributeInt(cl, pr, "MinHosts", (p->min_needed & 0xffff));  
  SetAttributeInt(cl, pr, "MaxHosts", p->max_needed);  
  SetAttributeString(cl, pr, "Rootdir", p->rootdir);                                    
  SetAttributeString(cl, pr, "Iwd", p->iwd);
  SetAttributeExpr(cl, pr, "Requirements", ((temp->res_class.requirements[0] == '\0')? "(Machine == \"vega7\") && (Arch == \"sun4x\") && (OpSys == \"Solaris2.5\")" : temp->res_class.requirements));
  SetAttributeExpr(cl, pr, "Preferences", "TRUE"); 
  SetAttributeFloat(cl, pr, "Local_CPU", 0.0);
  SetAttributeFloat(cl, pr, "Remote_CPU", 0.0);
  
  DisconnectQ(0);
  if (pr != -1) {
		  /* send the reschedule command to the schedd */

	  if( (scheddAddr = get_schedd_addr(my_full_hostname())) == NULL ) {
		  EXCEPT( "Can't find schedd address on %s", my_full_hostname() );
	  }
      
      /* Connect to the schedd */
      if ((sock = do_connect(scheddAddr, "condor_schedd", SCHED_PORT)) < 0) {
		  dprintf(D_ALWAYS, "Can't connect to condor_schedd on %s\n", hostname);
		  free(scheddAddr);
		  return;
	  }
	  free(scheddAddr);
  
      xdrs = xdr_Init( &sock, &xdr );                                                        
      xdrs->x_op = XDR_ENCODE; 
      cmd = RESCHEDULE; 
      ASSERT( xdr_int(xdrs, &cmd));     
      ASSERT( xdrrec_endofrecord(xdrs,TRUE) );
      xdr_destroy(xdrs);
      close(sock);

      hostInfo->next = ScheddList;
      ScheddList = hostInfo;
    }
  
  return resp_tag;
}

int NewHostFound(void)
{
  char buf[80];
  AcptLIST* hostInfo;
  char hostname[255];
  ClassLIST *temp = ClassList;
  ScheddLIST *schedd = ScheddList;
  ScheddLIST *prev = NULL;
  HostLIST *hostnode;
  int found;
      
  /* now get the values back from the schedd at the stdin not sure how this is done*/
      
  dprintf(D_ALWAYS, "Now inside NEWHOSTFOUND()\n");
  hostInfo = (AcptLIST *) malloc(sizeof(AcptLIST));
  
  if (readline(fileno(stdin), buf) < 0)
    dprintf(D_ALWAYS, "Error in readline %d\n", errno);
  
      
  dprintf(D_ALWAYS, "Read in Line: %s from Readline\n", buf);
  sscanf(buf, "%s %d %d", hostname, &hostInfo->Cluster, &hostInfo->Proc);
   /*   scanf("%d", &hostInfo->Cluster);
      hostInfo->Proc = -1;
      scanf("%d", &hostInfo->Proc);*/
  dprintf(D_ALWAYS, "got from schedd hostname = %s, cluster = %d, proc = %d\n", hostname,
	  hostInfo->Cluster, hostInfo->Proc);
      
      /* having got all this stuff make the changes in the class list, host list */

  found = 0;
  while ((schedd != NULL) && (!found))
    {
     if ((schedd->Cluster == hostInfo->Cluster) &&
	 (schedd->Proc == hostInfo->Proc))
	 found = 1;
     else
       {
	 prev = schedd;
	 schedd = schedd->next;
       }
   }

  if (!found)
  {
     dprintf(D_ALWAYS, "Unable to find Schedd entry %d.%d\n", 
	     hostInfo->Cluster, hostInfo->Proc);
     free(hostInfo);
     return 0;
  }

  found = 0;
  while ((temp != NULL) && (!found))
    {
     if (!strcmp(temp->res_class.class_name, schedd->classname))
	 found = 1;
     else
	 temp = temp->next;
   }

  if (!found)
  {
     dprintf(D_ALWAYS, "Unable to find Class %s\n", schedd->classname);
     free(hostInfo);
     if (prev == NULL)
       ScheddList = schedd->next;
     else
       prev->next = schedd->next;
     free(schedd);
     return 0;
  }

  /* the thing needs to be deleted from the schedd list */
  if (prev == NULL)
    ScheddList = schedd->next;
  else
    prev->next = schedd->next;
  free(schedd);
  
  hostnode = (HostLIST *) malloc(sizeof(HostLIST));
  strcpy(hostnode->hostname, hostname);
  
  hostnode->host_id = (HostList ? HostList->host_id : 0)+1;
  hostnode->next = HostList;
  HostList = hostnode;
  
  hostInfo->id = hostnode->host_id;
  hostInfo->next = temp->hlist;
  temp->hlist = hostInfo;

  return 1;
}

  
/************************************************/
int pseudo_carmi_delhosts(int count, HostId* hosts, ResponseTag resp_tag)
{
	/* 5 */
  int i;
  int todelete=0;
  HostLIST *prev, *temp;
#ifdef ACPT
  AcptLIST *acptprev,*acpttemp;
#endif
  HostId hid, SearchHost;
  HostId HostsRemoved[100];
  int hostCount;
  
  dprintf(D_ALWAYS, "CARMI_DELHOSTS(%d, hosts, %d)\n", count, resp_tag);
  dprintf(D_ALWAYS, "\t Hosts to be deleted are : ");
  for(i = 0; i < count; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", hosts[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  for(i=0; i<count; i++)
  {
	SearchHost = hosts[i];

	/* Look for SearchHost in ProcList->ClassList->Acceptlist */
	/*
#ifdef ACPT
	acptprev = NULL;
	for( acpttemp=ProcList->ClassList->AcptList; acpttemp; 
		acpttemp=acpttemp->next)
	{
		if(temp->id == SearchHost )
			break;
		acptprev = acpttemp;
	}

	if (acpttemp != NULL)
	{
		/* TBD: Check here for existance of process on host */
		/*
		if (acptprev == NULL)
			ProcList->ClassList->AcptList = acpttemp->next;
		else
			prev->next = acpttemp->next;

		free(acpttemp);
		HostsRemoved[hostCount++] = SearchHost;
	}

	*/

#endif

			



	/* Look for SearchHost in HostList and delete it */
	prev = NULL;
	for (temp=HostList; temp != NULL; temp= temp->next)
	{
		if (temp->host_id == SearchHost )
			break;
		prev = temp;
	}

	if (temp != NULL)	/* This host is to be deleted */
	{
		/* TBD: Check here for existance of process on host */

		/* Assuming host is empty */
		if (prev == NULL)
		{
			HostList = temp->next;
		}
		else
		{
			prev->next = temp->next;
		}
		free(temp);

		/* If this was not already removed from AcptList */
		if( HostsRemoved[hostCount] != SearchHost)
			HostsRemoved[hostCount++] = SearchHost;
		else
		  dprintf(D_ALWAYS,"DelHost was present ACPT and HostList\n");
	}
  }


  return 1;
}

/************************************************/
int pseudo_carmi_add_notify(char *class_name, ResponseTag resp_tag)
{

	/* 6 */
  AcptLIST *atemp;
  ClassLIST *ctemp;
  int count;
  int hid;
  int rval = 1;
  int zero = 0;

  xdr_syscall->x_op = XDR_ENCODE;
  dprintf(D_ALWAYS, "CARMI_ADD_NOTIFY(%s, %d) \n", class_name, resp_tag);

  /* Send back all hosts added in this class till now and leave
     it to the user to decide what to do with this info.
  */

  for(ctemp=ClassList; ctemp != NULL; ctemp = ctemp->next)
  {
	if (! strcmp(ctemp->res_class.class_name, class_name))
		break;
  }

  if (ctemp == NULL)
  {
	dprintf(D_ALWAYS, "Class Name does not exist in Add_notify\n");
	assert( xdr_int( xdr_syscall, &rval ) );
	assert( xdr_int( xdr_syscall, &zero ) );
	return resp_tag;
  }
  dprintf(D_ALWAYS, "Class Name does exist in Add_notify\n");

  count = 0;
  for( atemp=ctemp->hlist; atemp!=NULL; atemp=atemp->next)
	count++;

  assert( xdr_int (xdr_syscall, &rval ) );
  assert( xdr_int (xdr_syscall, &count ) );
  for( atemp=ctemp->hlist; atemp!=NULL; atemp=atemp->next)
  {
	hid = atemp->id;
	assert( xdr_int (xdr_syscall, &hid) );
  }

  return resp_tag;
}


/************************************************/
/* Procedure used by: 
	pseudo_carmi_delete_notify,  NotifyType = 1
	pseudo_carmi_resume_notify,  NotifyType = 2
	pseudo_carmi_suspend_notify, NotifyType = 3
*/

int
CheckStatus(int count, HostId *host_ids, int NotifyType)
{
        HostLIST *host;
        HostId ToReturn[MAX_HOSTS];
        int rval = 1;
        int i, ReturnCount=0;

        dprintf(D_ALWAYS, "In CheckStatus(%d,hostids,%d)\n",
                count, NotifyType);
        for(i=0; i<count; i++)
        {
                /* Check for status of host id  host_ids[i] */
                for( host=HostList; host != NULL; host = host->next)
                {
                        if ( host->host_id == host_ids[i] )
                                break;
                }

                if (host == NULL)
                        dprintf("Host Id %d wasn't in the list when sent for
                                %d type notify\n",((int)(host_ids[i])),
				NotifyType);

                if (host != NULL && host->status == NotifyType )
                {
                        ToReturn[ReturnCount++] = host->host_id;
                }
        }
        assert( xdr_int (xdr_syscall, &rval ) );
        assert( xdr_int (xdr_syscall, &ReturnCount ) );
        for( i=0; i<ReturnCount; i++)
        {
                assert( xdr_int (xdr_syscall, &(ToReturn[i])) );
        }

        return 0;       /* Success */
}

/************************************************/
int pseudo_carmi_delete_notify(int count, HostId *host_ids,
                                                   ResponseTag resp_tag)
{
        /* 7 */
  int i;
 
  dprintf(D_ALWAYS, "IN CARMI_DELETE_NOTIFY(%d, host, %d) \n", count, resp_tag);
  dprintf(D_ALWAYS, "\t Hosts are : ");
  for(i = 0; i < count; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", host_ids[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  if ( CheckStatus( count, host_ids, 1) )
  {
        /* Some error in AppendToNotifyList */
        dprintf(D_ALWAYS, "Some ERROR happened while in AppendToNotifyList
                in carmi_ops\n");
        return -1;
  }
  return 1;
}
/************************************************/
int pseudo_carmi_suspend_notify(int count, HostId *host_ids,
                                                   ResponseTag resp_tag)
{
        /* 8 */
  int i;
 
  dprintf(D_ALWAYS, "CARMI_SUSPEND_NOTIFY(%d, host, %d) \n", count, resp_tag);
  dprintf(D_ALWAYS, "\t Hosts are : ");
  for(i = 0; i < count; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", host_ids[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  if (CheckStatus( count, host_ids, 2))
  {
        /* Some error in AppendToNotifyList */
        dprintf(D_ALWAYS, "Some ERROR happened while in AppendToNotifyList
                in carmi_ops\n");
        return 0;
  }
 
  return 1;
}

/************************************************/
int pseudo_carmi_resume_notify(int count, HostId *host_ids,
                                                   ResponseTag resp_tag)
{
        /* 9 */
  int i;

  dprintf(D_ALWAYS, "CARMI_RESUME_NOTIFY(%d, host, %d) \n", count, resp_tag);
  dprintf(D_ALWAYS, "\t Hosts are : ");
  for(i = 0; i < count; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", host_ids[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  if (CheckStatus( count, host_ids, 3))
  {
        /* Some error in AppendToNotifyList */
        dprintf(D_ALWAYS, "Some ERROR happened while in AppendToNotifyList
                in carmi_ops\n");
        return 0;
  }
  return 1;
}

/************************************************/
int pseudo_carmi_class_spawn(char *executable, char **argv, char *class_name,
                                                   ResponseTag resp_tag)
{
	/* 10 */
  int i;
  ClassLIST *temp = ClassList;
  SpawnLIST *sp_list; 
  HostLIST *hlist;
  char Cl_str[5];
  char Pr_str[5];
  int Cl;
  int Pr;
  int hostid;
  int found = 0;
  AcptLIST* hostnode;
  char arg_str[_POSIX_ARG_MAX];
  pid_t pid;

  dprintf(D_ALWAYS, "CARMI_CLASS_SPAWN(%s, argv, %s, %d) \n", executable, class_name, resp_tag);
  dprintf(D_ALWAYS, "\t Argv is : ");
  arg_str[0] = '\0';
  if (argv)
    {      
      for(i=0; argv[i] != NULL; i++)
	{     
	  dprintf(D_ALWAYS | D_NOHEADER, "%s ", argv[i]);
	  strcat(arg_str, " ");
	  strcat(arg_str, argv[i]);
	}
    }
  
  dprintf(D_ALWAYS | D_NOHEADER, "\n");
 	
  /* Find Class Name in ClassList */
  while ((temp != NULL) && (!found))
    {
      if (!strcmp(temp->res_class.class_name, class_name))
	found = 1;
      else
	temp = temp->next;
    }
  
  if (!found) 
    {
      dprintf(D_ALWAYS, "Unable to find Class %s\n", class_name);
      return 0;
    }

  hostnode = temp->hlist;
  if (!hostnode)
    {
      dprintf(D_ALWAYS, "No host added to class %s\n", class_name);
      return 0;
    }

  /* now get the new proc structure */
  hostid = hostnode->id;
  hlist = HostList;
  found = 0;
  while (!found &&(hlist != NULL))
    {
      if (hlist->host_id == hostid)
	found = 1;
      else
	  hlist = hlist->next;
    }
 
  /* here reset the name of the executable and argv */

  Cl = hostnode->Cluster;
  Pr = hostnode->Proc;
  
  sprintf(Cl_str, "%d", hostnode->Cluster);
  sprintf(Pr_str, "%d", hostnode->Proc);
  
  temp->hlist = hostnode->next;
  free(hostnode);
 
  if ((pid = fork()) < 0)
     {
       /* call an exception */
       dprintf( D_ALWAYS, "fork() failed, errno = %d\n", errno);
       return -1;
     }

  if (pid != 0)
  {
    /* this is the parent */
    sp_list = (SpawnLIST *) malloc(sizeof(SpawnLIST));
    sp_list->pid = pid;
    sp_list->hostname = hlist->hostname;
    strcpy(sp_list->Cl_str, Cl_str);
    strcpy(sp_list->Pr_str, Pr_str);
    sp_list->next = SpawnList;
    SpawnList = sp_list;
  }

  if (pid == 0)
  {
    /* entered the child here */
  
  ConnectQ(NULL);
  
  SetAttributeString(Cl, Pr, "Cmd", executable);
  SetAttributeString(Cl, Pr, "Args", arg_str);
  _mkckpt(gen_ckpt_name(0, Cl, ICKPT, Pr), 
	  executable);
  DisconnectQ(0);
  exit(2);
  /* exit from the child */
  }

/* here the object file needs to be sent */
  
 /* regular_setup(hlist->hostname, Cl_str, Pr_str);*/
  return 1;
}

/************************************************/
int pseudo_carmi_process_info(ResponseTag resp_tag)
{
/*
/* carmi_process_info( ResponseTag resp_tag)
/*         returns(int nprocs, struct procinfo proc_list[nprocs])
	/* 11 */
  int nprocs;
  int rval = 1;
  ProcLIST *temp;
  PROC *procToSend;

  xdr_syscall->x_op = XDR_ENCODE;
  dprintf(D_ALWAYS, "CARMI_PROCESS_INFO(%d) \n", resp_tag);

  temp = ProcList;
  for( nprocs=0; temp != NULL; nprocs++)
	temp = temp->next;

  dprintf(D_ALWAYS, "%d number of procs found in CARMI_PROCESS_INFO\n",
	nprocs);


  assert( xdr_int( xdr_syscall, &rval ));
  assert( xdr_int( xdr_syscall, &nprocs ));


  dprintf(D_ALWAYS, "Sending %d processes thru CARMI\n", nprocs);
  for( temp=ProcList; temp != NULL; temp=temp->next)
  {
	dprintf(D_ALWAYS, "Sending process id = %d\n", temp->proc.id.proc);
	procToSend = &(temp->proc); 
	assert( xdr_proc( xdr_syscall, &(* procToSend)) );
  }


  return 1;
}

/************************************************/
int pseudo_carmi_class_definitions(ResponseTag resp_tag)
{
	/* 12 */
	ClassLIST *temp = ClassList;
	int count = 0;
	int rval= 1;

  dprintf(D_ALWAYS, "CARMI_CLASS_DEFINITIONS(%d) \n", resp_tag);
	xdr_syscall->x_op = XDR_ENCODE;

	for(count=0, temp=ClassList; temp != NULL; count++)
		temp = temp->next;

	assert(xdr_int(xdr_syscall, &rval));

	assert(xdr_int(xdr_syscall, &count));

	for(temp=ClassList; temp; temp =temp->next)
	{
		assert(xdr_rcclass(xdr_syscall, &(temp->res_class) ));
	}

	return 1;
}

/************************************************/
int pseudo_carmi_define_class(rcclass res_class, ResponseTag resp_tag)
{
	/* 13 */
	int already_exists = 0;
	ClassLIST* temp = ClassList;    

  dprintf(D_ALWAYS, "CARMI_DEFINE_CLASS(rcclass, %d) \n", resp_tag);
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.class_num = %d\n", res_class.class_num);
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.class_name = %s\n", res_class.class_name);
  if (res_class.requirements)
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.class_requirements = %s\n", res_class.requirements);
  else
    dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.class_requirements = NULL\n");
	
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.min_needed = %d\n", res_class.min_needed);
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.max_needed = %d\n", res_class.max_needed);
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.orig_max_needed = %d\n", res_class.orig_max_needed);
  dprintf(D_ALWAYS | D_NOHEADER, "\t rcclass.current = %d\n", res_class.current);  
	
	while((temp != NULL) && (!already_exists))
	{
		if (!strcmp(res_class.class_name, temp->res_class.class_name))
			already_exists = 1;
		
		temp = temp->next;
	}

	if (already_exists)
	{
		dprintf(D_ALWAYS, " Class Already Exists \n");
		return 0;
	}

	temp = (ClassLIST *) malloc( sizeof(ClassLIST) );
	/*	temp->res_class = res_class;	*/
	temp->res_class.class_num = (ClassList == NULL) ? 1 : 
					ClassList->res_class.class_num + 1;

	temp->res_class.class_name = (char*) malloc( strlen(res_class.class_name)+1 );
	strcpy( temp->res_class.class_name, res_class.class_name);

	if (res_class.requirements)
	temp->res_class.requirements=(char*)malloc(strlen(res_class.requirements)+1);
	else
/*	  temp->res_class.requirements=(char *)malloc(strlen("")); */
	  temp->res_class.requirements=(char *)malloc(255);
	
	if (res_class.requirements)
	  strcpy( temp->res_class.requirements, res_class.requirements);
	else
	  strcpy( temp->res_class.requirements, "");
	

	temp->res_class.min_needed = res_class.min_needed;
	temp->res_class.max_needed = res_class.max_needed;
	temp->res_class.orig_max_needed = res_class.orig_max_needed;
	temp->res_class.current = 0;

	temp->hlist = NULL;
	temp->next = ClassList;
	ClassList = temp;

  return 1;
}

/************************************************/
int pseudo_carmi_remove_class(char *class_name, ResponseTag resp_tag)
{
	/* 14 */
	ClassLIST *temp, *prev;
	int found = 0;

  dprintf(D_ALWAYS, "CARMI_REMOVE_CLASS(%s, %d) \n", class_name, resp_tag);

	temp = ClassList;
	prev = NULL;

	while( temp != NULL && ! found )
	{
		if ( ! strcmp( temp->res_class.class_name, class_name ) )
		{
			found = 1;
		}
		else
		{
			prev = temp;
			temp = temp->next;
		}
	}

	if (found)
	{
		if (temp->res_class.current)
		{
			dprintf(D_ALWAYS, "Failed to remove class %s \n", class_name);
			return 0;
		}

		if (prev == NULL)	/* Head is being removed */
		{
			ClassList = ClassList->next;
		}
		else
		{
			prev->next = temp->next;
		}

		/*
		assert( temp->add_notify_list == NULL );
		*/

		free(temp->res_class.class_name);
		free(temp->res_class.requirements);
		free(temp);
	}
	else
	{
		dprintf(D_ALWAYS,"Unable to delete %s: Class Name not found\n",class_name);
		return 0;
	}

  return 1;
}

/************************************************/
int pseudo_carmi_ckpt(int ckpt_cnt, ProcessId *ckpt_list, int consistent_cnt,
                      ProcessId *consistent_list, int flush_cnt,
                      ProcessId *flush_list, HostId id)
{
	/* 15 */
  int i;
  
  dprintf(D_ALWAYS, "CARMI_CKPT(%d, ckpt_list, %d, consistent_list, %d, flush_list, %d) \n", ckpt_cnt, consistent_cnt, flush_cnt, id);
  dprintf(D_ALWAYS, "\t Ckpt List is : ");
  for(i = 0; i < ckpt_cnt; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", ckpt_list[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");
  dprintf(D_ALWAYS | D_NOHEADER, "\t Consistent List is : ");
  for(i = 0; i < consistent_cnt; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", consistent_list[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");
  dprintf(D_ALWAYS | D_NOHEADER, "\t Flush List is : ");
  for(i = 0; i < flush_cnt; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", flush_list[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  return ckpt_cnt;
}

/************************************************/
int pseudo_carmi_restart(int restart_cnt, ProcessId *restart_list, 
                         int consistent_cnt, ProcessId *consistent_list)
{
	/* 16 */
  int i;
  
  dprintf(D_ALWAYS, "CARMI_RESTART(%d, restart_list, %d, consistent_list) \n", restart_cnt, consistent_cnt);
  dprintf(D_ALWAYS, "\t Restart List is : ");
  for(i = 0; i < restart_cnt; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", restart_list[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");
  dprintf(D_ALWAYS | D_NOHEADER, "\t Consistent List is : ");
  for(i = 0; i < consistent_cnt; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", consistent_list[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  return restart_cnt;
}

/************************************************/
int pseudo_carmi_migrate(ProcessId target, HostId host, int consistent_cnt,
                      ProcessId *consistent_list)
{
	/* 17 */
  int i;
  
  dprintf(D_ALWAYS, "CARMI_MIGRATE(%d, %d, %d, consistent_list) \n", target, host, consistent_cnt);
  dprintf(D_ALWAYS, "\t Consistent List is : ");
  for(i = 0; i < consistent_cnt; i++)
    dprintf(D_ALWAYS | D_NOHEADER, "%d ", consistent_list[i]);
  dprintf(D_ALWAYS | D_NOHEADER, "\n");

  return target;
}


