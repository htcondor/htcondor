// Function to Parse the DagMan initialization file

#include "parse.h"
#include "util.h"
#include "debug.h"
#include "list.h"

static const char   COMMENT    = '#';
static const char * DELIMITERS = " \t";
static const int    MAX_LENGTH = 255;

//---------------------------------------------------------------------------
bool parse (char *filename, Dag *dag) {

  assert (dag != NULL);
  
  FILE *fp = fopen(filename, "r");
  if (fp == NULL) {
    if (DEBUG_LEVEL(DEBUG_QUIET)) {
      printf ("%s: Could not open file %s for input",debug_progname,filename);
      perror (filename);
      return false;
    }
  }

  char line [MAX_LENGTH + 1];
  
  int lineNumber = 0;
  
  //
  // This loop will read every line of the input file
  //
  int len;
  bool done = false;

  while (!done && (len = getline(fp, line, MAX_LENGTH)) >= 0) {
    lineNumber++;
      
    if (len == 0) continue;     // Ignore blank lines
      
    //
    // Ignore comments
    //
    if (line[0] == COMMENT) continue;
    
    char *token = strtok(line, DELIMITERS);
      
    //
    // Handle a Job token
    //
    // Example Syntax is:  JOB j1 j1.condor
    //
    if (strcasecmp(token, "JOB") == 0) {

      char *jobName = strtok(NULL, DELIMITERS);
      if (jobName == NULL) {
        debug_println (DEBUG_QUIET, "%s(%d): Missing job name",
                       filename, lineNumber);
        debug_println (DEBUG_QUIET, "Example syntax is:  JOB j1 j1.condor");
        fclose(fp);
        return false;
      }
	  
      // Next token is the condor command file
      char *cmd = strtok(NULL, DELIMITERS);
      if (cmd == NULL) {
        debug_println (DEBUG_QUIET, "%s(%d): Missing condor cmd file",
                       filename, lineNumber);
        debug_println (DEBUG_QUIET, "Example syntax is:  JOB j1 j1.condor");
        continue;
      }
	  
      Job * job = new Job (jobName, cmd);
      if (job == NULL) debug_error (1, DEBUG_QUIET, "Out of Memory");
	  
      // Check if the user has pre-definied a Job as being done
	  
      char *done = strtok(0, DELIMITERS);
      if (done != NULL && strcasecmp(done, "DONE") == 0) {
        job->_Status = Job::STATUS_DONE;
      }

      if (!dag->Add (*job)) {
        if (DEBUG_LEVEL(DEBUG_QUIET)) {
          printf ("ERROR adding JOB ");
          job->Print();
          printf (" to Dag\n");
        }
        return false;
      } else if (DEBUG_LEVEL(DEBUG_DEBUG_3)) {
        printf ("%s: Added JOB: ", __FUNCTION__);
        job->Print();
        putchar('\n');
      }
    }
      
    //
    // Handle a Dependency token
    //
    // Example Syntax is:  PARENT p1 p2 p3 ... CHILD c1 c2 c3 ...
    //
    else if (strcasecmp(token, "PARENT") == 0) {

      List<Job> parents;

      char *jobName;

      while ((jobName = strtok (NULL, DELIMITERS)) != NULL &&
             strcasecmp (jobName, "CHILD") != 0) {
        Job * job = dag->GetJob(jobName);
        if (job == NULL) {
          debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                         filename, lineNumber, jobName);
          return false;
        }
        parents.Append (job);
      }

      // There must be one or more parent job names before the CHILD token
      if (parents.Number() < 1) {
        debug_println (DEBUG_QUIET, "%s(%d): Missing Parent Job names",
                       filename, lineNumber);
        debug_println (DEBUG_QUIET, "Example syntax is: "
                       "PARENT p1 p2 p3 CHILD c1 c2 c3");
        fclose(fp);
        return false;
      }

      if (jobName == NULL) {
        debug_println (DEBUG_QUIET, "%s(%d): Expected CHILD token",
                       filename, lineNumber);
        debug_println (DEBUG_QUIET, "Example syntax is: "
                       "PARENT p1 p2 p3 CHILD c1 c2 c3");
        fclose(fp);
        return false;
      }

      List<Job> children;

      while ((jobName = strtok (NULL, DELIMITERS)) != NULL) {
        Job * job = dag->GetJob(jobName);
        if (job == NULL) {
          debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                         filename, lineNumber, jobName);
          return false;
        }
        children.Append (job);
      }

      if (children.Number() < 1) {
        debug_println (DEBUG_QUIET, "%s(%d): Missing Child Job names",
                       filename, lineNumber);
        debug_println (DEBUG_QUIET, "Example syntax is: "
                       "PARENT p1 p2 p3 CHILD c1 c2 c3");
        fclose(fp);
        return false;
      }

      //
      // Now add all the dependencies
      //

      Job *parent;
      parents.Rewind();
      while ((parent = parents.Next()) != NULL) {
        Job *child;
        children.Rewind();
        while ((child = children.Next()) != NULL) {
          if (!dag->AddDependency (parent, child)) {
            debug_println (DEBUG_QUIET, "Failed to add dependency to dag");
            fclose(fp);
            return false;
          }
          if (DEBUG_LEVEL(DEBUG_DEBUG_3)) {
            printf ("%s: Added Dependency PARENT: ", __FUNCTION__);
            parent->Print();
            printf ("  CHILD: ");
            child->Print();
            putchar ('\n');
          }
        }
      }

    } else {
      //
      // Ignore bad tokens in the input file
      //
      debug_println (DEBUG_QUIET,
                     "%s(%d): Expected JOB or PARENT token",
                     filename, lineNumber);
      fclose(fp);
      return false;
    }
	
  }
  return true;
}
