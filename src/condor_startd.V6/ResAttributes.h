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

typedef int amask_t;

const amask_t A_PUBLIC	= 1;
const amask_t A_PRIVATE	= 2;
const amask_t A_STATIC	= 4;
const amask_t A_TIMEOUT	= 8;
const amask_t A_UPDATE	= 16;
const amask_t A_SHARED	= 32;
const amask_t A_SUMMED	= 64;
const amask_t A_EVALUATED = 128; 
/*
  NOTE: We don't want A_EVALUATED in A_ALL, since it's a special bit
  that only applies to the Resource class, and it shouldn't be set
  unless we explicity ask for it.
*/
const amask_t A_ALL		= (A_UPDATE | A_TIMEOUT | A_STATIC | A_SHARED | A_SUMMED );

#define IS_PUBLIC(mask)		((mask) & A_PUBLIC)
#define IS_PRIVATE(mask)	((mask) & A_PRIVATE)
#define IS_STATIC(mask)		((mask) & A_STATIC)
#define IS_TIMEOUT(mask)	((mask) & A_TIMEOUT)
#define IS_UPDATE(mask)		((mask) & A_UPDATE)
#define IS_SHARED(mask)		((mask) & A_SHARED)
#define IS_SUMMED(mask)		((mask) & A_SUMMED)
#define IS_EVALUATED(mask)	((mask) & A_EVALUATED)
#define IS_ALL(mask)		((mask) & A_ALL)


// Machine-wide attributes.  
class MachAttributes
{
public:
	MachAttributes();
	~MachAttributes();

	void init();

	void publish( ClassAd*, amask_t );  // Publish desired info to given CA
	void compute( amask_t );			  // Actually recompute desired stats

		// Compute kflops and mips on the given resource
	void benchmark( Resource*, int force = 0 );	

		// On shutdown, print one last D_IDLE debug message, so we don't
		// lose statistics when the startd is restarted.
	void final_idle_dprintf();

		// Functions to return the value of shared attributes
	int				num_cpus()	{ return m_num_cpus; };
	int				phys_mem()	{ return m_phys_mem; };
	unsigned long   virt_mem()	{ return m_virt_mem; };
	unsigned long   disk()		{ return m_disk; };
	float		load()			{ return m_load; };
	float		condor_load()	{ return m_condor_load; };
	time_t		keyboard_idle() { return m_idle; };
	time_t		console_idle()	{ return m_console_idle; };
	char*		subnet()		{ return m_subnet; };

private:
		// Dynamic info
	float			m_load;
	float			m_condor_load;
	float			m_owner_load;
	unsigned long   m_virt_mem;
	unsigned long   m_disk;
	time_t			m_idle;
	time_t			m_console_idle;
	int				m_mips;
	int				m_kflops;
	int				m_last_benchmark;   // Last time we computed benchmarks
	time_t			m_last_keypress; 	// Last time m_idle decreased
	bool			m_seen_keypress;    // Have we seen our first keypress yet?
	int				m_clock_day;
	int				m_clock_min;
		// Static info
	int				m_num_cpus;
	int				m_phys_mem;
	char*			m_arch;
	char*			m_opsys;
	char*			m_uid_domain;
	char*			m_filesystem_domain;
	char*			m_subnet;
	int				m_idle_interval; 	// for D_IDLE dprintf messages
};	


// CPU-specific attributes.  
class CpuAttributes
{
public:
	friend class AvailAttributes;

	CpuAttributes( MachAttributes*, int vm_type, int num_cpus, 
				   int num_phys_mem, float virt_mem_percent,
				   float disk_percent );

	void attach( Resource* );	// Attach to the given Resource
									   

	void publish( ClassAd*, amask_t );  // Publish desired info to given CA
	void compute( amask_t );			  // Actually recompute desired stats

		// Load average methods
	float condor_load() { return c_condor_load; };
	float owner_load() { return c_owner_load; };
	void set_owner_load( float v ) { c_owner_load = v; };

		// Keyboad activity methods
	void set_keyboard( time_t k ) { c_idle = k; };
	void set_console( time_t k ) { c_console_idle = k; };
	time_t keyboard_idle() { return c_idle; };
	time_t console_idle() { return c_console_idle; };
	int	type() { return c_type; };

	void display( amask_t );
	void dprintf( int, char*, ... );
	void show_totals( int );

private:
	Resource*	 	rip;
	MachAttributes*	map;

		// Dynamic info
	float			c_condor_load;
	float			c_owner_load;
	time_t			c_idle;
	time_t			c_console_idle;
	unsigned long   c_virt_mem;
	unsigned long   c_disk;

		// Static info
	int				c_phys_mem;
	int				c_num_cpus;

		// These hold the percentages of shared, dynamic resources
		// that are allocated to this CPU.
	float			c_virt_mem_percent;
	float			c_disk_percent;

	int				c_type;		// The type of this resource
};	


// Available machine-wide attributes
class AvailAttributes
{
public:
	AvailAttributes( MachAttributes* map );

	bool decrement( CpuAttributes* cap );
	void show_totals( int );

private:
	int				a_num_cpus;
	int				a_phys_mem;
	float			a_virt_mem_percent;
	float			a_disk_percent;
};


void deal_with_benchmarks( Resource* );

#endif _RES_ATTRIBUTES_H

