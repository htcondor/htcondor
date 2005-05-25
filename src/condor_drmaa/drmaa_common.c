/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
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

/* Global Variables used throughout the drmaa library */

#include "auxDrmaa.h"

#if !defined(CONDOR_DRMAA_STANDALONE)
BEGIN_C_DECLS
#endif
char* schedd_name; 
char* file_dir;

// The following are touched by: 
//   1) drmaa_run_jobs() - to add a job
//   2) drmaa_run_bulk_jobs() - to add a job
//   3) drmaa_control() - verifying job_id validity
//                        HOLD and SUSPEND
//   4) drmaa_synchronize() - to move jobs to reserved list
#ifdef WIN32
    CRITICAL_SECTION info_list_lock;
#else  
    #if HAS_PTHREADS
        pthread_mutex_t info_list_lock;
    #endif  
#endif 

condor_drmaa_job_info_t* job_info_list; 
int num_info_jobs;

#ifdef WIN32
    CRITICAL_SECTION reserved_list_lock;
#else
    #if HAS_PTHREADS
        pthread_mutex_t reserved_list_lock;
    #endif
#endif    

// List of reserved job IDs holds job IDs in being 
//    1) _control(REMOVE)ed,   job_state == BEING_KILLED
//    2) synchronized on.      job_state == SYNCHED
// If a job is in this list it cannot be further _control()ed,
// or _synchronize()d.  Why keep them in a different list rather than
// just changing their status?  A separate list makes it easier to check
// for _synchronize().  Terminating a job may take a long time
condor_drmaa_job_info_t* reserved_job_list;  
int num_reserved_jobs;

#if !defined(CONDOR_DRMAA_STANDALONE)
END_C_DECLS
#endif
