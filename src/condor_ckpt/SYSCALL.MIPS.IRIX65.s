		.text
		.align	2
		.globl	SYSCALL
		.ent	SYSCALL
SYSCALL:
		li	$2,1000
		syscall 
		bnez	$7,SYSCALL_L1
		nop 
		jr	$31
		nop
SYSCALL_L1:	
		move	$24,$31
		bal	SYSCALL_L2
		nop
SYSCALL_L2:	
		lui	$28,8
		addiu	$28,$28,17932
		addu	$28,$28,$31
		move	$31,$24
		lw	$25,-29436($28)
		nop 
		jr	$25
		nop
		.end	SYSCALL
