/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/* 
    This file defines the ResAttributes class which stores various
	attributes about a given resource such as load average, available
	swap space, disk space, etc.

   	Written 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/

#ifndef _RES_ATTRIBUTES_H
#define _RES_ATTRIBUTES_H

#include "condor_common.h"
#include "condor_classad.h"

class Resource;

typedef int amask_t;

const amask_t A_PUBLIC	= 1;
// no longer used: A_PRIVATE = 2
const amask_t A_STATIC	= 4;
const amask_t A_TIMEOUT	= 8;
const amask_t A_UPDATE	= 16;
const amask_t A_SHARED	= 32;
const amask_t A_SUMMED	= 64;
const amask_t A_EVALUATED = 128; 
const amask_t A_SHARED_SLOT = 256;
/*
  NOTE: We don't want A_EVALUATED in A_ALL, since it's a special bit
  that only applies to the Resource class, and it shouldn't be set
  unless we explicity ask for it.
  Same thing for A_SHARED_SLOT...
*/
const amask_t A_ALL	= (A_UPDATE | A_TIMEOUT | A_STATIC | A_SHARED | A_SUMMED);
/*
  HOWEVER: We do want to include A_EVALUATED and A_SHARED_SLOT in a
  single bitmask since there are multiple places in the startd where
  we really want *everything*, and it's lame to have to keep changing
  all those call sites whenever we add another special-case bit.
*/
const amask_t A_ALL_PUB	= (A_PUBLIC | A_ALL | A_EVALUATED | A_SHARED_SLOT);

#define IS_PUBLIC(mask)		((mask) & A_PUBLIC)
#define IS_STATIC(mask)		((mask) & A_STATIC)
#define IS_TIMEOUT(mask)	((mask) & A_TIMEOUT)
#define IS_UPDATE(mask)		((mask) & A_UPDATE)
#define IS_SHARED(mask)		((mask) & A_SHARED)
#define IS_SUMMED(mask)		((mask) & A_SUMMED)
#define IS_EVALUATED(mask)	((mask) & A_EVALUATED)
#define IS_SHARED_SLOT(mask) ((mask) & A_SHARED_SLOT)
#define IS_ALL(mask)		((mask) & A_ALL)

// This is used as a place-holder value when configuring slot shares of
// system resources.  It is later updated by dividing the remaining resources
// evenly between slots using AUTO_SHARE.
const float AUTO_SHARE = 123;

// The following avoids compiler warnings about == being dangerous
// for floats.
#define IS_AUTO_SHARE(share) ((int)share == (int)AUTO_SHARE)

// This is used as a place-holder value when configuring memory share
// for a slot.  It is later updated by dividing the remaining resources
// evenly between slots using AUTO_MEM.
const int AUTO_MEM = -123;

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

#if defined(WIN32)
		// For testing communication with the CredD, if one is
		// configured
	void credd_test();
	void reset_credd_test_throttle() { m_last_credd_test = 0; }
#endif

		// On shutdown, print one last D_IDLE debug message, so we don't
		// lose statistics when the startd is restarted.
	void final_idle_dprintf();

		// Functions to return the value of shared attributes
	int				num_cpus()	{ return m_num_cpus; };
	int				num_real_cpus()	{ return m_num_real_cpus; };
	int				phys_mem()	{ return m_phys_mem; };
	unsigned long   virt_mem()	{ return m_virt_mem; };
	float		load()			{ return m_load; };
	float		condor_load()	{ return m_condor_load; };
	time_t		keyboard_idle() { return m_idle; };
	time_t		console_idle()	{ return m_console_idle; };

private:
		// Dynamic info
	float			m_load;
	float			m_condor_load;
	float			m_owner_load;
	unsigned long   m_virt_mem;
	time_t			m_idle;
	time_t			m_console_idle;
	int				m_mips;
	int				m_kflops;
	int				m_last_benchmark;   // Last time we computed benchmarks
	time_t			m_last_keypress; 	// Last time m_idle decreased
	bool			m_seen_keypress;    // Have we seen our first keypress yet?
	int				m_clock_day;
	int				m_clock_min;
#if defined(WIN32)
	char*			m_local_credd;
	time_t			m_last_credd_test;
#endif
		// Static info
	int				m_num_cpus;
	int				m_num_real_cpus;
	int				m_phys_mem;
	char*			m_arch;
	char*			m_opsys;
	char*			m_uid_domain;
	char*			m_filesystem_domain;
	int				m_idle_interval; 	// for D_IDLE dprintf messages
	char*			m_ckptpltfrm;

#if defined ( WIN32 )
	int				m_got_windows_version_info;
	OSVERSIONINFOEX	m_window_version_info;
#endif

};	


// CPU-specific attributes.  
class CpuAttributes
{
public:
	friend class AvailAttributes;

	CpuAttributes( MachAttributes*, int slot_type, int num_cpus, 
				   int num_phys_mem, float virt_mem_fraction,
				   float disk_fraction,
				   MyString &execute_dir, MyString &execute_partition_id );

	void attach( Resource* );	// Attach to the given Resource
									   

	void publish( ClassAd*, amask_t );  // Publish desired info to given CA
	void compute( amask_t );			  // Actually recompute desired stats

		// Load average methods
	float condor_load() { return c_condor_load; };
	float owner_load() { return c_owner_load; };
	float total_load() { return c_owner_load + c_condor_load; };
	void set_owner_load( float v ) { c_owner_load = v; };

		// Keyboad activity methods
	void set_keyboard( time_t k ) { c_idle = k; };
	void set_console( time_t k ) { c_console_idle = k; };
	time_t keyboard_idle() { return c_idle; };
	time_t console_idle() { return c_console_idle; };
	int	type() { return c_type; };

	void display( amask_t );
	void dprintf( int, const char*, ... );
	void show_totals( int );

	float get_disk() { return c_disk; }
	float get_disk_fraction() { return c_disk_fraction; }
	unsigned long get_total_disk() { return c_total_disk; }
	char const *executeDir() { return c_execute_dir.Value(); }
	char const *executePartitionID() { return c_execute_partition_id.Value(); }

	CpuAttributes& operator+=( CpuAttributes& rhs);
	CpuAttributes& operator-=( CpuAttributes& rhs);

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
		// total_disk here means total disk space on the partition containing
		// this slot's execute directory.  Since each slot may have an
		// execute directory on a different partition, we have to store this
		// here in the per-slot data structure, rather than in the machine-wide
		// data structure.
	unsigned long   c_total_disk;

		// Static info
	int				c_phys_mem;
	int				c_num_cpus;

		// These hold the fractions of shared, dynamic resources
		// that are allocated to this CPU.
	float			c_virt_mem_fraction;

	float			c_disk_fraction; // share of execute dir partition
	MyString        c_execute_dir;
	MyString        c_execute_partition_id;  // unique id for partition

	int				c_type;		// The type of this resource
};	

class AvailDiskPartition
{
 public:
	AvailDiskPartition() {
		m_disk_fraction = 1.0;
		m_auto_count = 0;
	}
	float m_disk_fraction; // share of this partition that is not taken yet
	int m_auto_count; // number of slots using "auto" share of this partition
};

// Available machine-wide attributes
class AvailAttributes
{
public:
	AvailAttributes( MachAttributes* map );

	bool decrement( CpuAttributes* cap );
	bool computeAutoShares( CpuAttributes* cap );
	void show_totals( int dprintf_mask, CpuAttributes *cap );

private:
	int				a_num_cpus;
	int				a_phys_mem;
	int             a_phys_mem_auto_count; // number of slots specifying "auto"
	float			a_virt_mem_fraction;
	int             a_virt_mem_auto_count; // number of slots specifying "auto"

		// number of slots using "auto" for disk share in each partition
	HashTable<MyString,AvailDiskPartition> m_execute_partitions;

	AvailDiskPartition &GetAvailDiskPartition(MyString const &execute_partition_id);
};


void deal_with_benchmarks( Resource* );

#endif /* _RES_ATTRIBUTES_H */

