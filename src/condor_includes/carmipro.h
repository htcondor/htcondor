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
#if !defined(_CARMIPRO_H)
#define _CARMIPRO_H

#ifdef PVM34
#include "pvmproto.h"
#else
#include "pvmsdpro.h"
#endif


#define CARMI_FIRST	(SM_LAST+1)

#define CARMI_RESPAWN (CARMI_FIRST+1)
#define CARMI_CHKPT   (CARMI_FIRST+2)
#define CARMI_ADDHOST (CARMI_FIRST+3)
#define CARMI_SPAWN	  (CARMI_FIRST+4)
#define CARMI_VACATE_NOTIFY  (CARMI_FIRST + 6)
#define CARMI_CANCEL_REQ (CARMI_FIRST + 7)
#define CARMI_REQ_URL_FOR_FILE (CARMI_FIRST + 8)
#define CARMI_NOTIFY (CARMI_FIRST + 9)
#define CARMI_LAST    (CARMI_NOTIFY)

#define CO_CHECK_FIRST 						(CARMI_LAST+1)
#define CO_CHECK_REQ_CKPT					(CO_CHECK_FIRST + 1)
#define CO_CHECK_REQ_RESTART				(CO_CHECK_FIRST + 2)
#define CO_CHECK_REQ_MIGRATE				(CO_CHECK_FIRST + 3)
#define CO_CHECK_RM_CKPT					(CO_CHECK_FIRST + 4)
#endif
