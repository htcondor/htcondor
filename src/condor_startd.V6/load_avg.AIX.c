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

 

#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/file.h>

#include "debug.h"
#include "except.h"
static char *_FileName_ = __FILE__;		/* Used by EXCEPT (see except.h)     */

int		Kmem;



struct nlist nl[] = {
    { "avenrun" },
    { "" },
};

float
calc_load_avg()
{
	float lookup_load_avg();
	return lookup_load_avg();
}

/*
* Find addresses of various kernel variables in /dev/kmem.
*/
get_k_vars()
{
	if (getuid() == 0)
		set_root_euid();

        /* Open kmem for reading */
    if( (Kmem=open("/dev/kmem",O_RDONLY,0)) < 0 ) {
		set_condor_euid();
        EXCEPT( "open(/dev/kmem,O_RDONLY,0)" );
    }

		/* Get the kernel's name list */
	if( knlist(nl,1,sizeof(struct nlist)) < 0 ) {
		set_condor_euid();
		EXCEPT( "Can't get kernel name list" );
	}

	if (getuid() == 0)
		set_condor_euid();

}


/*
** Berkeley style kernels maintain three variables which represent load
** averages over the previous one, five, and fifteen minute intervals.
** Systems vary in whether they use traditional floating point numbers
** for this calculation, or opt for the efficiency of some type of
** scaled integer.  Here we just look up the one minute load average,
** and if necessary convert to a float.
*/

/*
** The SGI folks apparently use a scaled integer, but where is the scale
** factor defined?  This is a wild guess -- most likely wrong! -- mike
*/
#if defined(IRIX331)
#define FSCALE 1000
#endif

float
lookup_load_avg()
{
    off_t addr = nl[0].n_value;
	int		avenrun[3];
	float	one, five, fifteen;
	float	answer;

	avenrun[0] = 0;
	avenrun[1] = 0;
	avenrun[2] = 0;

    if( lseek(Kmem,addr,0) != addr ) {
        dprintf( D_ALWAYS, "Can't seek to addr 0x%x in /dev/kmem\n", addr );
        EXCEPT( "lseek" );
    }

    if( read(Kmem,(char *)avenrun,sizeof avenrun) != sizeof avenrun ) {
        EXCEPT( "read" );
    }

#if 0
	one = avenrun[0] / 65536.0;
	five = avenrun[1] / 65536.0;
	fifteen = avenrun[2] / 65536.0;
	dprintf( D_ALWAYS, "Load avg: %f %f %f\n", one, five, fifteen );
#endif

        /* just give back the 1 minute average for now */
	answer = avenrun[0] / 65536.0;
	return answer;
}
