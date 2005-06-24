/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "startd.h"
#include "condor_classad_namedlist.h"

ResMgr::ResMgr()
{
	totals_classad = NULL;
	config_classad = NULL;
	up_tid = -1;
	poll_tid = -1;

	m_attr = new MachAttributes;
	id_disp = NULL;

	nresources = 0;
	resources = NULL;
	type_nums = NULL;
	new_type_nums = NULL;
	is_shutting_down = false;
	cur_time = last_in_use = time( NULL );
}


ResMgr::~ResMgr()
{
	int i;
	if( config_classad ) delete config_classad;
	if( totals_classad ) delete totals_classad;
	if( id_disp ) delete id_disp;
	delete m_attr;
	if( resources ) {
		for( i = 0; i < nresources; i++ ) {
			delete resources[i];
		}
		delete [] resources;
	}
	for( i=0; i<max_types; i++ ) {
		if( type_strings[i] ) {
			delete type_strings[i];
		}
	}
	delete [] type_strings;
	delete [] type_nums;
	if( new_type_nums ) {
		delete [] new_type_nums;
	}		
}


void
ResMgr::init_config_classad( void )
{
	if( config_classad ) delete config_classad;
	config_classad = new ClassAd();

		// First, bring in everything we know we need
	configInsert( config_classad, "START", true );
	configInsert( config_classad, "SUSPEND", true );
	configInsert( config_classad, "CONTINUE", true );
	configInsert( config_classad, "PREEMPT", true );
	configInsert( config_classad, "KILL", true );
	configInsert( config_classad, "WANT_SUSPEND", true );
	configInsert( config_classad, "WANT_VACATE", true );
	configInsert( config_classad, ATTR_MAX_JOB_RETIREMENT_TIME, false );

		// Now, bring in things that we might need
	configInsert( config_classad, "PERIODIC_CHECKPOINT", false );
	configInsert( config_classad, "RunBenchmarks", false );
	configInsert( config_classad, ATTR_RANK, false );
	configInsert( config_classad, "SUSPEND_VANILLA", false );
	configInsert( config_classad, "CONTINUE_VANILLA", false );
	configInsert( config_classad, "PREEMPT_VANILLA", false );
	configInsert( config_classad, "KILL_VANILLA", false );
	configInsert( config_classad, "WANT_SUSPEND_VANILLA", false );
	configInsert( config_classad, "WANT_VACATE_VANILLA", false );

		// Next, try the IS_OWNER expression.  If it's not there, give
		// them a resonable default, instead of leaving it undefined. 
	if( ! configInsert(config_classad, ATTR_IS_OWNER, false) ) {
		char* tmp = (char*) malloc( strlen(ATTR_IS_OWNER) + 21 ); 
		if( ! tmp ) {
			EXCEPT( "Out of memory!" );
		}
		sprintf( tmp, "%s = (START =?= False)", ATTR_IS_OWNER );
		config_classad->Insert( tmp );
		free( tmp );
	}
		// Next, try the CpuBusy expression.  If it's not there, try
		// what's defined in cpu_busy (for backwards compatibility).  
		// If that's not there, give them a default of "False",
		// instead of leaving it undefined.
	if( ! configInsert(config_classad, ATTR_CPU_BUSY, false) ) {
		if( ! configInsert(config_classad, "cpu_busy", ATTR_CPU_BUSY,
						   false) ) { 
			char* tmp = (char*) malloc( strlen(ATTR_CPU_BUSY) + 9 ); 
			if( ! tmp ) {
				EXCEPT( "Out of memory!" );
			}
			sprintf( tmp, "%s = False", ATTR_CPU_BUSY );
			config_classad->Insert( tmp );
			free( tmp );
		}
	}

		// Now, bring in anything the user has said to include
	config_fill_ad( config_classad );
}


void
ResMgr::init_resources( void )
{
	int i;
	char *tmp;
	CpuAttributes** new_cpu_attrs;

		// These things can only be set once, at startup, so they
		// don't need to be in build_cpu_attrs() at all.
	if( (tmp = param("MAX_VIRTUAL_MACHINE_TYPES")) ) {
		max_types = atoi( tmp ) + 1;
		free( tmp );
	} else {
		max_types = 11;
	}
		// The reason this isn't on the stack is b/c of the variable
		// nature of max_types. *sigh*  
	type_strings = new StringList*[max_types];
	memset( type_strings, 0, (sizeof(StringList*) * max_types) );

		// Fill in the type_strings array with all the appropriate
		// string lists for each type definition.  This only happens
		// once!  If you change the type definitions, you must restart
		// the startd, or else too much weirdness is possible.
	initTypes( 1 );

		// First, see how many VMs of each type are specified.
	nresources = countTypes( &type_nums, true );

	if( ! nresources ) {
			// We're not configured to advertise any nodes.
		resources = NULL;
		id_disp = new IdDispenser( num_cpus(), 1 );
		return;
	}

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
	id_disp = new IdDispenser( num_cpus(), i+1 );

		// Finally, we can free up the space of the new_cpu_attrs
		// array itself, now that all the objects it was holding that
		// we still care about are stashed away in the various
		// Resource objects.  Since it's an array of pointers, this
		// won't touch the objects at all.
	delete [] new_cpu_attrs;
}


bool
ResMgr::reconfig_resources( void )
{
	int t, i, cur, num;
	CpuAttributes** new_cpu_attrs;
	int max_num = num_cpus();
	int* cur_type_index;
	Resource*** sorted_resources;	// Array of arrays of pointers.
	Resource* rip;

		// See if any new types were defined.  Don't except if there's
		// any errors, just dprintf().
	initTypes( 0 );

		// First, see how many VMs of each type are specified.
	num = countTypes( &new_type_nums, false );

	if( typeNumCmp(new_type_nums, type_nums) ) {
			// We want the same number of each VM type that we've got
			// now.  We're done!
		delete [] new_type_nums;
		new_type_nums = NULL;
		return true;
	}

		// See if the config file allows for a valid set of
		// CpuAttributes objects.  
	new_cpu_attrs = buildCpuAttrs( num, new_type_nums, false );
	if( ! new_cpu_attrs ) {
			// There was an error, abort.  We still return true to
			// indicate that we're done doing our thing...
		dprintf( D_ALWAYS, "Aborting virtual machine type reconfig.\n" );
		delete [] new_type_nums;
		new_type_nums = NULL;
		return true;
	}

		////////////////////////////////////////////////////
		// Sort all our resources by type and state.
		////////////////////////////////////////////////////

		// Allocate and initialize our arrays.
	sorted_resources = new Resource** [max_types];
	for( i=0; i<max_types; i++ ) {
		sorted_resources[i] = new Resource* [max_num];
		memset( sorted_resources[i], 0, (max_num*sizeof(Resource*)) ); 
	}

	cur_type_index = new int [max_types];
	memset( cur_type_index, 0, (max_types*sizeof(int)) );

		// Populate our sorted_resources array by type.
	for( i=0; i<nresources; i++ ) {
		t = resources[i]->type();
		(sorted_resources[t])[cur_type_index[t]] = resources[i];
		cur_type_index[t]++;
	}
	
		// Now, for each type, sort our resources by state.
	for( t=0; t<max_types; t++ ) {
		assert( cur_type_index[t] == type_nums[t] );
		qsort( sorted_resources[t], type_nums[t], 
			   sizeof(Resource*), &claimedRankCmp );
	}

		////////////////////////////////////////////////////
		// Decide what we need to do.
		////////////////////////////////////////////////////
	cur = -1;
	for( t=0; t<max_types; t++ ) {
		for( i=0; i<new_type_nums[t]; i++ ) {
			cur++;
			if( ! (sorted_resources[t])[i] ) {
					// If there are no more existing resources of this
					// type, we'll need to allocate one.
				alloc_list.Append( new_cpu_attrs[cur] );
				continue;
			}
			if( (sorted_resources[t])[i]->type() ==
				new_cpu_attrs[cur]->type() ) {
					// We've already got a Resource for this VM, so we
					// can delete it.
				delete new_cpu_attrs[cur];
				continue;
			}
		}
			// We're done with the new VMs of this type.  See if there
			// are any Resources left over that need to be destroyed.
		for( ; i<max_num; i++ ) {
			if( (sorted_resources[t])[i] ) {
				destroy_list.Append( (sorted_resources[t])[i] );
			} else {
				break;
			}
		}
	}

		////////////////////////////////////////////////////
		// Finally, act on our decisions.
		////////////////////////////////////////////////////

		// Everything we care about in new_cpu_attrs is saved
		// elsewhere, and the rest has already been deleted, so we
		// should now delete the array itself. 
	delete [] new_cpu_attrs;

		// Cleanup our memory.
	for( i=0; i<max_types; i++ ) {	
		delete [] sorted_resources[i];
	}
	delete [] sorted_resources;
	delete [] cur_type_index;

		// See if there's anything to destroy, and if so, do it. 
	destroy_list.Rewind();
	while( destroy_list.Next(rip) ) {
		rip->dprintf( D_ALWAYS,
					  "State change: resource no longer needed by configuration\n" );
		rip->set_destination_state( delete_state );
	}

		// Finally, call our helper, so that if all the VMs we need to
		// get rid of are gone by now, we'll allocate the new ones. 
	return processAllocList();
}


CpuAttributes** 
ResMgr::buildCpuAttrs( int total, int* type_num_array, bool except )
{
	int i, j, num;
	CpuAttributes* cap;
	CpuAttributes** cap_array;

		// Available system resources.
	AvailAttributes avail( m_attr );
	
	cap_array = new CpuAttributes* [total];
	if( ! cap_array ) {
		EXCEPT( "Out of memory!" );
	}

	num = 0;
	for( i=0; i<max_types; i++ ) {
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
					delete cap;	// This isn't in our array yet.
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
		free(tmp);
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
	int* new_type_nums = new int[max_types];

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
		free(tmp);
	}

	for( i=1; i<max_types; i++ ) {
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
	for( i=0; i<max_types; i++ ) {
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


// Methods to manipulate the supplemental ClassAd list
int
ResMgr::adlist_register( const char *name )
{
	return extra_ads.Register( name );
}

int
ResMgr::adlist_replace( const char *name, ClassAd *newAd )
{
	return extra_ads.Replace( name, newAd );
}

int
ResMgr::adlist_delete( const char *name )
{
	return extra_ads.Delete( name );
}

int
ResMgr::adlist_publish( ClassAd *resAd, amask_t mask )
{
	// Check the mask
	if (  ( mask & ( A_PUBLIC | A_UPDATE ) ) != ( A_PUBLIC | A_UPDATE )  ) {
		return 0;
	}

	return extra_ads.Publish( resAd );
}


bool
ResMgr::hasOppClaim( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->hasOppClaim() ) {
			return true;
		}
	}
	return false;
}


bool
ResMgr::hasAnyClaim( void )
{
	if( ! resources ) {
		return false;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->hasAnyClaim() ) {
			return true;
		}
	}
	return false;
}


Claim*
ResMgr::getClaimByPid( pid_t pid )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByPid(pid)) ) {
			return foo;
		}
	}
	return NULL;
}


Claim*
ResMgr::getClaimById( const char* id )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimById(id)) ) {
			return foo;
		}
	}
	return NULL;
}


Claim*
ResMgr::getClaimByGlobalJobId( const char* id )
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByGlobalJobId(id)) ) {
			return foo;
		}
	}
	return NULL;
}

Claim *
ResMgr::getClaimByGlobalJobIdAndId( const char *job_id,
									const char *claimId)
{
	Claim* foo = NULL;
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( (foo = resources[i]->findClaimByGlobalJobId(job_id)) ) {
			if( foo == resources[i]->findClaimById(claimId) ) {
				return foo;	
			}
		}
	}
	return NULL;

}


Resource*
ResMgr::findRipForNewCOD( ClassAd* ad )
{
	if( ! resources ) {
		return NULL;
	}
	int requirements;
	int i;

		/*
          We always ensure that the request's Requirements, if any,
		  are met.  Other than that, we give out COD claims to
		  Resources in the following order:  

		  1) the Resource with the least # of existing COD claims (to
  		     ensure round-robin across resources
		  2) in case of a tie, the Resource in the best state (owner
   		     or unclaimed, not claimed)
		  3) in case of a tie, the Claimed resource with the lowest
  		     value of machine Rank for its claim
		*/

		// sort resources based on the above order
	resource_sort( newCODClaimCmp );

		// find the first one that matches our requirements 
	for( i = 0; i < nresources; i++ ) {
		if( ad->EvalBool( ATTR_REQUIREMENTS, resources[i]->r_classad,
						  requirements ) == 0 ) {
			requirements = 0;
		}
		if( requirements ) { 
			return resources[i];
		}
	}
	return NULL;
}



Resource*
ResMgr::get_by_cur_id( char* id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->idMatches(id) ) {
			return resources[i];
		}
	}
	return NULL;
}


Resource*
ResMgr::get_by_any_id( char* id )
{
	if( ! resources ) {
		return NULL;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		if( resources[i]->r_cur->idMatches(id) ) {
			return resources[i];
		}
		if( resources[i]->r_pre &&
			resources[i]->r_pre->idMatches(id) ) {
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
ResMgr::state( void )
{
	if( ! resources ) {
		return owner_state;
	}
	State s;
	Resource* rip;
	int i, is_owner = 0;
	for( i = 0; i < nresources; i++ ) {
		rip = resources[i];
			// if there are *any* COD claims at all (active or not),
			// we should say this machine is claimed so preen doesn't
			// try to clean up directories for the COD claim(s).
		if( rip->r_cod_mgr->numClaims() > 0 ) {
			return claimed_state;
		}
		s = rip->state();
		switch( s ) {
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
ResMgr::final_update( void )
{
	if( ! resources ) {
		return;
	}
	walk( &Resource::final_update );
}


int
ResMgr::force_benchmark( void )
{
	if( ! resources ) {
		return 0;
	}
	return resources[0]->force_benchmark();
}


int
ResMgr::send_update( int cmd, ClassAd* public_ad, ClassAd* private_ad )
{
	int num = 0;

	if( Collectors ) {
		num = Collectors->sendUpdates (cmd, public_ad, private_ad);
	}  

		// Increment the resmgr's count of updates.
	num_updates++;
	return num;
}


void
ResMgr::first_eval_and_update_all( void )
{
	num_updates = 0;
	walk( &Resource::eval_and_update );
	report_updates();
	check_polling();
	check_use();
}


void
ResMgr::eval_and_update_all( void )
{
	compute( A_TIMEOUT | A_UPDATE );
	first_eval_and_update_all();
}


void
ResMgr::eval_all( void )
{
	num_updates = 0;
	compute( A_TIMEOUT );
	walk( &Resource::eval_state );
	report_updates();
	check_polling();
}


void
ResMgr::report_updates( void )
{
	if( !num_updates ) {
		return;
	}

	if( Collectors ) {
		MyString list;
		Daemon * collector;
		Collectors->rewind();
		while (Collectors->next (collector)) {
			list += collector->fullHostname();
			list += " ";
		}

		dprintf( D_FULLDEBUG,
				 "Sent %d update(s) to the collector (%s)\n", 
				 num_updates, list.Value());
	}  
}


void
ResMgr::compute( amask_t how_much )
{
	if( ! resources ) {
		return;
	}

		// Since lots of things want to know this, just get it from
		// the kernel once and share the value...
	cur_time = time( 0 ); 

	m_attr->compute( (how_much & ~(A_SUMMED)) | A_SHARED );
	walk( &Resource::compute, (how_much & ~(A_SHARED)) );
	m_attr->compute( (how_much & ~(A_SHARED)) | A_SUMMED );
	walk( &Resource::compute, (how_much | A_SHARED) );

		// Sort the resources so when we're assigning owner load
		// average and keyboard activity, we get to them in the
		// following state order: Owner, Unclaimed, Matched, Claimed
		// Preempting 
	resource_sort( ownerStateCmp );

	assign_load();
	assign_keyboard();

		// Now that everything has actually been computed, we can
		// refresh our internal classad with all the current values of
		// everything so that when we evaluate our state or any other
		// expressions, we've got accurate data to evaluate.
	walk( &Resource::refresh_classad, how_much );

		// Now that we have an updated internal classad for each
		// resource, we can "compute" anything where we need to 
		// evaluate classad expressions to get the answer.
	walk( &Resource::compute, A_EVALUATED );

		// Next, we can publish any results from that to our internal
		// classads to make sure those are still up-to-date
	walk( &Resource::refresh_classad, A_EVALUATED );

		// Finally, now that all the internal classads are up to date
		// with all the attributes they could possibly have, we can
		// publish the cross-VM attributes desired from
		// STARTD_VM_ATTRS into each VM's internal ClassAd.
	walk( &Resource::refresh_classad, A_SHARED_VM );

		// Now that we're done, we can display all the values.
	walk( &Resource::display, how_much );
}


void
ResMgr::publish( ClassAd* cp, amask_t how_much )
{
	char line[100];

	if( IS_UPDATE(how_much) && IS_PUBLIC(how_much) ) {
		sprintf( line, "%s=%d", ATTR_TOTAL_VIRTUAL_MACHINES, num_vms() );
		cp->Insert( line ); 
	}

	starter_mgr.publish( cp, how_much );
}


void
ResMgr::publishVmAttrs( ClassAd* cap )
{
	if( ! resources ) {
		return;
	}
	int i;
	for( i = 0; i < nresources; i++ ) {
		resources[i]->publishVmAttrs( cap );
	}
}


void
ResMgr::assign_load( void )
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
ResMgr::assign_keyboard( void )
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
	max = (cur_time - startd_startup) + disconnected_keyboard_boost;
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
ResMgr::check_polling( void )
{
	if( ! resources ) {
		return;
	}

	if( hasOppClaim() || m_attr->condor_load() > 0 ) {
		start_poll_timer();
	} else {
		cancel_poll_timer();
	}
}


int
ResMgr::start_update_timer( void )
{
	up_tid = 
		daemonCore->Register_Timer( update_interval, update_interval,
							(TimerHandlercpp)&ResMgr::eval_and_update_all,
							"eval_and_update_all", this );
	if( up_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	return TRUE;
}


int
ResMgr::start_poll_timer( void )
{
	if( poll_tid >= 0 ) {
			// Timer already started.
		return TRUE;
	}
	poll_tid = 
		daemonCore->Register_Timer( polling_interval,
							polling_interval, 
							(TimerHandlercpp)&ResMgr::eval_all,
							"poll_resources", this );
	if( poll_tid < 0 ) {
		EXCEPT( "Can't register DaemonCore timer" );
	}
	dprintf( D_FULLDEBUG, "Started polling timer.\n" );
	return TRUE;
}


void
ResMgr::cancel_poll_timer( void )
{
	int rval;
	if( poll_tid != -1 ) {
		rval = daemonCore->Cancel_Timer( poll_tid );
		if( rval < 0 ) {
			dprintf( D_ALWAYS, "Failed to cancel polling timer (%d): "
					 "daemonCore error\n", poll_tid );
		} else {
			dprintf( D_FULLDEBUG, "Canceled polling timer (%d)\n",
					 poll_tid );
		}
		poll_tid = -1;
	}
}


void
ResMgr::reset_timers( void )
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


void
ResMgr::deleteResource( Resource* rip )
{
		// First, find the rip in the resources array:
	int i, j, dead = -1;
	Resource** new_resources = NULL;
	Resource* rip2;

	for( i = 0; i < nresources; i++ ) {
		if( resources[i] == rip ) {
			dead = i;
			break;
		}
	}
	if( dead < 0 ) {
			// Didn't find it.  This is where we'll hit if resources
			// is NULL.  We should never get here, anyway (we'll never
			// call deleteResource() if we don't have any resources. 
		EXCEPT( "ResMgr::deleteResource() failed: couldn't find resource" );
	}

	if( nresources > 1 ) {
			// There are still more resources after this one is
			// deleted, so we'll need to make a new resources array
			// without this resource. 
		new_resources = new Resource* [ nresources - 1 ];
		j = 0;
		for( i = 0; i < nresources; i++ ) {
			if( i == dead ) {
				continue;
			} 
			new_resources[j++] = resources[i];
		}
	} 

		// Remove this rip from our destroy_list.
	destroy_list.Rewind();
	while( destroy_list.Next(rip2) ) {
		if( rip2 == rip ) {
			destroy_list.DeleteCurrent();
			break;
		}
	}

		// Now, delete the old array and start using the new one, if
		// it's there at all.  If not, it'll be NULL and this will all
		// still be what we want.
	delete [] resources;
	resources = new_resources;
	nresources--;
	
		// Return this Resource's ID to the dispenser.
	id_disp->insert( rip->r_id );

		// Tell the collector this Resource is gone.
	rip->final_update();

		// Log a message that we're going away
	rip->dprintf( D_ALWAYS, "Resource no longer needed, deleting\n" );

		// At last, we can delete the object itself.
	delete rip;

		// Now that a Resource is gone, see if we're done deleting
		// Resources and see if we should allocate any. 
	if( processAllocList() ) {
			// We're done allocating, so we can finish our reconfig. 
		finish_main_config();
	}
}


void
ResMgr::makeAdList( ClassAdList *list )
{
	ClassAd* ad;
	int i;

		// Make sure everything is current
	compute( A_TIMEOUT | A_UPDATE );

		// We want to insert ATTR_LAST_HEARD_FROM into each ad.  The
		// collector normally does this, so if we're servicing a
		// QUERY_STARTD_ADS commannd, we need to do this ourselves or
		// some timing stuff won't work. 
	char buf[1024];
	sprintf( buf, "%s = %d", ATTR_LAST_HEARD_FROM, (int)cur_time );

	for( i=0; i<nresources; i++ ) {
		ad = new ClassAd;
		resources[i]->publish( ad, A_ALL_PUB ); 
		ad->Insert( buf );
		list->Insert( ad );
	}
}


bool
ResMgr::processAllocList( void )
{
	if( ! destroy_list.IsEmpty() ) {
			// Can't start allocating until everything has been
			// destroyed.
		return false;
	}
	if( alloc_list.IsEmpty() ) {
		return true;  // Since there's nothing to allocate...
	}

		// We're done destroying, and there's something to allocate.  
	int i, new_size, new_num = alloc_list.Number();
	new_size = nresources + new_num;
	Resource** new_resources = new Resource* [ new_size ];
	CpuAttributes* cap;

		// Copy over the old Resource pointers.  If nresources is 0
		// (b/c we used to be configured to have no VMs), this won't
		// copy anything (and won't seg fault).
	memcpy( (void*)new_resources, (void*)resources, 
			(sizeof(Resource*)*nresources) );

		// Create the new Resource objects.
	alloc_list.Rewind();
	for( i=nresources; i<new_size; i++ ) {
		alloc_list.Next(cap);
		new_resources[i] = new Resource( cap, id_disp->next() );
		alloc_list.DeleteCurrent();
	}	

		// Switch over to new_resources:
	if( resources ) {
		delete [] resources;
	}
	resources = new_resources;
	nresources = new_size;
	delete [] type_nums;
	type_nums = new_type_nums;
	new_type_nums = NULL;

	return true; 	// Since we're done allocating.
}


void
ResMgr::check_use( void ) 
{
	int now = time(NULL);
	if( hasAnyClaim() ) {
		last_in_use = now;
	}
	if( ! startd_noclaim_shutdown ) {
			// Nothing to do.
		return;
	}
	if( now - last_in_use > startd_noclaim_shutdown ) {
			// We've been unused for too long, send a SIGTERM to our
			// parent, the condor_master. 
		dprintf( D_ALWAYS, 
				 "No resources have been claimed for %d seconds\n", 
				 startd_noclaim_shutdown );
		dprintf( D_ALWAYS, "Shutting down Condor on this machine.\n" );
		daemonCore->Send_Signal( daemonCore->getppid(), SIGTERM );
	}
}


int
ownerStateCmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2, diff;
	float fval1, fval2;
	State s;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);
		// Since the State enum is already in the "right" order for
		// this kind of sort, we don't need to do anything fancy, we
		// just cast the state enum to an int and we're done.
	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val1 - val2;
	if( diff ) {
		return diff;
	}
		// We're still here, means we've got the same state.  If that
		// state is "Claimed" or "Preempting", we want to break ties
		// w/ the Rank expression, else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval1 - fval2);
		return diff;
	} 
	return 0;
}


// This is basically the same as above, except we want it in exactly
// the opposite order, so reverse the signs.
int
claimedRankCmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2, diff;
	float fval1, fval2;
	State s;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);

	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val2 - val1;
	if( diff ) {
		return diff;
	}
		// We're still here, means we've got the same state.  If that
		// state is "Claimed" or "Preempting", we want to break ties
		// w/ the Rank expression, else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval2 - fval1);
		return diff;
	} 
	return 0;
}


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
int
newCODClaimCmp( const void* a, const void* b )
{
	Resource *rip1, *rip2;
	int val1, val2, diff;
	int numCOD1, numCOD2;
	float fval1, fval2;
	State s;
	rip1 = *((Resource**)a);
	rip2 = *((Resource**)b);

	numCOD1 = rip1->r_cod_mgr->numClaims();
	numCOD2 = rip2->r_cod_mgr->numClaims();

		// In the first case, sort based on # of COD claims
	diff = numCOD1 - numCOD2;
	if( diff ) {
		return diff;
	}

		// If we're still here, we've got same # of COD claims, so
		// sort based on State.  Since the state enum is already in
		// the "right" order for this kind of sort, we don't need to
		// do anything fancy, we just cast the state enum to an int
		// and we're done.
	s = rip1->state();
	val1 = (int)s;
	val2 = (int)rip2->state();
	diff = val1 - val2;
	if( diff ) {
		return diff;
	}

		// We're still here, means we've got the same number of COD
		// claims and the same state.  If that state is "Claimed" or
		// "Preempting", we want to break ties w/ the Rank expression,
		// else, don't worry about ties.
	if( s == claimed_state || s == preempting_state ) {
		fval1 = rip1->r_cur->rank();
		fval2 = rip2->r_cur->rank();
		diff = (int)(fval1 - fval2);
		return diff;
	} 
	return 0;
}






IdDispenser::IdDispenser( int size, int seed ) :
	free_ids(size+2)
{
	int i;
	free_ids.setFiller(true);
	free_ids.fill(true);
	for( i=0; i<seed; i++ ) {
		free_ids[i] = false;
	}
}


int
IdDispenser::next( void )
{
	int i;
	for( i=1 ; ; i++ ) {
		if( free_ids[i] ) {
			free_ids[i] = false;
			return i;
		}
	}
}


void
IdDispenser::insert( int id )
{
	if( free_ids[id] ) {
		EXCEPT( "IdDispenser::insert: %d is already free", id );
	}
	free_ids[id] = true;
}
