/* 
** Copyright 1994 by Miron Livny, and Mike Litzkow
** 
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted,
** provided that the above copyright notice appear in all copies and that
** both that copyright notice and this permission notice appear in
** supporting documentation, and that the names of the University of
** Wisconsin and the copyright holders not be used in advertising or
** publicity pertaining to distribution of the software without specific,
** written prior permission.  The University of Wisconsin and the 
** copyright holders make no representations about the suitability of this
** software for any purpose.  It is provided "as is" without express
** or implied warranty.
** 
** THE UNIVERSITY OF WISCONSIN AND THE COPYRIGHT HOLDERS DISCLAIM ALL
** WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE UNIVERSITY OF
** WISCONSIN OR THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT
** OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
** OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
** OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
** OR PERFORMANCE OF THIS SOFTWARE.
** 
** Author:  Mike Greger
**
*/ 

#include <unistd.h>
#include <stdio.h>
#include "condor_debug.h"
#include "image.h"
#include "condor_syscalls.h"


// Class to hold proc file system stuff
class Proc {
public:
	Proc();
	void read_proc();
	long data_start_addr();
	long data_end_addr();
	long stack_start_addr();
	long stack_end_addr();
private:
	long	proc_pid;
	char	proc_comm[PATH_MAX];
	char	proc_state;
	long	proc_ppid;
	long	proc_pgrp;
	long	proc_session;
	long	proc_tty;
	long	proc_tpgid;
	unsigned long	proc_flags;
	unsigned long	proc_min_flt;
	unsigned long	proc_cmin_flt;
	unsigned long	proc_maj_flt;
	unsigned long	proc_cmaj_flt;
	long	proc_utime;
	long	proc_stime;
	long	proc_cutime;
	long	proc_cstime;
	long	proc_counter;
	long	proc_priority;
	unsigned long	proc_timeout;
	unsigned long	proc_it_real_value;
	long	proc_start_time;
	unsigned long	proc_vsize;
	unsigned long	proc_rss;
	unsigned long	proc_rlim;
	unsigned long	proc_start_code;
	unsigned long	proc_end_code;
	unsigned long	proc_stack_start_addr;
	unsigned long	proc_kstk_esp;
	unsigned long	proc_kstk_eip;
	long	proc_signal;
	long	proc_blocked;
	long	proc_sigignore;
	long	proc_sigcatch;
	unsigned long	proc_wchan;
	long	proc_stack_end_addr;
};

Proc::Proc()
{
}

void Proc::read_proc()
{
	int		scm;
	pid_t		pid;
	FILE		*proc;
	char		fn[PATH_MAX];


        scm = SetSyscalls( SYS_LOCAL | SYS_UNMAPPED );
        pid = getpid();

	sprintf(fn ,"/proc/%d/stat", pid);
        proc=fopen(fn, "r");
	if(!proc) {
		printf("Can't open %s for reading\n", fn);
	}

	fscanf(proc, "%d %s %c %d %d %d %d %d",
		&proc_pid, proc_comm, &proc_state, &proc_ppid, &proc_pgrp,
		&proc_session, &proc_tty, &proc_tpgid);
	fscanf(proc, "%u %u %u %u %u",
		&proc_flags, &proc_min_flt, &proc_cmin_flt,
		&proc_maj_flt, &proc_cmaj_flt);
	fscanf(proc, "%d %d %d %d %d %d",
		&proc_utime, &proc_stime, &proc_cutime, &proc_cstime, 
		&proc_counter, &proc_priority);
	fscanf(proc, "%u %u %d",
		&proc_timeout, &proc_it_real_value, &proc_start_time);
	fscanf(proc, "%u %u %u %u %u %u %u %u",
		&proc_vsize, &proc_rss, &proc_rlim, &proc_start_code,
		&proc_end_code, &proc_stack_start_addr, &proc_kstk_esp,
		&proc_kstk_eip);
	fscanf(proc, "%d %d %d %d %u",
		&proc_signal, &proc_blocked, &proc_sigignore, &proc_sigcatch,
		&proc_wchan);

        fclose(proc);
	SetSyscalls(scm);
	proc_stack_end_addr = 0xc0000000;
}


long Proc::data_start_addr()
{
	read_proc();
	return((long)proc_end_code);

//Not correct yet.
//	jmp_buf		env;

//        (void)SETJMP( env );
//	return(long)env->__bp;
}
long Proc::data_end_addr()
{
	read_proc();
	return(long)sbrk(0);
}
long Proc::stack_start_addr()
{
	read_proc();
//	return((long)proc_stack_start_addr);

//Correct ??
	jmp_buf		env;

	(void)SETJMP( env );
	return JMP_BUF_SP(env) & ~1023;
}
long Proc::stack_end_addr()
{
	read_proc();
	return((long)proc_stack_end_addr-1);
}

/*
  Return starting address of the data segment
*/
long
data_start_addr()
{
	Proc	p;
	/*printf("Data Start: 0x%x\n", p.data_start_addr());*/
	return(p.data_start_addr());
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
	Proc	p;
	/*printf("Data End  : 0x%x\n", p.data_end_addr());*/
	return(p.data_end_addr());
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
BOOL StackGrowsDown()
{
	return TRUE;
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
	return 4;
}

/*
  Return starting address of stack segment.
*/
long
stack_start_addr()
{
	Proc	p;
	return(p.stack_start_addr());
}

/*
  Return ending address of stack segment.
*/
long
stack_end_addr()
{
	Proc	p;
	return(p.stack_end_addr());
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/
void
patch_registers( void *generic_ptr )
{
	//Any Needed??
}
