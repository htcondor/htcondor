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
 *  We will use uptime(1) to get the load average.  We will return the one
 *  minute load average by parsing its output.  This is fairly portable and
 *  doesn't require root permission.  Sample output from a mips-dec-ultrix4.3
 *
 *  example% /usr/ucb/uptime
 *    8:52pm  up 1 day, 22:28,  4 users,  load average: 0.23, 0.08, 0.01
 *
 *  The third last number is the required load average.
 */

#define DEFAULT_LOADAVG         0.10

#include "condor_common.h"
#include "condor_debug.h"

static char            *uptime_path;

/*
 *  path_to_uptime
 *
 *  Check for executable uptime is /usr/ucb and /usr/bin in that order.
 *  If uptime is found in either of these, return the full path of uptime,
 *  e.g., /usr/ucb/uptime.  Otherwise return NULL.
 */

char *path_to_uptime(void)
{
        static char upt_path[16];

        if (access("/usr/ucb/uptime", X_OK) == 0)
        {
                strcpy(upt_path, "/usr/ucb/uptime");
                return upt_path;
    }
        else if (access("/usr/bin/uptime", X_OK) == 0)
        {
                strcpy(upt_path, "/usr/bin/uptime");
                return upt_path;
    }
        else if (access("/usr/bsd/uptime", X_OK) == 0)
        {
                strcpy(upt_path, "/usr/bsd/uptime");
                return upt_path;
    }
        else
                return NULL;
}

float
lookup_load_avg_via_uptime()
{
	
	float    loadavg;
	FILE *output_fp;
	int counter;
	char word[20];

	if (uptime_path == NULL) {
		uptime_path = path_to_uptime();
	}

	/*  We start uptime and pipe its output to ourselves.
	 *  Then we read word by word till we get "load average".  We read the
	 *  next number.  This is the number we want.
	 */
	if (uptime_path != NULL) {
		if ((output_fp = popen(uptime_path, "r")) == NULL) {
//			dprintf(D_ALWAYS, "popen(\"uptime\")");
			return DEFAULT_LOADAVG;
		}
		
		do { 
			if (fscanf(output_fp, "%s", word) == EOF) {
				dprintf(D_ALWAYS,"can't get \"average:\" from uptime\n");
				pclose(output_fp);
				return DEFAULT_LOADAVG;
			}
//			dprintf(D_FULLDEBUG, "%s ", word);
			
			if (strcmp(word, "average:") == 0) {
				/*
				 *  We are at the required position.  Read in the next
				 *  floating
				 *  point number.  That is the required average.
				 */
				if (fscanf(output_fp, "%f", &loadavg) != 1) {
					dprintf(D_ALWAYS, "can't read loadavg from uptime\n");
					pclose(output_fp);
					return DEFAULT_LOADAVG;
				} else {
//					dprintf(D_FULLDEBUG,"The loadavg is %f\n",loadavg);
				}
				
				/*
				 *  Some callers of this routine may have a SIGCHLD handler.
				 *  If this is so, calling pclose will interfere withthat.
				 *  We check if this is the case and use fclose instead.
				 *  -- Ajitk
				 */
				pclose(output_fp);
//				dprintf(D_FULLDEBUG, "\n");
				return loadavg;
			}
		} while (!feof(output_fp)); 
		
		/*
		 *  Reached EOF before getting at load average!  -- Ajitk
		 */
        
		pclose(output_fp);
        
		/*EXCEPT("uptime eof");*/
	}
	
	/* not reached */
	return DEFAULT_LOADAVG;
}

