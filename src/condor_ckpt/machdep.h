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


#ifndef _MACHDEP_H
#define _MACHDEP_H

#if defined(Solaris)

	extern "C" int brk( void * );
	extern "C" void *sbrk( int );
	typedef void (*SIG_HANDLER)( ... );
#	define SETJMP setjmp
#	define LONGJMP longjmp

#elif defined(LINUX)

	extern "C" int brk( void *);

#	if defined(I386)
	extern "C" void *sbrk( int );
#	elif defined(X86_86)
	extern "C" void *sbrk( long int );
#	endif 

	typedef void (*SIG_HANDLER)( int );
#	define SETJMP setjmp
#	define LONGJMP longjmp
	extern "C" int kill( pid_t, int );

#	if defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
	/* glibc27 (starting with 26, but we leapt over that revision in our stdunv
		support) encrypts pointer values in the glibc runtime, such as the stack
		pointer in the jmp_buf. This piece of code is the algortihm used in 
		glibc 2.7-18 by debian 5.0 (the glibc27 revision to which we coded our
		support) for pointer encryption and decryption. WARNING: These macros
		are not expressions and do not use them as such. Treat them as functions
		which modify their argument but without being passed the address of
		them. I know that you aren't passing the pointer to it and what not,
		but hey, that's inline assembly for you!
	*/

#		if defined(I386)
			/* 24 is the magic number which represents the offset
				to the pointer_guard variable in a tcbhead_t
				structure. This location contains the encryption
				value which is used to encrypt runtime pointers in
				glibc such as the stack location in the jump buf
				structure. The address to the start of the tcbhead
				structure is stored in the %gs register. This
				algorithm was deduced from online readings.
			*/

#			ifdef __ASSEMBLER__
#				define PTR_ENCRYPT(reg)  \
					xorl %gs:24, reg;   \
					roll $9, reg
#				define PTR_DECRYPT(reg)  \
					rorl $9, reg;       \
					xorl %gs:24, reg
#			else /* not defined __ASSEMBLER__ */
#				define PTR_ENCRYPT(variable)  \
					asm ("xorl %%gs:24, %0\n"   \
						"roll $9, %0"          \
						: "=r" (variable)           \
						: "0" (variable))
#				define PTR_DECRYPT(variable)  \
					asm ("rorl $9, %0\n"    \
						"xorl %%gs:24, %0" \
						: "=r" (variable)       \
						: "0" (variable))
#			endif /* __ASSEMBLER__ */

#		elif defined(X86_64)
			
			/* The magic number for this architecture is 48
				and it is stored in %fs. See the above I386
				comment for what it means.
			*/

#			ifdef __ASSEMBLER__
#				define PTR_ENCRYPT(reg)  \
					xorq %fs:48, reg;   \
					rolq $17, reg
#				define PTR_DECRYPT(reg)  \
					rorq $17, reg;       \
					xorq %fs:48, reg
#			else /* not defined __ASSEMBLER__ */
#				define PTR_ENCRYPT(variable)  \
					asm ("xorq %%fs:48, %0\n"   \
						"rolq $17, %0"          \
						: "=r" (variable)           \
						: "0" (variable))
#				define PTR_DECRYPT(variable)  \
					asm ("rorq $17, %0\n"    \
						"xorq %%fs:48, %0" \
						: "=r" (variable)       \
						: "0" (variable))
#			endif /* __ASSEMBLER__ */
#		else
#			error "Please determine how to deal with PTR_ENCRYPT/PTR_DECRYPT."
#		endif

#	else /* Not defibed glibc 27 */

#		define PTR_ENCRYPT(reg)
#		define PTR_DECRYPT(reg)

#	endif /* GLIBC27 */

#else

#	error UNKNOWN PLATFORM

#endif

#endif /* _MACHDEP_H */












