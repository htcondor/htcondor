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

#include <sys/table.h>

class LoadVector {
public:
	LoadVector();
	void	Update();
	float	Short()		{ return short_avg; }
	float	Medium()	{ return medium_avg; }
	float	Long()		{ return long_avg; }
private:
	float	short_avg;
	float	medium_avg;
	float	long_avg;
};

extern "C" {
	float calc_load_avg();
	void get_k_vars();
}


#define TESTING 0
#if TESTING
int
main()
{
	LoadVector	vect;

	for(;;) {
		printf( "load avg: %f\n", calc_load_avg() );
		sleep( 1 );
	}
	return 0;
}

#define dprintf fprintf
#define D_ALWAYS stdout
#define D_FULLDEBUG stdout

#endif /* TESTING */


static LoadVector MyLoad;

extern "C" {

float
calc_load_avg()
{
	MyLoad.Update();
	dprintf( D_FULLDEBUG, "Load avg: %f %f %f\n", 
			 MyLoad.Short(), MyLoad.Medium(), MyLoad.Long() );
	return MyLoad.Short();
}

void
get_k_vars() {}
}


LoadVector::LoadVector()
{
	Update();
}

void
LoadVector::Update()
{
	struct tbl_loadavg	avg;

	if( table(TBL_LOADAVG,0,(char *)&avg,1,sizeof(avg)) < 0 ) {
		dprintf( D_ALWAYS, "Can't get load average info from kernel\n" );
		exit( 1 );
	}

	if( avg.tl_lscale ) {
		short_avg = avg.tl_avenrun.l[0] / (float)avg.tl_lscale;
		medium_avg = avg.tl_avenrun.l[1] / (float)avg.tl_lscale;
		long_avg = avg.tl_avenrun.l[2] / (float)avg.tl_lscale;
	} else {
		short_avg = avg.tl_avenrun.d[0];
		medium_avg = avg.tl_avenrun.d[1];
		long_avg = avg.tl_avenrun.d[2];
	}
}
