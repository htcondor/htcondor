/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

#include "condor_common.h"

#include "job.h"
#include "parse.h"
#include "util.h"
#include "debug.h"
#include "list.h"
#include "util_lib_proto.h"

static const char   COMMENT    = '#';
static const char * DELIMITERS = " \t";
static const int    MAX_LENGTH = 255;

//-----------------------------------------------------------------------------
void exampleSyntax (const char * example) {
    debug_println (DEBUG_QUIET, "Example syntax is: %s", example);
}

//-----------------------------------------------------------------------------
bool isKeyWord (char *token) {
    const unsigned int numKeyWords = 6;
    const char * keywords[numKeyWords] = {
        "JOB", "PARENT", "CHILD", "PRE", "POST", "DONE"
    };
    for (unsigned int i = 0 ; i < numKeyWords ; i++) {
        if (!strcasecmp (token, keywords[i])) return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
bool parse (char *filename, Dag *dag) {

    assert (dag != NULL);
  
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        if (DEBUG_LEVEL(DEBUG_QUIET)) {
            debug_println (DEBUG_QUIET, "Could not open file %s for input",
                           filename);
            return false;
        }
    }

    char *line;
  
    int lineNumber = 0;
  
    //
    // This loop will read every line of the input file
    //
    int len;
    bool done = false;

    while ( !done && ((line=getline(fp)) != NULL) ) {
        lineNumber++;
		if ( line ) {
			len = strlen(line);
		}

        //
        // Find the terminating '\0'
        //
        char * endline = line;
        while (*endline != '\0') endline++;

        if (len == 0) continue;            // Ignore blank lines
        if (line[0] == COMMENT) continue;  // Ignore comments

        char *token = strtok(line, DELIMITERS);
      
        //
        // Handle a Job token
        //
        // Example Syntax is:  JOB j1 j1.condor [DONE]
        //
        if (strcasecmp(token, "JOB") == 0) {
            const char * example = "JOB j1 j1.condor";

            char *jobName = strtok(NULL, DELIMITERS);
            if (jobName == NULL) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing job name",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            // The JobName cannot be a keyword
            //
            if (isKeyWord(jobName)) {
                debug_println (DEBUG_QUIET,
                               "%s(%d): JobName cannot be a keyword.",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            // Next token is the condor command file
            //
            char *cmd = strtok(NULL, DELIMITERS);
            if (cmd == NULL) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing condor cmd file",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }
	  
            Job * job = new Job (jobName, cmd);
            if (job == NULL) debug_error (1, DEBUG_QUIET, "Out of Memory");
	  
            // Check if the user has pre-definied a Job as being done
            //
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
                fclose(fp);
                return false;
            } else if (DEBUG_LEVEL(DEBUG_DEBUG_3)) {
                printf ("%s: Added JOB: ", __FUNCTION__);
                job->Print();
                putchar('\n');
            }
        }
      
        //
        // Handle a SCRIPT token
        //
        // Example Syntax is:  SCRIPT (PRE|POST) JobName ScriptName Args ...
        //
        else if ( strcasecmp(token, "SCRIPT") == 0 ) {
            const char * example = "SCRIPT (PRE|POST) JobName Script Args ...";
            Job * job = NULL;

            //
            // Second keyword is either PRE or POST
            //
            bool   post;
            char * prepost = strtok (NULL, DELIMITERS);
            if (prepost == NULL) goto MISSING_PREPOST;
            else if (!strcasecmp (prepost, "PRE" )) post = false;
            else if (!strcasecmp (prepost, "POST")) post = true;
            else {
              MISSING_PREPOST:
                debug_println (DEBUG_QUIET, "%s(%d): Expected PRE or POST",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }
            
            //
            // Third token is the JobName
            //
            char *jobName = strtok(NULL, DELIMITERS);
            if (jobName == NULL) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing job name",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            } else if (isKeyWord(jobName)) {
                debug_println (DEBUG_QUIET,
                               "%s(%d): JobName cannot be a keyword.",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            } else {
                job = dag->GetJob(jobName);
                if (job == NULL) {
                    debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                                   filename, lineNumber, jobName);
                    fclose(fp);
                    return false;
                }
            }

            //
            // Make sure this job doesn't already have a script
            //
            if (post ? job->_scriptPost != NULL : job->_scriptPre  != NULL) {
                debug_println (DEBUG_QUIET, "%s(%d): Job previously assigned "
                               "a %s script.", filename, lineNumber,
                               post ? "POST" : "PRE");
                fclose(fp);
                return false;
            }

            //
            // The rest of the line is the script and args
            //
            char * rest = jobName;
            while (*rest != '\0') rest++;
            if (rest < endline)   rest++;

            if( post ) {
				job->_scriptPost =
					new Script( post, rest, job );
				if( job->_scriptPost == NULL ) {
					debug_error( 1, DEBUG_SILENT,
								 "ERROR: out of memory (%s() in %s:%d)!\n",
								 __FUNCTION__, __FILE__, __LINE__ );
				}
			}
            else {
				job->_scriptPre =
					new Script( post, rest, job );
				if( job->_scriptPre == NULL ) {
					debug_error( 1, DEBUG_SILENT,
								 "ERROR: out of memory (%s() in %s:%d)!\n",
								 __FUNCTION__, __FILE__, __LINE__ );
				}
			}
        }

        //
        // Handle a Dependency token
        //
        // Example Syntax is:  PARENT p1 p2 p3 ... CHILD c1 c2 c3 ...
        //
        else if (strcasecmp(token, "PARENT") == 0) {
            const char * example = "PARENT p1 p2 p3 CHILD c1 c2 c3";
            
            List<Job> parents;

            char *jobName;

            while ((jobName = strtok (NULL, DELIMITERS)) != NULL &&
                   strcasecmp (jobName, "CHILD") != 0) {
                Job * job = dag->GetJob(jobName);
                if (job == NULL) {
                    debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                                   filename, lineNumber, jobName);
                    fclose(fp);
                    return false;
                }
                parents.Append (job);
            }

            // There must be one or more parent job names before
            // the CHILD token
            if (parents.Number() < 1) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing Parent Job names",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            if (jobName == NULL) {
                debug_println (DEBUG_QUIET, "%s(%d): Expected CHILD token",
                               filename, lineNumber);
                exampleSyntax (example);
                fclose(fp);
                return false;
            }

            List<Job> children;

            while ((jobName = strtok (NULL, DELIMITERS)) != NULL) {
                Job * job = dag->GetJob(jobName);
                if (job == NULL) {
                    debug_println (DEBUG_QUIET, "%s(%d): Unknown Job %s",
                                   filename, lineNumber, jobName);
                    fclose(fp);
                    return false;
                }
                children.Append (job);
            }

            if (children.Number() < 1) {
                debug_println (DEBUG_QUIET, "%s(%d): Missing Child Job names",
                               filename, lineNumber);
                exampleSyntax (example);
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
                        debug_println (DEBUG_QUIET,
                                       "Failed to add dependency to dag");
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
            // Bad token in the input file
            //
            debug_println( DEBUG_QUIET,
                           "%s(%d): Expected JOB, SCRIPT, or PARENT token",
                           filename, lineNumber );
            fclose(fp);
            return false;
        }
	
    }
    return true;
}
