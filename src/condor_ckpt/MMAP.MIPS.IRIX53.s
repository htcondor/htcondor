		.text
		.align	2
		.globl	MMAP
		.ent	MMAP
MMAP:
		li $2,1134
		syscall 
		bnez $7,MMAP_L1
		nop 
		jr $31
		nop
MMAP_L1:	
		move $24,$31
		bal MMAP_L2
		nop
MMAP_L2:	
		lui $28,8
		addiu $28,$28,16724
		addu $28,$28,$31
		move $31,$24
		lw $25,-29436($28)
		nop 
		jr $25
		nop 
		.end MMAP
