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
		li $2, -1
		jr	$31
		nop
		.end MMAP
