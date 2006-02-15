/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/*
    This file defines the ResMgr class, the "resource manager".  This
	object contains an array of pointers to Resource objects, one for
	each resource defined the config file.  

	Written on 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/
#ifndef _CONDOR_RESMGR_H
#define _CONDOR_RESMGR_H

#include "simplelist.h"
#include "extArray.h"
#include "condor_classad_namedlist.h"

#include "Resource.h"
#include "claim.h"
#include "starter_mgr.h"

#if HAVE_BACKFILL
#include "backfill_mgr.h"
#if HAVE_BOINC
#include "boinc_mgr.h"
#endif /* HAVE_BOINC */
#endif /* HAVE_BACKFILL */


typedef int (Resource::*ResourceMember)();
typedef float (Resource::*ResourceFloatMember)();
typedef void (Resource::*ResourceMaskMember)(amask_t);
typedef void (Resource::*VoidResourceMember)();
typedef int (*ComparisonFunc)(const void *, const void *);


class IdDispenser
{
public:
	IdDispenser( int size, int seed );
	int		next( void );
	void	insert( int id );
private:
	ExtArray<bool> free_ids;
};

class ResMgr : public Service
{
public:
	ResMgr();
	~ResMgr();

	void	init_socks( void );
	void	init_resources( void );
	bool	reconfig_resources( void );

	void	compute( amask_t );
	void	publish( ClassAd*, amask_t );
	void	publishVmAttrs( ClassAd* );
	void    refresh_benchmarks();

	void	assign_load( void );
	void	assign_keyboard( void );

	bool 	needsPolling( void );
	bool 	hasAnyClaim( void );
	bool	is_smp( void ) { return( num_cpus() > 1 ); }
	int		num_cpus( void ) { return m_attr->num_cpus(); }
	int		num_vms( void ) { return nresources; }

	int		send_update( int, ClassAd*, ClassAd* );
	void	final_update( void );
	
		// Evaluate the state of all resources.
	void	eval_all( void );

		// Evaluate and send updates for all resources.
	void	eval_and_update_all( void );

		// The first one is special, since we already computed
		// everything and we don't need to recompute anything.
	void	first_eval_and_update_all( void );	

	// These two functions walk through the array of rip pointers and
	// call the specified function on each resource.  The first takes
	// functions that take a rip as an arg.  The second takes Resource
	// member functions that take no args.  The third takes a Resource
	// member function that takes an amask_t as its only arg.
	void	walk( int(*)(Resource*) );
	void	walk( ResourceMember );
	void	walk( VoidResourceMember );
	void	walk( ResourceMaskMember, amask_t );

	// These functions walk through the array of rip pointers, calls
	// the specified function on each one, sums the resulting return
	// values, and returns the total.
	int		sum( ResourceMember );
	float	sum( ResourceFloatMember );

	// This function walks through the array of rip pointers, calls
	// the specified function (which should return an int) on each
	// one, finds the Resource that gave the maximum value of the
	// function, and returns a pointer to that Resource.
	Resource*	res_max( ResourceMember, int* val = NULL );

	// Sort our Resource pointer array with the given comparison
	// function.  
	void resource_sort( ComparisonFunc );

	// Manipulate the supplemental Class Ad list
	int		adlist_register( const char *name );
	int		adlist_replace( const char *name, ClassAd *ad, 
							bool report_diff = false );
	int		adlist_delete( const char *name );
	int		adlist_publish( ClassAd *resAd, amask_t mask );

	// Methods to control various timers
	void	check_polling( void );	// See if we need to poll frequently
	int		start_update_timer(void); // Timer for updating the CM(s)
	int		start_poll_timer( void ); // Timer for polling the resources
	void	cancel_poll_timer( void );
	void	reset_timers( void );	// Reset the period on our timers,
									// in case the config has changed.

	Claim*		getClaimByPid( pid_t );	// Find Claim by pid of starter
	Claim*		getClaimById( const char* id );	// Find Claim by ClaimId
	Claim*		getClaimByGlobalJobId( const char* id );
	Claim*		getClaimByGlobalJobIdAndId( const char *claimId,
											const char *job_id);

	Resource*	findRipForNewCOD( ClassAd* ad );
	Resource*	get_by_cur_id(char*);	// Find rip by ClaimId of r_cur
	Resource*	get_by_any_id(char*);	// Find rip by r_cur or r_pre
	Resource*	get_by_name(char*);		// Find rip by r_name
	Resource*	get_by_vm_id(int);		// Find rip by r_id
	State		state( void );			// Return the machine state


	int	force_benchmark( void ); 	// Force a resource to benchmark
	
	void report_updates( void );	// Log updates w/ dprintf()

	MachAttributes*	m_attr;		// Machine-wide attribute object
	
	ClassAd*	totals_classad;
	ClassAd*	config_classad;

	void		init_config_classad( void );

	void		deleteResource( Resource* );

	void		makeAdList( ClassAdList* );   // Make a list of the
		// ClassAds from each VM we represent.

	void		markShutdown() { is_shutting_down = true; };
	bool		isShuttingDown() { return is_shutting_down; };

	StarterMgr starter_mgr;

#if HAVE_BACKFILL
	BackfillMgr* m_backfill_mgr;
	void backfillMgrDone();
#endif /* HAVE_BACKFILL */

	time_t	now( void ) { return cur_time; };

private:

	Resource**	resources;		// Array of pointers to Resource objects
	int			nresources;		// Size of the array

	IdDispenser* id_disp;
	bool 		is_shutting_down;

		// Current virtual machine type we're parsing. 
	int			currentVMType;		

	int		num_updates;
	int		up_tid;		// DaemonCore timer id for update timer
	int		poll_tid;	// DaemonCore timer id for polling timer
	time_t	startTime;		// Time that we started
	time_t	cur_time;		// current time

	StringList**	type_strings;	// Array of StringLists that
		// define the resource types specified in the config file.  
	int*		type_nums;		// Number of each type.
	int*		new_type_nums;	// New numbers of each type.
	int			max_types;		// Maximum # of types.

		// Data members we need to handle dynamic reconfig:
	SimpleList<CpuAttributes*>		alloc_list;		
	SimpleList<Resource*>			destroy_list;

	// List of Supplemental ClassAds to publish
	NamedClassAdList				extra_ads;

		// Builds a CpuAttributes object to represent the virtual
		// machine described by the given machine type.
	CpuAttributes*	build_vm( int type, bool except = false );	

	    // Returns the percentage represented by the given fraction or
		// percent string.
	float		parse_value( const char*, bool except = false );

		// All the logic of computing an integer number of cpus or
		// physical memory out of a percentage share.   
	int			compute_cpus( float share );
	int			compute_phys_mem( float share );

		/* 
		  Return a pointer to a newly-allocated array of CpuAttributes
		  objects of the number and types specified in the config
		  file.  You actually pass in an int for the total number of
		  CpuAttributes objects you expect to create and an array of
		  ints that holds the types you care about (which should be
		  created with countTypes()).  The array will be in
		  type-order.  Returns NULL on error.  The last argument
		  specifies if the function should EXCEPT() on error, or just
		  dprintf() and return.
		*/
	CpuAttributes** buildCpuAttrs( int total, int* type_num_array, 
								   bool except = false );

		// Initialize our type_strings array (a ResMgr data member)
		// for all VM types.   
	void initTypes( bool except = false );

		/*
		  Count the number of each VM type specified in the config
		  file.  If no type-specific entires are found, check for
		  NUM_VIRTUAL_MACHINES, and return that for type 0.  If
		  that's not defined, just use num_cpus() for type 0.  We
		  return the total number of VMs to be used, and set the given
		  array pointer to point to a newly allocated array of ints,
		  indexed by type number.  
		*/
	int countTypes( int** array_ptr, bool except = false );

 		/* 
		  Compare the two arrays of ints and see if they hold the
		  same values.  Returns true if they're the same.
		*/
	bool typeNumCmp( int* a, int* b );

		/* 
		  See if the destroy_list is empty.  If so, allocate whatever
		  we need to allocate.
		*/
	bool processAllocList( void );

	int last_in_use;	// Timestamp of the last time we were in use.

		/* 
		   Function to deal with checking if we're in use, or, if not,
		   if it's been too long since we've been used and if we need
		   to "pull the plug" and exit (based on the
		   STARTD_NOCLAIM_SHUTDOWN parameter).
		*/
	void check_use( void );

#if HAVE_BACKFILL
	bool backfillConfig( void );
	bool m_backfill_shutdown_pending;
#endif /* HAVE_BACKFILL */

};


// Comparison function for sorting resources:

// Sort on State, with Owner state resources coming first, etc.
int ownerStateCmp( const void*, const void* );

// Sort on State, with Claimed state resources coming first.  Break
// ties with the value of the Rank expression for Claimed resources.
int claimedRankCmp( const void*, const void* );

/*
  Sort resource so their in the right order to give out a new COD
  Claim.  We give out COD claims in the following order:  
  1) the Resource with the least # of existing COD claims (to ensure
     round-robin across resources
  2) in case of a tie, the Resource in the best state (owner or
     unclaimed, not claimed)
  3) in case of a tie, the Claimed resource with the lowest value of
     machine Rank for its claim
*/
int newCODClaimCmp( const void*, const void* );


#endif /* _CONDOR_RESMGR_H */
