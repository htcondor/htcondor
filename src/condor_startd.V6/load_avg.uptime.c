/* 
** Copyright 1986, 1987, 1988, 1989, 1990, 1991 by the Condor Design Team
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
** Author:   Dhaval Shah (?)
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

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

#include <stdio.h>
#include <unistd.h>
#include "debug.h"
#include "except.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

char            *uptime_path;

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
        else
                return NULL;
}

float
lookup_load_avg_via_uptime()
{
	
	extern int      HasSigchldHandler;
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

