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

class ResAttributes
{
public:
	ResAttributes(Resource *);
	~ResAttributes();

	void update( ClassAd* );	// Refresh attributes only needed for updating CM
	void timeout( ClassAd* );	// Refresh attributes needed at every timeout
	void benchmark();	// Compute kflops and mips

	float 		load()			{return r_load;};
	float		condor_load()	{return r_condor_load;};
	int			idle() 			{return r_idle;};
	int			console_idle()	{return r_console_idle;};
	unsigned long	virtmem()	{return r_virtmem;};
	unsigned long	disk()		{return r_disk;};
#if !defined(WIN32)
	char*		afs_cell();
#endif

private:
	Resource*	 	rip;
	float			r_load;
	float			r_condor_load;
	unsigned long   r_virtmem;
	unsigned long   r_disk;
	int             r_idle;
	int             r_console_idle;
	int				r_mips;
	int				r_kflops;
	int				r_last_benchmark;   // Last time we computed benchmarks
#if !defined(WIN32)
	AFS_Info*		r_afs_info;
#endif

};	

void deal_with_benchmarks( Resource* );

#endif _RES_ATTRIBUTES_H
