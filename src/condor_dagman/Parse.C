// Function to Parse the DagMan initialization file

#include "DagMan.h"
#include "util.h"

int
Parse(FILE *fp, DagMan *dagman)
{
  char line[MAX_LINE_LENGTH + 1];
  bool done;
  NodeID jobid, parent, child;
  bool found;
  int len;
  char *command, *arg1, *arg2, *KillJob;
  int lineNumber;
  NodeInfo *info;
  
  int status;
  
  // The input file uses names to identify jobs, and the DagMan class
  // uses integers (i.e., NodeID)
  
  done = false;
  found = false;
  lineNumber = 0;
  
  //
  // This loop will read every line of the input file
  //
  while (!done && (len = getline(fp, line, MAX_LINE_LENGTH + 1)) >= 0)
    {
      lineNumber++;
      
      //
      // Ignore blank lines
      //
      if (len == 0)
	{
	  continue;
	}
      
      //
      // Ignore comments
      //
      if (!strncmp(line, COMMENT_PREFIX, strlen(COMMENT_PREFIX)))
	{
	  continue;
	}
      
      command = strtok(line, DELIMITERS);
      
      //
      // Handle a Job command
      //
      if (!strcasecmp(command, "Job"))
	{
	  // Next token is the job name
	  arg1 = strtok(0, DELIMITERS);
	  if (arg1 == NULL)
	    {
	      fprintf(stderr, "WARNING: invalid command on line %d: %s\n",
		      lineNumber, line);
	      continue;
	    }
	  
	  // Next token is the condor command file
	  arg2 = strtok(0, DELIMITERS);
	  if (arg2 == NULL)
	    {
	      fprintf(stderr, "WARNING: invalid command on line %d: %s\n",
		      lineNumber, line);
	      continue;
	    }
	  
	  info = new NodeInfo(arg1, arg2);
	  
	  // Check if the user has pre-definied a Job as being done
	  
	  KillJob = strtok(0, DELIMITERS);
	  if ( (KillJob != NULL) && (!strcmp(KillJob, "DONE")) )
	    {
	      info->SetNodeStatus(DONE);
	    }
	  else
	    {
	      info->SetNodeStatus(READY);
	    }
	  
	  status = dagman->AddNode(*info, jobid);
	  
#ifdef DEBUG
	  printf("Back in Parse.C ...\n");
#endif
	  
	  if (status != OK)
	    {
	      fprintf(stderr, "ERROR inserting job %s: ", arg1);
	      fprintf(stderr, "\n");
	      return 1;
	    }
#ifdef DEBUG
	  fprintf(stderr, "Created job %s. command file: %s NodeName : %d\n",
		  arg1, arg2, jobid);
#endif
	  
	}
      
      //
      // Handle a dependency command
      //
      else if (!strcasecmp(command, "Dependency"))
	{
	  // Next token is the name of the parent job
	  arg1 = strtok(0, DELIMITERS);
	  if (arg1 == NULL)
	    {
	      continue;
	    }
	  
	  // Make sure the job has already been created by looking
	  // up its name in the hash table
	  
	  found = dagman->NodeLookUp(arg1, parent);
	  if (!found)
	    {
	      fprintf(stderr,
		      "Unknown job in dependency on line %d: %s\n",
		      lineNumber, arg1);
	      return 1;
	    }
	  
#ifdef DEBUG	    
	  printf("%s has NodeID %d\n", arg1, parent);
#endif
	    
	  // Each remaining token is the name of a child job
	  while ((arg2 = strtok(0, DELIMITERS)) != NULL)
	    {
	      // Verify the job by looking it up in list
	      found = dagman->NodeLookUp(arg2, child);
	      if (!found)
		{
		  fprintf(stderr,
			  "Unknown job in dependency on line %d: %s\n",
			  lineNumber, arg2);
		  return 1;
		}
	      
#ifdef DEBUG		
	      printf("%s has NodeID %d\n", arg2, child);
#endif
	      
	      // Now add a dependency to the DagMan
	      status = dagman->AddDependency(parent, child);
	      if (status != OK)
		{
		  fprintf(stderr, "ERROR adding dependency %s -> %s: ",
			  arg1, arg2);
		  fprintf(stderr, "\n");
		  return 1;
		}
	      
#ifdef DEBUG
	      fprintf(stderr, "Created dependency %s -> %s\n",
		      arg1, arg2);
#endif
	      
	    }
	  
	}
      else
	{
	  //
	    // Ignore bad commands in the input file
	    //
	    fprintf(stderr, "WARNING: invalid command on line %d: %s\n",
		    lineNumber, line);
	    continue;
	  }
	
      } // End of while loop for reading all lines in file
    
    return 0;
    
}

