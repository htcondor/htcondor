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
