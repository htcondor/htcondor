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

#ifndef CONDOR_AUX_DRMAA_H
#define CONDOR_AUX_DRMAA_H

#include "condor_common.h"

BEGIN_C_DECLS

#include "lib_condor_drmaa.h"

#if HAS_PTHREADS
    #include <pthread.h> 
#else
    // nothng - UNIX platforms without pthreads are not guaranteed thread safety
#endif

#define YES "Y"
#define NO "N"
#define SUCCESS 0
#define FAILURE 1
#define MIN_JOBID_LEN 10   

#ifdef WIN32
    #define DRMAA_DIR "drmaa\\"
    #define SUBMIT_FILE_DIR "submitFiles\\"
    #define LOG_FILE_DIR "logs\\"
#else
    #define DRMAA_DIR "drmaa/"
    #define SUBMIT_FILE_DIR "submitFiles/"
    #define LOG_FILE_DIR "logs/"
#endif

#define SUBMIT_FILE_EXTN ".sub"
#define LOG_FILE_EXTN ".log"
#define MAX_LOG_FILE_LINE_LEN 1000
#define MAX_FILE_NAME_LEN 1024
#define MAX_JOBID_LEN DRMAA_JOBNAME_BUFFER
#define SUBMIT_FILE_COL_SIZE 20   // size of column in submit file
#define SUBMIT_CMD "condor_submit"
#define SUBMIT_CMD_LEN 2000
#define HOLD_CMD "condor_hold -name"
#define HOLD_CMD_LEN 2000
#define RELEASE_CMD "condor_release -name"
#define RELEASE_CMD_LEN 2000
#define TERMINATE_CMD "condor_rm -name"
#define TERMINATE_CMD_LEN 2000
#define MAX_READ_LEN 1024  // max # of bytes to read
#define JOBID_TOKENIZER "."
#define NUM_SUPP_SCALAR_ATTR 12  // # of supported scalar attributes
#define NUM_SUPP_VECTOR_ATTR 3

#define STAT_ABORTED -1
#define STAT_UNKNOWN 0       // 1 through 99 are signals
#define STAT_NOR_BASE 100
#define MIN_SIGNAL_NM_LEN 100

#define WAIT_FOREVER_SLP_TM 30  // # of seconds to wait if waited job active

/* Structures */
struct drmaa_attr_names_s {
    char** attrs; // pointer to array of char*s
    int size;
    int index;  // holds which element of array we are currently on
};

struct drmaa_attr_values_s {
    char** values;  // pointer to array of char*s
    int size;   // number of values
    int index;  // current element
};

struct drmaa_job_ids_s {
    char** values; // pointer to array of char*s
    int size;
    int index;
};

// used as a means of determining if job is finished
typedef enum {
    SUBMITTED,  // _run_job()
    CONTROLLED, // _control()'s HOLD, RELEASE only    
    HELD,       // _control() HOLD successful    
    FINISHED    // successfully _synchronized() on and not disposed of
} job_state_t;

typedef struct condor_drmaa_job_info_s {
    job_state_t state;  // job's current state
    char id[MAX_JOBID_LEN];  
           // <schedd name, job name on that schedd> (of fixed length) 
    struct condor_drmaa_job_info_s* next;  // next job_id
#ifdef WIN32
    CRITICAL_SECTION lock;
#else
#if HAS_PTHREADS
    pthread_mutex_t lock; // locked when _control() called on it
#endif
#endif    
} condor_drmaa_job_info_t;

typedef struct job_attr_s {
    char name[DRMAA_ATTR_BUFFER];   // name of attribute
    union {
	char* value;   // if num_value = 1, attribute value held here
	char** values; // if num_values > 1, attribute values held here
    } val;
    unsigned int num_values;  // the number of values pointed to by attr_value
    struct job_attr_s* next;  // next attribute in list
} job_attr_t;

struct drmaa_job_template_s {
    unsigned int num_attr;  // num attributes in this template
    job_attr_t* head;  // head of linked list of attributes
};

END_C_DECLS

#endif  /** CONDOR_AUX_DRMAA_H **/
