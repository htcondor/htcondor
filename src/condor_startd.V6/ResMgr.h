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
    This file defines the ResMgr class, the "resource manager".  This
	object contains an array of pointers to Resource objects, one for
	each resource defined the config file.  

	Written on 10/9/97 by Derek Wright <wright@cs.wisc.edu>
*/
#ifndef _RESMGR_H
#define _RESMGR_H

typedef int (Resource::*ResourceMember)();
typedef float (Resource::*ResourceFloatMember)();
typedef void (Resource::*ResourceMaskMember)(amask_t);
typedef void (Resource::*VoidResourceMember)();
typedef int (*ComparisonFunc)(const void *, const void *);

class ResMgr : public Service
{
public:
	ResMgr();
	~ResMgr();

	void	init_socks();
	void	init_resources();

	void	compute( amask_t );
	void	publish( ClassAd*, amask_t );

	void	assign_load();
	void	assign_keyboard();

	bool 	in_use();
	bool	is_smp() { return( num_cpus() > 1 ); };
	int		num_cpus() { return m_attr->num_cpus(); };

	int		send_update( ClassAd*, ClassAd* );
	void	final_update();
	
		// Evaluate the state of all resources.
	void	eval_all();

		// Evaluate and send updates for all resources.
	void	eval_and_update_all();

		// The first one is special, since we already computed
		// everything and we don't need to recompute anything.
	void	first_eval_and_update_all();	

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

	// Hack function
	Resource*	rip() {return resources[0];};

	// Methods to control various timers
	void	check_polling();		// See if we need to poll frequently
	int		start_update_timer();	// Timer for updating the CM(s)
	int		start_poll_timer();		// Timer for polling the resources
	void	cancel_poll_timer();
	void	reset_timers();			// Reset the period on our timers,
									// in case the config has changed.

	Resource*	get_by_pid(int);		// Find rip by pid of starter
	Resource*	get_by_cur_cap(char*);	// Find rip by r_cur->capab
	Resource*	get_by_any_cap(char*);	// Find rip by r_cur or r_pre
	Resource*	get_by_name(char*);		// Find rip by r_name
	State	state();					// Return the machine state

	int	force_benchmark();		// Force a resource to benchmark
	
	void report_updates();		// Log updates w/ dprintf()

	MachAttributes*	m_attr;		// Machine-wide attribute object
	
	ProcAPI*		m_proc;		// Info from /proc about this machine 

private:
	Resource**	resources;		// Array of pointers to resource objects
	int			nresources;		// Size of the array
	SafeSock*	coll_sock;
	SafeSock*	view_sock;
	int			currentVMType;		// Current virtual machine type
		// we're parsing. 

	int		num_updates;
	int		up_tid;		// DaemonCore timer id for update timer
	int		poll_tid;	// DaemonCore timer id for polling timer

		// Builds a CpuAttributes object to represent the virtual
		// machine described by the given config file string list.  
	CpuAttributes*	build_vm( StringList* );	
	    // Returns the percentage represented by the given fraction or
		// percent string.
	float			parse_value( char* );
		// All the logic of computing an integer number of cpus out of
		// a percentage share.   
	int				compute_cpus( float share );
};

// Comparison functions for sorting resources:

// Sort on State, with Owner state resources coming first, etc.
int owner_state_cmp( const void*, const void* );

#endif _RESMGR_H
