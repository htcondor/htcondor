#include "condor_debug.h"
#include "sysapi.h"
#include "sysapi_externs.h"

/* this function will dump out the state of the _sysapi_* variables, then
   call sysapi_reconfig(), then dump out the stateof the variables again, 
   then actually call all of the sysapi_*(_raw)? functions and dump what
   those functions return. All of these functions are entry points and
   can be called seperately if need be to test stuff */

/* this function will dump the state of the cached variables in reconfig.C */
extern "C" void
sysapi_test_dump_internal_vars(void)
{
	dprintf(D_ALWAYS, "_sysapi_config = %s\n", 
		_sysapi_config==TRUE?"TRUE":"FALSE");

	dprintf(D_ALWAYS, "_sysapi_console_devices = %p\n",_sysapi_console_devices);
	dprintf(D_ALWAYS, "_sysapi_last_x_event = %d\n", _sysapi_last_x_event);
	dprintf(D_ALWAYS, "_sysapi_reserve_afs_cache = %s\n", 
		_sysapi_reserve_afs_cache==TRUE?"TRUE":"FALSE");
	dprintf(D_ALWAYS, "_sysapi_reserve_disk = %d\n", _sysapi_reserve_disk);
	dprintf(D_ALWAYS, "_sysapi_startd_has_bad_utmp = %s\n",
		_sysapi_startd_has_bad_utmp==TRUE?"TRUE":"FALSE");
}

/* this function calls every function in sysapi that makes sense to call and
   prints out its value */
extern "C" void
sysapi_test_dump_functions(void)
{
	int foo = 0;
	float bar = 0;
	char *qux = NULL;
	time_t t0, t1;

	foo = sysapi_phys_memory_raw();
	dprintf(D_ALWAYS, "sysapi_phys_memory_raw() -> %d\n", foo);
	foo = sysapi_phys_memory();
	dprintf(D_ALWAYS, "sysapi_phys_memory() -> %d\n", foo);

	foo = sysapi_diskspace_raw("/");
	dprintf(D_ALWAYS, "sysapi_diskspace_raw() -> %d\n", foo);
	foo = sysapi_diskspace("/");
	dprintf(D_ALWAYS, "sysapi_diskspace() -> %d\n", foo);

	foo = sysapi_ncpus_raw();
	dprintf(D_ALWAYS, "sysapi_ncpus_raw() -> %d\n", foo);
	foo = sysapi_ncpus();
	dprintf(D_ALWAYS, "sysapi_ncpus() -> %d\n", foo);

	foo = sysapi_mips_raw();
	dprintf(D_ALWAYS, "sysapi_mips_raw() -> %d\n", foo);
	foo = sysapi_mips();
	dprintf(D_ALWAYS, "sysapi_mips() -> %d\n", foo);

	foo = sysapi_kflops_raw();
	dprintf(D_ALWAYS, "sysapi_kflops_raw() -> %d\n", foo);
	foo = sysapi_kflops();
	dprintf(D_ALWAYS, "sysapi_kflops() -> %d\n", foo);

	sysapi_idle_time_raw(&t0, &t1);
	dprintf(D_ALWAYS,"sysapi_idle_time_raw() -> (%f,%f)\n",(float)t0,(float)t1);
	sysapi_idle_time(&t0, &t1);
	dprintf(D_ALWAYS, "sysapi_idle_time() -> (%f,%f)\n", t0, t1);

	bar = sysapi_load_avg_raw();
	dprintf(D_ALWAYS, "sysapi_load_avg_raw() -> %f\n", bar);
	bar = sysapi_load_avg();
	dprintf(D_ALWAYS, "sysapi_load_avg() -> %f\n", bar);

	qux = sysapi_condor_arch();
	dprintf(D_ALWAYS, "sysapi_condor_arch -> %s\n", qux);
	free(qux);
	qux = sysapi_uname_arch();
	dprintf(D_ALWAYS, "sysapi_uname_arch -> %s\n", qux);
	free(qux);

	foo = sysapi_virt_memory_raw();
	dprintf(D_ALWAYS, "sysapi_virt_memory_raw() -> %d\n", foo);
	foo = sysapi_virt_memory();
	dprintf(D_ALWAYS, "sysapi_virt_memory() -> %d\n", foo);
}

/* the main entry function, this will do all the magic */
extern "C" void
sysapi_test_dump(void)
{
	sysapi_test_dump_internal_vars();
	sysapi_reconfig();
	sysapi_test_dump_internal_vars();

	sysapi_test_dump_functions();
}
