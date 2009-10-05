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


#include "condor_common.h"
#include "startd.h"
#include <math.h>


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
	m_idle_interval = -1;
	m_ckptpltfrm = NULL;

	m_clock_day = -1;
	m_clock_min = -1;
	m_condor_load = -1.0;
	m_console_idle = 0;
	m_idle = 0;
	m_load = -1.0;
	m_owner_load = -1.0;
	m_virt_mem = 0;

		// Number of CPUs.  Since this is used heavily by the ResMgr
		// instantiation and initialization, we need to have a real
		// value for this as soon as the MachAttributes object exists.
	m_num_cpus = sysapi_ncpus();

	m_num_real_cpus = sysapi_ncpus_raw();

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

	dprintf( D_FULLDEBUG, "Memory: Detected %d megs RAM\n", m_phys_mem );

		// identification of the checkpointing platform signature
	m_ckptpltfrm = strdup(sysapi_ckptpltfrm());

#if defined ( WIN32 )
	// Get the version information of the copy of Windows 
	// we are running
	ZeroMemory ( &m_window_version_info, sizeof ( OSVERSIONINFOEX ) );
	m_window_version_info.dwOSVersionInfoSize = 
		sizeof ( OSVERSIONINFOEX );	
	m_got_windows_version_info = 
		GetVersionEx ( (OSVERSIONINFO*) &m_window_version_info );
	if ( !m_got_windows_version_info ) {
		m_window_version_info.dwOSVersionInfoSize = 
			sizeof ( OSVERSIONINFO );	
		m_got_windows_version_info = 
			GetVersionEx ( (OSVERSIONINFO*) &m_window_version_info );
		if ( !m_got_windows_version_info ) {
			dprintf ( D_ALWAYS, "MachAttributes: failed to "
				"get Windows version information.\n" );
		}
	}
	m_local_credd = NULL;
	m_last_credd_test = 0;
#endif
}


MachAttributes::~MachAttributes()
{
	if( m_arch ) free( m_arch );
	if( m_opsys ) free( m_opsys );
	if( m_uid_domain ) free( m_uid_domain );
	if( m_filesystem_domain ) free( m_filesystem_domain );
	if( m_ckptpltfrm ) free( m_ckptpltfrm );
#if defined(WIN32)
	if( m_local_credd ) free( m_local_credd );
#endif
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

		m_idle_interval = param_integer( "IDLE_INTERVAL", -1 );

			// checkpoint platform signature
		if (m_ckptpltfrm) {
			free(m_ckptpltfrm);
		}
		m_ckptpltfrm = strdup(sysapi_ckptpltfrm());
    }


	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {

		m_virt_mem = sysapi_swap_space();
		dprintf( D_FULLDEBUG, "Swap space: %lu\n", m_virt_mem );

#if defined(WIN32)
		credd_test();
#endif
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
	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

			// STARTD_IP_ADDR
		cp->Assign( ATTR_STARTD_IP_ADDR,
					daemonCore->InfoCommandSinfulString() );

        cp->Assign( ATTR_ARCH, m_arch );

		cp->Assign( ATTR_OPSYS, m_opsys );

		cp->Assign( ATTR_UID_DOMAIN, m_uid_domain );

		cp->Assign( ATTR_FILE_SYSTEM_DOMAIN, m_filesystem_domain );

		cp->Assign( ATTR_HAS_IO_PROXY, true );

		cp->Assign( ATTR_CHECKPOINT_PLATFORM, m_ckptpltfrm );

#if defined ( WIN32 )
		// publish the Windows version information
		if ( m_got_windows_version_info ) {
			cp->Assign( ATTR_WINDOWS_MAJOR_VERSION, 
				(int)m_window_version_info.dwMajorVersion );
			cp->Assign( ATTR_WINDOWS_MINOR_VERSION, 
				(int)m_window_version_info.dwMinorVersion );
			cp->Assign( ATTR_WINDOWS_BUILD_NUMBER, 
				(int)m_window_version_info.dwBuildNumber );
			// publish the extended Windows version information if we
			// have it at our disposal
			if ( sizeof ( OSVERSIONINFOEX ) == 
				 m_window_version_info.dwOSVersionInfoSize ) {
				cp->Assign( ATTR_WINDOWS_SERVICE_PACK_MAJOR, 
					(int)m_window_version_info.wServicePackMajor );
				cp->Assign( ATTR_WINDOWS_SERVICE_PACK_MINOR, 
					(int)m_window_version_info.wServicePackMinor );
				cp->Assign( ATTR_WINDOWS_PRODUCT_TYPE, 
					(int)m_window_version_info.wProductType );
			}
		}
#endif

	}

	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_TOTAL_VIRTUAL_MEMORY, (int)m_virt_mem );

		cp->Assign( ATTR_TOTAL_CPUS, m_num_cpus );

		cp->Assign( ATTR_TOTAL_MEMORY, m_phys_mem );

			// KFLOPS and MIPS are only conditionally computed; thus, only
			// advertise them if we computed them.
		if ( m_kflops > 0 ) {
			cp->Assign( ATTR_KFLOPS, m_kflops );
		}
		if ( m_mips > 0 ) {
			cp->Assign( ATTR_MIPS, m_mips );
		}

#if defined(WIN32)
		if ( m_local_credd != NULL ) {
			cp->Assign(ATTR_LOCAL_CREDD, m_local_credd);
		}
#endif
	}

		// We don't want this inserted into the public ad automatically
	if( IS_UPDATE(how_much) || IS_TIMEOUT(how_much) ) {
		cp->Assign( ATTR_LAST_BENCHMARK, m_last_benchmark );
	}


	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_TOTAL_LOAD_AVG, rint(m_load * 100) / 100.0);
		
		cp->Assign( ATTR_TOTAL_CONDOR_LOAD_AVG, rint(m_condor_load * 100) / 100.0);
		
		cp->Assign( ATTR_CLOCK_MIN, m_clock_min );

		cp->Assign( ATTR_CLOCK_DAY, m_clock_day );
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

		// The MIPS calculation occassionally returns a negative number.
		// Our best guess for the cause is that times() is returning a
		// smaller value after the calculation than before. So we
		// explicitly check for that here and log what we see.
		// See condor-admin #14595
	dprintf( D_FULLDEBUG, "About to compute mips\n" );
#ifndef WIN32
	struct tms before, after;
	times(&before);
#endif
	int new_mips_calc = sysapi_mips();
#ifndef WIN32
	times(&after);
#endif
	dprintf( D_FULLDEBUG, "Computed mips: %d\n", new_mips_calc );
	if ( new_mips_calc <= 0 ) {
		dprintf( D_ALWAYS, "Non-positive MIPS calculated!\n" );
#ifndef WIN32
		dprintf( D_ALWAYS, "   ticks before: %ld, ticks after: %ld\n",
			 (long)before.tms_utime, (long)after.tms_utime );
#endif
	}

	if ( m_mips == -1 ) {
			// first time we've done benchmarks
		m_mips = new_mips_calc;
	} else if ( new_mips_calc > 0 ) {
			// compute a weighted average,
			// but only if our new calculation is positive
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
		// Update all ClassAds with this new value for LastBenchmark.
		resmgr->refresh_benchmarks();

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

#if defined(WIN32)
void
MachAttributes::credd_test()
{
	// Attempt to perform a NOP on our CREDD_HOST. This will test
	// our ability to authenticate with DAEMON-level auth, and thus
	// fetch passwords. If we succeed, we'll advertise the CREDD_HOST

	char *credd_host = param("CREDD_HOST");

	if (credd_host == NULL) {
		if (m_local_credd != NULL) {
			free(m_local_credd);
			m_local_credd = NULL;
		}
		return;
	}

	if (m_local_credd != NULL) {
		if (strcmp(m_local_credd, credd_host) == 0) {
			free(credd_host);
			return;
		}
		else {
			free(m_local_credd);
			m_local_credd = NULL;
			m_last_credd_test = 0;
		}
	}

	time_t now = time(NULL);
	double thresh = (double)param_integer("CREDD_TEST_INTERVAL", 300);
	if (difftime(now, m_last_credd_test) > thresh) {
		Daemon credd(DT_CREDD);
		if (credd.locate()) {
			Sock *sock = credd.startCommand(CREDD_NOP, Stream::reli_sock, 20);
			if (sock != NULL) {
				sock->decode();
				if (sock->end_of_message()) {
					m_local_credd = credd_host;
				}
			}
		}
		m_last_credd_test = now;
	}
	if (credd_host != m_local_credd) {
		free(credd_host);
	}
}
#endif

CpuAttributes::CpuAttributes( MachAttributes* map_arg, 
							  int slot_type,
							  int num_cpus, 
							  int num_phys_mem,
							  float virt_mem_fraction,
							  float disk_fraction,
							  MyString &execute_dir,
							  MyString &execute_partition_id )
{
	map = map_arg;
	c_type = slot_type;
	c_num_cpus = num_cpus;
	c_phys_mem = num_phys_mem;
	c_virt_mem_fraction = virt_mem_fraction;
	c_disk_fraction = disk_fraction;
	c_execute_dir = execute_dir;
	c_execute_partition_id = execute_partition_id;
	c_idle = -1;
	c_console_idle = -1;
	c_disk = 0;
	c_total_disk = 0;

	c_condor_load = -1.0;
	c_owner_load = -1.0;
	c_virt_mem = 0;
	rip = NULL;
}


void
CpuAttributes::attach( Resource* res_ip )
{
	this->rip = res_ip;
}


void
CpuAttributes::publish( ClassAd* cp, amask_t how_much )
{
	if( IS_UPDATE(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_VIRTUAL_MEMORY, (int)c_virt_mem );

		cp->Assign( ATTR_TOTAL_DISK, (int)c_total_disk );

		cp->Assign( ATTR_DISK, (int)c_disk );
	}

	if( IS_TIMEOUT(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_CONDOR_LOAD_AVG, rint(c_condor_load * 100) / 100.0 );

		cp->Assign( ATTR_LOAD_AVG, rint((c_owner_load + c_condor_load) * 100) / 100.0 );

		cp->Assign( ATTR_KEYBOARD_IDLE, (int)c_idle );
  
			// ConsoleIdle cannot be determined on all platforms; thus, only
			// advertise if it is not -1.
		if( c_console_idle != -1 ) {
			cp->Assign( ATTR_CONSOLE_IDLE, (int)c_console_idle );
		}
	}

	if( IS_STATIC(how_much) || IS_PUBLIC(how_much) ) {

		cp->Assign( ATTR_MEMORY, c_phys_mem );

		cp->Assign( ATTR_CPUS, c_num_cpus );
	}
}


void
CpuAttributes::compute( amask_t how_much )
{
	float val;

	if( IS_UPDATE(how_much) && IS_SHARED(how_much) ) {

			// Shared attributes that we only get a fraction of
		val = map->virt_mem() * c_virt_mem_fraction;
		c_virt_mem = (unsigned long)floor( val );
	}

	if( IS_TIMEOUT(how_much) && !IS_SHARED(how_much) ) {

		// Dynamic, non-shared attributes we need to actually compute
		c_condor_load = rip->compute_condor_load();

		c_total_disk = sysapi_disk_space(rip->executeDir());
		if (IS_UPDATE(how_much)) {
			dprintf(D_FULLDEBUG, "Total execute space: %lu\n", c_total_disk);
		}

		val = c_total_disk * c_disk_fraction;
		c_disk = (unsigned long)floor( val );
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
			 "Cpus: %d", c_num_cpus);

	if( c_phys_mem == AUTO_MEM ) {
		::dprintf( dflag | D_NOHEADER, 
				   ", Memory: auto" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Memory: %d",c_phys_mem );
	}
	

	if( IS_AUTO_SHARE(c_virt_mem_fraction) ) {
		::dprintf( dflag | D_NOHEADER, 
				   ", Swap: auto" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Swap: %.2f%%", 100*c_virt_mem_fraction );
	}
	

	if( IS_AUTO_SHARE(c_disk_fraction) ) {
		::dprintf( dflag | D_NOHEADER, 
				   ", Disk: auto\n" );
	}
	else {
		::dprintf( dflag | D_NOHEADER, 
				   ", Disk: %.2f%%\n", 100*c_disk_fraction );
	}
}


void
CpuAttributes::dprintf( int flags, const char* fmt, ... )
{
	va_list args;
	va_start( args, fmt );
	rip->dprintf_va( flags, fmt, args );
	va_end( args );
}


CpuAttributes&
CpuAttributes::operator+=( CpuAttributes& rhs )
{
	c_num_cpus += rhs.c_num_cpus;
	c_phys_mem += rhs.c_phys_mem;
	c_virt_mem_fraction += rhs.c_virt_mem_fraction;
	c_disk_fraction += rhs.c_disk_fraction;

	compute( A_TIMEOUT | A_UPDATE ); // Re-compute

	return *this;
}

CpuAttributes&
CpuAttributes::operator-=( CpuAttributes& rhs )
{
	c_num_cpus -= rhs.c_num_cpus;
	c_phys_mem -= rhs.c_phys_mem;
	c_virt_mem_fraction -= rhs.c_virt_mem_fraction;
	c_disk_fraction -= rhs.c_disk_fraction;

	compute( A_TIMEOUT | A_UPDATE ); // Re-compute

	return *this;
}

AvailAttributes::AvailAttributes( MachAttributes* map ):
	m_execute_partitions(500,MyStringHash,updateDuplicateKeys)
{
	a_num_cpus = map->num_cpus();
	a_phys_mem = map->phys_mem();
	a_phys_mem_auto_count = 0;
	a_virt_mem_fraction = 1.0;
	a_virt_mem_auto_count = 0;
}

AvailDiskPartition &
AvailAttributes::GetAvailDiskPartition(MyString const &execute_partition_id)
{
	AvailDiskPartition *a = NULL;
	if( m_execute_partitions.lookup(execute_partition_id,a) < 0 ) {
			// No entry found for this partition.  Create one.
		m_execute_partitions.insert( execute_partition_id, AvailDiskPartition() );
		m_execute_partitions.lookup(execute_partition_id,a);
		ASSERT(a);
	}
	return *a;
}

bool
AvailAttributes::decrement( CpuAttributes* cap ) 
{
	int new_cpus, new_phys_mem;
	float new_virt_mem, new_disk, floor = -0.000001f;
	
	new_cpus = a_num_cpus - cap->c_num_cpus;

	new_phys_mem = a_phys_mem;
	if( cap->c_phys_mem != AUTO_MEM ) {
		new_phys_mem -= cap->c_phys_mem;
	}

	new_virt_mem = a_virt_mem_fraction;
	if( !IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
		new_virt_mem -= cap->c_virt_mem_fraction;
	}

	AvailDiskPartition &partition = GetAvailDiskPartition( cap->executePartitionID() );
	new_disk = partition.m_disk_fraction;
	if( !IS_AUTO_SHARE(cap->c_disk_fraction) ) {
		new_disk -= cap->c_disk_fraction;
	}

	if( new_cpus < floor || new_phys_mem < floor || 
		new_virt_mem < floor || new_disk < floor ) {
		return false;
	} else {
		a_num_cpus = new_cpus;

		a_phys_mem = new_phys_mem;
		if( cap->c_phys_mem == AUTO_MEM ) {
			a_phys_mem_auto_count += 1;
		}

		a_virt_mem_fraction = new_virt_mem;
		if( IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
			a_virt_mem_auto_count += 1;
		}

		partition.m_disk_fraction = new_disk;
		if( IS_AUTO_SHARE(cap->c_disk_fraction) ) {
			partition.m_auto_count += 1;
		}
	}
	return true;
}

bool
AvailAttributes::computeAutoShares( CpuAttributes* cap )
{
	if( cap->c_phys_mem == AUTO_MEM ) {
		ASSERT( a_phys_mem_auto_count > 0 );
		int new_value = a_phys_mem / a_phys_mem_auto_count;
		if( new_value < 1 ) {
			return false;
		}
		cap->c_phys_mem = new_value;
	}

	if( IS_AUTO_SHARE(cap->c_virt_mem_fraction) ) {
		ASSERT( a_virt_mem_auto_count > 0 );
		float new_value = a_virt_mem_fraction / a_virt_mem_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_virt_mem_fraction = new_value;
	}

	if( IS_AUTO_SHARE(cap->c_disk_fraction) ) {
		AvailDiskPartition &partition = GetAvailDiskPartition( cap->c_execute_partition_id );
		ASSERT( partition.m_auto_count > 0 );
		float new_value = partition.m_disk_fraction / partition.m_auto_count;
		if( new_value <= 0 ) {
			return false;
		}
		cap->c_disk_fraction = new_value;
	}
	return true;
}


void
AvailAttributes::show_totals( int dflag, CpuAttributes *cap )
{
	AvailDiskPartition &partition = GetAvailDiskPartition( cap->c_execute_partition_id );
	::dprintf( dflag | D_NOHEADER, 
			 "Cpus: %d, Memory: %d, Swap: %.2f%%, Disk: %.2f%%\n",
			 a_num_cpus, a_phys_mem, 100*a_virt_mem_fraction,
			 100*partition.m_disk_fraction );
}
