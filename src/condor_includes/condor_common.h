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

/**********************************************************************
** C++ specific stuff 
**********************************************************************/
#if defined(__cplusplus)

#include <iostream.h>
#include <iomanip.h>
#include <fstream.h>

#endif /* __cplusplus */


#endif /* CONDOR_COMMON_H */
