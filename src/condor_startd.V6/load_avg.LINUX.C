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

#include "condor_common.h"
#include "condor_debug.h"

static char *_FileName_ = __FILE__;

class LoadVector {
public:
	int		Update();
	float	Short()		{ return short_avg; }
	float	Medium()	{ return medium_avg; }
	float	Long()		{ return long_avg; }
private:
	float	short_avg;
	float	medium_avg;
	float	long_avg;
};


int
LoadVector::Update()
{
    FILE	*proc;
	struct utsname buf;
	int		major, minor, patch;

	// Obtain the kernel version so that we know what /proc looks like..
	if( uname(&buf) < 0 )  return -1;
	sscanf(buf.release, "%d.%d.%d", &major, &minor, &patch);

	// /proc/loadavg looks like:

	// Kernel Version 2.0.0:
	// 0.03 0.03 0.09 2/42 15582

    proc=fopen("/proc/loadavg","r");
    if(!proc)
	return -1;

	switch(major) {
		case 1:
		case 2:
    		fscanf(proc, "%f %f %f", &short_avg, &medium_avg, &long_avg);
			break;

		default:
			dprintf(D_ALWAYS, "/proc format unknown for kernel version %d.%d.%d",
				major, minor, patch);
    		fclose(proc);
			return -1;
			break;
	}

    fclose(proc);

    dprintf(D_FULLDEBUG, "Load avg: %f %f %f\n", short_avg, medium_avg, long_avg);

    return 1;
}


extern "C" {

float
calc_load_avg()
{
	LoadVector MyLoad;
	MyLoad.Update();
	return MyLoad.Short();
}

void
get_k_vars() {}


} /* extern "C" */
