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
static const char  COMMAND_ADTYPE	[] = "Command";
static const char  REPLY_ADTYPE		[] = "Reply";

#endif

