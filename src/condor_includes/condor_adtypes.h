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

 

#ifndef __CONDOR_ADTYPES__
#define __CONDOR_ADTYPES__

static const char  STARTD_ADTYPE	[] = "Machine";
static const char  SCHEDD_ADTYPE	[] = "Scheduler";
static const char  MASTER_ADTYPE	[] = "DaemonMaster";
static const char  CKPT_SRVR_ADTYPE	[] = "CkptServer";
static const char  JOB_ADTYPE		[] = "Job";
static const char  QUERY_ADTYPE		[] = "Query";
static const char  COLLECTOR_ADTYPE	[] = "Collector";
static const char  CKPT_FILE_ADTYPE	[] = "CkptFile";
static const char  USERAUTH_ADTYPE	[] = "Authentication";
static const char  LICENSE_ADTYPE	[] = "License";
static const char  STORAGE_ADTYPE	[] = "Storage";
static const char  ANY_ADTYPE		[] = "Any";
static const char  SUBMITTER_ADTYPE	[] = "Submitter";

#endif

