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

#ifndef DRMAA_COMMON_H
#define DRMAA_COMMON_H

/* Global Variables defined in auxDrmaa.h */
extern char* schedd_name; 
extern char* file_dir;

#ifdef WIN32
    extern CRITICAL_SECTION info_list_lock;
#else  
    #if HAS_PTHREADS
        extern pthread_mutex_t info_list_lock;
    #endif  
#endif 
extern condor_drmaa_job_info_t* job_info_list; 
extern int num_info_jobs;

#ifdef WIN32
    extern CRITICAL_SECTION reserved_list_lock;
#else
    #if HAS_PTHREADS
        extern pthread_mutex_t reserved_list_lock;
    #endif
#endif    
extern condor_drmaa_job_info_t* reserved_job_list;  
extern int num_reserved_jobs;

#endif /* DRMAA_COMMON_H */
