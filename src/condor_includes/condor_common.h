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

#if defined(GSS_AUTHENTICATION)
#ifndef _GSSAPI_H_
#define _GSSAPI_H_
#include "gssapi.h"
#endif
#endif

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
