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
#include "startd.h"

ResMgr::ResMgr()
{
	coll_sock = NULL;
	view_sock = NULL;
	totals_classad = NULL;
	up_tid = -1;
	poll_tid = -1;

	m_proc = new ProcAPI;
	m_attr = new MachAttributes;
	m_avail = new AvailAttributes( m_attr );

	nresources = 0;
	resources = NULL;
}


ResMgr::~ResMgr()
{
	int i;
	delete m_attr;
	delete m_avail;
	delete m_proc;
	delete coll_sock;
	if( view_sock ) {
		delete view_sock;
	}
	if( resources ) {
		for( i = 0; i < nresources; i++ ) {
			delete resources[i];
		}
		delete [] resources;
	}
	for( i=0; i<=max_types; i++ ) {
		if( type_strings[i] ) {
			delete type_strings[i];
		}
	}
	delete [] type_strings;
	delete [] type_nums;
}


void
ResMgr::init_resources()
{
	CpuAttributes* cap;
	int i, j, num;
	float share;
	char buf[64], *tmp;

	if( (tmp = param("MAX_VIRTUAL_MACHINE_TYPES")) ) {
		max_types = atoi( tmp );
		free( tmp );
	} else {
		max_types = 10;
	}
		// The reason these aren't on the stack is b/c of the variable
		// nature of max_types. *sigh*  
	type_nums = new int[max_types+1];
	type_strings = new StringList*[max_types+1];

		// So, first see if we're trying to advertise any virtual
		// machines of specific types, and if so, grab the type info. 
	for( i=0; i<=max_types; i++ ) {
		type_strings[i] = NULL;
		type_nums[i] = 0;
		sprintf( buf, "NUM_VIRTUAL_MACHINES_TYPE_%d", i );
		if( (tmp = param(buf)) ) {
			type_nums[i] = atoi( tmp );
			free( tmp ) ;
			nresources += type_nums[i];
			sprintf( buf, "VIRTUAL_MACHINE_TYPE_%d", i );
			if( ! (tmp = param(buf)) ) {
				EXCEPT( "NUM_VITUAL_MACHINES_TYPE_%d is %d, %s undefined.",	
						i, type_nums[i], buf );
			}
			type_strings[i] = new StringList();
			type_strings[i]->initializeFromString( tmp );
			free( tmp );
		}
	}

	if( nresources ) {
			// We found type-specific stuff mentioned, so use that. 
		num=0;
		resources = new Resource*[nresources];
		for( i=0; i<=max_types; i++ ) {
			if( type_nums[i] ) {
				currentVMType = i;
				for( j=0; j<type_nums[i]; j++ ) {
					cap = build_vm( i );
					if( m_avail->decrement(cap) ) {
						resources[num] = new Resource( cap, num+1 );
						num++;
					} else {
							// We ran out of system resources.  Since
							// this is at the init stage, we EXCEPT. 
						dprintf( D_ALWAYS, 
								 "ERROR: Can't allocate %s virtual machine of type %d\n",
								 num_string(j+1), i );
						dprintf( D_ALWAYS | D_NOHEADER, "\tRequesting: " );
						cap->show_totals( D_ALWAYS );
						dprintf( D_ALWAYS | D_NOHEADER, "\tAvailable:  " );
						m_avail->show_totals( D_ALWAYS );
						EXCEPT( "Ran of of system resources" );
					}
				}					
			}
		}
	} else {
			// We're evenly dividing things, so we only have to figure
			// out how many nodes to advertise.
		if( (tmp = param("NUM_VIRTUAL_MACHINES")) ) { 
			nresources = atoi( tmp );
			free( tmp );
		} else {
			nresources = num_cpus();
		}
		if( nresources ) {
			resources = new Resource*[nresources];
			share = (float)1 / num_cpus();
			for( i=0; i<nresources; i++ ) {
				cap = new CpuAttributes( m_attr, -1, 1, 
										 compute_phys_mem(share), 
										 share, share );
				resources[i] = new Resource( cap, i+1 );
			}
		} else {
				// We're not supposed to advertise any VMs. 
				// Anything special to do here?
		}
	}
}


void
ResMgr::reconfig_resources()
{
	int i, num = 0, num_set = 0;
	char buf[64], *tmp;
	CpuAttributes* cap;
	float share;

		// See if we're trying to advertise any virtual machines of
		// specific types. 
	for( i=0; i<=max_types; i++ ) {
		type_nums[i] = 0;
		sprintf( buf, "NUM_VIRTUAL_MACHINES_TYPE_%d", i );
		if( (tmp = param(buf)) ) {
			num_set = 1;
			type_nums[i] = atoi( tmp );
			free( tmp );
			num += type_nums[i];
		}
	}
}


CpuAttributes*
ResMgr::build_vm( int type )
{
	StringList* list = type_strings[type];
	char *attr, *val;
	int cpus=0, ram=0;
	float disk=0, swap=0, share;

		// For this parsing code, deal with the following example
		// string list:
		// "c=1, r=25%, d=1/4, s=25%"
	list->rewind();
	while( (attr = list->next()) ) {
		if( ! (val = strchr(attr, '=')) ) {
				// There's no = in this description, it must be one 
				// percentage or fraction for all attributes.
				// For example "1/4" or "25%".  So, we can just parse
				// it as a percentage and use that for everything.
			share = parse_value( attr );
			if( share <= 0 ) {
				dprintf( D_ALWAYS, "ERROR: Bad description of machine type %d: ",
						 currentVMType );
				dprintf( D_ALWAYS | D_NOHEADER,  "\"%s\" is invalid.\n", attr );
				dprintf( D_ALWAYS | D_NOHEADER, 
						 "\tYou must specify a percentage (like \"25%\"), " );
				dprintf( D_ALWAYS | D_NOHEADER, "a fraction (like \"1/4\"),\n" );
				dprintf( D_ALWAYS | D_NOHEADER, 
						 "\tor list all attributes (like \"c=1, r=25%, s=25%, d=25%\").\n" );
				dprintf( D_ALWAYS | D_NOHEADER, 
						 "\tSee the manual for details.\n" );
				exit( 4 );
			}
				// We want to be a little smart about CPUs and RAM, so put all
				// the brains in seperate functions.
			cpus = compute_cpus( share );
			ram = compute_phys_mem( share );
			return new CpuAttributes( m_attr, type, cpus, ram, share, share );
		}
			// If we're still here, this is part of a string that
			// lists out seperate attributes and the share for each one.

			// Get the value for this attribute.  It'll either be a
			// percentage, or it'll be a distinct value (in which
			// case, parse_value() will return negative.
		if( ! val[1] ) {
			dprintf( D_ALWAYS, 
					 "Can't parse attribute \"%s\" in description of machine type %d\n",
					 attr, currentVMType );
			exit( 4 );
		}
		share = parse_value( &val[1] );

			// Figure out what attribute we're dealing with.
		switch( attr[0] ) {
		case 'c':
			cpus = compute_cpus( share );
			break;
		case 'r':
		case 'm':
			ram = compute_phys_mem( share );
			break;
		case 's':
		case 'v':
			if( share > 0 ) {
				swap = share;
			} else {
				dprintf( D_ALWAYS,
						 "You must specify a percent or fraction for swap in machine type %d\n", 
						 currentVMType ); 
				exit( 4 );
			}
			break;
		case 'd':
			if( share > 0 ) {
				disk = share;
			} else {
				dprintf( D_ALWAYS, 
						 "You must specify a percent or fraction for disk in machine type %d\n", 
						currentVMType ); 
				exit( 4 );
			}
			break;
		default:
			dprintf( D_ALWAYS, "Unknown attribute \"%s\" in machine type %d\n", 
					 attr, currentVMType );
			exit( 4 );
			break;
		}
	}

		// We're all done parsing the string.  Any attribute not
		// listed defaults to an even share.
	share = (float)1 / num_cpus();
	if( ! cpus ) {
		cpus = compute_cpus( share );
	}
	if( ! swap ) {
		swap = share;
	}
	if( ! disk ) {
		disk = share;
	}
	if( ! ram ) {
		ram = compute_phys_mem( share );
	}

		// Now create the object.
	return new CpuAttributes( m_attr, type, cpus, ram, swap, disk );
}


/* 
   Parse a string in one of a few formats, and return a float that
   represents the value.  Either it's a fraction, like "1/4", or it's
   a percent, like "25%" (in both cases we'd return .25), or it's a
   regular value, like "64", in which case, we return the negative
   value, so that our caller knows it's a value, not a percentage. 
*/
float
ResMgr::parse_value( const char* str )
{
	char *tmp, *foo = strdup( str );
	float val;
	if( (tmp = strchr(foo, '%')) ) {
			// It's a percent
		*tmp = '\0';
		val = (float)atoi(foo) / 100;
		free( foo );
		return val;
	} else if( (tmp = strchr(foo, '/')) ) {
			// It's a fraction
		*tmp = '\0';
		if( ! tmp[1] ) {
			dprintf( D_ALWAYS, "Can't parse attribute \"%s\" in description of machine type %d\n",
					 foo, currentVMType );
			exit( 4 );
		}
		val = (float)atoi(foo) / ((float)atoi(&tmp[1]));
		free( foo );
		return val;
	} else {
			// This must just be a value.  Return it as a negative
			// float, so the caller knows it's not a percentage.
		val = -(float)atoi(foo);
		free( foo );
		return val;
	}
}
 
 
/*
  Generally speaking, we want to round down for fractional amounts of
  a CPU.  However, we never want to advertise less than 1.  Plus, if
  the share in question is negative, it means it's a real value, not a
  percentage.
*/
int
ResMgr::compute_cpus( float share )
{
	int cpus;
	if( share > 0 ) {
		cpus = (int)floor( share * num_cpus() );
	} else {
		cpus = (int)floor( -share );
	}
	if( ! cpus ) {
		cpus = 1;
	}
	return cpus;
}


/*
  Generally speaking, we want to round down for fractional amounts of
  physical memory.  However, we never want to advertise less than 1.
  Plus, if the share in question is negative, it means it's a real
  value, not a percentage.
*/
int
ResMgr::compute_phys_mem( float share )
{
	int phys_mem;
	if( share > 0 ) {
		phys_mem = (int)floor( share * m_attr->phys_mem() );
	} else {
		phys_mem = (int)floor( -share );
	}
	if( ! phys_mem ) {
		phys_mem = 1;
	}
	return phys_mem;
}


void
ResMgr::init_socks()
{
	if( coll_sock ) {
		delete coll_sock;
	}
	coll_sock = new SafeSock( collector_host, 
							  COLLECTOR_UDP_COMM_PORT );

	if( view_sock ) {
		delete view_sock;
	}
	if( condor_view_host ) {
		view_sock = new SafeSock( condor_view_host, 
								  CONDOR_VIEW_PORT );
	}
}


void
ResMgr::walk( int(*func)(Resource*) )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		func(resources[i]);
	}
}


void
ResMgr::walk( ResourceMember memberfunc )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
}


void
ResMgr::walk( VoidResourceMember memberfunc )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))();
	}
}


void
ResMgr::walk( ResourceMaskMember memberfunc, amask_t mask ) 
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		(resources[i]->*(memberfunc))(mask);
	}
}


int
ResMgr::sum( ResourceMember memberfunc )
{
	if( ! resources ) {
		return 0;
	}
	int i, tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


float
ResMgr::sum( ResourceFloatMember memberfunc )
{
	if( ! resources ) {
		return 0;
	}
	int i;
	float tot = 0;
	for( i = 0; i < nresources; i++ ) {
		tot += (resources[i]->*(memberfunc))();
	}
	return tot;
}


Resource*
ResMgr::res_max( ResourceMember memberfunc, int* val )
{
	if( ! resources ) {
		return NULL;
	}
	Resource* rip = NULL;
	int i, tmp, max = INT_MIN;

	for( i = 0; i < nresources; i++ ) {
		tmp = (resources[i]->*(memberfunc))();
		if( tmp > max ) {
			max = tmp;
			rip = resources[i];
		}
	}
	if( val ) {
		*val = max;
	}
	return rip;
}


void
ResMgr::resource_sort( ComparisonFunc compar )
{
	if( ! resources ) {
		return;
	}
	if( nresources > 1 ) {
		qsort( resources, nresources, sizeof(Resource*), compar );
	} 
}


bool
ResMgr::in_use( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->in_use() ) {
			return true;
		}
	}
	return false;
}


Resource*
ResMgr::get_by_pid( int pid )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_starter->pid() == pid ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_cur_cap( char* cap )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->cap()->matches(cap) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_cap( char* cap )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->cap()->matches(cap) ) {
			return resources[i];
		}
		if( (resources[i]->r_pre) &&
			(resources[i]->r_pre->cap()->matches(cap)) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_name( char* name )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( !strcmp(resources[i]->r_name, name) ) {
			return resources[i];
		}
	}
	return NULL;
}




State
ResMgr::state()
{
	if( ! resources ) {
		return owner_state;
	}
	State s;
	int i, is_owner = 0;
	for( i = 0; i < nresources; i++ ) {
		switch( (s = resources[i]->state()) ) {
		case claimed_state:
		case preempting_state:
			return s;
			break;
		case owner_state:
			is_owner = 1;
			break;
		default:
			break;
		}
	}
	if( is_owner ) {
		return owner_state;
	} else {
		return s;
	}
}


void
ResMgr::final_update()
{
	if( ! resources ) {
		return;
	}
	walk( Resource::final_update );
}


int
ResMgr::force_benchmark()
{
	if( ! resources ) {
		return 0;
	}
	return resources[0]->force_benchmark();
}


int
ResMgr::send_update( ClassAd* public_ad, ClassAd* private_ad )
{
	int num = 0;

	if( coll_sock ) {
		if( send_classad_to_sock(coll_sock, public_ad, private_ad) ) {
			num++;
		} else {
			dprintf( D_ALWAYS,
					 "Error sending UDP update to the collector (%s)\n", 
					 collector_host );
		}
	}  

		// If we have an alternate collector, send public CA there.
	if( view_sock ) {
		if( send_classad_to_sock(view_sock, public_ad, NULL) ) {
			num++;
		} else {
			dprintf( D_ALWAYS, 
					 "Error sending UDP update to the collector (%s)\n", 
					 collector_host );
		}
	}

		// Increment the resmgr's count of updates.
	num_updates++;
	return num;
}


void
ResMgr::first_eval_and_update_all()
{
	num_updates = 0;
	walk( Resource::eval_and_update );
	report_updates();
	check_polling();
}


void
ResMgr::eval_and_update_all()
{
	num_updates = 0;
	compute( A_TIMEOUT | A_UPDATE );
	walk( Resource::eval_and_update );
	report_updates();
	check_polling();
}


void
ResMgr::eval_all()
{
	num_updates = 0;
	compute( A_TIMEOUT );
	walk( Resource::eval_state );
	report_updates();
	check_polling();
}


void
ResMgr::report_updates()
{
	if( !num_updates ) {
		return;
	}
	if( coll_sock ) {
		dprintf( D_FULLDEBUG,
				 "Sent %d update(s) to the collector (%s)\n", 
				 num_updates, collector_host );
	}  
	if( view_sock ) {
		dprintf( D_FULLDEBUG, 
				 "Sent %d update(s) to the condor_view host (%s)\n",
				 num_updates, condor_view_host );
	}
}


void
ResMgr::compute( amask_t how_much )
{
	if( ! resources ) {
		return;
	}

	m_attr->compute( (how_much & ~(A_SUMMED)) | A_SHARED );
	walk( Resource::compute, (how_much & ~(A_SHARED)) );
	m_attr->compute( (how_much & ~(A_SHARED)) | A_SUMMED );
	walk( Resource::compute, (how_much | A_SHARED) );

		// Sort the resources so when we're assigning owner load
		// average and keyboard activity, we get to them in the
		// following state order: Owner, Unclaimed, Matched, Claimed
		// Preempting 
	resource_sort( owner_state_cmp );

	assign_load();
	assign_keyboard();

		// Now that we're done assigning, display all values 
	walk( Resource::display, how_much );
}


void
ResMgr::assign_load()
{
	if( ! resources ) {
		return;
	}

	int i;
	float total_owner_load = m_attr->load() - m_attr->condor_load();
	if( total_owner_load < 0 ) {
		total_owner_load = 0;
	}
	if( is_smp() ) {
			// Print out the totals we already know.
		dprintf( D_LOAD, 
				 "%s %.3f\t%s %.3f\t%s %.3f\n",  
				 "SystemLoad:", m_attr->load(),
				 "TotalCondorLoad:", m_attr->condor_load(),
				 "TotalOwnerLoad:", total_owner_load );

			// Initialize everything to 0.  Only need this for SMP
			// machines, since on single CPU machines, we just assign
			// all OwnerLoad to the 1 CPU.
		for( i = 0; i < nresources; i++ ) {
			resources[i]->set_owner_load( 0 );
		}
	}

		// So long as there's at least two more resources and the
		// total owner load is greater than 1.0, assign an owner load
		// of 1.0 to each CPU.  Once we get below 1.0, we assign all
		// the rest to the next CPU.  So, for non-SMP machines, we
		// never hit this code, and always assign all owner load to
		// cpu1 (since i will be initialized to 0 but we'll never
		// enter the for loop).  
	for( i = 0; i < (nresources - 1) && total_owner_load > 1; i++ ) {
		resources[i]->set_owner_load( 1.0 );
		total_owner_load -= 1.0;
	}
	resources[i]->set_owner_load( total_owner_load );
}


void
ResMgr::assign_keyboard()
{
	if( ! resources ) {
		return;
	}

	int i;
	time_t console = m_attr->console_idle();
	time_t keyboard = m_attr->keyboard_idle();
	time_t max;

	if( is_smp() ) {
			// First, initialize all CPUs to the max idle time we've
			// got, which is some configurable amount of minutes
			// longer than the time since we started up. 
		max = (time(0) - startd_startup) + disconnected_keyboard_boost;
		for( i = 0; i < nresources; i++ ) {
			resources[i]->r_attr->set_console( max );
			resources[i]->r_attr->set_keyboard( max );
		}
	}

		// Now, assign console activity to all CPUs that care.
		// Notice, we should also assign keyboard here, since if
		// there's console activity, there's (by definition) keyboard 
		// activity as well.
	for( i = 0; i < console_vms  && i < nresources; i++ ) {
		resources[i]->r_attr->set_console( console );
		resources[i]->r_attr->set_keyboard( console );
	}

		// Finally, assign keyboard activity to all CPUS that care. 
	for( i = 0; i < keyboard_vms && i < nresources; i++ ) {
		resources[i]->r_attr->set_keyboard( keyboard );
	}
}


void
ResMgr::check_polling()
{
	if( ! resources ) {
		return;
	}

	if( in_use() || m_attr->condor_load() > 0 ) {
		start_poll_timer();
	} else {
		cancel_poll_timer();
	}
}


int
ResMgr::start_update_timer()
{
	up_tid = 
		daemonCore->Register_Timer( update_interval, update_interval,
									(TimerHandlercpp)eval_and_update_all,
									"eval_and_update_all", this );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
ResMgr::start_poll_timer()
{
	if( poll_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	poll_tid = 
		daemonCore->Register_Timer( polling_interval,
									polling_interval, 
									(TimerHandlercpp)eval_all,
									"poll_resources", this );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	dprintf( D_FULLDEBUG, "Started polling timer.\n" );
	return TRUE;
}


void
ResMgr::cancel_poll_timer()
{
	if( poll_tid != -1 ) {
		daemonCore->Cancel_Timer( poll_tid );
		poll_tid = -1;
		dprintf( D_FULLDEBUG, "Canceled polling timer.\n" );
	}
}


void
ResMgr::reset_timers()
{
	if( poll_tid != -1 ) {
		daemonCore->Reset_Timer( poll_tid, polling_interval, 
								 polling_interval );
	}
	if( up_tid != -1 ) {
		daemonCore->Reset_Timer( up_tid, update_interval, 
								 update_interval );
	}
}


int
owner_state_cmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);
		// Since the State enum is already in the "right" order for
		// this kind of sort, we don't need to do anything fancy, we
		// just cast the state enum to an int and we're done.
	val1 = (int)rip1->state();
	val2 = (int)rip2->state();
	return val1 - val2;
}

