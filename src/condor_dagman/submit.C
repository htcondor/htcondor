#include "condor_common.h" /* for <stdio.h>,<stdlib.h>,<string.h>,<unistd.h> */

//
// Local DAGMan includes
//
#include "submit.h"
#include "util.h"
#include "debug.h"

static bool submit_try (const char *exe,
                        const char * command,
                        CondorID & condorID) {
  
  FILE * fp = popen(command, "r");
  if (fp == NULL) {
    debug_println (DEBUG_QUIET, "%s: popen() failed!", command);
    return false;
  }
  
  //----------------------------------------------------------------------
  // Parse Condor's return message for Condor ID.  This desperately
  // needs to be replaced by a Condor Submit API.
  //
  // Typical condor_submit output for Condor v6 looks like:
  //
  //   Submitting job(s).
  //   Logging submit event(s).
  //   1 job(s) submitted to cluster 2267.
  //----------------------------------------------------------------------

  char buffer[UTIL_MAX_LINE_LENGTH];
  buffer[0] = '\0';
  
  // Look for the line containing the word "cluster".
  // If we get an EOF, then something went wrong with condor_submit, so
  // we return false.  The caller of this function can retry the submit
  // by repeatedly calling this function.

  do {
    if (util_getline(fp, buffer, UTIL_MAX_LINE_LENGTH) == EOF) {
      pclose(fp);
      return false;
    }
  } while (strstr(buffer, "cluster") == NULL);
  
  {
    int status = pclose(fp);
    if (status == -1) {
      perror (command);
      return false;
    }
  }

  if (1 == sscanf(buffer, "1 job(s) submitted to cluster %d",
                  & condorID._cluster)) {
//    if (DEBUG_LEVEL(DEBUG_DEBUG_2)) {
//      printf ("%s assigned condorID ", exe);
//      condorID.Print();
//      putchar('\n');
//    }
  }
  
  return true;
}

//-------------------------------------------------------------------------
bool submit_submit (const char * cmdFile, CondorID & condorID) {
  // the '-p' parameter to condor_submit will now cause condor_submit
  // to pause ~4 seconds after a successfull submit.  this prevents
  // the race condition of condor_submit finishing before dagman
  // does a pclose, which at least on SGI IRIX causes a nasty 'Broken Pipe'
  // [joev] - took this out on Todd's advice ....
  
  // char * exe = "condor_submit -p";
  const char * exe = "condor_submit";
  
  // 'subproc' is not used in newer versions of condor Version 6 doesn't
  // use 'proc' either.  We set this to zero for now
  
  char * command = new char[strlen(exe) + strlen(cmdFile) + 10];
  if (command == NULL) {
    debug_error (1, DEBUG_QUIET, "%s %s: Out of memory", exe, cmdFile);
  }
  sprintf(command, "%s %s 2>&1", exe, cmdFile);
  
  bool success = false;
  const int tries = 6;
  
  for (int i = 1 ; i <= tries && !success ; i++) {
    success = submit_try (exe, command, condorID);
    if (!success) {
      const int wait = 10;
      debug_println (DEBUG_NORMAL, "condor_submit try %d/%d failed, "
                     "will try again in %d seconds", i, tries, wait);
      sleep(wait);
    }
  }
  if (!success && DEBUG_LEVEL(DEBUG_QUIET)) {
    printf ("condor_submit failed after %d tries.\n", tries);
    printf ("Submit command was: %s\n", command);
  }
  delete [] command;
  return success;
}

