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
    This file defines the ResAttributes class which stores various
	attributes about a given resource such as load average, available
	swap space, disk space, etc.

   	Written 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _RES_ATTRIBUTES_H
#define _RES_ATTRIBUTES_H

#if !defined(WIN32)
#include "afs.h"
#endif

enum ResAttr_t { UPDATE, TIMEOUT, ALL, PUBLIC };

class ResAttributes
{
public:
	ResAttributes( Resource* );
	~ResAttributes();

	void init( ClassAd* );		// Initialize all static info into classad
	void refresh( ClassAd*, ResAttr_t );  // Refresh given CA w/ desired info
	void compute( ResAttr_t );		// Actually recompute desired stats.  
	void benchmark();			// Compute kflops and mips

private:
	Resource*	 	rip;
		// Dynamic info
	float			r_load;
	float			r_condor_load;
	unsigned long   r_virtmem;
	unsigned long   r_disk;
	time_t			r_idle;
	time_t			r_console_idle;
	int				r_mips;
	int				r_kflops;
	int				r_last_benchmark;   // Last time we computed benchmarks
	int				r_clock_day;
	int				r_clock_min;
		// Static info
	int				r_phys_mem;
#if !defined(WIN32)
	AFS_Info*		r_afs_info;
#endif
};	

void deal_with_benchmarks( Resource* );

#endif _RES_ATTRIBUTES_H

