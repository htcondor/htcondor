/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

 

#ifndef TYPES_H
#define TYPES_H

#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include <sys/types.h>
#if defined(OSF1)
#undef FD_ZERO
#define FD_ZERO(p)     memset((char *)(p), 0, sizeof(*(p)))
#endif

#include <fcntl.h>
#include <signal.h>

extern "C" {
#include "condor_types.h"
}

#include "debug.h"
#include "except.h"

typedef int BOOLEAN;


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define SECOND	1
#define MINUTE	(SECOND * 60)
#define HOUR	(MINUTE * 60)
#define DAY		(24 * HOUR)

#endif
