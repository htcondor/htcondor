/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
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
#include "condor_blkng_full_disk_io.h"

#include "cedar_enums.h"
#include "../condor_sysapi/sysapi.h"

/**********************************************************************
** C++ specific stuff 
**********************************************************************/
#if defined(__cplusplus)

#include <iostream.h>
#include <iomanip.h>
#include <fstream.h>

#endif /* __cplusplus */

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



#endif /* CONDOR_COMMON_H */
