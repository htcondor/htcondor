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


MachAttributes::MachAttributes()
{
	m_mips = -1;
	m_kflops = -1;
	m_last_benchmark = 0;
	m_last_keypress = time(0)-1;
	m_seen_keypress = false;

	m_arch = NULL;
	m_opsys = NULL;
	m_uid_domain = NULL;
	m_filesystem_domain = NULL;
	m_subnet = NULL;
	m_idle_interval = -1;

		// Number of CPUs.  Since this is used heavily by the ResMgr
		// instantiation and initialization, we need to have a real
		// value for this as soon as the MachAttributes object exists.
	m_num_cpus = sysapi_ncpus();

		// The same is true of physical memory.  If we had an error in
		// sysapi_phys_memory(), we need to just EXCEPT with a message
		// telling the user to define MEMORY manually.
	m_phys_mem = sysapi_phys_memory();
	if( m_phys_mem <= 0 ) {
		dprintf( D_ALWAYS, 
				 "Error computing physical memory with calc_phys_mem().\n" );
		dprintf( D_ALWAYS | D_NOHEADER, 
				 "\t\tMEMORY parameter not defined in config file.\n" );
		dprintf( D_ALWAYS | D_NOHEADER, 
				 "\t\tTry setting MEMORY to the number of megabytes of RAM.\n"
				 );
		EXCEPT( "Can't compute physical memory." );
	}
}


MachAttributes::~MachAttributes()
{
	if( m_arch ) free( m_arch );
	if( m_opsys ) free( m_opsys );
	if( m_uid_domain ) free( m_uid_domain );
	if( m_filesystem_domain ) free( m_filesystem_domain );
	if( m_subnet ) free( m_subnet );
}


void
MachAttributes::init()
{
	this->compute( A_ALL );
}


void
MachAttributes::compute( amask_t how_much )
{
	if( IS_STATIC(how_much) && IS_SHARED(how_much) ) {

			// Since we need real values for them as soon as a
			// MachAttributes object is instantiated, we handle number
			// of CPUs and physical memory in the constructor, not
			// here.  -Derek Wright 2/5/99 

			// Arch, OpSys, FileSystemDomain and UidDomain.  Note:
			// these will always return something, since config() will
			// insert values if we don't have them in the config file.
		if( m_arch ) {
			free( m_arch );
		}
		m_arch = param( "ARCH" );

		if( m_opsys ) {
			free( m_opsys );
		}
		m_opsys = param( "OPSYS" );

		if( m_uid_domain ) {
			free( m_uid_domain );
		}
		m_uid_domain = param( "UID_DOMAIN" );
		dprintf( D_FULLDEBUG, "%s = \"%s\"\n", ATTR_UID_DOMAIN,
				 m_uid_domain );

		if( m_filesystem_domain ) {
			free( m_filesystem_domain );
		}
		m_filesystem_domain = param( "FILESYSTEM_DOMAIN" );
		dprintf( D_FULLDEBUG, "%s = \"%s\"\n", ATTR_FILE_SYSTEM_DOMAIN,
				 m_filesystem_domain );

			// Subnet name
		if( m_subnet ) {
			free( m_subnet );
		}
		m_subnet = calc_subnet_name( my_full_hostname() );
		dprintf( D_FULLDEBUG, "%s = \"%s\"\n", ATTR_SUBNET,
				 m_subnet );

		char *tmp = param( "IDLE_INTERVAL" );
		if (tmp) {
			m_idle_interval = atoi(tmp);
			free(tmp);
		} else {
			m_idle_interval = -1;
		}
	}


	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {
		m_virt_mem = sysapi_swap_space();
		dprintf( D_FULLDEBUG, "Swap space: %lu\n", m_virt_mem );
		
		m_disk = sysapi_disk_space(exec_path);
		dprintf( D_FULLDEBUG, "Disk space: %lu\n", m_disk );
	}


	if( IS_TIMEOUT(how_much) && IS_SHARED(how_much) ) {
		m_load = sysapi_load_avg();

		sysapi_idle_time( &m_idle, &m_console_idle );

		time_t my_timer;
		struct tm *the_time;
		time( &my_timer );
		the_time = localtime(&my_timer);
		m_clock_min = (the_time->tm_hour * 60) + the_time->tm_min;
		m_clock_day = the_time->tm_wday;

		if (m_last_keypress < my_timer - m_idle) {
			if (m_idle_interval >= 0) {
				int duration = my_timer - m_last_keypress;
				if (duration > m_idle_interval) {
					if (m_seen_keypress) {
						dprintf(D_IDLE, "end idle interval of %d sec.\n",
								duration);
					} else {
						dprintf(D_IDLE,
								"first keyboard event %d sec. after startup\n",
								duration);
					}
				}
			}
			m_last_keypress = my_timer;
			m_seen_keypress = true;
		}
	}

	if( IS_TIMEOUT(how_much) && IS_SUMMED(how_much) ) {
		m_condor_load = resmgr->sum( &Resource::condor_load );
		if( m_condor_load > m_load ) {
			m_condor_load = m_load;
		}
	}
}

void
MachAttributes::final_idle_dprintf()
{
	if (m_idle_interval >= 0) {
		time_t my_timer = time(0);
		int duration = my_timer - m_last_keypress;
		if (duration > m_idle_interval) {
			dprintf(D_IDLE, "keyboard idle for %d sec. before shutdown\n",
					duration);
		}
	}
}

void
MachAttributes::publish( ClassAd* cp, amask_t how_much) 
{
	char line[100];

	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

			// STARTD_IP_ADDR 
		sprintf( line, "%s = \"%s\"", ATTR_STARTD_IP_ADDR, 
				 daemonCore->InfoCommandSinfulString() );
		cp->Insert( line );

		sprintf( line, "%s = \"%s\"", ATTR_ARCH, m_arch );
		cp->Insert( line );

		sprintf( line, "%s = \"%s\"", ATTR_OPSYS, m_opsys );
		cp->Insert( line );

		sprintf( line, "%s = \"%s\"", ATTR_UID_DOMAIN, m_uid_domain );
		cp->Insert( line );

		sprintf( line, "%s = \"%s\"", ATTR_FILE_SYSTEM_DOMAIN, 
				 m_filesystem_domain );
		cp->Insert( line );

		sprintf( line, "%s = \"%s\"", ATTR_SUBNET, m_subnet );
		cp->Insert( line );

		sprintf( line, "%s = TRUE", ATTR_HAS_IO_PROXY );
		cp->Insert(line);
	}

	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		sprintf( line, "%s=%lu", ATTR_TOTAL_VIRTUAL_MEMORY, m_virt_mem );
		cp->Insert( line ); 

		sprintf( line, "%s=%lu", ATTR_TOTAL_DISK, m_disk );
		cp->Insert( line ); 

			// KFLOPS and MIPS are only conditionally computed; thus, only
			// advertise them if we computed them.
		if ( m_kflops > 0 ) {
			sprintf( line, "%s=%d", ATTR_KFLOPS, m_kflops );
			cp->Insert( line );
		}
		if ( m_mips > 0 ) {
			sprintf( line, "%s=%d", ATTR_MIPS, m_mips );
			cp->Insert( line );
		}
	}

		// We don't want this inserted into the public ad automatically
	if( IS_UPDATE(how_much) || IS_TIMEOUT(how_much) ) {
		sprintf( line, "%s=%d", ATTR_LAST_BENCHMARK, m_last_benchmark );
		cp->Insert( line );
	}


	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		sprintf( line, "%s=%.2f", ATTR_TOTAL_LOAD_AVG, m_load );
		cp->Insert(line);
		
		sprintf( line, "%s=%.2f", ATTR_TOTAL_CONDOR_LOAD_AVG, m_condor_load );
		cp->Insert(line);
		
		sprintf(line, "%s=%d", ATTR_CLOCK_MIN, m_clock_min );
		cp->Insert(line); 

		sprintf(line, "%s=%d", ATTR_CLOCK_DAY, m_clock_day );
		cp->Insert(line); 
	}
}


void
MachAttributes::benchmark( Resource* rip, int force )
{

	if( ! force ) {
		if( rip->state() != unclaimed_state &&
			rip->activity() != idle_act ) {
			dprintf( D_ALWAYS, 
					 "Tried to run benchmarks when not idle, aborting.\n" );
			return;
		}
			// Enter benchmarking activity
		dprintf( D_ALWAYS, "State change: %s is TRUE\n", 
				 ATTR_RUN_BENCHMARKS );
		rip->change_state( benchmarking_act );
	}

	dprintf( D_FULLDEBUG, "About to compute mips\n" );
	int new_mips_calc = sysapi_mips();
	dprintf( D_FULLDEBUG, "Computed mips: %d\n", new_mips_calc );

	if ( m_mips == -1 ) {
			// first time we've done benchmarks
		m_mips = new_mips_calc;
	} else {
			// compute a weighted average
		m_mips = (m_mips * 3 + new_mips_calc) / 4;
	}

	dprintf( D_FULLDEBUG, "About to compute kflops\n" );
	int new_kflops_calc = sysapi_kflops();
	dprintf( D_FULLDEBUG, "Computed kflops: %d\n", new_kflops_calc );
	if ( m_kflops == -1 ) {
			// first time we've done benchmarks
		m_kflops = new_kflops_calc;
	} else {
			// compute a weighted average
		m_kflops = (m_kflops * 3 + new_kflops_calc) / 4;
	}

	dprintf( D_FULLDEBUG, "recalc:DHRY_MIPS=%d, CLINPACK KFLOPS=%d\n",
			 m_mips, m_kflops);

	m_last_benchmark = (int)time(NULL);

	if( ! force ) {
		dprintf( D_ALWAYS, "State change: benchmarks completed\n" );
		rip->change_state( idle_act );
	}
}


void
deal_with_benchmarks( Resource* rip )
{
	ClassAd* cp = rip->r_classad;

	if( rip->isSuspendedForCOD() ) {
			// if there's a COD job, we definitely don't want to run
			// benchmarks
		return;
	}

	int run_benchmarks = 0;
	if( cp->EvalBool( ATTR_RUN_BENCHMARKS, cp, run_benchmarks ) == 0 ) {
		run_benchmarks = 0;
	}

	if( run_benchmarks ) {
		resmgr->m_attr->benchmark( rip );
	}
}


CpuAttributes::CpuAttributes( MachAttributes* map, 
							  int vm_type,
							  int num_cpus, 
							  int num_phys_mem,
							  float virt_mem_percent,
							  float disk_percent )
{
	this->map = map;
	c_type = vm_type;
	c_num_cpus = num_cpus;
	c_phys_mem = num_phys_mem;
	c_virt_mem_percent = virt_mem_percent;
	c_disk_percent = disk_percent;
	c_idle = -1;
	c_console_idle = -1;
}


void
CpuAttributes::attach( Resource* rip )
{
	this->rip = rip;
}


void
CpuAttributes::publish( ClassAd* cp, amask_t how_much )
{
	char line[100];

	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		sprintf( line, "%s=%lu", ATTR_VIRTUAL_MEMORY, c_virt_mem );
		cp->Insert( line ); 

		sprintf( line, "%s=%lu", ATTR_DISK, c_disk );
		cp->Insert( line ); 
	}

	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		sprintf( line, "%s=%.2f", ATTR_CONDOR_LOAD_AVG, c_condor_load );
		cp->Insert(line);

		sprintf( line, "%s=%.2f", ATTR_LOAD_AVG, 
				 (c_owner_load + c_condor_load) );
		cp->Insert(line);

		sprintf(line, "%s=%d", ATTR_KEYBOARD_IDLE, (int)c_idle );
		cp->Insert(line); 
  
			// ConsoleIdle cannot be determined on all platforms; thus, only
			// advertise if it is not -1.
		if( c_console_idle != -1 ) {
			sprintf( line, "%s=%d", ATTR_CONSOLE_IDLE, (int)c_console_idle );
			cp->Insert(line); 
		}
	}

	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

		sprintf( line, "%s=%d", ATTR_MEMORY, c_phys_mem );
		cp->Insert(line);

		sprintf( line, "%s=%d", ATTR_CPUS, c_num_cpus );
		cp->Insert(line);
	}
}


void
CpuAttributes::compute( amask_t how_much )
{
	float val;

	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {

			// Shared attributes that we only get a percentage of
		val = map->virt_mem() * c_virt_mem_percent;
		c_virt_mem = (unsigned long)floor( val );
		
		val = map->disk() * c_disk_percent;
		c_disk = (unsigned long)floor( val );
	}

	if( IS_TIMEOUT(how_much) && !IS_SHARED(how_much) ) {

		// Dynamic, non-shared attributes we need to actually compute
		c_condor_load = rip->compute_condor_load();
	}	
}


void
CpuAttributes::display( amask_t how_much )
{
	if( IS_UPDATE(how_much) ) {
		dprintf( D_KEYBOARD, 
				 "Idle time: %s %-8d %s %d\n",  
				 "Keyboard:", (int)c_idle, 
				 "Console:", (int)c_console_idle );

		dprintf( D_LOAD, 
				 "%s %.2f  %s %.2f  %s %.2f\n",  
				 "SystemLoad:", c_condor_load + c_owner_load,
				 "CondorLoad:", c_condor_load,
				 "OwnerLoad:", c_owner_load );
	} else {
		if( DebugFlags & D_LOAD ) {
			dprintf( D_FULLDEBUG, 
					 "%s %.2f  %s %.2f  %s %.2f\n",  
					 "SystemLoad:", c_condor_load + c_owner_load,
					 "CondorLoad:", c_condor_load,
					 "OwnerLoad:", c_owner_load );
		}
		if( DebugFlags & D_KEYBOARD ) {
			dprintf( D_FULLDEBUG, 
					 "Idle time: %s %-8d %s %d\n",  
					 "Keyboard:", (int)c_idle, 
					 "Console:", (int)c_console_idle );
		}
	}
}	


void
CpuAttributes::show_totals( int dflag )
{
	::dprintf( dflag | D_NOHEADER, 
			 "Cpus: %d, Memory: %d, Swap: %.2f%%, Disk: %.2f%%\n",
			 c_num_cpus, c_phys_mem, 100*c_virt_mem_percent, 
			 100*c_disk_percent );
}


void
CpuAttributes::dprintf( int flags, char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


AvailAttributes::AvailAttributes( MachAttributes* map )
{
	a_num_cpus = map->num_cpus();
	a_phys_mem = map->phys_mem();
	a_virt_mem_percent = 1.0;
	a_disk_percent = 1.0;
}


bool
AvailAttributes::decrement( CpuAttributes* cap ) 
{
	int new_cpus, new_phys_mem;
	float new_virt_mem, new_disk, floor = -0.000001f;
	
	new_cpus = a_num_cpus - cap->c_num_cpus;
	new_phys_mem = a_phys_mem - cap->c_phys_mem;
	new_virt_mem = a_virt_mem_percent - cap->c_virt_mem_percent;
	new_disk = a_disk_percent - cap->c_disk_percent;

	if( new_cpus < floor || new_phys_mem < floor || 
		new_virt_mem < floor || new_disk < floor ) {
		return false;
	} else {
		a_num_cpus = new_cpus;
		a_phys_mem = new_phys_mem;
		a_virt_mem_percent = new_virt_mem;
		a_disk_percent = new_disk;
	}
	return true;
}


void
AvailAttributes::show_totals( int dflag )
{
	::dprintf( dflag | D_NOHEADER, 
			 "Cpus: %d, Memory: %d, Swap: %.2f%%, Disk: %.2f%%\n",
			 a_num_cpus, a_phys_mem, 100*a_virt_mem_percent, 
			 100*a_disk_percent );
}
