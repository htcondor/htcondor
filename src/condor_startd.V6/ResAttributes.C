#include "startd.h"
static char *_FileName_ = __FILE__;

ResAttributes::ResAttributes( Resource* rip )
{
#if !defined(WIN32)
	r_afs_info = new AFS_Info();
#endif
	r_mips = -1;
	r_kflops = -1;
	this->rip = rip;
	update( rip->r_classad );
	timeout( rip->r_classad );
}


ResAttributes::~ResAttributes()
{
#if !defined(WIN32)
	delete r_afs_info;
#endif
}


void
ResAttributes::update( ClassAd* cp) 
{
	char line[100];

	r_virtmem = calc_virt_memory();
	dprintf( D_FULLDEBUG, "Swap space: %lu\n", r_virtmem );
	sprintf( line, "%s=%lu", ATTR_VIRTUAL_MEMORY, r_virtmem );
 	cp->Insert( line ); 

	r_disk = calc_disk();
	dprintf( D_FULLDEBUG, "Disk space: %lu\n", r_disk );
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


void
ResAttributes::timeout( ClassAd* cp ) 
{
	char line[100];
	float tmp;

	r_load = calc_load_avg();
	sprintf( line, "%s=%f", ATTR_LOAD_AVG, r_load );
	cp->Insert(line);

	r_condor_load = rip->condor_load();
	sprintf( line, "%s=%f", ATTR_CONDOR_LOAD_AVG, r_condor_load );
	cp->Insert(line);

	tmp = r_load - r_condor_load;
	if( tmp < 0 ) {
		tmp = 0;
	}
	dprintf( D_LOAD, 
			 "SystemLoad: %.3f\tCondorLoad: %.3f\tNonCondorLoad: %.3f\n", 
			 r_load, r_condor_load, tmp );

	calc_idle_time( r_idle, r_console_idle );

	sprintf(line, "%s=%d", ATTR_KEYBOARD_IDLE, r_idle );
	cp->Insert(line); 
  
	// ConsoleIdle cannot be determined on all platforms; thus, only
	// advertise if it is not -1.
	if( r_console_idle != -1 ) {
		sprintf( line, "%s=%d", ATTR_CONSOLE_IDLE, r_console_idle );
		cp->Insert(line); 
	}
}


void
ResAttributes::benchmark(void)
{
	ClassAd* cp = rip->r_classad;	

	if( rip->state() != unclaimed_state ) {
		dprintf( D_ALWAYS, 
				 "Tried to run benchmarks when not unclaimed, aborting.\n" );
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
	if ( r_kflops == 0 ) {
			// first time we've done benchmarks
		r_kflops = new_kflops_calc;
	} else {
			// compute a weighted average
		r_kflops = (r_kflops * 3 + new_kflops_calc) / 4;
	}

	dprintf( D_FULLDEBUG, "recalc:DHRY_MIPS=%d, CLINPACK KFLOPS=%d\n",
			 r_mips, r_kflops);

	char tmp[100];
	sprintf( tmp, "%s=%d", ATTR_LAST_BENCHMARK, (int)time(NULL) );
	cp->InsertOrUpdate( tmp );

	rip->change_state( idle_act );
}


#if !defined(WIN32)
char*
ResAttributes::afs_cell()
{
	return r_afs_info->my_cell();
}
#endif


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
