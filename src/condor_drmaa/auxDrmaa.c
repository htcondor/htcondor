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

#include "auxDrmaa.h"
#include "drmaa_common.h"

/** Determines if a given string is a number.
    @param str the string to test
    @return true (upon success) or false
*/
static int is_number(const char* str);

/** Allocates a new job_attr_t
    @return pointer to a new job template on the heap or NULL on failure.
*/
static job_attr_t* create_job_attribute();

/** Deallocates a job_attr_t
    @param ja job attribute to deallocate
*/
static void destroy_job_attribute(job_attr_t* ja);

/** Allocates a new condor_drmaa_job_info_t.  job_id must not be longer than MAX_JOBID_LEN.
    @return pointer to a new job info on the heap or NULL on failure
*/
static condor_drmaa_job_info_t* create_job_info(const char* job_id);

/** Deallocates a condor_drmaa_job_info_t */
static void destroy_job_info(condor_drmaa_job_info_t* job_info);

/** Determines if a given attribute name is valid.
    @param name attribute name
    @param drmaa_context_error_buf contains a context sensitive error upon
           fail returned
    @return true (upon success) or false
*/
static int is_valid_attr_name(const char* name, char* error_diagnosis, 
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
static int is_valid_attr_value(int* err_cd, const char* name, const char* value, 
			char* error_diagnosis, size_t error_diag_len);

/** Determines if a given attribute name represents a scalar attribute.
    Assumes that name is valid per is_valid_attr_name()
    @param name name of attribute
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error_diagnosis buffer
    @return true (upon succcess) or false
*/
static int is_scalar_attr(const char* name, char* error_diagnosis, 
		   size_t error_diag_len);

/** Determines if a given attribute name represents a vectorr attribute.
    Assumes that an attribute name is valid per is_valid_attr_name().
    @param name name of attribute
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error_diagnosis buffer
    @return true or false
*/
static int is_vector_attr(const char* name, char* error_diagnosis, 
		   size_t error_diag_len);

/** Determines if a given attribute name is supported by this DRMAA library.
    Assumes that name is valid per is_valid_attr_name()
    @param name name of attribute
    @param error_diagnosis contains a context sensitive error upon failure
    @param error_diag_len length of error_diagnosis buffer
    @return true (upon success) or false
*/
static int is_supported_attr(const char* name, char* error_diagnosis, 
		      size_t error_diag_len);

/** Prints the given job attributes.
    Used in debugging.
*/
/*static void print_ja(const job_attr_t* ja);*/

/** Allocates a new drmaa_job_template_t
    @return the job template or NULL upon failure
*/
static drmaa_job_template_t* create_job_template();

/** Determines if a given job template is valid.
    @param jt job template
    @param drmaa_context_error_buf contains a context sensitive error upon
           fail returned
    @return true (upon success) or false
*/
static int is_valid_job_template(const drmaa_job_template_t* jt, char* error_diagnosis,
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
static job_attr_t* find_attr(const drmaa_job_template_t* jt, const char* name, 
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
static int contains_attr(const drmaa_job_template_t* jt, const char* name, 
		  char* error_diagnosis, size_t error_diag_len);

/** Prints a given job template.  Useful in debugging. */
/*static void print_jt(const drmaa_job_template_t* jt);*/

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
static int create_submit_file(char** submit_fn, const drmaa_job_template_t* jt, 
		       char* error_diagnosis, size_t error_diag_len);

/** Generates a unique file name.
    @param fname allocated and filled with the unique file name on success
    @return SUCCESS or FAILURE
*/
static int generate_unique_file_name(char** fname);

/** Write a given job attribute to the opened submit file file stream.
    Assumes that ja is valid.
    @return SUCCESS or FAILURE
*/
static int write_job_attr(FILE* fs, const job_attr_t* ja);

/** Handles the DRMAA placeholders:
      $drmaa_incr_ph$
      $drmaa_hd_ph$
      $drmaa_wd_ph$
    @return newly allocated string with substituted placeholders
 */
static char * substitute_placeholders(const char *orig);

/** Writes a given vector job attribute to the opened submit file file stream.
    Assumes that ja is valid.
    @return number of characters written or negative if an error occurred
*/
static int write_v_job_attr(FILE* fs, const job_attr_t* ja);

/** Writes special job attributes to the opened submit file file stream.
    Special job attributes are those whose values depend on other attributes'
    values.  Assumes that jt is valid.
    @return SUCCESS or FAILURE
*/
static int write_special_attrs(FILE* fs, const drmaa_job_template_t* jt);

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
static int submit_job(char* job_id, size_t job_id_len, const char* submit_file_name, 
		char* error_diagnosis, size_t error_diag_len);

/** Determines if stat represents a valid stat code
    @return true or false
*/
static int is_valid_stat(const int stat);

/** Determines if a given job id is valid
    @return true or false
*/
static int is_valid_job_id(const char* job_id);

/** Given a valid job_id, open's its log file for reading only.
    @return pointer to file stream or NULL
*/
static FILE* open_log_file(const char* job_id); 

/** Removes the log file of the given job id
    @return true upon success, false on failure
*/
static int rm_log_file(const char* job_id);

/** Waits for a given job_id to complete by monitoring the log file
    @param dispose If true, deletes log file
    @return DRMAA_ERRNO_s very similar to drmaa_wait()
 */
static int wait_job(const char* job_id, const int dispose, const int get_stat_rusage, 
	     int* stat, signed long timeout, const time_t start, 
	     drmaa_attr_values_t** rusage, char* error_diagnosis, 
	     size_t error_diag_len);

/** Creates a drmaa_attr_values_t of given size 
    @return the drmaa_attr_values_t or NULL (upon failure)
*/
static drmaa_attr_values_t* create_dav(int size);

/** Moves the given list of job_ids from the reserved_jobs_list to the
    job_info_list.  Caller must have locks to both lists before calling.
    If a given item in jobids is not found in the reserved_jobs_list, it
    is ignored and the function continues.
    @return true or false (upon success or failure, respectively)
*/
static int mv_jobs_res_to_info(const char* job_ids[]);    

/** Moves the given valid jobid from the job_reserved list to the job_info
    list.  This method acquires and releases locks to both lists appropriately.
    Returns false if jobid not found on job_reserved_list.
    @return true (on success) or false
*/
static int mv_job_res_to_info(const char* jobid);

/** Removes a given job id from the job_info_list.  Method acquires
    and releases job_info_list_lock.
    @return true (upon success) or false 
*/
static int rm_infolist(const char* job_id);

/** Removes a given job id from the reserved_job_list.  This method acquires and 
    releases reserved_job_list lock itself.
    @return true (upon success) or false 
*/
static int rm_reslist(const char* job_id);

/** Change the status of a given job on the reserved list to FINISHED.  This
    method acquires and releases the reserved_job_list lock itself.  If the 
    jobid is not found in the reserved_jobs_list, the function returns false.
    @return true (upon success) or false
*/
static int mark_res_job_finished(const char* job_id);

/** Releases both the reserved_list and info_list locks and
    returns the error code given.  The caller must have both
    locks before calling this method.
    @return drmaa_err_code
*/
static int rel_locks(const int drmaa_err_code);

/** Determines the library's base directory, allocates the required memory
    for buf, and copies the full path name there.  A successful result contains
    a trailing forward slash or backslash, depanding upon the system.
    @return true (on success) or false otherwise
*/
static int get_base_dir(char** buf);

/** Places the given jobid on hold.  Assumes job is in proper state already.
    @return drmaa error code appropriate for drmaa_control()
*/
static int hold_job(const char* jobid, char* error_diagnosis, size_t error_diag_len);

/** Releases the given jobid, which is assumed to be already on hold.
    @return drmaa error code appropriate for drmaa_control()
*/
static int release_job(const char* jobid, char* error_diagnosis, size_t error_diag_len);

/** Terminates the given valid jobid by calling condor_rm.  Does not 
    remove the jobid from the library's internal data structures.
    @return drmaa error code appropriate for drmaa_control()
*/
static int terminate_job(const char* jobid, char* error_diagnosis, 
		  size_t error_diag_len);

/** Frees the memory held by the entries in the job_info_list */
static void free_job_info_list();

/** Frees the memory held by the entries in the reserved_job_list */
static void free_reserved_job_list();

/** Removes all files in the submit file directory */
static void clean_submit_file_dir();

/** Unlocks the info_list_lock */
static void unlock_info_list_lock();

/** Unlocks the reserved_list_lock */
static void unlock_reserved_list_lock();

/** Unlocks the lock of the given condor_drmaa_job_info_t */
static void unlock_job_info(condor_drmaa_job_info_t* job_info);

/** Obtains the name of the local schedd.  Sets the "schedd_name" global
    variable upon success.  
    @return true (upon success) or false
*/
static int get_schedd_name(char *error_diagnosis, size_t error_diag_len);

static int 
is_number(const char* str)
{
    int result = 1;
    int i;

    for (i=0; i < strlen(str); i++){
	if (!isdigit((int)str[i])){
	    result = 0;
	    break;
	}
    }

    return result;
}

static job_attr_t* 
create_job_attribute()
{
    job_attr_t* result = (job_attr_t*)malloc(sizeof(job_attr_t));

    if (result != NULL){
	result->num_values = 0;
	result->next = NULL;
    }

    return result;
}

static condor_drmaa_job_info_t*
create_job_info(const char* job_id)
{
    condor_drmaa_job_info_t* result = NULL;

    if (strlen(job_id)+1 <= MAX_JOBID_LEN){
	result = (condor_drmaa_job_info_t*)malloc(sizeof(condor_drmaa_job_info_t));
	if (result != NULL) {
	    snprintf(result->id, MAX_JOBID_LEN, "%s", job_id);
	    result->next = NULL;
#ifdef WIN32
	    InitializeCriticalSection(&result->lock);
#else
#if HAS_PTHREADS
	    pthread_mutex_init(&result->lock, NULL);
#endif
#endif    
	}
    }

    return result;
}

static void
destroy_job_attribute(job_attr_t* ja)
{
    int i;

    if (ja->num_values == 1)
	free(ja->val.value);
    else if (ja->num_values > 1){
	for (i = 0; i < ja->num_values; i++)
	    free(ja->val.values[i]);
    }
    free(ja);
}

static void
destroy_job_info(condor_drmaa_job_info_t* job_info)
{
    if (job_info != NULL){
#ifdef WIN32
	DeleteCriticalSection(&job_info->lock);
#else
#if HAS_PTHREADS
	pthread_mutex_destroy(&job_info->lock);
#endif
#endif
    }
    free(job_info);
}

static int 
is_valid_attr_name(const char* name, char* error_diagnosis, 
		   size_t error_diag_len)
{
    int result = 0;

    if (name == NULL)
	snprintf(error_diagnosis, error_diag_len, "Attribute name is empty");
    else if ((strlen(name) + 1) > DRMAA_ATTR_BUFFER)
	snprintf(error_diagnosis, error_diag_len, 
		 "Attribute name exceeds DRMAA_ATTR_BUFFER");
    else if (strcmp(name, DRMAA_REMOTE_COMMAND) != 0 &&
	     strcmp(name, DRMAA_JS_STATE) != 0 && 
	     strcmp(name, DRMAA_WD) != 0 &&
	     strcmp(name, DRMAA_JOB_CATEGORY) != 0 && 
	     strcmp(name, DRMAA_NATIVE_SPECIFICATION) != 0 &&
	     strcmp(name, DRMAA_BLOCK_EMAIL) != 0 && 
	     strcmp(name, DRMAA_START_TIME) != 0 && 
	     strcmp(name, DRMAA_JOB_NAME) != 0 && 
	     strcmp(name, DRMAA_INPUT_PATH) != 0 && 
	     strcmp(name, DRMAA_OUTPUT_PATH) != 0 && 
	     strcmp(name, DRMAA_ERROR_PATH) != 0 && 
	     strcmp(name, DRMAA_JOIN_FILES) != 0 && 
	     strcmp(name, DRMAA_TRANSFER_FILES) != 0 && 
	     strcmp(name, DRMAA_DEADLINE_TIME) != 0 && 
	     strcmp(name, DRMAA_WCT_HLIMIT) != 0 && 
	     strcmp(name, DRMAA_WCT_SLIMIT) != 0 && 
	     strcmp(name, DRMAA_DURATION_HLIMIT) != 0 && 
	     strcmp(name, DRMAA_DURATION_SLIMIT) != 0 &&
	     strcmp(name, DRMAA_V_ARGV) != 0 &&
	     strcmp(name, DRMAA_V_ENV) != 0 &&
	     strcmp(name, DRMAA_V_EMAIL) != 0)
	snprintf(error_diagnosis, error_diag_len, "Unrecognized attribute name");
    else
	result = 1;

    return result;
}


static int 
is_valid_attr_value(int* err_cd, const char* name, const char* value, 
		    char* error_diagnosis, size_t error_diag_len)
{
    int result = 0;
    int i_value;

    if (value == NULL){
	snprintf(error_diagnosis, error_diag_len, "%s: no value specified",
		 name);
	*err_cd = DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
    }
    else {
	// case for each attribute name
	// TODO: Include cases for validating all other JobAttrName value
	if (strcmp(name, DRMAA_BLOCK_EMAIL) == 0){
	    // must be 0 or 1
	    if (!is_number(value)){
		snprintf(error_diagnosis, error_diag_len, "%s: not a number",
			 name);
		*err_cd = DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
	    }
	    else {
		i_value = atoi(value); 
		if (i_value != 0 && i_value != 1){ 
		    snprintf(error_diagnosis, error_diag_len, "%s: must be a 0"\
			     " or 1", name);
		    *err_cd = DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
		}
		else		
		    result = 1;
	    }
	} 
	else {
	    // TODO: add validation for all other supported attributes
	    result = 1;  
	}
    }

    return result;
}

static int 
is_scalar_attr(const char* name, char* error_diagnosis, size_t error_diag_len)
{
    int result = 0;

    if (name == NULL)
	snprintf(error_diagnosis, error_diag_len, "Attribute name is empty");
    else if (strcmp(name, DRMAA_REMOTE_COMMAND) == 0 ||
	     strcmp(name, DRMAA_JS_STATE) == 0 || 
	     strcmp(name, DRMAA_WD) == 0 ||
	     strcmp(name, DRMAA_JOB_CATEGORY) == 0 || 
	     strcmp(name, DRMAA_NATIVE_SPECIFICATION) == 0 ||
	     strcmp(name, DRMAA_BLOCK_EMAIL) == 0 || 
	     strcmp(name, DRMAA_START_TIME) == 0 || 
	     strcmp(name, DRMAA_JOB_NAME) == 0 || 
	     strcmp(name, DRMAA_INPUT_PATH) == 0 || 
	     strcmp(name, DRMAA_OUTPUT_PATH) == 0 || 
	     strcmp(name, DRMAA_ERROR_PATH) == 0 || 
	     strcmp(name, DRMAA_JOIN_FILES) == 0 ||
	     strcmp(name, DRMAA_TRANSFER_FILES) == 0 ||  
	     strcmp(name, DRMAA_DEADLINE_TIME) == 0 || 
	     strcmp(name, DRMAA_WCT_HLIMIT) == 0 || 
	     strcmp(name, DRMAA_WCT_SLIMIT) == 0 || 
	     strcmp(name, DRMAA_DURATION_HLIMIT) == 0 || 
	     strcmp(name, DRMAA_DURATION_SLIMIT) == 0)
	result = 1;
    else
	snprintf(error_diagnosis, error_diag_len,
		"Attribute name does not specify a scalar value");

    return result;
}

static int 
is_vector_attr(const char* name, char* error_diagnosis, size_t error_diag_len)
{
    int result = 0;

    if (name == NULL)
	snprintf(error_diagnosis, error_diag_len, "Attribute name is empty");
    else if (strcmp(name, DRMAA_V_ARGV) == 0 ||
	     strcmp(name, DRMAA_V_ENV) == 0 || 
	     strcmp(name, DRMAA_V_EMAIL) == 0)
	result = 1;
    else
	snprintf(error_diagnosis, error_diag_len, 
		 "Attribute name does not specify a vector value");

    return result;
}

static int 
is_supported_attr(const char* name, char* error_diagnosis, size_t error_diag_len)
{
    int result = 0;

    if (name == NULL)
	snprintf(error_diagnosis, error_diag_len, "Attribute name is empty");
    else if (strcmp(name, DRMAA_REMOTE_COMMAND) == 0 ||
	     strcmp(name, DRMAA_JS_STATE) == 0 || 
	     //strcmp(name, DRMAA_WD) == 0 || 
	     //strcmp(name, DRMAA_JOB_CATEGORY) == 0 || 
	     strcmp(name, DRMAA_NATIVE_SPECIFICATION) == 0 ||
	     strcmp(name, DRMAA_BLOCK_EMAIL) == 0 || 
	     //strcmp(name, DRMAA_START_TIME) == 0 || 
	     //strcmp(name, DRMAA_JOB_NAME) == 0 || 
	     strcmp(name, DRMAA_INPUT_PATH) == 0 || 
	     strcmp(name, DRMAA_OUTPUT_PATH) == 0 || 
	     strcmp(name, DRMAA_ERROR_PATH) == 0 || 
	     //strcmp(name, DRMAA_JOIN_FILES) == 0 ||
	     /*strcmp(name, DRMAA_TRANSFER_FILES) == 0 ||
	       strcmp(name, DRMAA_DEADLINE_TIME) == 0 || 
	       strcmp(name, DRMAA_WCT_HLIMIT) == 0 || 
	       strcmp(name, DRMAA_WCT_SLIMIT) == 0 || 
	       strcmp(name, DRMAA_DURATION_HLIMIT) == 0 || 
	       strcmp(name, DRMAA_DURATION_SLIMIT) == 0 || */
	     strcmp(name, DRMAA_V_ARGV) == 0 ||  
	     strcmp(name, DRMAA_V_ENV) == 0 || 
	     strcmp(name, DRMAA_V_EMAIL) == 0)
	result = 1;
    else
	snprintf(error_diagnosis, error_diag_len, 
		 "Attribute is not currently supported");

    return result;
}

/*
static void 
print_ja(const job_attr_t* ja)
{
    int i;

    printf("\tName: %s   # of Values: %d\n", ja->name, ja->num_values);
    if (ja->num_values == 1)		   
	printf("\t\tValue 1: \"%s\"\n", ja->val.value);
    else {
	for(i=0; i < ja->num_values; i++)
	    printf("\t\tValue %d: \"%s\"\n", i, ja->val.values[i]);
    }
}
*/

static drmaa_job_template_t* 
create_job_template()
{
    drmaa_job_template_t* jt = (drmaa_job_template_t*)
	malloc(sizeof(drmaa_job_template_t*));

    if (jt != NULL){
	jt->num_attr = 0;
	jt->head = NULL;
    }

    return jt;
}

static int 
is_valid_job_template(const drmaa_job_template_t* jt, char* error_diagnosis,
	  size_t error_diag_len)
{
    int result = 0;

    if (jt == NULL)
	snprintf(error_diagnosis, error_diag_len, "Job template is null");
    else {
	result = 1;  
	/*  // TODO
        cur = jt;
	while (cur != NULL){
	    result = isValidJobAttribute(cur, drmaa_context_error_buf);
	    if (result)
		cur = cur->next;
	    else
		break;
	}
	*/
    }

    return result;
}

static job_attr_t* 
find_attr(const drmaa_job_template_t* jt, const char* name, 
	  char* error_diagnosis, size_t error_diag_len)
{
    job_attr_t* result = jt->head;
    int found_attr = 0;

    while (!found_attr && result != NULL){
	if (strcmp(result->name, name) == 0)
	    found_attr = 1;
	else 
	    result = result->next;
    }

    if (!found_attr){
	result = NULL;
	snprintf(error_diagnosis, error_diag_len, "Unable to find %s in the " \
		 "job template.", name);
    }    

    return result;
}

static int 
contains_attr(const drmaa_job_template_t* jt, const char* name, 
	      char* error_diagnosis, size_t error_diag_len)
{
    int result = 0;
    job_attr_t* cur = jt->head;

    while (!result && cur != NULL){
	if (strcmp(cur->name, name) == 0){
	    result = 1;
	    snprintf(error_diagnosis, error_diag_len, 
		     "Attribute already set in job template");
	}
	else
	    cur = cur->next;
    }
    
    return result;
}

/*
static void
print_jt(const drmaa_job_template_t* jt)
{
    job_attr_t* ja;
    int i;

    if (jt == NULL)
	printf("print_jt(): NULL job template\n");
    else {
	printf("print_jt(): Job Template has %d attribute(s)\n", jt->num_attr);

	ja = jt->head;
	i = 0;
	while (ja != NULL){
	    printf("\tAttribute #%d", i);
	    print_ja(ja);
	    ja = ja->next;
	    i++;
	}
    }
}
*/

static int 
create_submit_file(char** submit_fn, const drmaa_job_template_t* jt, 
		   char* error_diagnosis, size_t error_diag_len)
{
    char* gen_file_name;
    FILE* fs;
    time_t now;
    job_attr_t* ja;

    // Generate a unique file name
    if (generate_unique_file_name(&gen_file_name) != SUCCESS){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to generate submit file name");
	return DRMAA_ERRNO_TRY_LATER;
    }	    

    // Prepare the submit file name
    if ((*submit_fn = (char*)malloc(strlen(file_dir) + 
				    strlen(SUBMIT_FILE_DIR) +
				    strlen(gen_file_name) + 1)) == NULL){
	free(gen_file_name);
	return DRMAA_ERRNO_NO_MEMORY;
    }
    sprintf(*submit_fn, "%s%s%s", file_dir, SUBMIT_FILE_DIR, gen_file_name);
    free(gen_file_name);

    // Create the file
    if ((fs = fopen(*submit_fn, "w")) == NULL){
	free(*submit_fn);
 	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to create submission file");
	return DRMAA_ERRNO_TRY_LATER;
    }

    // Write the job template into the file
    if (fprintf(fs, "#\n# Condor Submit file\n") < 1){
	free(*submit_fn);
	snprintf(error_diagnosis, error_diag_len, 
		 "Failed to write to submit file");
	fclose(fs);
	return DRMAA_ERRNO_TRY_LATER;
    }
    if ((int)(now = time(NULL)) != -1)
	fprintf(fs, "# Automatically generated by DRMAA library on %s",
		ctime(&now));
    else 
	fprintf(fs, "# Automatically generated by DRMAA library\n");
    fprintf(fs, "#\n\n");
    fprintf(fs, "%-*s= %s%s%s.$(Cluster).$(Process)%s\n", 
	    SUBMIT_FILE_COL_SIZE, "Log", file_dir, LOG_FILE_DIR, 
	    schedd_name, LOG_FILE_EXTN);  
    fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, "Universe", "standard");
    ja = jt->head;
    while (ja != NULL){
	if (write_job_attr(fs, ja) != SUCCESS){
	    free(*submit_fn);
	    snprintf(error_diagnosis, error_diag_len, 
		     "Unable to write job attribute to file");
	    fclose(fs);
	    return DRMAA_ERRNO_TRY_LATER;
	}
	ja = ja->next;
    }

    // Write ja's that require knowledge of other ja's
    if (write_special_attrs(fs, jt) != SUCCESS){
	free(*submit_fn);
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to write job attributes to file");
	fclose(fs);
	return DRMAA_ERRNO_TRY_LATER;
    }
    fprintf(fs, "Queue 1\n");  
    fclose(fs);
    
    return DRMAA_ERRNO_SUCCESS;
}

static int 
generate_unique_file_name(char** fname)
{
    int i;
    char tmp[1000];
    time_t tm;

    sleep(1);  // or could rand(), but how guarantee unique?
    tm = time(NULL);
    i = strftime(tmp, sizeof(tmp)-1, "%Y%m%d_%H%M%S_submission",
		 localtime(&tm));
    snprintf(tmp+i, sizeof(tmp)-1, "%s", SUBMIT_FILE_EXTN);
    *fname = (char*)malloc(strlen(tmp)+1);
    strcpy(*fname, tmp);

    return SUCCESS;
}

static char * 
substitute_placeholders(const char *orig)
{
    char *result = NULL, *tmp, *loc;
    int i, j;

    /* Test if orig contains any placeholders */
    if (strstr(orig, DRMAA_PLACEHOLDER_INCR) == NULL &&
	strstr(orig, DRMAA_PLACEHOLDER_HD) == NULL &&
	strstr(orig, DRMAA_PLACEHOLDER_WD) == NULL){

	result = strdup(orig);
    }
    else {
	result = (char*)malloc(strlen(orig) + 1000);
	tmp = strdup(orig);

	/* $drmaa_incr_ph$ */	
	if ((loc = strstr(tmp, DRMAA_PLACEHOLDER_INCR)) != NULL){
	    for (i=0; &tmp[i] != loc; i++){
		result[i] = tmp[i];
	    }
	    result[i] = '\0';	    

	    strcat(result, "$(Process)");
	    j = i + strlen("$(Process)");
	    i += strlen(DRMAA_PLACEHOLDER_INCR);

	    for(; tmp[i] != '\0'; i++, j++){
		result[j] = tmp[i];
	    }
	    result[j] = '\0';
	    
	    free(tmp);
	    tmp = strdup(result);
	}

	/* $drmaa_hd_ph$ */
	if ((loc = strstr(tmp, DRMAA_PLACEHOLDER_HD)) != NULL){
	    for(i=0; &tmp[i] != loc; i++){
		result[i] = tmp[i];
	    }
	    result[i] = '\0';

	    strcat(result, "$ENV(HOME)");  /* TODO: UNIX vs Windows */
	    j = i + strlen("$ENV(HOME)");
	    i += strlen(DRMAA_PLACEHOLDER_HD);
	    
	    for(; tmp[i] != '\0'; i++, j++){
		result[j] = tmp[i];
	    }
	    result[j] = '\0';

	    free(tmp);
	    tmp = strdup(result);
	}

	/* $drmaa_wd_ph$ */
	// TODO:  may involve initialdir

	free(tmp);
    }

    return result;
}

static int 
write_job_attr(FILE* fs, const job_attr_t* ja)
{
    int result = FAILURE;
    int num_bw;
    char *sub_ph;

    if (strcmp(ja->name, DRMAA_REMOTE_COMMAND) == 0){
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Executable", ja->val.value);
    }
    else if (strcmp(ja->name, DRMAA_JS_STATE) == 0){
	if (strcmp(ja->val.value, DRMAA_SUBMISSION_STATE_ACTIVE) == 0)
	    num_bw = fprintf(fs, "%-*s= False\n", SUBMIT_FILE_COL_SIZE, 
				   "Hold");
	else
	    num_bw = fprintf(fs, "%-*s= True\n", SUBMIT_FILE_COL_SIZE, 
				   "Hold");
    }
    /*
    else if (strcmp(ja->name, DRMAA_WD) == 0){
	// TODO: $drmaa_incr_ph$
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Initialdir",ja->val.value);
    }
    else if (strcmp(ja->name, DRMAA_JOB_CATEGORY) == 0){
	// TODO: DRMAA_JOB_CATEGORY

    }
    */
    else if (strcmp(ja->name, DRMAA_NATIVE_SPECIFICATION) == 0){
	num_bw = fprintf(fs, "%-*s\n", SUBMIT_FILE_COL_SIZE, ja->val.value); 
    }
    else if (strcmp(ja->name, DRMAA_BLOCK_EMAIL) == 0){
	if (strcmp(ja->val.value, "1") == 0)
	    num_bw = fprintf(fs, "%-*s= Never\n", SUBMIT_FILE_COL_SIZE, 
				   "Notification");
    }     
    /*
    else if (strcmp(ja->name, DRMAA_START_TIME) == 0){
	// TODO: 
    }
    else if (strcmp(ja->name, DRMAA_JOB_NAME) == 0){
	// TODO: 
    }
    */
    else if (strcmp(ja->name, DRMAA_INPUT_PATH) == 0){
	sub_ph = substitute_placeholders(ja->val.value);
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Input", sub_ph);
	free(sub_ph);
    }
    else if (strcmp(ja->name, DRMAA_OUTPUT_PATH) == 0){
	// TODO: remove this quick and dirty implementation
	sub_ph = substitute_placeholders(ja->val.value);
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Output", sub_ph);
	free(sub_ph);
    }
    else if (strcmp(ja->name, DRMAA_ERROR_PATH) == 0){
	// TODO: $drmaa_incr_ph$, $drmaa_hd_ph$, $drmaa_wd_ph$
	sub_ph = substitute_placeholders(ja->val.value);
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Error", sub_ph);
	free(sub_ph);
    }
    /*
    else if (strcmp(ja->name, DRMAA_JOIN_FILES) == 0){
    }
    */
    /*
// TODO
#define DRMAA_TRANSFER_FILES "drmaa_transfer_files"
#define DRMAA_DEADLINE_TIME "drmaa_deadline_time"
#define DRMAA_WCT_HLIMIT "drmaa_wct_hlimit"
#define DRMAA_WCT_SLIMIT "drmaa_wct_slimit"
#define DRMAA_DURATION_HLIMIT "drmaa_durartion_hlimit"
#define DRMAA_DURATION_SLIMIT "drmaa_durartion_slimit"
    */
    else if (strcmp(ja->name, DRMAA_V_ARGV) == 0){
	fprintf(fs, "%-*s= ", SUBMIT_FILE_COL_SIZE, "Arguments");
	num_bw = write_v_job_attr(fs, ja);
    }
    else if (strcmp(ja->name, DRMAA_V_ENV) == 0){
	fprintf(fs, "%-*s= ", SUBMIT_FILE_COL_SIZE, "Environment");
	num_bw = write_v_job_attr(fs, ja);
    }
    else if (strcmp(ja->name, DRMAA_V_EMAIL) == 0){
	fprintf(fs, "%-*s= ", SUBMIT_FILE_COL_SIZE, "Notify_user");
	num_bw = write_v_job_attr(fs, ja);
    }

    if (num_bw >= 0)
	result = SUCCESS;

    return result;
}

static int 
write_v_job_attr(FILE* fs, const job_attr_t* ja)
{
    int i, result;

    if (ja->num_values == 1)
	result = fprintf(fs, "%s\n", ja->val.value);
    else {
	for (i = 0; i < ja->num_values; i++){
	    if ((result = fprintf(fs, "%s", ja->val.values[i]) < 0))
		break;	
    
	    // Space DRMAA_V_ENV values
	    if (strcmp(ja->name, DRMAA_V_ENV) == 0 &&
		(i+1) < ja->num_values)
		fprintf(fs, ";");  // TODO: Unix vs. Windows

	    fprintf(fs, " ");
	}
	result = fprintf(fs, "\n");
    }

    return result;
}

static int 
write_special_attrs(FILE* fs, const drmaa_job_template_t* jt)
{
    int result = 0;
    //job_attr_t* ja, ja2;

    /* JOIN_FILES may depend on DRMAA_OUTPUT_PATH */
    // TODO: $drmaa_incr_ph$, $drmaa_hd_ph$, $drmaa_wd_ph$
    /*
    if (contains_attr(jt, DRMAA_JOIN_FILES, NULL, 0)){
	if ((ja = find_attr(jt, DRMAA_JOIN_FILES, NULL, 0)) != NULL){
	    if (strcmp((char*)ja->value, YES) == 0){
		if (contains_attr(jt, DRMAA_OUTPUT_PATH
	    }
	}
	}*/

    /* DRMAA_ERROR_STREAM may depend on DRMAA_JOIN_FILES */

    return result;
}

static int 
submit_job(char* job_id, size_t job_id_len, const char* submit_file_name, 
	    char* error_diagnosis, size_t error_diag_len)
{
    FILE* fs;
    char buffer[MAX_READ_LEN];
    char last_buffer[MAX_READ_LEN];
    char cmd[SUBMIT_CMD_LEN];
    char cluster_num[MAX_JOBID_LEN];
    char job_num[MAX_JOBID_LEN];

    // Prepare command
    sprintf(cmd, "%s %s", SUBMIT_CMD, submit_file_name);

    // Submit to condor
    fs = popen(cmd, "r");
    if (fs == NULL){
	snprintf(error_diagnosis, error_diag_len,
		 "Unable to perform submit call");
	return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    }
    else if ((int)fs == -1){
	snprintf(error_diagnosis, error_diag_len, "Submit call failed");
	return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;	
    }

    // Parse output - look for "<X> job<s> submitted to cluster <#>" line
    do {
	if (fgets(buffer, MAX_READ_LEN, fs) == NULL){
	    snprintf(error_diagnosis, error_diag_len, "%s", last_buffer);
	    pclose(fs);
	    return DRMAA_ERRNO_DENIED_BY_DRM;
	}
	strcpy(last_buffer, buffer);
    } while (strstr(buffer, "submitted to cluster") == NULL);

    // TODO: May have warnings.  If warnings, remove job, copy warnings
    // into error_diag, and return error

    // Parse job number and cluster number
    sscanf(buffer, "%s job(s) submitted to cluster %s", job_num, 
	   cluster_num);
    cluster_num[strlen(cluster_num) - 1] = '\0';  // squash trailing period

    // Verify job_id_len is large enough
    if ( (strlen(schedd_name) + strlen(cluster_num) + strlen(job_num) 
	  + 1 + (2 * strlen(JOBID_TOKENIZER))) > (int)job_id_len ){
	snprintf(error_diagnosis, error_diag_len, "Job ID length is too small");
	pclose(fs);
	// TODO: remove job from condor?	
	return DRMAA_ERRNO_TRY_LATER;	
    }
    
    // Fill job_id with <schedd name.cluster id.job id>
    sprintf(job_id, "%s%s%s%s0", schedd_name, JOBID_TOKENIZER, cluster_num,
	    JOBID_TOKENIZER);
    pclose(fs);

    return DRMAA_ERRNO_SUCCESS;
}

static int
is_valid_stat(const int stat)
{
    return stat >= STAT_ABORTED;
}

static int
is_valid_job_id(const char* job_id)
{
    return (job_id != NULL && 
	    strlen(job_id) >= MIN_JOBID_LEN &&
	    strlen(job_id)+1 < MAX_JOBID_LEN );
}

static FILE* 
open_log_file(const char* job_id)
{
    char log_file_nm[MAX_FILE_NAME_LEN];

    snprintf(log_file_nm, MAX_FILE_NAME_LEN-1, "%s%s%s%s", file_dir, 
	     LOG_FILE_DIR, job_id, LOG_FILE_EXTN);
    return fopen(log_file_nm, "r");
}

static int 
rm_log_file(const char* job_id)
{
    char log_file_nm[2000];
    sprintf(log_file_nm, "%s%s%s%s", file_dir, LOG_FILE_DIR, job_id, 
	    LOG_FILE_EXTN);
    return remove(log_file_nm)? 0 : 1;
}

static int 
wait_job(const char* job_id, const int dispose, const int get_stat_rusage,
	 int* stat, signed long timeout, const time_t start,
	 drmaa_attr_values_t** rusage, char* error_diagnosis, 
	 size_t error_diag_len)
{
    int result = DRMAA_ERRNO_INVALID_JOB;
    int time_up = 0, found_job_term = 0, job_exit_val;
    double sleep_len;
    FILE* logFS;
    char line[MAX_LOG_FILE_LINE_LEN], r_val[MAX_LOG_FILE_LINE_LEN];

    if (get_stat_rusage)
	*rusage = NULL;

    // 1. Open log file
    if ((logFS = open_log_file(job_id)) == NULL){
	snprintf(error_diagnosis, error_diag_len, "Unable to open log file");
	return DRMAA_ERRNO_INVALID_JOB;
    }

    // 2. Scan for completion event within timeframe
    while (!found_job_term && !time_up){
	
	while (!found_job_term && fgets(line, sizeof(line), logFS) != NULL){
	    if (strstr(line, "Job terminated") != NULL){
		found_job_term = 1;
		
		if (get_stat_rusage){
		    // scan further for status info
		    sleep(1);  // wait for further I/O
		    if (fgets(line, sizeof(line), logFS) != NULL){
			if (strstr(line, "Normal termination") != NULL){
			    sscanf(line, "%*s Normal termination (return " \
				   "value %d)", &job_exit_val);
			    if (job_exit_val > -1)
				*stat = STAT_NOR_BASE + job_exit_val;
			    else
				*stat = STAT_NOR_BASE;
			}
			else if (strstr(line, "Abnormal termination (signal") 
				 != NULL){
			    sscanf(line, "(signal %d)", &job_exit_val);
			    //*stat = condor_sig_to_drmaa(job_exit_val); // TODO
			    *stat = STAT_UNKNOWN;
			}
			else 
			    *stat = STAT_UNKNOWN;  // really "abnormal term"
		    }
		
		    // scan further for rusage data
		    sleep(1); // wait for further I/O
		    while (fgets(line, sizeof(line), logFS) != NULL){
			if (strstr(line, "Run Bytes Sent By Job") != NULL){
			    sscanf(line, "%s - Run Bytes Sent By Job", r_val);
			    *rusage = create_dav(1);
			    (*rusage)->values[0] = (char*)malloc(strlen(r_val)+1);
			    strcpy((*rusage)->values[0], r_val);
			    break;
			}		    
			// TODO: other rusage values
		    }
		}
	    }
	    else if ((strstr(line, "Job not properly linked for Condor") 
		      != NULL)
		     || (strstr(line, "aborted") != NULL)){
		found_job_term = 1;
		if (get_stat_rusage)
		    *stat = STAT_ABORTED;		
	    }
	}

	// check if time is up
	if (timeout != DRMAA_TIMEOUT_WAIT_FOREVER){
	    if (timeout == DRMAA_TIMEOUT_NO_WAIT)
		time_up = 1;
	    else if (difftime(time(NULL), start) >= (double)timeout)
		time_up = 1;
	}


	// set up for next pass
	if (!time_up && !found_job_term){
	    rewind(logFS);

	    // wait for more output to log file
	    if (timeout == DRMAA_TIMEOUT_WAIT_FOREVER)
		sleep_len = WAIT_FOREVER_SLP_TM; 
	    else {
		sleep_len = difftime(time(NULL), start) / 2;
		if (sleep_len > UINT_MAX)
		    sleep_len = UINT_MAX;
	    }
	    sleep((unsigned int)sleep_len);
	}
    }
    

    // 3. Dispose of job data (log file only)
    fclose(logFS);
    if (found_job_term){
	result = DRMAA_ERRNO_SUCCESS;
	if (dispose){
	    if (!rm_log_file(job_id))
		snprintf(error_diagnosis, error_diag_len, 
			 "Warning: Failed to delete log file");
	}
    }
    else {
	if (get_stat_rusage)
	    *stat = STAT_UNKNOWN;
	result = DRMAA_ERRNO_EXIT_TIMEOUT;
    }

    return result;
}

static drmaa_attr_values_t*
create_dav(int size)
{
    drmaa_attr_values_t* dav = (drmaa_attr_values_t*)
	malloc(sizeof(drmaa_attr_values_t));
    
    if (dav != NULL){
	if ((dav->values = (char**)malloc(sizeof(char*)*size)) == NULL)
	    free(dav);
	else {
	    dav->index = 0;
	    dav->size = size;
	}
    }

    return dav;
}

static int 
mv_jobs_res_to_info(const char *job_ids[])
{
    int i = 0;
    condor_drmaa_job_info_t* cur_res, *last;

    while (job_ids[i] != NULL){
	cur_res = reserved_job_list;
	last = cur_res;
	while (cur_res != NULL){
	    if (strcmp(cur_res->id, job_ids[i]) == 0){
		if (last == cur_res)
		    reserved_job_list = cur_res->next;
		else
		    last->next = cur_res->next;
		cur_res->next = job_info_list;
		job_info_list = cur_res;
		cur_res->state = SUBMITTED;
		num_reserved_jobs--;
		num_info_jobs++;
		cur_res = NULL; // break
	    }
	    else {
		last = cur_res;
		cur_res = cur_res->next;
	    }
	}
	i++;
    }
    
    return 1;
}

static int 
mv_job_res_to_info(const char* jobid)
{
    int result = 0;
    condor_drmaa_job_info_t* cur_res, *last;    

#ifdef WIN32
    EnterCriticalSection(&reserved_list_lock);
    EnterCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&reserved_list_lock) == 0 &&
	pthread_mutex_lock(&info_list_lock)){
#endif
#endif
	cur_res = reserved_job_list;
	last = cur_res;
	while (!result && cur_res != NULL){
	    if (strcmp(cur_res->id, jobid) == 0){
		if (last == cur_res)
		    reserved_job_list = cur_res->next;
		else
		    last->next = cur_res->next;
		cur_res->next = job_info_list;
		job_info_list = cur_res;
		if (cur_res->state != FINISHED)
		    cur_res->state = SUBMITTED;
		num_reserved_jobs--;
		num_info_jobs++;
		result = 1;
	    }
	    else {
		last = cur_res;
		cur_res = cur_res->next;
	    }
	}

	rel_locks(0);
#if (!defined(WIN32) && HAS_PTHREADS)
    }
#endif
    return result;
}

static int
rm_infolist(const char* job_id)
{
    int result = 0;
    condor_drmaa_job_info_t* cur, *last;

#ifdef WIN32
    EnterCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&info_list_lock) == 0){
#endif
#endif    
 	cur = job_info_list;
	last = cur;
	while (cur != NULL){
	    if (strcmp(cur->id, job_id) == 0){
		if (cur == last)
		    job_info_list = cur->next;
		else 
		    last->next = cur->next;
		destroy_job_info(cur);
		num_info_jobs--;
		result = 1;
		cur = NULL;  // break;
	    }
	    else {
		last = cur;
		cur = cur->next;
	    }
	}
	unlock_info_list_lock();
#if (!defined(WIN32) && HAS_PTHREADS)
    }
#endif
    return result;
}

static int
rm_reslist(const char* job_id)
{
    int result = 0;
    condor_drmaa_job_info_t* cur, *last;

#ifdef WIN32
    EnterCriticalSection(&reserved_list_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&reserved_list_lock) == 0){
#endif
#endif    
	cur = reserved_job_list;
	last = cur;
	while (cur != NULL){
	    if (strcmp(cur->id, job_id) == 0){
		if (cur == last)
		    reserved_job_list = cur->next;
		else 
		    last->next = cur->next;
		destroy_job_info(cur);
		num_reserved_jobs--;
		result = 1;
		cur = NULL;  // break
	    }
	    else {
		last = cur;
		cur = cur->next;
	    }
	}
	unlock_reserved_list_lock();
#if (!defined(WIN32) && HAS_PTHREADS)
    }
#endif
    return result;
}

static int 
mark_res_job_finished(const char* job_id)
{
    int result = 0;
    condor_drmaa_job_info_t* cur;
#ifdef WIN32
    EnterCriticalSection(&reserved_list_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&reserved_list_lock) == 0){
#endif
#endif    
	cur = reserved_job_list;
	while (cur != NULL){
	    if (strcmp(cur->id, job_id) == 0){
		cur->state = FINISHED;
		result = 1;
		break;
	    }
	    else
		cur = cur->next;
	}
	unlock_reserved_list_lock();
#if (!defined(WIN32) && HAS_PTHREADS)
    }
#endif
    return result;
}

static int 
rel_locks(const int drmaa_err_code)
{
    unlock_info_list_lock();
    unlock_reserved_list_lock();
    return drmaa_err_code;
}

static int 
get_base_dir(char** buf)
{
    char* dir;
    struct stat s;

    // ala condor_c++_util/directory.C's temp_dir_path()
    dir = getenv("TEMP");
    if (!dir)
	dir = getenv("TMP");    
    if (!dir)
	dir = getenv("SPOOL");
    if (!dir){
#ifdef WIN32
	dir = strdup("c:\\Temp\\");
#else
	dir = strdup("/tmp/");
#endif
	if (stat(dir, &s) != 0 || !S_ISDIR(s.st_mode)){
	    free(dir);
#ifdef WIN32
	    dir = strdup("c:\\");
#else
	    dir = strdup("/");
#endif
	}	    
    }

#ifdef WIN32
    if (dir[strlen(dir)-1] == '\\') {
#else
    if (dir[strlen(dir)-1] == '/'){
#endif
	*buf = (char*)malloc(strlen(dir) + strlen(DRMAA_DIR) + 1);
	strcpy(*buf, dir);
    } else {
	*buf = (char*)malloc(strlen(dir) + strlen(DRMAA_DIR) + 2);
	strcpy(*buf, dir);
#ifdef WIN32
	strcat(*buf, "\\");
#else
	strcat(*buf, "/");
#endif
    }
    strcat(*buf, DRMAA_DIR);    
    free(dir);

    return 1;
}

static int 
hold_job(const char* jobid, char* error_diagnosis, size_t error_diag_len)
{
    int result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    char cmd[HOLD_CMD_LEN];
    char clu_proc[MAX_JOBID_LEN], buf[MAX_READ_LEN];
    FILE* fs;

    // prepare command: "condor_hold -name scheddname cluster.process"
    if (strstr(jobid, schedd_name) != jobid){
	snprintf(error_diagnosis, error_diag_len, "Unexpected job id format");
	return DRMAA_ERRNO_INVALID_JOB;
    }
    strcpy(clu_proc, jobid+strlen(schedd_name)+1);  // 1 for schedd.clu.proc
    snprintf(cmd, sizeof(cmd), "%s %s %s", HOLD_CMD, schedd_name, clu_proc);

    // execute command
    fs = popen(cmd, "r");
    if (fs == NULL){
	snprintf(error_diagnosis, error_diag_len, "Unable to perform hold call");
	return DRMAA_ERRNO_NO_MEMORY;
    }
    else if ((int)fs == -1){
	snprintf(error_diagnosis, error_diag_len, "Hold call failed");
	return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;	
    }

    // Parse output for success/failure
    if (fgets(buf, MAX_READ_LEN, fs) == NULL){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to read result of hold call");
	return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;		
    }
    // "Job $cluster.$proc held"
    // "Job $cluster.$proc already held"
    // "Job $cluster.$proc not found"
    // "condor_hold: Can't find address for schedd $scheddname"
    if (strstr(buf, "Job") != NULL){
	if (strstr(buf, "not found") != NULL){
	    snprintf(error_diagnosis, error_diag_len, "Job not found");
	    result = DRMAA_ERRNO_INVALID_JOB;		
	}
	else if (strstr(buf, "held") != NULL)
	    result = DRMAA_ERRNO_SUCCESS;
	else {
	    snprintf(error_diagnosis, error_diag_len, "%s", buf);
	    result = DRMAA_ERRNO_HOLD_INCONSISTENT_STATE;		
	}
    }
    else {
	snprintf(error_diagnosis, error_diag_len, "%s", buf);
	result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    }

    return result;
}

static int 
release_job(const char* jobid, char* error_diagnosis, size_t error_diag_len)
{
    int result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    char cmd[RELEASE_CMD_LEN];
    char clu_proc[MAX_JOBID_LEN], buf[MAX_READ_LEN];
    FILE* fs;

    // prepare command: "condor_release -name schedddname cluster.process"
    if (strstr(jobid, schedd_name) != jobid){
	snprintf(error_diagnosis, error_diag_len, "Unexpected job id format");
	return DRMAA_ERRNO_INVALID_JOB;
    }

    // 1 for schedd.clu.proc
    snprintf(clu_proc, sizeof(clu_proc), "%s", jobid+strlen(schedd_name)+1);

    snprintf(cmd, sizeof(cmd), "%s %s %s", RELEASE_CMD, schedd_name, clu_proc);

    // execute command
    fs = popen(cmd, "r");
    if (fs == NULL){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to perform release call");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if ((int)fs == -1){
	snprintf(error_diagnosis, error_diag_len, "Release call failed");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    
    // Parse output for success/failure
    if (fgets(buf, MAX_READ_LEN, fs) == NULL){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to read result of release call");
	return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;		
    }    
    // condor_release: unknown host schedd
    // Invalid cluster # from 0.0.
    // Warning: unrecognized ".0" skipped
    // Job 1.0 not found
    // Job 939091.0 released
    // Job 939091.0 not held to be released
    if (strstr(buf, "Job") != NULL){
	if (strstr(buf, "not found") != NULL){
	    snprintf(error_diagnosis, error_diag_len, "Job not found");
	    result = DRMAA_ERRNO_INVALID_JOB;		
	}
	else if (strstr(buf, "released") != NULL)
	    result = DRMAA_ERRNO_SUCCESS;
	else {
	    snprintf(error_diagnosis, error_diag_len, "%s", buf);
	    result = DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE;		
	}
    }
    else {
	snprintf(error_diagnosis, error_diag_len, "%s", buf);
	result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    }    

    return result;
}

static int 
terminate_job(const char* jobid, char* error_diagnosis, size_t error_diag_len)
{
    int result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    char cmd[TERMINATE_CMD_LEN];
    char clu_proc[MAX_JOBID_LEN], buf[MAX_READ_LEN];
    FILE* fs;

    // prepare command: "condor_rm -name schedddname cluster.process"
    if (strstr(jobid, schedd_name) != jobid){
	snprintf(error_diagnosis, error_diag_len, "Unexpected job id format");
	return DRMAA_ERRNO_INVALID_JOB;
    }

    // 1 for schedd.clu.proc
    snprintf(clu_proc, sizeof(clu_proc), "%s", jobid+strlen(schedd_name)+1);

    snprintf(cmd, sizeof(cmd), "%s %s %s", TERMINATE_CMD, schedd_name, clu_proc);

    // execute command
    fs = popen(cmd, "r");
    if (fs == NULL){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to perform terminate call");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if ((int)fs == -1){
	snprintf(error_diagnosis, error_diag_len, "Terminate call failed");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }

    // Parse output for success/failure
    if (fgets(buf, MAX_READ_LEN, fs) == NULL){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to read result of release call");
	return DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;		
    }    
    // Job 939609.0 marked for removal
    // Job 939609.0 not found
    // condor_rm: Can't find address for schedd $scheddname
    if (strstr(buf, "Job") != NULL){
	if (strstr(buf, "not found") != NULL){
	    snprintf(error_diagnosis, error_diag_len, "Job not found");
	    result = DRMAA_ERRNO_INVALID_JOB;		
	}
	else if (strstr(buf, "marked for removal") != NULL)
	    result = DRMAA_ERRNO_SUCCESS;
	else {
	    snprintf(error_diagnosis, error_diag_len, "%s", buf);
	    result = DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE;		
	}
    }
    else {
	snprintf(error_diagnosis, error_diag_len, "%s", buf);
	result = DRMAA_ERRNO_DRM_COMMUNICATION_FAILURE;
    }    
    
    return result;
}

static void
free_job_info_list()
{
    condor_drmaa_job_info_t* cur = job_info_list, *last = NULL;

    while (cur != NULL){
	free(last);
	last = cur;
	cur = cur->next;
    }
    free(last);
}

static void
free_reserved_job_list()
{
    condor_drmaa_job_info_t* cur = reserved_job_list, *last = NULL;

    while (cur != NULL){
	free(last);
	last = cur;
	cur = cur->next;
    }
    free(last);
}

static void 
clean_submit_file_dir()
{
    char cmd[3000];
#ifdef WIN32
    snprintf(cmd, sizeof(cmd)-1, "del /Q %s%s*.%s", file_dir, SUBMIT_FILE_DIR,
	     SUBMIT_FILE_EXTN);
#else
    snprintf(cmd, sizeof(cmd)-1, "rm -f %s%s%c%s", file_dir, SUBMIT_FILE_DIR,
	     '*', SUBMIT_FILE_EXTN);    
#endif
    system(cmd);
}

static void
unlock_info_list_lock()
{
#ifdef WIN32
    LeaveCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
    pthread_mutex_unlock(&info_list_lock);
#endif
#endif
}

static void 
unlock_reserved_list_lock()
{
#ifdef WIN32
    EnterCriticalSection(&reserved_list_lock);
#else
#if HAS_PTHREADS
    pthread_mutex_unlock(&reserved_list_lock);
#endif
#endif    
}

static void 
unlock_job_info(condor_drmaa_job_info_t* job_info)
{
#ifdef WIN32
    EnterCriticalSection(&job_info->lock);
#else
#if HAS_PTHREADS
    pthread_mutex_unlock(&job_info->lock);
#endif
#endif    
}

static int 
get_schedd_name(char *error_diagnosis, size_t error_diag_len)
{
    int result = 0;
#ifdef WIN32
    char tmp[1000];
    DWORD bufsize;

    if (GetComputerNameEx(ComputerNameDnsFullyQualified, tmp, 
			  &bufsize)){
	result = 1;
	schedd_name = strdup(tmp);	
    }
#else
    struct utsname host_info;

    if (uname(&host_info) == -1)
	snprintf(error_diagnosis, error_diag_len, 
		 "Failed to obtain name of local schedd");
    else {
	result = 1;
	schedd_name = strdup(host_info.nodename);
    }
#endif
    return result;
}
