/* This file has been rewritten to remove the fact that if the syscall failed
	it would try and call _cerror relative to itself. errno now has no
	meaning should this call fail. You will only get a -1 as a return 
	value. If I have time, I'll come back and explore what I would need
	to do to support an errno like thing.
	-pete 01/11/00
*/

		#.file 1 "SYSCALL.MIPS.IRIX65.s"
		.option pic2
		.section .text
		.align	2
		.globl	SYSCALL
		.ent	SYSCALL
SYSCALL:
		li	$2,1000
		syscall 
		nop
		bnez	$7,SYSCALL_L1
		nop
		jr	$31
		nop
SYSCALL_L1:	
		li $2, -1
		jr	$31
		nop
		.end	SYSCALL
