#include "image.h"

/*
  Return starting address of the data segment
*/
extern int __data_start;
long
data_start_addr()
{
	return (long)&__data_start;
}

/*
  Return ending address of the data segment
*/
long
data_end_addr()
{
	return (long)sbrk(0);
}

/*
  Return TRUE if the stack grows toward lower addresses, and FALSE
  otherwise.
*/
BOOL StackGrowsDown()
{
	return FALSE;
}

/*
  Return the index into the jmp_buf where the stack pointer is stored.
  Expect that the jmp_buf will be viewed as an array of integers for
  this.
*/
int JmpBufSP_Index()
{
	return 1;
}

/*
  Return starting address of stack segment.
*/
#include <sys/param.h>
long
stack_start_addr()
{
	return USRSTACK; // Well known value
}

/*
  Return ending address of stack segment.
*/
long
stack_end_addr()
{
	jmp_buf env;
	(void)SETJMP( env );
	return (JMP_BUF_SP(env) + 1023) & ~1023; // Curr sp, rounded up
}

/*
  Patch any registers whose vaules should be different at restart
  time than they were at checkpoint time.
*/

#include <sys/signal.h>
#include <stdio.h>

#define N_SPACE_REGS 8

struct reginfo {
	int sr_reg[N_SPACE_REGS];
};

extern "C" int _sigreturn();
extern "C" void	get_reginfo( struct reginfo * );

void
patch_registers( void *generic_ptr )
{
	struct sigcontext	*scp = (struct sigcontext *)generic_ptr;
	struct save_state	*ss = &(scp->sc_sl.sl_ss);
	int		i, j;
	struct reginfo rinfo;
	int old_spaces[N_SPACE_REGS];
	int new_spaces[N_SPACE_REGS];
	int old_pcsq_head, old_pcsq_tail;
	int new_pcsq_head, new_pcsq_tail;

	scp->sc_sfm.fm_crp = (int)_sigreturn;
	get_reginfo( &rinfo );

	/* there should be a better way... */
	old_spaces[0] = ss->ss_sr0;
	old_spaces[1] = ss->ss_sr1;
	old_spaces[2] = ss->ss_sr2;
	old_spaces[3] = ss->ss_sr3;
	old_spaces[4] = ss->ss_sr4;
	old_spaces[5] = ss->ss_sr5;
	old_spaces[6] = ss->ss_sr6;
	old_spaces[7] = ss->ss_sr7;

	/*
	 * now we determine what the new space registers should be
	 * hp-ux controls sr4-sr7, so they are the main ones.  sr0-sr3
	 * are set to point to the same ones in 4-7 as before.
	 * We also make ss_pcsq_* point into the same space as before.
	 *
	 */


	old_pcsq_head = ss->ss_pcsq_head;
	old_pcsq_tail = ss->ss_pcsq_tail;

	for(i=0;i<N_SPACE_REGS;i++)
	{
		new_spaces[i] = rinfo.sr_reg[i];
		if (i < 4 && old_spaces[i])
			for(j=4;j<N_SPACE_REGS;j++)
				if (old_spaces[i] == old_spaces[j])
					new_spaces[i] = rinfo.sr_reg[j];
		if (ss->ss_pcsq_head == old_spaces[i])
			new_pcsq_head = new_spaces[i];
		if (ss->ss_pcsq_tail == old_spaces[i])
			new_pcsq_tail = new_spaces[i];
#if VERBOSE
		printf("sr%d was 0x%x, now is 0x%x\n", i, old_spaces[i], new_spaces[i]);
#endif
	}

#if VERBOSE
	printf( "pcsq_head was 0x%x, now is 0x%x\n", old_pcsq_head, new_pcsq_head );
	printf( "pcsq_tail was 0x%x, now is 0x%x\n", old_pcsq_tail, new_pcsq_tail );
#endif


	/* now assign the newly computed values back */

	ss->ss_sr0 = new_spaces[0];
	ss->ss_sr1 = new_spaces[1];
	ss->ss_sr2 = new_spaces[2];
	ss->ss_sr3 = new_spaces[3];
	ss->ss_sr4 = new_spaces[4];
	ss->ss_sr5 = new_spaces[5];
	ss->ss_sr6 = new_spaces[6];
	ss->ss_sr7 = new_spaces[7];
	ss->ss_pcsq_head = new_pcsq_head;
	ss->ss_pcsq_tail = new_pcsq_tail;
}
