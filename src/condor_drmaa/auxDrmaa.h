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

/** Determines if a given string is a number.
    @param str the string to test
    @return true (upon success) or false
*/
int is_number(const char* str);

/** Allocates a new job_attr_t
    @return pointer to a new job template on the heap or NULL on failure.
*/
job_attr_t* create_job_attribute();

/** Deallocates a job_attr_t
    @param ja job attribute to deallocate
*/
void destroy_job_attribute(job_attr_t* ja);

/** Allocates a new condor_drmaa_job_info_t.  job_id must not be longer than MAX_JOBID_LEN.
    @return pointer to a new job info on the heap or NULL on failure
*/
condor_drmaa_job_info_t* create_job_info(const char* job_id);

/** Deallocates a condor_drmaa_job_info_t */
void destroy_job_info(condor_drmaa_job_info_t* job_info);

/** Determines if a given attribute name is valid.
    @param name attribute name
    @param drmaa_context_error_buf contains a context sensitive error upon
           fail returned
    @return true (upon success) or false
*/
int is_valid_attr_name(const char* name, char* error_diagnosis, 
		       size_t error_diag_len);

/** Determines if a given attribute value is 1) of the proper format and
    2) a valid value.  Assumes name is supported per is_supported_attr()
    @param err_cd set to DRMAA_ERRNO_INVALID_ATTRIBUTE_FORMAT, 
           DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE, or 
	   DRMAA_ERRNO_CONFLICTING_ATTRIBUTE_VALUES in the appropriate case
    @param name attribute name
    @param value attribute value
    @param drmaa_context_error_buf contains a context sensitive error upon
           fail returned
    @return true (upon success) or false
*/
int is_valid_attr_value(int* err_cd, const char* name, const char* value, 
			char* error_diagnosis, size_t error_diag_len);

/** Determines if a given attribute name represents a scalar attribute.
    Assumes that name is valid per is_valid_attr_name()
    @param name name of attribute
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error_diagnosis buffer
    @return true (upon succcess) or false
*/
int is_scalar_attr(const char* name, char* error_diagnosis, 
		   size_t error_diag_len);

/** Determines if a given attribute name represents a vectorr attribute.
    Assumes that an attribute name is valid per is_valid_attr_name().
    @param name name of attribute
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error_diagnosis buffer
    @return true or false
*/
int is_vector_attr(const char* name, char* error_diagnosis, 
		   size_t error_diag_len);

/** Determines if a given attribute name is supported by this DRMAA library.
    Assumes that name is valid per is_valid_attr_name()
    @param name name of attribute
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error_diagnosis buffer
    @return true (upon success) or false
*/
int is_supported_attr(const char* name, char* error_diagnosis, 
		      size_t error_diag_len);

/** Prints the given job attributes.
    Used in debugging.
*/
void print_ja(const job_attr_t* ja);

/** Allocates a new drmaa_job_template_t
    @return the job template or NULL upon failure
*/
drmaa_job_template_t* create_job_template();

/** Determines if a given job template is valid.
    @param jt job template
    @param drmaa_context_error_buf contains a context sensitive error upon
           fail returned
    @return true (upon success) or false
*/
int is_valid_job_template(const drmaa_job_template_t* jt, char* error_diagnosis,
	      size_t error_diag_len);

/** Searches a given job template for the job attribute corresponding to name. 
    The first one found is returned.  If none is found, NULL is returned.
    jt is assumed to be valid per is_valid_job_template() and name is 
    assumed to be valid per is_supported_attr()
    @param jt job template
    @param name name of job attribute
    @param drmaa_context_error_buf contains a context sensitive error upon
           fail returned
    @return a pointer to the job attribute or NULL
*/
job_attr_t* find_attr(const drmaa_job_template_t* jt, const char* name, 
		     char* error_diagnosis, size_t error_diag_len);

/** Determines if a given attribute is already set in a given job template.
    Assumes that jt is valid per is_valid_job_template() and that name is 
    supported per is_supported_attr()
    @param jt job template
    @param name attribute name
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error buffer
    @return true (upon success) or false
*/
int contains_attr(const drmaa_job_template_t* jt, const char* name, 
		  char* error_diagnosis, size_t error_diag_len);

/** Prints a given job template.  Useful in debugging. */
void print_jt(const drmaa_job_template_t* jt);

/** Creates a Condor submit file for the given job template
    @param submit_fn submit file name allocated and returned upon SUCCESS
    @param jt job template Must be valid per is_valid_job_template()
    @param error_diagnosis contains a context sensitive error upon
           fail returned
    @param error_diag_len length of the error buffer
    @return one of the following error codes:
            DRMAA_ERRNO_SUCCESS
	    DRMAA_ERRNO_TRY_LATER
	    DRMAA_ERRNO_NO_MEMORY
*/
int create_submit_file(char** submit_fn, const drmaa_job_template_t* jt, 
		       char* error_diagnosis, size_t error_diag_len);

/** Generates a unique file name.
    @param fname allocated and filled with the unique file name on success
    @return SUCCESS or FAILURE
*/
int generate_unique_file_name(char** fname);

/** Write a given job attribute to the opened submit file file stream.
    Assumes that ja is valid.
    @return SUCCESS or FAILURE
*/
int write_job_attr(FILE* fs, const job_attr_t* ja);

/** Writes a given vector job attribute to the opened submit file file stream.
    Assumes that ja is valid.
    @return number of characters written or negative if an error occurred
*/
int write_v_job_attr(FILE* fs, const job_attr_t* ja);

/** Writes special job attributes to the opened submit file file stream.
    Special job attributes are those whose values depend on other attributes'
    values.  Assumes that jt is valid.
    @return SUCCESS or FAILURE
*/
int write_special_attrs(FILE* fs, const drmaa_job_template_t* jt);

/** Submits a job to condor using the given submit file.
    @param error_diagnosis contains a context sensitive error upon
           fail returned
    @param error_diag_len length of the error buffer
    @return one of the following error codes:
            DRMAA_ERRNO_SUCCESS
	    DRMAA_ERRNO_TRY_LATER
	    DRMAA_ERRNO_DENIED_BY_DRM
	    DRMAA_ERRNO_NO_MEMORY
	    DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE
	    DMRAA_ERRNO_AUTH_FAILURE
*/
int submit_job(char* job_id, size_t job_id_len, const char* submit_file_name, 
		char* error_diagnosis, size_t error_diag_len);

/** Determines if stat represents a valid stat code
    @return true or false
*/
int is_valid_stat(const int stat);

/** Determines if a given job id is valid
    @return true or false
*/
int is_valid_job_id(const char* job_id);

/** Given a valid job_id, open's its log file for reading only.
    @return pointer to file stream or NULL
*/
FILE* open_log_file(const char* job_id); 

/** Removes the log file of the given job id
    @return true upon success, false on failure
*/
int rm_log_file(const char* job_id);

/** Waits for a given job_id to complete by monitoring the log file
    @param dispose If true, deletes log file
    @return DRMAA_ERRNO_s very similar to drmaa_wait()
 */
int wait_job(const char* job_id, const int dispose, const int get_stat_rusage, 
	     int* stat, signed long timeout, const time_t start, 
	     drmaa_attr_values_t** rusage, char* error_diagnosis, 
	     size_t error_diag_len);

/** Creates a drmaa_attr_values_t of given size 
    @return the drmaa_attr_values_t or NULL (upon failure)
*/
drmaa_attr_values_t* create_dav(int size);

/** Moves the given list of job_ids from the reserved_jobs_list to the
    job_info_list.  Caller must have locks to both lists before calling.
    If a given item in jobids is not found in the reserved_jobs_list, it
    is ignored and the function continues.
    @return true or false (upon success or failure, respectively)
*/
int mv_jobs_res_to_info(const drmaa_job_ids_t* jobids);    

/** Moves the given valid jobid from the job_reserved list to the job_info
    list.  This method acquires and releases locks to both lists appropriately.
    Returns false if jobid not found on job_reserved_list.
    @return true (on success) or false
*/
int mv_job_res_to_info(const char* jobid);

/** Removes a given job id from the job_info_list.  Method acquires
    and releases job_info_list_lock.
    @return true (upon success) or false 
*/
int rm_infolist(const char* job_id);

/** Removes a given job id from the reserved_job_list.  This method acquires and 
    releases reserved_job_list lock itself.
    @return true (upon success) or false 
*/
int rm_reslist(const char* job_id);

/** Change the status of a given job on the reserved list to FINISHED.  This
    method acquires and releases the reserved_job_list lock itself.  If the 
    jobid is not found in the reserved_jobs_list, the function returns false.
    @return true (upon success) or false
*/
int mark_res_job_finished(const char* job_id);

/** Releases both the reserved_list and info_list locks and
    returns the error code given.  The caller must have both
    locks before calling this method.
    @return drmaa_err_code
*/
int rel_locks(const int drmaa_err_code);

/** Determines the library's base directory, allocates the required memory
    for buf, and copies the full path name there.  A successful result contains
    a trailing forward slash or backslash, depanding upon the system.
    @return true (on success) or false otherwise
*/
int get_base_dir(char** buf);

/** Places the given jobid on hold.  Assumes job is in proper state already.
    @return drmaa error code appropriate for drmaa_control()
*/
int hold_job(const char* jobid, char* error_diagnosis, size_t error_diag_len);

/** Releases the given jobid, which is assumed to be already on hold.
    @return drmaa error code appropriate for drmaa_control()
*/
int release_job(const char* jobid, char* error_diagnosis, size_t error_diag_len);

/** Terminates the given valid jobid by calling condor_rm.  Does not 
    remove the jobid from the library's internal data structures.
    @return drmaa error code appropriate for drmaa_control()
*/
int terminate_job(const char* jobid, char* error_diagnosis, 
		  size_t error_diag_len);

/** Frees the memory held by the entries in the job_info_list */
void free_job_info_list();

/** Frees the memory held by the entries in the reserved_job_list */
void free_reserved_job_list();

/** Removes all files in the submit file directory */
void clean_submit_file_dir();

/** Unlocks the info_list_lock */
void unlock_info_list_lock();

/** Unlocks the reserved_list_lock */
void unlock_reserved_list_lock();

/** Unlocks the lock of the given condor_drmaa_job_info_t */
void unlock_job_info(condor_drmaa_job_info_t* job_info);

/** Obtains the name of the local schedd.  Sets the "schedd_name" global
    variable upon success.  
    @return true (upon success) or false
*/
int get_schedd_name(char *error_diagnosis, size_t error_diag_len);

END_C_DECLS

#endif  /** CONDOR_AUX_DRMAA_H **/
