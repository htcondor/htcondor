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
#include "condor_file_lock.h"

/******************************
** Unix specifics
******************************/
#else

#define _POSIX_SOURCE
#include "_condor_fix_types.h"
#include "condor_fix_stdio.h"
#include "condor_fix_unistd.h"
#include "condor_fix_string.h"
#include "condor_fix_signal.h"
#include "condor_fix_sys_ioctl.h"
#include "condor_file_lock.h"
#include "condor_fix_sys_stat.h"
#include "condor_fix_sys_wait.h"
#include "condor_fix_assert.h"
#include "condor_fix_socket.h"
#include <sys/utsname.h>		
#include "condor_fix_limits.h"
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>	
#include <errno.h>
#include <netdb.h>
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

/**********************************************************************
** C++ specific stuff 
**********************************************************************/
#if defined(__cplusplus)

#include <iostream.h>
#include <iomanip.h>

#endif __cplusplus

#endif CONDOR_COMMON_H






