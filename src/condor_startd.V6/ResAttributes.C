#include "startd.h"
static char *_FileName_ = __FILE__;

ResAttributes::ResAttributes( Resource* rip )
{
	this->rip = rip;
	this->update();
	this->timeout();
	r_mips = -1;
	r_kflops = -1;
}


void
ResAttributes::update(void) 
{
	r_virtmem = calc_virt_memory();
	dprintf( D_FULLDEBUG, "Swap space: %d\n", r_virtmem );

	r_disk = calc_disk();
	dprintf( D_FULLDEBUG, "Disk space: %d\n", r_disk );
}


void
ResAttributes::timeout(void) 
{
	r_load = calc_load_avg();
		// calc_load_avg already does a D_FULLDEBUG dprintf
//	dprintf( D_FULLDEBUG, "Load average: %f\n", r_load );

	r_condor_load = rip->condor_load();
	dprintf( D_FULLDEBUG, "Condor load: %f\n", r_condor_load );

	calc_idle_time( r_idle, r_console_idle );
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
