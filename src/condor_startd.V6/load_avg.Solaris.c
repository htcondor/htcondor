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
** Author:  Michael J. Litzkow
** 	         University of Wisconsin, Computer Sciences Dept.
** 
*/ 

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <nlist.h>
#include <kvm.h>

#include "debug.h"
#include "except.h"

struct nlist nl[] = {
    { "avenrun" }, {0}
};

int Kmem;

#define DEFAULT_LOADAVG         0.10

float lookup_load_avg();
double kvm_load_avg();

float
calc_load_avg()
{
	if (getuid() == 0)
		return (float)kvm_load_avg();
	else
		return lookup_load_avg();
}

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

/*
 *  We will try reading the kernel variables if we are executing as root.
 *  Otherwise we try uptime.  If that fails, we return a constant load
 *  average.
 */



/*
** Berkeley style kernels maintain three variables which represent load
** averages over the previous one, five, and fifteen minute intervals.
** Systems vary in whether they use traditional floating point numbers
** for this calculation, or opt for the efficiency of some type of
** scaled integer.  Here we just look up the one minute load average,
** and if necessary convert to a float.
 *  Path to the uptime program.  NULL if uptime is not accessible.
 */

static char *_FileName_ = __FILE__;     /* Used by EXCEPT (see except.h)     */

char            *uptime_path = "/usr/ucb/uptime";

float
lookup_load_avg()
{
	
	extern int      HasSigchldHandler;
	float    loadavg;
	FILE *output_fp;
	int counter;
	char word[20];

	dprintf(D_ALWAYS,"Executing as nonroot\n");
	
	
	/*  Check uptime_path.  If NULL, no point trying to use uptime.
	 *  Otherwise, we will start uptime and pipe its output to ourselves.
	 *  Then we read word by word till we get "load average".  We read the
	 *  next number.  This is the number we want.
	 */
	if (uptime_path != NULL) {
		if ((output_fp = popen(uptime_path, "r")) == NULL) {
			dprintf(D_ALWAYS, "popen(\"uptime\")");
			return DEFAULT_LOADAVG;
		}
		
		do { 
			if (fscanf(output_fp, "%s", word) == EOF) {
				dprintf(D_ALWAYS,"can't get \"average:\" from uptime\n");
				pclose(output_fp);
				return DEFAULT_LOADAVG;
			}
			dprintf(D_FULLDEBUG, "%s ", word);
			
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
					dprintf(D_FULLDEBUG,"The loadavg is %f\n",loadavg);
				}
				
				/*
				 *  Some callers of this routine may have a SIGCHLD handler.
				 *  If this is so, calling pclose will interfere withthat.
				 *  We check if this is the case and use fclose instead.
				 *  -- Ajitk
				 */
				pclose(output_fp);
				dprintf(D_FULLDEBUG, "\n");
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

double
kvm_load_avg(void)
{
	long avenrun[3];
	int i;
	int offset;
	kvm_t *kd;

	set_root_euid();
	kd = kvm_open(NULL, NULL, NULL, O_RDONLY, "condor_startd");
	set_condor_euid();

	/* test kvm_open return value */
	if (kd == NULL) {
		EXCEPT("kvm_load_avg: kvm_open");
	}

	if (kvm_nlist(kd, nl) < 0) {
		kvm_close(kd);
		EXCEPT("kvm_load_avg: kvm_nlist");
	}

	if (nl[0].n_type == 0) {
		kvm_close(kd);
		EXCEPT("kvm_load_avg: avenrun not found");
	}

	if (kvm_read(kd, nl[0].n_value, (char *) avenrun, sizeof (avenrun))
	    != sizeof (avenrun)) {
		kvm_close(kd);
		EXCEPT("kvm_load_avg: avenrun not found");
  	}

	kvm_close(kd);
	return ((double)(avenrun[2]) / FSCALE);
}

/* just adding get_k_vars to avoid runtime errors */

get_k_vars()
{
	if (getuid() == 0)
		set_root_euid(); 

        /* Open kmem for reading */
    if( (Kmem=open("/dev/kmem",O_RDONLY,0)) < 0 ) {
	if (getuid() == 0)
		set_condor_euid();
        EXCEPT( "open(/dev/kmem,O_RDONLY,0)" );
    }

	if (getuid() == 0)
		set_condor_euid();

}
