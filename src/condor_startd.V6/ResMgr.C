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
	config_classad = NULL;
	up_tid = -1;
	poll_tid = -1;

	m_proc = new ProcAPI;
	m_attr = new MachAttributes;
	id_disp = NULL;

	nresources = 0;
	resources = NULL;
}


ResMgr::~ResMgr()
{
	int i;
	if( config_classad ) delete config_classad;
	if( totals_classad ) delete totals_classad;
	if( id_disp ) delete id_disp;
	delete m_attr;
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
ResMgr::init_config_classad()
{
	if( config_classad ) delete config_classad;
	config_classad = new ClassAd();
	config( config_classad );
}


void
ResMgr::init_resources()
{
	int i, j, num;
	char *tmp;
	CpuAttributes** new_cpu_attrs;

		// These things can only be set once, at startup, so they
		// don't need to be in build_cpu_attrs() at all.
	if( (tmp = param("MAX_VIRTUAL_MACHINE_TYPES")) ) {
		max_types = atoi( tmp );
		free( tmp );
	} else {
		max_types = 10;
	}
		// The reason this isn't on the stack is b/c of the variable
		// nature of max_types. *sigh*  
	type_strings = new StringList*[max_types+1];
	memset( type_strings, 0, (sizeof(StringList*) * max_types+1) );

		// Fill in the type_strings array with all the appropriate
		// string lists for each type definition.  This only happens
		// once!  If you change the type definitions, you must restart
		// the startd, or else too much weirdness is possible.
	initTypes( 1 );

		// First, see how many VMs of each type are specified.
	nresources = countTypes( &type_nums, true );

		// See if the config file allows for a valid set of
		// CpuAttributes objects.  Since this is the startup-code
		// we'll let it EXCEPT() if there is an error.
	new_cpu_attrs = buildCpuAttrs( nresources, type_nums, true );
	if( ! new_cpu_attrs ) {
		EXCEPT( "buildCpuAttrs() failed and should have already EXCEPT'ed" );
	}

		// Now, we can finally allocate our resources array, and
		// populate it.  
	resources = new Resource*[nresources];
	for( i=0; i<nresources; i++ ) {
		resources[i] = new Resource( new_cpu_attrs[i], i+1 );
	}

		// We can now seed our IdDispenser with the right VM id. 
	id_disp = new IdDispenser( i+1 );

		// Finally, we can free up the space of the new_cpu_attrs
		// array itself, now that all the objects it was holding that
		// we still care about are stashed away in the various
		// Resource objects.  Since it's an array of pointers, this
		// won't touch the objects at all.
	delete [] new_cpu_attrs;
}


void
ResMgr::reconfig_resources()
{
	int i, diff, num = 0;
	int *new_type_nums;
	CpuAttributes** new_cpu_attrs;

		// See if any new types were defined.  Don't except if there's
		// any errors, just dprintf().
	initTypes( 0 );

		// First, see how many VMs of each type are specified.
	num = countTypes( &new_type_nums, false );

	if( typeNumCmp(new_type_nums, type_nums) ) {
			// We want the same number of each VM type that we've got
			// now.  We're done!
		return;
	}

	for( i=0; i<=max_types; i++ ) {
		diff = new_type_nums[i] - type_nums[i];
		if( diff > 0 ) {
				// More of this type requested than we already have.
				// We need to make some new VMs of this type.
		} else if ( diff < 0 ) {
				// We want less of this type than we have now.  We
				// need to evict some and free up the resources. 
		} else {
				// No change
		}
	}

		// Now, sort our resources by type, to see what we have.
	resource_sort( vm_type_cmp );
}



CpuAttributes** 
ResMgr::buildCpuAttrs( int total, int* type_num_array, bool except )
{
	int i, j, num;
	CpuAttributes* cap;
	CpuAttributes** cap_array;
	float share;
	char* tmp;

		// Available system resources.
	AvailAttributes avail( m_attr );
	
	cap_array = new CpuAttributes* [total];
	if( ! cap_array ) {
		EXCEPT( "Out of memory!" );
	}

	num = 0;
	for( i=0; i<=max_types; i++ ) {
		if( type_num_array[i] ) {
			currentVMType = i;
			for( j=0; j<type_num_array[i]; j++ ) {
				cap = build_vm( i, except );
				if( avail.decrement(cap) ) {
					cap_array[num] = cap;
					num++;
				} else {
						// We ran out of system resources.  
					dprintf( D_ALWAYS, 
							 "ERROR: Can't allocate %s virtual machine of type %d\n",
							 num_string(j+1), i );
					dprintf( D_ALWAYS | D_NOHEADER, "\tRequesting: " );
					cap->show_totals( D_ALWAYS );
					dprintf( D_ALWAYS | D_NOHEADER, "\tAvailable:  " );
					avail.show_totals( D_ALWAYS );
					if( except ) {
						EXCEPT( "Ran out of system resources" );
					} else {
							// Gracefully cleanup and abort
						for( i=0; i<num; i++ ) {
							delete cap_array[i];
						}
						delete [] cap_array;
						return NULL;
					}
				}					
			}
		}
	}
	return cap_array;
}
	

void
ResMgr::initTypes( bool except )
{
	int i;
	char* tmp;
	char buf[128];

	sprintf( buf, "VIRTUAL_MACHINE_TYPE_0" );
	if( (tmp = param(buf)) ) {
		if( except ) {
			EXCEPT( "Can't define %s in the config file", buf );
		} else {
			dprintf( D_ALWAYS, 
					 "Can't define %s in the config file, ignoring\n",
					 buf ); 
		}
	}

	if( ! type_strings[0] ) {
			// Type 0 is the special type for evenly divided VMs. 
		type_strings[0] = new StringList();
		sprintf( buf, "1/%d", num_cpus() );
		type_strings[0]->initializeFromString( buf );
	}	

	for( i=1; i < max_types; i++ ) {
		if( type_strings[i] ) {
			continue;
		}
		sprintf( buf, "VIRTUAL_MACHINE_TYPE_%d", i );
		if( ! (tmp = param(buf)) ) {
			continue;
		}
		type_strings[i] = new StringList();
		type_strings[i]->initializeFromString( tmp );
		free( tmp );
	}
}


int
ResMgr::countTypes( int** array_ptr, bool except )
{
	int i, num=0, num_set=0;
	char* tmp;
	char buf[128];
	int* new_type_nums = new int[max_types+1];

	if( ! array_ptr ) {
		EXCEPT( "ResMgr:countTypes() called with NULL array_ptr!" );
	}

		// Type 0 is special, user's shouldn't define it.
	sprintf( buf, "NUM_VIRTUAL_MACHINES_TYPE_0" );
	if( (tmp = param(buf)) ) {
		if( except ) {
			EXCEPT( "Can't define %s in the config file", buf );
		} else {
			dprintf( D_ALWAYS, 
					 "Can't define %s in the config file, ignoring\n",
					 buf ); 
		}
	}

	for( i=1; i<=max_types; i++ ) {
		new_type_nums[i] = 0;
		sprintf( buf, "NUM_VIRTUAL_MACHINES_TYPE_%d", i );
		if( (tmp = param(buf)) ) {
			num_set = 1;
			new_type_nums[i] = atoi( tmp );
			free( tmp );
			num += new_type_nums[i];
		}
	}

	if( num_set ) {
			// We found type-specific stuff, use that.
		new_type_nums[0] = 0;
	} else {
			// We haven't found any special types yet.  Therefore,
			// we're evenly dividing things, so we only have to figure
			// out how many nodes to advertise.
		if( (tmp = param("NUM_VIRTUAL_MACHINES")) ) { 
			new_type_nums[0] = atoi( tmp );
			free( tmp );
		} else {
			new_type_nums[0] = num_cpus();
		}
		num = new_type_nums[0];
	}
	*array_ptr = new_type_nums;
	return num;
}


bool
ResMgr::typeNumCmp( int* a, int* b )
{
	int i;
	for( i=0; i<=max_types; i++ ) {
		if( a[i] != b[i] ) {
			return false;
		}
	}
	return true; 
}


CpuAttributes*
ResMgr::build_vm( int type, bool except )
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
			share = parse_value( attr, except );
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
				if( except ) {
					DC_Exit( 4 );
				} else {	
					return NULL;
				}
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
			if( except ) {
				DC_Exit( 4 );
			} else {	
				return NULL;
			}
		}
		share = parse_value( &val[1], except );
		if( ! share ) {
				// Invalid share.
		}

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
				if( except ) {
					DC_Exit( 4 );
				} else {	
					return NULL;
				}
			}
			break;
		case 'd':
			if( share > 0 ) {
				disk = share;
			} else {
				dprintf( D_ALWAYS, 
						 "You must specify a percent or fraction for disk in machine type %d\n", 
						currentVMType ); 
				if( except ) {
					DC_Exit( 4 );
				} else {	
					return NULL;
				}
			}
			break;
		default:
			dprintf( D_ALWAYS, "Unknown attribute \"%s\" in machine type %d\n", 
					 attr, currentVMType );
			if( except ) {
				DC_Exit( 4 );
			} else {	
				return NULL;
			}
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
ResMgr::parse_value( const char* str, bool except )
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
			if( except ) {
				DC_Exit( 4 );
			} else {	
				return 0;
			}
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
	coll_sock = Collector->safeSock();

	if( view_sock ) {
		delete view_sock;
	}
	if( condor_view_host ) {
		view_sock = new SafeSock;
		if (!view_sock->connect( condor_view_host, CONDOR_VIEW_PORT )) {
			dprintf(D_ALWAYS, "Failed to connect to condor view server "
					"<%s:%d>\n", condor_view_host, CONDOR_VIEW_PORT);
		}
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
	walk( &(Resource::final_update) );
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
					 Collector->fullHostname() );
		}
	}  

		// If we have an alternate collector, send public CA there.
	if( view_sock ) {
		if( send_classad_to_sock(view_sock, public_ad, NULL) ) {
			num++;
		} else {
			dprintf( D_ALWAYS, 
					 "Error sending UDP update to the collector (%s)\n", 
					 condor_view_host );
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
	walk( &(Resource::eval_and_update) );
	report_updates();
	check_polling();
}


void
ResMgr::eval_and_update_all()
{
	num_updates = 0;
	compute( A_TIMEOUT | A_UPDATE );
	walk( &(Resource::eval_and_update) );
	report_updates();
	check_polling();
}


void
ResMgr::eval_all()
{
	num_updates = 0;
	compute( A_TIMEOUT );
	walk( &(Resource::eval_state) );
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
				 num_updates, Collector->fullHostname() );
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
	walk( &(Resource::compute), (how_much & ~(A_SHARED)) );
	m_attr->compute( (how_much & ~(A_SHARED)) | A_SUMMED );
	walk( &(Resource::compute), (how_much | A_SHARED) );

		// Sort the resources so when we're assigning owner load
		// average and keyboard activity, we get to them in the
		// following state order: Owner, Unclaimed, Matched, Claimed
		// Preempting 
	resource_sort( owner_state_cmp );

	assign_load();
	assign_keyboard();

		// Now that we're done assigning, display all values 
	walk( &(Resource::display), how_much );
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
		if( DebugFlags & D_LOAD ) {
			dprintf( D_FULLDEBUG, 
					 "%s %.3f\t%s %.3f\t%s %.3f\n",  
					 "SystemLoad:", m_attr->load(),
					 "TotalCondorLoad:", m_attr->condor_load(),
					 "TotalOwnerLoad:", total_owner_load );
		}

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

		// First, initialize all CPUs to the max idle time we've got,
		// which is some configurable amount of minutes longer than
		// the time since we started up.
	max = (time(0) - startd_startup) + disconnected_keyboard_boost;
	for( i = 0; i < nresources; i++ ) {
		resources[i]->r_attr->set_console( max );
		resources[i]->r_attr->set_keyboard( max );
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
									(TimerHandlercpp)&eval_and_update_all,
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
									(TimerHandlercpp)&eval_all,
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


int
vm_type_cmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);
		// Since we just want to sort on the resource type, which is
		// an integer, we don't need to do anything fancy. 
	return( rip1->type() - rip2->type() );
}


IdDispenser::IdDispenser( int seed )
{
	set.Add( seed );
}


int
IdDispenser::next()
{
	int id;
	set.StartIterations();
	set.Iterate( id );
	set.RemoveLast();
	set.Add( id+1 );
	return id;
}


void
IdDispenser::insert( int id )
{
	set.Add( id );
}


