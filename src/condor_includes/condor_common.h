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
#ifndef CONDOR_COMMON_H
#define CONDOR_COMMON_H

/**********************************************************************
** System specific headers and definitions
*********************************************************************/

/******************************
** Windoze NT specifics
******************************/
#if defined(WIN32)

#define NOGDI
#define NOUSER
#define NOSOUND
#include <winsock2.h>
#include <windows.h>
#include "_condor_fix_nt.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include "condor_file_lock.h"

/******************************
** Unix specifics
******************************/
#else

#define _POSIX_SOURCE
#include "_condor_fix_types.h"
#include "condor_fix_unistd.h"
#include <stdarg.h>		/* Include this before stdio.h so GNU's va_list is defined */
#include "condor_fix_stdio.h"
#include "condor_fdset.h"
#include "condor_fix_string.h"
#include "condor_fix_signal.h"
#include "condor_fix_sys_ioctl.h"
#include "condor_fix_sys_stat.h"
#include "condor_fix_sys_wait.h"
#include "condor_file_lock.h"
#include "condor_fix_assert.h"
#include "condor_fix_socket.h"
#include "condor_fix_netdb.h"
#include <sys/utsname.h>		
#include "condor_fix_limits.h"
#include <stdlib.h>
#include <sys/resource.h>
#include <ctype.h>
#include <fcntl.h>	
#include <errno.h>
#include <pwd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <values.h>
#include <math.h>

#endif /* System specific stuff */


/**********************************************************************
** Condor specific headers and definitions
**********************************************************************/
#include "condor_constants.h"
#include "condor_macros.h"


/**********************************************************************
** C++ specific stuff 
**********************************************************************/
#if defined(__cplusplus)

#include <iostream.h>
#include <iomanip.h>

#endif /* __cplusplus */


#endif /* CONDOR_COMMON_H */
