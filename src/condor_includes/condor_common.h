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

#ifndef CONDOR_COMMON_H
#define CONDOR_COMMON_H


/**********************************************************************
** autoconf/configure generated configuration header
*********************************************************************/
#if HAVE_CONFIG_H
#include "config.h"
#endif

/**********************************************************************
** Special macros and things we always want our header files to have
*********************************************************************/
#include "condor_header_features.h"

/**********************************************************************
** System specific headers and definitions
*********************************************************************/
#include "condor_system.h"

/**********************************************************************
** Condor specific headers and definitions
**********************************************************************/
#include "condor_constants.h"
#include "condor_macros.h"
#include "condor_open.h"
#include "condor_blkng_full_disk_io.h"

/*********************************************************************
** On WinNT, we _must_ make redefine the assert() macro to be EXCEPT.
** One reason we must do this is the system assert() on NT is a
** no-op on a RELEASE build, and we do not expect this behavior.
** This re-definition is done in condor_debug.h.
** To further complicate matters, the WinNT <assert.h> file does
** not check if it has already been included.  This means any
** system header file which includes <assert.h> after we include
** condor_fix_assert.h will redefine assert again.  So, on WinNT
** we _must_ include condor_debug.h again here as the last include
** file, after we've finished including all system header files.
*********************************************************************/
#ifdef WIN32
#include "condor_debug.h"
#endif

#ifdef WIN32
/*********************************************************************
** On Windows Server 2003 SDK, it does not have inet_pton/inet_ntop
** Instead, they have windows version of it.
** Here, it includes condor_ipv6.Windows.h which implements inet_pton/
** inet_ntop using Winsock function.
** Currently, only inet_pton is implemented.
*********************************************************************/
#include "condor_ipv6.WINDOWS.h"
#endif


#endif /* CONDOR_COMMON_H */
