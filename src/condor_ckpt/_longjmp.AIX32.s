#*******************************************************************
# setjmp/longjmp for threads on the RS6000
#
# Note that special setjmp/longjmp routines are needed
# to override the stack pointer check in the C library
# setjmp/longjmp routines. There, a sanity check is made to
# ensure that the new stack pointer in a longjmp is always
# greater than the current stack pointer. This check is made
# to ensure that a user process only jumps back to an older stack
# frame. Unfortunately, with multiple thread stacks, this sort of
# sanity check doesn't make any sense.
#
# WARNING: These functions save only the stack context.
# They do not save the signal mask.
#*******************************************************************

include(`/usr/include/sys/comlink.m4')

        .file   "jmp.s"

	.set	jmpmask, 0	# signal mask part 1 in jump buffer
	.set	jmpmask1, 4	# signal mask part 2 in jump buffer
	.set	jmpret, 8	# return address offset in jump buffer
	.set	jmpstk, 12	# stack pointer offset in jump buffer
	.set	jmptoc, 16	# TOC pointer offset in jump buffer
	.set	jmpregs, 20	# registers offset in jump buffer
	.set	jmpcr,96	# condition reg offset in jump buffer
	.set	jmprsv1,100	# reserved (alignment)
	.set	jmpfpr,104	# floating point regs offset in jump buffer
	.set	jmprsv2,248	# reserved
	.set	jmpbufsize,256	# total length = 256 bytes

#-------------------------------------------------------------------
# NAME: 
#   setjmp
#
# FUNCTION: 
#   The setjmp function works just like the C-library setjmp function.
#
#   WARNING: This function saves only the stack context.
#   It does not save the signal mask.
#
# NOTES:
#   Input:   r3 -> label_t for jump buffer
#   Output:  r3 = 0
#-------------------------------------------------------------------

	S_PROLOG(_setjmp)

	mfcr	r5 			# copy the condition reg
	stm	r13, jmpregs(r3)	# save non-volatile regs
	st	r5, jmpcr(r3)           # save the condition reg
	mflr	r4			# copy the return address
	st	r1, jmpstk(r3)		# save the stack pointer
	st	r2, jmptoc(r3)		# save the TOC pointer
	st	r4, jmpret(r3)		# save the return address
	stfd	f14, jmpfpr (r3)        # save non-volatile fp regs
	stfd	f15, jmpfpr +  1 * 8 (r3)
	stfd	f16, jmpfpr +  2 * 8 (r3)
	stfd	f17, jmpfpr +  3 * 8 (r3)
	stfd	f18, jmpfpr +  4 * 8 (r3)
	stfd	f19, jmpfpr +  5 * 8 (r3)
	stfd	f20, jmpfpr +  6 * 8 (r3)
	stfd	f21, jmpfpr +  7 * 8 (r3)
	stfd	f22, jmpfpr +  8 * 8 (r3)
	stfd	f23, jmpfpr +  9 * 8 (r3)
	stfd	f24, jmpfpr + 10 * 8 (r3)
	stfd	f25, jmpfpr + 11 * 8 (r3)
	stfd	f26, jmpfpr + 12 * 8 (r3)
	stfd	f27, jmpfpr + 13 * 8 (r3)
	stfd	f28, jmpfpr + 14 * 8 (r3)
	stfd	f29, jmpfpr + 15 * 8 (r3)
	stfd	f30, jmpfpr + 16 * 8 (r3)
	stfd	f31, jmpfpr + 17 * 8 (r3)
	cal	r3, 0(0)		# return 0 

	S_EPILOG

#-------------------------------------------------------------------
# NAME: 
#   longjmp
#
# FUNCTION: 
#   The setjmp function works just like the C-library longjmp function
#   but does not test the stack pointer before restoring the saved state.
#
#   WARNING: This function saves only the stack context.
#   It does not save the signal mask.
#
# NOTES:
#   Input:  r3 -> label_t for jump buffer
#	    r4 = return code for caller of CoSetJmp
#
#   Output: r3 = return code passed by caller, (test for 0 removed)
#-------------------------------------------------------------------

	S_PROLOG(_longjmp)
	
	l       r5, jmpret(r3)		# restore return address
	l       r1, jmpstk(r3)		# restore stack pointer
	l       r2, jmptoc(r3)		# restore TOC pointer
	lfd	f14, jmpfpr (r3)        # restore non-volatile fp regs
	lfd	f15, jmpfpr +  1 * 8 (r3)
	lfd	f16, jmpfpr +  2 * 8 (r3)
	lfd	f17, jmpfpr +  3 * 8 (r3)
	lfd	f18, jmpfpr +  4 * 8 (r3)
	lfd	f19, jmpfpr +  5 * 8 (r3)
	lfd	f20, jmpfpr +  6 * 8 (r3)
	lfd	f21, jmpfpr +  7 * 8 (r3)
	lfd	f22, jmpfpr +  8 * 8 (r3)
	lfd	f23, jmpfpr +  9 * 8 (r3)
	lfd	f24, jmpfpr + 10 * 8 (r3)
	lfd	f25, jmpfpr + 11 * 8 (r3)
	lfd	f26, jmpfpr + 12 * 8 (r3)
	lfd	f27, jmpfpr + 13 * 8 (r3)
	lfd	f28, jmpfpr + 14 * 8 (r3)
	lfd	f29, jmpfpr + 15 * 8 (r3)
	lfd	f30, jmpfpr + 16 * 8 (r3)
	lfd	f31, jmpfpr + 17 * 8 (r3)
	mtlr	r5			# return to location after CoSetJmp call
	lm      r13, jmpregs(r3)	# restore non-volatile regs
	l	r5, jmpcr(r3)		# load saved condition regs
	mr	r3, r4			# load return value
	mtcrf	0x38, r5                # restore condition regs 2-4
	S_EPILOG
