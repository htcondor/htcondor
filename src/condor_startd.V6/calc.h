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
void calc_idle_time(int &, int &);

int tty_idle_time(char*);
#if !defined(OSF1)
int tty_pty_idle_time();
#else
time_t tty_pty_idle_time();
#endif

#endif _CALC_H
