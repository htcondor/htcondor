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


 

#undef VERBOSE   /* patch_register debug messages if VERBOSE defined */

#include <string.h>
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
	unsigned int sr_reg[N_SPACE_REGS];
};


#include <sys/_inttypes.h>

extern "C" {
#include <ucontext.h>
}

void
patch_registers( void *generic_ptr )
{
	ucontext_t	*ucp = (ucontext_t *)generic_ptr;
	save_state_t *ss = (save_state_t *) &(ucp->uc_mcontext);
	frame_marker_t *frame;
	int		i, j, fromwide;
	struct reginfo rinfo; 
	struct reginfo wide_rinfo;
	unsigned int old_spaces[N_SPACE_REGS];
	unsigned int new_spaces[N_SPACE_REGS];
	unsigned int old_pcsq_head, old_pcsq_tail;
	unsigned int new_pcsq_head, new_pcsq_tail;
	unsigned int wide_old_spaces[N_SPACE_REGS];
	unsigned int wide_new_spaces[N_SPACE_REGS];
	unsigned int wide_old_pcsq_head, wide_old_pcsq_tail;
	unsigned int wide_new_pcsq_head, wide_new_pcsq_tail;
	ucontext_t cur_uc;

	/* zero-out all wide_* variables, to make certain we dont get
	 * strange hi-order addresses when restarting a checkpoint created
	 * on a 64-bit CPU on a 32-bit CPU */
	memset(wide_old_spaces,0,N_SPACE_REGS * sizeof(unsigned int));
	memset(wide_new_spaces,0,N_SPACE_REGS * sizeof(unsigned int));
	memset(&wide_rinfo,0,sizeof(struct reginfo));
	wide_old_pcsq_head = 0;
	wide_old_pcsq_tail = 0;
	wide_new_pcsq_head = 0;
	wide_new_pcsq_tail = 0;
	
	/* Now grab the current space register values */
	/* get_reginfo( &rinfo ); */
	getcontext( &cur_uc );


	/* Now read in the old space register values.  They will be stored in
	 * a different place depending upon if the checkpoint was created
	 * on a PA-RISC 2.0 (64-bit registers) or not */
	if ( UseWideRegs(ss) ) {
		/* checkpoint containts 64-bit space registers */
#ifdef VERBOSE
		printf("Reading out WIDE registers from ckpt\n");
#endif
		fromwide = 1;
		old_spaces[0] = ss->ss_wide.ss_32.ss_sr0_lo;
		old_spaces[1] = ss->ss_wide.ss_32.ss_sr1_lo;
		old_spaces[2] = ss->ss_wide.ss_32.ss_sr2_lo;
		old_spaces[3] = ss->ss_wide.ss_32.ss_sr3_lo;
		old_spaces[4] = ss->ss_wide.ss_32.ss_sr4_lo;
		old_spaces[5] = ss->ss_wide.ss_32.ss_sr5_lo;
		old_spaces[6] = ss->ss_wide.ss_32.ss_sr6_lo;
		old_spaces[7] = ss->ss_wide.ss_32.ss_sr7_lo;
		wide_old_spaces[0] = ss->ss_wide.ss_32.ss_sr0_hi;
		wide_old_spaces[1] = ss->ss_wide.ss_32.ss_sr1_hi;
		wide_old_spaces[2] = ss->ss_wide.ss_32.ss_sr2_hi;
		wide_old_spaces[3] = ss->ss_wide.ss_32.ss_sr3_hi;
		wide_old_spaces[4] = ss->ss_wide.ss_32.ss_sr4_hi;
		wide_old_spaces[5] = ss->ss_wide.ss_32.ss_sr5_hi;
		wide_old_spaces[6] = ss->ss_wide.ss_32.ss_sr6_hi;
		wide_old_spaces[7] = ss->ss_wide.ss_32.ss_sr7_hi;
		old_pcsq_head = ss->ss_wide.ss_32.ss_pcsq_head_lo;
		wide_old_pcsq_head = ss->ss_wide.ss_32.ss_pcsq_head_hi;
		old_pcsq_tail = ss->ss_wide.ss_32.ss_pcsq_tail_lo;
		wide_old_pcsq_tail = ss->ss_wide.ss_32.ss_pcsq_tail_hi;
	} else {
		/* checkpoint contains 32-bit space registers */
#ifdef VERBOSE
		printf("Reading out NARROW registers from ckpt\n");
#endif
		fromwide = 0;
		old_spaces[0] = ss->ss_narrow.ss_sr0;
		old_spaces[1] = ss->ss_narrow.ss_sr1;
		old_spaces[2] = ss->ss_narrow.ss_sr2;
		old_spaces[3] = ss->ss_narrow.ss_sr3;
		old_spaces[4] = ss->ss_narrow.ss_sr4;
		old_spaces[5] = ss->ss_narrow.ss_sr5;
		old_spaces[6] = ss->ss_narrow.ss_sr6;
		old_spaces[7] = ss->ss_narrow.ss_sr7;
		old_pcsq_head = ss->ss_narrow.ss_pcsq_head;
		old_pcsq_tail = ss->ss_narrow.ss_pcsq_tail;
	}

	/* Now gather up the current space register values, compare
	 * them to the old values we dredged up above, and patch them.
	 * Again, where we get/patch the current space registers 
	 * depends upon if the _restart_ machine is using 64-bit wide
	 * space registers or not.  */
	if ( !(UseWideRegs(&(cur_uc.uc_mcontext))) ||
		 ( (UseWideRegs(&(cur_uc.uc_mcontext))) && (!fromwide) &&  
			(!(cur_uc.uc_mcontext.ss_flags & SS_NARROWISINVALID)) ) ) { 
	/* here we want to restart using narrow (32-bit) registers */

#ifdef VERBOSE
	printf("about to modify NARROW regs\n");
	printf("SS_NARROWISINVALID = %x\n",cur_uc.uc_mcontext.ss_flags & SS_NARROWISINVALID);
#endif

	if ( UseWideRegs(&(cur_uc.uc_mcontext)) ) {
#ifdef VERBOSE
		printf("Patching cuz ckpt came from narrow!!!\n");
#endif
		ss->ss_flags |= SS_WIDEREGS;
		ss->ss_wide.ss_32.ss_gr1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr1_hi;
		ss->ss_wide.ss_32.ss_rp_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_rp_hi;
		ss->ss_wide.ss_32.ss_gr3_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr3_hi;
		ss->ss_wide.ss_32.ss_gr4_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr4_hi;
		ss->ss_wide.ss_32.ss_gr5_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr5_hi;
		ss->ss_wide.ss_32.ss_gr6_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr6_hi;
		ss->ss_wide.ss_32.ss_gr7_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr7_hi;
		ss->ss_wide.ss_32.ss_gr8_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr8_hi;
		ss->ss_wide.ss_32.ss_gr9_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr9_hi;
		ss->ss_wide.ss_32.ss_gr10_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr10_hi;
		ss->ss_wide.ss_32.ss_gr11_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr11_hi;
		ss->ss_wide.ss_32.ss_gr12_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr12_hi;
		ss->ss_wide.ss_32.ss_gr13_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr13_hi;
		ss->ss_wide.ss_32.ss_gr14_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr14_hi;
		ss->ss_wide.ss_32.ss_gr15_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr15_hi;
		ss->ss_wide.ss_32.ss_gr16_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr16_hi;
		ss->ss_wide.ss_32.ss_gr17_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr17_hi;
		ss->ss_wide.ss_32.ss_gr18_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr18_hi;
		ss->ss_wide.ss_32.ss_gr19_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr19_hi;
		ss->ss_wide.ss_32.ss_gr20_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr20_hi;
		ss->ss_wide.ss_32.ss_gr21_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr21_hi;
		ss->ss_wide.ss_32.ss_gr22_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr22_hi;
		ss->ss_wide.ss_32.ss_arg3_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg3_hi;
		ss->ss_wide.ss_32.ss_arg2_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg2_hi;
		ss->ss_wide.ss_32.ss_arg1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg1_hi;
		ss->ss_wide.ss_32.ss_arg0_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg0_hi;
		ss->ss_wide.ss_32.ss_dp_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_dp_hi;
		ss->ss_wide.ss_32.ss_ret0_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_ret0_hi;
		ss->ss_wide.ss_32.ss_ret1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_ret1_hi;
		ss->ss_wide.ss_32.ss_sp_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sp_hi;
		ss->ss_wide.ss_32.ss_gr31_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr31_hi;
		ss->ss_wide.ss_32.ss_cr11_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr11_hi;
		ss->ss_wide.ss_32.ss_pcoq_head_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcoq_head_hi;
		ss->ss_wide.ss_32.ss_pcsq_head_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcsq_head_hi;
		ss->ss_wide.ss_32.ss_pcoq_tail_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcoq_tail_hi;
		ss->ss_wide.ss_32.ss_pcsq_tail_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcsq_tail_hi;
		ss->ss_wide.ss_32.ss_cr15_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr15_hi;
		ss->ss_wide.ss_32.ss_cr19_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr19_hi;
		ss->ss_wide.ss_32.ss_cr20_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr20_hi;
		ss->ss_wide.ss_32.ss_cr21_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr21_hi;
		ss->ss_wide.ss_32.ss_cr22_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr22_hi;
		ss->ss_wide.ss_32.ss_cpustate_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cpustate_hi;
		ss->ss_wide.ss_32.ss_sr4_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr4_hi;
		ss->ss_wide.ss_32.ss_sr0_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr0_hi;
		ss->ss_wide.ss_32.ss_sr1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr1_hi;
		ss->ss_wide.ss_32.ss_sr2_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr2_hi;
		ss->ss_wide.ss_32.ss_sr3_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr3_hi;
		ss->ss_wide.ss_32.ss_sr5_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr5_hi;
		ss->ss_wide.ss_32.ss_sr6_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr6_hi;
	}

	/* grab value of current space registers */
	rinfo.sr_reg[0] = cur_uc.uc_mcontext.ss_narrow.ss_sr0;
	rinfo.sr_reg[1] = cur_uc.uc_mcontext.ss_narrow.ss_sr1;
	rinfo.sr_reg[2] = cur_uc.uc_mcontext.ss_narrow.ss_sr2;
	rinfo.sr_reg[3] = cur_uc.uc_mcontext.ss_narrow.ss_sr3;
	rinfo.sr_reg[4] = cur_uc.uc_mcontext.ss_narrow.ss_sr4;
	rinfo.sr_reg[5] = cur_uc.uc_mcontext.ss_narrow.ss_sr5;
	rinfo.sr_reg[6] = cur_uc.uc_mcontext.ss_narrow.ss_sr6;
	rinfo.sr_reg[7] = cur_uc.uc_mcontext.ss_narrow.ss_sr7;
	
	/*
	 * now we determine what the new space registers should be
	 * hp-ux controls sr4-sr7, so they are the main ones.  sr0-sr3
	 * are set to point to the same ones in 4-7 as before.
	 * We also make ss_pcsq_* point into the same space as before.
	 * We still do 64-bit compares on old values in case this
	 * process came from a PA-RISC 2.0 machine
	 */

	for(i=0;i<N_SPACE_REGS;i++)
	{
		new_spaces[i] = rinfo.sr_reg[i];
		if (i < 4 && (old_spaces[i] || wide_old_spaces[i]) )
			for(j=4;j<N_SPACE_REGS;j++)
				if ((old_spaces[i] == old_spaces[j]) &&
					(wide_old_spaces[i] == wide_old_spaces[j]))
					new_spaces[i] = rinfo.sr_reg[j];
		if ((old_pcsq_head == old_spaces[i]) &&
			(wide_old_pcsq_head == wide_old_spaces[i]))
			new_pcsq_head = new_spaces[i];
		if ((old_pcsq_tail == old_spaces[i]) &&
			(wide_old_pcsq_tail == wide_old_spaces[i])) 
			new_pcsq_tail = new_spaces[i];

#ifdef VERBOSE
		printf("sr%d was 0x%x, now is 0x%x\n", i, old_spaces[i], new_spaces[i]);
#endif

	}    /* end of for i loop */

#ifdef VERBOSE
	printf( "pcsq_head was 0x%x, now is 0x%x\n", old_pcsq_head, new_pcsq_head );
	printf( "pcsq_tail was 0x%x, now is 0x%x\n", old_pcsq_tail, new_pcsq_tail );
#endif

	/* now assign the newly computed values back */
	ss->ss_narrow.ss_sr0 = new_spaces[0];
	ss->ss_narrow.ss_sr1 = new_spaces[1];
	ss->ss_narrow.ss_sr2 = new_spaces[2];
	ss->ss_narrow.ss_sr3 = new_spaces[3];
	ss->ss_narrow.ss_sr4 = new_spaces[4];
	ss->ss_narrow.ss_sr5 = new_spaces[5];
	ss->ss_narrow.ss_sr6 = new_spaces[6];
	ss->ss_narrow.ss_sr7 = new_spaces[7];
	ss->ss_narrow.ss_pcsq_head = new_pcsq_head;
	ss->ss_narrow.ss_pcsq_tail = new_pcsq_tail;

	/* tell HPUX we modifed in the narrow space by clearing the
	 * SS_MODIFIEDWIDE flag. */
	ss->ss_flags &= ~SS_MODIFIEDWIDE;


	}  /* end of using narrowregisters */
	else {   
	/* here we want wide (64-bit) registers */	

#ifdef VERBOSE
	printf("about to modify WIDE regs\n");
	printf("SS_NARROWISINVALID = %x\n",cur_uc.uc_mcontext.ss_flags & SS_NARROWISINVALID);
#endif

	/* if restarting a checkpoint from a narrow architecture, patch all the
	 * hi-order integer register values with something sane (i.e. current value) */
	if ( !fromwide ) {
#ifdef VERBOSE
		printf("patching!!!!!\n");
#endif
		ss->ss_wide.ss_32.ss_gr1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr1_hi;
		ss->ss_wide.ss_32.ss_rp_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_rp_hi;
		ss->ss_wide.ss_32.ss_gr3_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr3_hi;
		ss->ss_wide.ss_32.ss_gr4_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr4_hi;
		ss->ss_wide.ss_32.ss_gr5_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr5_hi;
		ss->ss_wide.ss_32.ss_gr6_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr6_hi;
		ss->ss_wide.ss_32.ss_gr7_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr7_hi;
		ss->ss_wide.ss_32.ss_gr8_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr8_hi;
		ss->ss_wide.ss_32.ss_gr9_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr9_hi;
		ss->ss_wide.ss_32.ss_gr10_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr10_hi;
		ss->ss_wide.ss_32.ss_gr11_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr11_hi;
		ss->ss_wide.ss_32.ss_gr12_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr12_hi;
		ss->ss_wide.ss_32.ss_gr13_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr13_hi;
		ss->ss_wide.ss_32.ss_gr14_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr14_hi;
		ss->ss_wide.ss_32.ss_gr15_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr15_hi;
		ss->ss_wide.ss_32.ss_gr16_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr16_hi;
		ss->ss_wide.ss_32.ss_gr17_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr17_hi;
		ss->ss_wide.ss_32.ss_gr18_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr18_hi;
		ss->ss_wide.ss_32.ss_gr19_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr19_hi;
		ss->ss_wide.ss_32.ss_gr20_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr20_hi;
		ss->ss_wide.ss_32.ss_gr21_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr21_hi;
		ss->ss_wide.ss_32.ss_gr22_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr22_hi;
		ss->ss_wide.ss_32.ss_arg3_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg3_hi;
		ss->ss_wide.ss_32.ss_arg2_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg2_hi;
		ss->ss_wide.ss_32.ss_arg1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg1_hi;
		ss->ss_wide.ss_32.ss_arg0_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_arg0_hi;
		ss->ss_wide.ss_32.ss_dp_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_dp_hi;
		ss->ss_wide.ss_32.ss_ret0_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_ret0_hi;
		ss->ss_wide.ss_32.ss_ret1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_ret1_hi;
		ss->ss_wide.ss_32.ss_sp_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sp_hi;
		ss->ss_wide.ss_32.ss_gr31_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_gr31_hi;
		ss->ss_wide.ss_32.ss_cr11_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr11_hi;
		ss->ss_wide.ss_32.ss_pcoq_head_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcoq_head_hi;
		ss->ss_wide.ss_32.ss_pcsq_head_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcsq_head_hi;
		ss->ss_wide.ss_32.ss_pcoq_tail_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcoq_tail_hi;
		ss->ss_wide.ss_32.ss_pcsq_tail_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_pcsq_tail_hi;
		ss->ss_wide.ss_32.ss_cr15_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr15_hi;
		ss->ss_wide.ss_32.ss_cr19_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr19_hi;
		ss->ss_wide.ss_32.ss_cr20_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr20_hi;
		ss->ss_wide.ss_32.ss_cr21_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr21_hi;
		ss->ss_wide.ss_32.ss_cr22_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cr22_hi;
		ss->ss_wide.ss_32.ss_cpustate_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_cpustate_hi;
		ss->ss_wide.ss_32.ss_sr4_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr4_hi;
		ss->ss_wide.ss_32.ss_sr0_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr0_hi;
		ss->ss_wide.ss_32.ss_sr1_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr1_hi;
		ss->ss_wide.ss_32.ss_sr2_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr2_hi;
		ss->ss_wide.ss_32.ss_sr3_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr3_hi;
		ss->ss_wide.ss_32.ss_sr5_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr5_hi;
		ss->ss_wide.ss_32.ss_sr6_hi = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr6_hi;
	}

	/* grab the value of the current space registers */
	rinfo.sr_reg[0] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr0_lo;
	rinfo.sr_reg[1] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr1_lo;
	rinfo.sr_reg[2] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr2_lo;
	rinfo.sr_reg[3] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr3_lo;
	rinfo.sr_reg[4] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr4_lo;
	rinfo.sr_reg[5] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr5_lo;
	rinfo.sr_reg[6] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr6_lo;
	rinfo.sr_reg[7] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr7_lo;
	wide_rinfo.sr_reg[0] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr0_hi;
	wide_rinfo.sr_reg[1] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr1_hi;
	wide_rinfo.sr_reg[2] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr2_hi;
	wide_rinfo.sr_reg[3] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr3_hi;
	wide_rinfo.sr_reg[4] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr4_hi;
	wide_rinfo.sr_reg[5] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr5_hi;
	wide_rinfo.sr_reg[6] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr6_hi;
	wide_rinfo.sr_reg[7] = cur_uc.uc_mcontext.ss_wide.ss_32.ss_sr7_hi;

	/*
	 * now we determine what the new space registers should be
	 * hp-ux controls sr4-sr7, so they are the main ones.  sr0-sr3
	 * are set to point to the same ones in 4-7 as before.
	 * We also make ss_pcsq_* point into the same space as before.
	 *
	 */

	for(i=0;i<N_SPACE_REGS;i++)
	{
		new_spaces[i] = rinfo.sr_reg[i];
		wide_new_spaces[i] = wide_rinfo.sr_reg[i];
		if (i < 4 && (old_spaces[i] || wide_old_spaces[i]))
			for(j=4;j<N_SPACE_REGS;j++)
				if ((wide_old_spaces[i] == wide_old_spaces[j]) &&
					(old_spaces[i] == old_spaces[j])) {
					new_spaces[i] = rinfo.sr_reg[j];
					wide_new_spaces[i] = wide_rinfo.sr_reg[j];
				}
		if ((wide_old_pcsq_head == wide_old_spaces[i]) &&
			(old_pcsq_head == old_spaces[i])) {
			new_pcsq_head = new_spaces[i];
			wide_new_pcsq_head = wide_new_spaces[i];
		}
		if ((wide_old_pcsq_tail == wide_old_spaces[i]) &&
			(old_pcsq_tail == old_spaces[i])) {
			new_pcsq_tail = new_spaces[i];
			wide_new_pcsq_tail = wide_new_spaces[i];
		}

#ifdef VERBOSE
		printf("sr%d was lo=0x%x,hi=0x%x, now is lo=0x%x,hi=0x%x\n", i, old_spaces[i], wide_old_spaces[i], new_spaces[i], wide_new_spaces[i]);
#endif

	}    /* end of for i loop */

#ifdef VERBOSE
	printf( "pcsq_head was lo=0x%x,hi=0x%x; now is lo=0x%x,hi=0x%x\n", old_pcsq_head, wide_old_pcsq_head, new_pcsq_head, wide_new_pcsq_head );
	printf( "pcsq_tail was lo=0x%x,hi=0x%x; now is lo=0x%x,hi=0x%x\n", old_pcsq_tail, wide_old_pcsq_tail, new_pcsq_tail, wide_new_pcsq_tail );
#endif


	/* now assign the newly computed values back */

	ss->ss_wide.ss_32.ss_sr0_lo = new_spaces[0];
	ss->ss_wide.ss_32.ss_sr1_lo = new_spaces[1];
	ss->ss_wide.ss_32.ss_sr2_lo = new_spaces[2];
	ss->ss_wide.ss_32.ss_sr3_lo = new_spaces[3];
	ss->ss_wide.ss_32.ss_sr4_lo = new_spaces[4];
	ss->ss_wide.ss_32.ss_sr5_lo = new_spaces[5];
	ss->ss_wide.ss_32.ss_sr6_lo = new_spaces[6];
	ss->ss_wide.ss_32.ss_sr7_lo = new_spaces[7];
	ss->ss_wide.ss_32.ss_pcsq_head_lo = new_pcsq_head;
	ss->ss_wide.ss_32.ss_pcsq_tail_lo = new_pcsq_tail;
	ss->ss_wide.ss_32.ss_sr0_hi = wide_new_spaces[0];
	ss->ss_wide.ss_32.ss_sr1_hi = wide_new_spaces[1];
	ss->ss_wide.ss_32.ss_sr2_hi = wide_new_spaces[2];
	ss->ss_wide.ss_32.ss_sr3_hi = wide_new_spaces[3];
	ss->ss_wide.ss_32.ss_sr4_hi = wide_new_spaces[4];
	ss->ss_wide.ss_32.ss_sr5_hi = wide_new_spaces[5];
	ss->ss_wide.ss_32.ss_sr6_hi = wide_new_spaces[6];
	ss->ss_wide.ss_32.ss_sr7_hi = wide_new_spaces[7];
	ss->ss_wide.ss_32.ss_pcsq_head_hi = wide_new_pcsq_head;
	ss->ss_wide.ss_32.ss_pcsq_tail_hi = wide_new_pcsq_tail;

	/* tell HPUX we modified registers in the _wide_ area
	 * by setting the SS_MODIFIEDWIDE flag  */
	ss->ss_flags |= SS_MODIFIEDWIDE;

	/* and set the SS_WIDEREGS flag, in case we are restarting a checkpoint created
	 * on a 32-bit CPU */
	ss->ss_flags |= SS_WIDEREGS;

	}	/* end of want wide registers */

}
