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
		// Static info
	int				r_phys_mem;
#if !defined(WIN32)
	AFS_Info*		r_afs_info;
#endif
};	

void deal_with_benchmarks( Resource* );

#endif _RES_ATTRIBUTES_H

