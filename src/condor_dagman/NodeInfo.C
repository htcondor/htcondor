#ifndef _NODEINFO_
#define _NODEINFO_

#include "DagMan.h"

NodeInfo::NodeInfo()
{
  // Does Nothing .... 
}

NodeInfo::~NodeInfo()
{
  printf("Inside the NodeInfo destructor ... \n");
}

NodeInfo::NodeInfo(char *NewJobName, 
		   char *CommandFileName, 
		   char *CondorLogFileName)
{
  // NodeID will be assigned when the NodeInfo class is linked to the 
  // Nodelist
  NodeName = -1; 

  cmdfile = new char[strlen(CommandFileName)+1];
  strcpy(cmdfile, CommandFileName);

  JobName =  new char[strlen(NewJobName)+1];
  strcpy(JobName, NewJobName);

  if (CondorLogFileName != NULL)
    {
      condorlog = new char[strlen(CondorLogFileName)+1];
      strcpy(condorlog, CondorLogFileName);
    }

  NodeStatus = NOT_READY;
  
  // initialize condor variables
  CondorID.cluster = -1;
  CondorID.proc = -1;
  CondorID.subproc = -1;

  // delete all elements on the 3 lists, if any
  NodeID TempID;

  incoming.Rewind();
  while (incoming.Next(TempID))
    {
      incoming.DeleteCurrent();
    }

  outgoing.Rewind();
  while (outgoing.Next(TempID))
    {
      outgoing.DeleteCurrent();
    }

  waiting.Rewind();
  while (waiting.Next(TempID))
    {
      waiting.DeleteCurrent();
    }
}

int 
NodeInfo::SetNodeID(NodeID id)
{
  NodeName = id;

  return OK;
}

int 
NodeInfo::GetNodeID(NodeID &id)
{
  id = NodeName;

  return OK;
}

int 
NodeInfo::SetCmdFile(char *NewCmdfile)
{
  cmdfile = strdup(NewCmdfile);

  return OK;
}

int 
NodeInfo::GetCmdFile(char *&GetCmdfile)
{
  GetCmdfile = strdup(cmdfile);

  return OK;
}

int 
NodeInfo::SetJobName(char *NewJobName)
{
  JobName = strdup(NewJobName);

  return OK;
}

int 
NodeInfo::GetJobName(char *&GetJobName)
{
  GetJobName = strdup(JobName);
  
  return OK;
}

int 
NodeInfo::SetCondorLogFileName(char *NewCondorLogFile)
{
  condorlog = strdup(NewCondorLogFile);

  return OK;
}

int 
NodeInfo::GetCondorLogFileName(char *&GetCondorLogFile)
{
  if (condorlog != NULL)
    {
      GetCondorLogFile = strdup(condorlog);
    }
  else
    {
      GetCondorLogFile = NULL;
    }

  return OK;
}

int 
NodeInfo::SetNodeStatus(JobStatus Nstatus)
{
  NodeStatus = Nstatus;

  return OK;
}

int 
NodeInfo::GetNodeStatus(JobStatus &Nstatus)
{
  Nstatus = NodeStatus;

  return OK;
}


int 
NodeInfo::GetCondorInfo(int &cluster, int &proc, int &subproc)
{
  cluster = CondorID.cluster;
  proc = CondorID.proc;
  subproc = CondorID.subproc;
  
  return OK;
}

int 
NodeInfo::UpdateCondorInfo(int cluster, int proc, int subproc)
{
  CondorID.cluster = cluster;
  CondorID.proc = proc;
  CondorID.subproc = subproc;

  return OK;
}

int 
NodeInfo::AddIncoming(NodeID NewJob)
{
  NodeID *NewNode = new NodeID();

  *NewNode = NewJob;

  incoming.Append(*NewNode);

  return OK;
}

int 
NodeInfo::RemoveIncoming(NodeID OldJob)
{
  if (incoming.IsEmpty())
    {
      // List is Empty
      return NOT_OK;
    }

  incoming.Rewind();
  
  NodeID CurrentNodeID;

  while(incoming.Next(CurrentNodeID))
    {
      if (CurrentNodeID == OldJob)
	{
	  incoming.DeleteCurrent();

	  return OK;
	}
    }

  // Element Not Found
  return NOT_OK;
}  

int 
NodeInfo::AddOutgoing(NodeID NewJob)
{
  NodeID *NewNode = new NodeID();

  *NewNode = NewJob;

  outgoing.Append(*NewNode);

  return OK;
}

int 
NodeInfo::RemoveOutgoing(NodeID OldJob)
{
  if (outgoing.IsEmpty())
    {
      // List is Empty
      return NOT_OK;
    }
  outgoing.Rewind();

  NodeID CurrentNodeID;
  
  while(outgoing.Next(CurrentNodeID))
    {
      if (CurrentNodeID == OldJob)
	{
	  outgoing.DeleteCurrent();
	  return OK;
	}
    }

  // Element Not Found
  return NOT_OK;
}  

int 
NodeInfo::AddWaiting(NodeID NewJob)
{
  NodeID *NewNode = new NodeID();

  *NewNode = NewJob;

  waiting.Append(*NewNode);

  return OK;
}

int 
NodeInfo::RemoveWaiting(NodeID OldJob)
{
  if (waiting.IsEmpty())
    {
      // List is Empty
      return NOT_OK;
    }
  waiting.Rewind();
  
  NodeID CurrentNodeID;

  while(waiting.Next(CurrentNodeID))
    {
      if (CurrentNodeID == OldJob)
	{
	  waiting.DeleteCurrent();
	  return OK;
	}
    }
  
  return OK;
}  

bool 
NodeInfo::IsEmpty(char *ListName)
{

  if (!strcmp(ListName, "incoming"))
    {
      // check if incoming list is empty
      return(incoming.IsEmpty());
    }

  if (!strcmp(ListName, "outgoing"))
    {
      // check if incoming list is empty
      return(outgoing.IsEmpty());
    }

  if (!strcmp(ListName, "waiting"))
    {
      // check if incoming list is empty
      return(waiting.IsEmpty());
    }

  // this is an error situation
  return FALSE;

}

void 
NodeInfo::Dump()
{
  printf("\t\t\t ***** \n");
  printf("\t\tNodeName : %d\n", NodeName);
  printf("\t\tCommand File : %s\n", cmdfile);
  printf("\t\tJobName : %s\n", JobName);
  printf("\t\tNodeStatus : ");

  switch(NodeStatus)
    {
    case READY:
      printf("READY\n");
      break;
    case SUBMITTED:
      printf("SUBMITTED\n");
      break;
    case DONE:
      printf("DONE\n");
      break;
    case NOT_READY:
      printf("NOT_READY\n");
      break;
    case RUNNING:
      printf("RUNNING\n");
      break;
    case ERROR:
      printf("ERROR\n");
      break;
    default:
      printf("unknown\n");
    }

  printf("\n");

  printf("\t\tCondor variables :\n");
  printf("\t\t\tCLUSTER: %d PROC: %d SUBPROC: %d\n",
	 CondorID.cluster, CondorID.proc, CondorID.subproc);

  // Check incoming queue

  NodeID *TempID;

  if (!waiting.IsEmpty())
    {
      printf("\t\tWaiting Queue\n");
       printf("\t\t\t");
       waiting.Rewind();

       while ( (TempID = waiting.Next()) )
	 {
	   printf("%d -> ",*TempID);
	 }
       printf("\n");
    }
  
  if (!incoming.IsEmpty())
    {
      printf("\t\tIncoming Queue\n");
      printf("\t\t\t");
      incoming.Rewind();

      while ( (TempID = incoming.Next()) )
	{
	  printf("%d -> ",*TempID);
	}
      printf("\n");
    }

  if (!outgoing.IsEmpty())
    {
      printf("\t\tOutgoing Queue\n");
      printf("\t\t\t");
      outgoing.Rewind();
      
      while ( (TempID = outgoing.Next()) )
	{
	  printf("%d -> ",*TempID);
	}
      printf("\n");
    }
  

}

#endif
