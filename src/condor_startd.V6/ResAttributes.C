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
#include "startd.h"
static char *_FileName_ = __FILE__;

ResAttributes::ResAttributes( Resource* rip )
{
#if !defined(WIN32)
	r_afs_info = new AFS_Info();
#endif
	r_mips = -1;
	r_kflops = -1;
	r_last_benchmark = 0;
	this->rip = rip;
}


ResAttributes::~ResAttributes()
{
#if !defined(WIN32)
	delete r_afs_info;
#endif
}


void
ResAttributes::compute( ResAttr_t how_much )
{
	if( how_much != TIMEOUT ) {
		r_virtmem = calc_virt_memory();
		dprintf( D_FULLDEBUG, "Swap space: %lu\n", r_virtmem );

		r_disk = calc_disk();
		dprintf( D_FULLDEBUG, "Disk space: %lu\n", r_disk );
	}

	if( how_much != UPDATE ) {
		r_load = calc_load_avg();
		r_condor_load = rip->condor_load();
		float tmp = r_load - r_condor_load;
		if( tmp < 0 ) {
			tmp = 0;
		}
		dprintf( D_LOAD, 
				 "SystemLoad: %.3f\tCondorLoad: %.3f\tNonCondorLoad: %.3f\n",  
				 r_load, r_condor_load, tmp );

		calc_idle_time( r_idle, r_console_idle );
	}
}


void
ResAttributes::refresh( ClassAd* cp, ResAttr_t how_much) 
{
	char line[100];

	if( how_much != TIMEOUT ) {
		sprintf( line, "%s=%lu", ATTR_VIRTUAL_MEMORY, r_virtmem );
		cp->Insert( line ); 

		sprintf( line, "%s=%lu", ATTR_DISK, r_disk );
		cp->Insert( line ); 

			// KFLOPS and MIPS are only conditionally computed; thus, only
			// advertise them if we computed them.
		if ( r_kflops > 0 ) {
			sprintf( line, "%s=%d", ATTR_KFLOPS, r_kflops );
			cp->Insert( line );
		}
		if ( r_mips > 0 ) {
			sprintf( line, "%s=%d", ATTR_MIPS, r_mips );
			cp->Insert( line );
		}
	}
		// We don't want this inserted into the public ad automatically
	if( how_much == UPDATE || how_much == ALL ) {
		sprintf( line, "%s=%d", ATTR_LAST_BENCHMARK, r_last_benchmark );
		cp->Insert( line );
	}

	if( how_much != UPDATE ) {
		sprintf( line, "%s=%f", ATTR_LOAD_AVG, r_load );
		cp->Insert(line);
		
		sprintf( line, "%s=%f", ATTR_CONDOR_LOAD_AVG, r_condor_load );
		cp->Insert(line);
		
		sprintf(line, "%s=%d", ATTR_KEYBOARD_IDLE, (int)r_idle );
		cp->Insert(line); 
  
			// ConsoleIdle cannot be determined on all platforms; thus, only
			// advertise if it is not -1.
		if( r_console_idle != -1 ) {
			sprintf( line, "%s=%d", ATTR_CONSOLE_IDLE, (int)r_console_idle );
			cp->Insert(line); 
		}
	}
}


void
ResAttributes::benchmark(void)
{
	if( rip->state() != unclaimed_state &&
		rip->activity() != idle_act ) {
		dprintf( D_ALWAYS, 
				 "Tried to run benchmarks when not idle, aborting.\n" );
		return;
	}

		// Enter benchmarking activity
	rip->change_state( benchmarking_act );

	dprintf( D_FULLDEBUG, "About to compute mips\n" );
	int new_mips_calc = calc_mips();
	dprintf( D_FULLDEBUG, "Computed mips: %d\n", new_mips_calc );

	if ( r_mips == -1 ) {
			// first time we've done benchmarks
		r_mips = new_mips_calc;
	} else {
			// compute a weighted average
		r_mips = (r_mips * 3 + new_mips_calc) / 4;
	}

	dprintf( D_FULLDEBUG, "About to compute kflops\n" );
	int new_kflops_calc = calc_kflops();
	dprintf( D_FULLDEBUG, "Computed kflops: %d\n", new_kflops_calc );
	if ( r_kflops == -1 ) {
			// first time we've done benchmarks
		r_kflops = new_kflops_calc;
	} else {
			// compute a weighted average
		r_kflops = (r_kflops * 3 + new_kflops_calc) / 4;
	}

	dprintf( D_FULLDEBUG, "recalc:DHRY_MIPS=%d, CLINPACK KFLOPS=%d\n",
			 r_mips, r_kflops);

	r_last_benchmark = (int)time(NULL);

	rip->change_state( idle_act );
}


void
deal_with_benchmarks( Resource* rip )
{
	ClassAd* cp = rip->r_classad;

	int run_benchmarks = 0;
	if( cp->EvalBool( ATTR_RUN_BENCHMARKS, cp, run_benchmarks ) == 0 ) {
		run_benchmarks = 0;
	}

	if( run_benchmarks ) {
		rip->r_attr->benchmark();
	}
}


void
ResAttributes::init(ClassAd* ca)
{
	char* ptr;
	char tmp[512];

		// Compute and insert all static info

#if !defined(WIN32) /* NEED TO PORT TO WIN32 */
		// AFS cell
	if( r_afs_info->my_cell() ) {
		sprintf( tmp, "%s = \"%s\"", ATTR_AFS_CELL,
				 r_afs_info->my_cell() );
		ca->Insert( tmp );
		dprintf( D_FULLDEBUG, "%s\n", tmp );
	} else {
		dprintf( D_FULLDEBUG, "AFS_Cell not set\n" );
	}
#endif

		// Physical memory
	r_phys_mem = calc_phys_memory();
	if( r_phys_mem <= 0 ) {
			// calc_phys_memory() failed to give us something useful,
			// try paraming. 
		if( (ptr = param("MEMORY")) != NULL ) {
			r_phys_mem = atoi(ptr);
			free(ptr);
		}
	}
	if( r_phys_mem > 0 ) {
		sprintf( tmp, "%s = %d", ATTR_MEMORY, r_phys_mem );
		ca->Insert( tmp );
	}

		// Compute and insert all dynamic info
	this->compute( ALL );
	this->refresh( ca, ALL );
}
