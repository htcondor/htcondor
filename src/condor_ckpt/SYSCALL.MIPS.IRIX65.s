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
