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


#include "condor_common.h"
#include "condor_debug.h"
#include "condor_pers.h"

void patch_personality(void)
{

#if defined( LINUX ) && (defined(I386) || defined(X86_64))
        // alter the personality of this process to use i386 memory layout
        // methods and no exec_shield(brk & stack segment randomization).
        // We must use a syscall() here because the personality() call was
        // defined in redhat 9 and not earlier, even though the kernel
        // entry point existed. Even though the personality has been altered
        // it will not become active in this process space until an exec()
        // happens. 
        
        // update: after kernel 2.6.12.2 revision, exec shield and the 
        // randomization of the va space were separated, and using PER_LINUX32
        // now only turns of exec-shield, leaving va randomization on.
        // The stupid 0x40000 flag is ADDR_NO_RANDOMIZE, which 
        // isn't yet defined for use outside of the kernel except in very new
        // linux distributions.  From my compatibility testing, using this
        // magic number on kernel pre 2.6.12.2 *appears* to have no detrimental
        // effects.

#if defined(I386)
#   if defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
#       define  MAGIC_NUMBER (PER_LINUX32|ADDR_COMPAT_LAYOUT|ADDR_NO_RANDOMIZE)
#   else
#       define  MAGIC_NUMBER (PER_LINUX32 | 0x0200000 | 0x0040000)
#   endif
#elif defined(X86_64)
#   if defined(GLIBC25) || defined(GLIBC27) || defined(GLIBC211) || defined(GLIBC212) || defined(GLIBC213)
#       define  MAGIC_NUMBER (ADDR_COMPAT_LAYOUT | ADDR_NO_RANDOMIZE)
#   else
#       define  MAGIC_NUMBER (0x0200000 | 0x0040000)
#   endif
#else
#   error "Please determine the right syscall to disable va randomization!"
#endif

        if (syscall(SYS_personality, MAGIC_NUMBER) == -1) {
            EXCEPT("Unable to set personality: %d(%s)! "
                    "Memory layout will be uncheckpointable!\n", errno,
                    strerror(errno));
        }

#endif 

}
