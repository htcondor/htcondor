#ifndef _CALC_H
#define _CALC_H

extern "C" {
	float calc_load_avg();
	int calc_disk();
	int	calc_phys_memory();
	int	calc_virt_memory();
	int calc_ncpus();
	int calc_kflops();
	int calc_mips();
	int dhry_mips();
	int clinpack_kflops();
}
void calc_idle_time(time_t &, time_t &);
time_t utmp_pty_idle_time( time_t );
time_t all_pty_idle_time( time_t );
time_t dev_idle_time( char*, time_t );

#endif _CALC_H
