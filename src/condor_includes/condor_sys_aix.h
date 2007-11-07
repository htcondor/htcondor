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

#ifndef CONDOR_SYS_AIX_H
#define CONDOR_SYS_AIX_H

/* Make a clean slate for AIX to determine what I should have */
#undef _POSIX_SOURCE
#undef _XOPEN_SOURCE
#undef _XOPEN_SOURCE_EXTENDED
#undef _ALL_SOURCE
#undef _ANSI_C_SOURCE
/* but I do need this... */
#ifndef _LONG_LONG
#define _LONG_LONG 1
#endif
#include <standards.h>

/* Bring in the usual definition */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/statfs.h>

/* So we can use things like WIFEXITED */
#include <sys/id.h>
#include <sys/wait.h>

/* XXX the odmi uses the variable name 'class' in certain structures which
	cause the C++ compiler to scream loudly. So, this means that the ODMI is
	only available in .c files, not .C files. What a pain! */
#ifndef __cplusplus
/* Bring in the creepy ODMI stuff tht I use to divine runtime attributes from
	the system like swap space and stuff */
#include <odmi.h>
/* For the various classes(e.g., CuAt) I can get out of /etc/objrepos */
#include <sys/cfgodm.h>
#endif

/* to determine swap space sizes */
#include <sys/vminfo.h>

/* This brings in a "union wait" structure which I don't know if I truly
	need, so comment it out for now */
/*#include <sys/m_wait.h>*/

/* Um... AIX doesn't define WCOREFLG or WCOREDUMP, so here's an educated guess.
	Also be aware that if I was using a union wait structure, this would have 
	to be different.
	*/
#ifndef WCOREFLG
#define WCOREFLG (0x80)
#endif 

#ifndef WCOREDUMP
#define WCOREDUMP(x) ((x) & (WCOREFLG))
#endif

#define HAS_U_TYPES	1


#endif






