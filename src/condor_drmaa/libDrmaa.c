
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

#include "auxDrmaa.c"   /* so we can pull in the static functions that
						   should be kept hidden from end-user code 
						 */

/* ------------------- private variables ------------------- */
static const char* DRMAA_ERR_MSGS [] = {
    /* ------------- these are relevant to all sections -------------- */
    "Success",
    "Internal error",
    "Failed to communicate properly with Condor",
    "Unauthorized use of Condor",
    "Invalid argument",
    "No active DRMAA session exists",
    "Unable to allocate needed memory",
    /* -------------- init and exit specific --------------- */
    "Invalid contact string",
    "Problem using default contact string",
    "No default contact string selected",
    "Initialization failed",
    "A DRMAA session is already active",
    "DRMS failed to exit properly",
    /* ---------------- job attributes specific -------------- */
    "Invalid attribute format",
    "Invalid attribute value",
    "Conflicting attribute values",
    /* --------------------- job submission specific -------------- */
    "Please try again later",
    "Request denied",
    /* ------------------------- job control specific -------------- */
    "Invalid job id",
    "Current job state does not permit it to be resumed",
    "Current job state does not permit it to be suspended", 
    "Current job state does not permit it to be put on hold",
    "Current job state does not permit it to be released",
    "Timeout expired", 
    "No rusage or stat information could be retrieved"
};
static const char* SIGNAL_NAMES[] = {
    "SIGHUP",   // TODO: more in signal(7)
    "SIGINT",
    "SIGKILL",
    "SIGSEGV",
    "SIGPIPE",
    "SIGALRM",
    "SIGTERM",
    "SIGURS1",
    "SIGURS2"
};

#ifdef WIN32
static CRITICAL_SECTION is_init_lock;
static int is_init_lock_initialized = 0;
#else
    #if HAS_PTHREADS
        static pthread_mutex_t is_init_lock = PTHREAD_MUTEX_INITIALIZER;
    // #else  // no lib init lock
    #endif
#endif
static int is_init = 0;

/* ------------------- private helper routines ------------------- */
static int is_lib_init();
//static int condor_sig_to_drmaa(const int condor_sig);
static void unlock_is_init_lock();

/* ---------- C/C++ language binding specific interfaces -------- */
int 
drmaa_get_next_attr_name(drmaa_attr_names_t* values, char *value, int value_len)
{
    int result = DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
    if ((values != NULL) && (values->index < values->size)){
	snprintf(value, value_len, values->attrs[values->index]);
	++values->index;  
	result = DRMAA_ERRNO_SUCCESS;
    }

    return result;
}

int 
drmaa_get_next_attr_value(drmaa_attr_values_t* values, char *value, 
			  int value_len)
{
    int result = DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
    if ((values != NULL) && (values->index < values->size)){
	snprintf(value, value_len, values->values[values->index]);
	++values->index;
	result = DRMAA_ERRNO_SUCCESS;
    }

    return result;
}

int
drmaa_get_next_job_id(drmaa_job_ids_t* values, char *value, int value_len)
{
    int result = DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
    if ((values != NULL) && (values->index < values->size)){
	snprintf(value, value_len, values->values[values->index]);
	++values->index;
	result = DRMAA_ERRNO_SUCCESS;
    }

    return result;
}

void
drmaa_release_attr_names(drmaa_attr_names_t* values)
{
    int i;

    if (values != NULL){
	for (i=0; i < values->size; i++)
	    free(values->attrs[i]);
	free(values->attrs);
	free(values);
    }
}

void drmaa_release_attr_values(drmaa_attr_values_t* values)
{
    int i;
    
    if (values != NULL){
	if (values->values != NULL){
	    for (i=0; i < values->size; i++)
		free(values->values[i]);
	}
	free(values);
    }
}

void
drmaa_release_job_ids(drmaa_job_ids_t* values)
{
    int i;
    
    if (values != NULL){
	for (i=0; i < values->size; i++)
	    free(values->values[i]);

	free(values);
    }
}

/* ------------------- init/exit routines ------------------- */
int
drmaa_init(const char *contact, char *error_diagnosis, size_t error_diag_len)
{
    int max_len;
    char* dir;

#ifdef WIN32
    if (!is_init_lock_initialized) {
	InitializeCriticalSection(&is_init_lock);
	is_init_lock_initialized = 1;
    }
    EnterCriticalSection(&is_init_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&is_init_lock) == 0){
#endif
#endif    	
        if (is_init){
	    unlock_is_init_lock();
            return DRMAA_ERRNO_ALREADY_ACTIVE_SESSION;
	}
	// TODO: contact condor_status
	    
	// Obtain base file directory path
	if (!get_base_dir(&file_dir)){
	    snprintf(error_diagnosis, error_diag_len, 
		     "Failed to determine base directory");
	    unlock_is_init_lock();
	    return DRMAA_ERRNO_INTERNAL_ERROR;
	}

	// Create directories
	if (mkdir(file_dir, S_IXUSR|S_IRUSR|S_IWUSR) == -1 && 
	    errno != EEXIST) {
	    snprintf(error_diagnosis, error_diag_len,
		     "Failed to make base directory");
	    unlock_is_init_lock();
	    return DRMAA_ERRNO_INTERNAL_ERROR;
	}
    
	max_len = strlen(SUBMIT_FILE_DIR) > strlen(LOG_FILE_DIR)?
	    strlen(SUBMIT_FILE_DIR) : strlen(LOG_FILE_DIR);
	dir = (char*)malloc(strlen(file_dir) + max_len + 1);
	strcpy(dir, file_dir);
	strcat(dir, SUBMIT_FILE_DIR);
	if (mkdir(dir, S_IXUSR|S_IRUSR|S_IWUSR) == -1 && errno != EEXIST){
	    snprintf(error_diagnosis, error_diag_len, 
		     "Failed to make submit file directory");
	    unlock_is_init_lock();
	    return DRMAA_ERRNO_INTERNAL_ERROR;
	}

	strcpy(dir, file_dir);
	strcat(dir, LOG_FILE_DIR);
	if (mkdir(dir, S_IXUSR|S_IRUSR|S_IWUSR) == -1 && errno != EEXIST){
	    snprintf(error_diagnosis, error_diag_len, 
		     "Failed to make log file directory");
	    unlock_is_init_lock();
	    return DRMAA_ERRNO_INTERNAL_ERROR;	    
	}
	free(dir);

	// TODO: Remove all files in submit directory, in case _exit() failed
	
	// TODO: verify library has write, read, and delete access to 
	// all its directories

	// Obtain name of local schedd
	if (!get_schedd_name(error_diagnosis, error_diag_len)){
	    unlock_is_init_lock();
	    return DRMAA_ERRNO_INTERNAL_ERROR;	    
	}	

	// Initialize other local data structures
	is_init = 1;
#ifdef WIN32
	InitializeCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
	pthread_mutex_init(&info_list_lock, NULL); 
#endif
#endif
	job_info_list = NULL;
	num_info_jobs = 0;
#ifdef WIN32
	InitializeCriticalSection(&reserved_list_lock);
#else
#if HAS_PTHREADS
	pthread_mutex_init(&reserved_list_lock, NULL);
#endif
#endif    
	reserved_job_list = NULL;
	num_reserved_jobs = 0;

	unlock_is_init_lock();
#if (!defined(WIN32) && HAS_PTHREADS)
    }    
    else {
	snprintf(error_diagnosis, error_diag_len, 
		 "Failed to determine library status");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
#endif
    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_exit(char *error_diagnosis, size_t error_diag_len)
{
    int result = DRMAA_ERRNO_DRMS_EXIT_ERROR;
    if (!is_lib_init())
        return DRMAA_ERRNO_NO_ACTIVE_SESSION;

#ifdef WIN32
    EnterCriticalSection(&is_init_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&is_init_lock) == 0){
#endif
#endif
        is_init = 0;
	clean_submit_file_dir();  
	free_job_info_list();
	free_reserved_job_list();
	free(file_dir);
	free(schedd_name);

	// TODO: remove all old log files

	result = DRMAA_ERRNO_SUCCESS;	
	unlock_is_init_lock();
#ifdef WIN32 
	DeleteCriticalSection(&is_init_lock);
#else
#if HAS_PTHREADS
    }
    else 
      snprintf(error_diagnosis, error_diag_len, 
	       "Failed to determine library status.");
#endif
#endif
    return result;
}


/* ------------------- job template routines ------------------- */
int 
drmaa_allocate_job_template(drmaa_job_template_t **jt, char *error_diagnosis, 
			    size_t error_diag_len)
{
    int result = DRMAA_ERRNO_NO_MEMORY;

    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }

    if ((*jt = create_job_template()) != NULL)
	result = DRMAA_ERRNO_SUCCESS;

    return result;
}

int 
drmaa_delete_job_template(drmaa_job_template_t *jt, char *error_diagnosis, 
			  size_t error_diag_len)
{
    job_attr_t* cur_ja;
    job_attr_t* last_ja;    

    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (!is_valid_job_template(jt, error_diagnosis, error_diag_len))
        return DRMAA_ERRNO_INVALID_ARGUMENT;

    cur_ja = jt->head;
    while (cur_ja != NULL){
        last_ja = cur_ja;
	cur_ja = cur_ja->next;
	destroy_job_attribute(last_ja);
    }
    free(jt);

    return DRMAA_ERRNO_SUCCESS;
}


int
drmaa_set_attribute(drmaa_job_template_t *jt, const char *name, 
		    const char *value, char *error_diagnosis, 
		    size_t error_diag_len)
{
    int result = DRMAA_ERRNO_NO_MEMORY;
    job_attr_t* ja;;

    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (!is_valid_job_template(jt, error_diagnosis, error_diag_len) ||
	     !is_valid_attr_name(name, error_diagnosis, error_diag_len) ||
	     !is_scalar_attr(name, error_diagnosis, error_diag_len) ||
	     !is_supported_attr(name, error_diagnosis, error_diag_len))
	return DRMAA_ERRNO_INVALID_ARGUMENT;	
    else if (contains_attr(jt, name, error_diagnosis, error_diag_len))
	return DRMAA_ERRNO_CONFLICTING_ATTRIBUTE_VALUES;
    else if (!is_valid_attr_value(&result, name, value, error_diagnosis,
				  error_diag_len))
        return result;
    
    // make new job_attr_t and set it at the head of the jt's list
    if ((ja = create_job_attribute()) == NULL)
	return DRMAA_ERRNO_NO_MEMORY;
    ja->next = jt->head;
    jt->head = ja;
    ++jt->num_attr;

    // set job attribute's fields
    snprintf(ja->name, DRMAA_ATTR_BUFFER, "%s", name);
    if ((ja->val.value = (char*)malloc(strlen(value) + 1)) == NULL)
	return DRMAA_ERRNO_NO_MEMORY;
    ja->num_values = 1;
    strcpy(ja->val.value, value);
    result = DRMAA_ERRNO_SUCCESS;

    return result;
}

int 
drmaa_get_attribute(drmaa_job_template_t *jt, const char *name, char *value,
		    size_t value_len, char *error_diagnosis, 
		    size_t error_diag_len)
{
    job_attr_t* ja = NULL;

    if(!is_lib_init()) {
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (!is_valid_job_template(jt, error_diagnosis, error_diag_len) ||
	     !is_valid_attr_name(name, error_diagnosis, error_diag_len) ||
	     !is_scalar_attr(name, error_diagnosis, error_diag_len) ||
	     !is_supported_attr(name, error_diagnosis, error_diag_len))
	return DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
    
    // look for attribute name
    if ((ja = find_attr(jt, name, error_diagnosis, error_diag_len)) == NULL)
	return DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;

    // copy scalar value to output buffer
    snprintf(value, value_len, "%s", ja->val.value);

    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_set_vector_attribute(drmaa_job_template_t *jt, const char *name,
			   const char *value[], char *error_diagnosis, 
			   size_t error_diag_len)
{
    int result, index = 0;
    job_attr_t* ja;;  

    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (!is_valid_job_template(jt, error_diagnosis, error_diag_len) ||
	     !is_valid_attr_name(name, error_diagnosis, error_diag_len) ||
	     !is_vector_attr(name, error_diagnosis, error_diag_len) ||
	     !is_supported_attr(name, error_diagnosis, error_diag_len))
        return DRMAA_ERRNO_INVALID_ARGUMENT;
    else if (contains_attr(jt, name, error_diagnosis, error_diag_len))
	return DRMAA_ERRNO_CONFLICTING_ATTRIBUTE_VALUES;    

    // verify values[] has at least one value
    if (value == NULL || value[0] == NULL){
	snprintf(error_diagnosis, error_diag_len, "No values specified");
	return DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;
    }

    // validate all attribute values
    while (value[index] != NULL){
	if (!is_valid_attr_value(&result, name, value[index], error_diagnosis,
				 error_diag_len))
	    return result;	
	index++;
    }    

    // make new job_attr_t and set it at the head of the jt's list
    if ((ja = create_job_attribute()) == NULL)
	return DRMAA_ERRNO_NO_MEMORY;
    ja->next = jt->head;
    jt->head = ja;
    ++jt->num_attr;

    // set job attribute's fields
    snprintf(ja->name, DRMAA_ATTR_BUFFER, "%s", name);
    ja->num_values = index;

    // set job attribute's values, allocating space according to number of values
    if (ja->num_values == 1){
	if ((ja->val.value = (char*)malloc(strlen(value[0]) + 1)) == NULL)
	    return DRMAA_ERRNO_NO_MEMORY;
	strcpy(ja->val.value, value[0]);
    }
    else {
	if((ja->val.values = (char**)malloc(sizeof(char*)*index)) == NULL)
	    return DRMAA_ERRNO_NO_MEMORY;
	index = 0;
	while (index < ja->num_values){
	    ja->val.values[index] = (char*)malloc(strlen(value[index]) + 1);
	    if (ja->val.values[index] == NULL)
		return DRMAA_ERRNO_NO_MEMORY;
	    else {
		strcpy(ja->val.values[index], value[index]);
		++index;
	    }
	}
    }

    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_get_vector_attribute(drmaa_job_template_t *jt, const char *name,
			   drmaa_attr_values_t **values, 
			   char *error_diagnosis, size_t error_diag_len)
{
    job_attr_t* ja;
    int index = 0, value_len;

    if(!is_lib_init()) {
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (!is_valid_job_template(jt, error_diagnosis, error_diag_len) ||
	     !is_valid_attr_name(name, error_diagnosis, error_diag_len) ||
	     !is_vector_attr(name, error_diagnosis, error_diag_len) ||
	     !is_supported_attr(name, error_diagnosis, error_diag_len))
	return DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;

    // look for attribute
    if ((ja = find_attr(jt, name, error_diagnosis, error_diag_len)) == NULL)
	return DRMAA_ERRNO_INVALID_ATTRIBUTE_VALUE;

    // allocate memory for drmaa_attr_values_t
    if ((*values = create_dav(ja->num_values)) == NULL){
	snprintf(error_diagnosis, error_diag_len, 
		 "Unable to allocate memory for values");
	return DRMAA_ERRNO_NO_MEMORY;
    }
        
    // copy vector values into output buffer, allocating space for values
    if (ja->num_values == 1){
	value_len = strlen(ja->val.value);	
	(*values)->values[index] = (char*)malloc(value_len + 1);
	if ((*values)->values[index] == NULL){
	    snprintf(error_diagnosis, error_diag_len, 
		     "Unable to allocate memory for values");
	    return DRMAA_ERRNO_NO_MEMORY;
	}
	((*values)->values[index])[value_len] = '\0'; 
	strcpy(((*values)->values)[index], ja->val.value);	
    }
    else {
	while (index < ja->num_values){
	    value_len = strlen(ja->val.values[index]);	
	    (*values)->values[index] = (char*)malloc(value_len + 1);
	    if ((*values)->values[index] == NULL){
		snprintf(error_diagnosis, error_diag_len,
			 "Unable to allocate memory for values");
		return DRMAA_ERRNO_NO_MEMORY;
	    }
	    ((*values)->values[index])[value_len] = '\0'; 
	    strcpy(((*values)->values)[index], ja->val.values[index]);
	    index++;
	}
    }	

    return DRMAA_ERRNO_SUCCESS;
}


int 
drmaa_get_attribute_names(drmaa_attr_names_t **values, char *error_diagnosis, 
			  size_t error_diag_len)
{
    int i;
    
    if(!is_lib_init()) {
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    
    // allocate memory 
    if ((*values = (drmaa_attr_names_t*)malloc(sizeof(drmaa_attr_names_t)))
	== NULL)
	return DRMAA_ERRNO_NO_MEMORY;	
    (*values)->index = 0;
    (*values)->size = NUM_SUPP_SCALAR_ATTR;
    if (((*values)->attrs = (char**)malloc(sizeof(char*) * NUM_SUPP_SCALAR_ATTR))
	== NULL)
	return DRMAA_ERRNO_NO_MEMORY;
    for (i=0; i < NUM_SUPP_SCALAR_ATTR; i++){
	(*values)->attrs[i] = (char*)malloc(DRMAA_ATTR_BUFFER);
	if ((*values)->attrs[i] == NULL)
	    return DRMAA_ERRNO_NO_MEMORY;
	// null terminate all attribute name strings
	(*values)->attrs[i][DRMAA_ATTR_BUFFER-1] = '\0';  
    }

    // copy attribute names into place
    strncpy((*values)->attrs[0], DRMAA_REMOTE_COMMAND, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[1], DRMAA_JS_STATE, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[2], DRMAA_WD, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[3], DRMAA_JOB_CATEGORY, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[4], DRMAA_NATIVE_SPECIFICATION, 
	    DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[5], DRMAA_BLOCK_EMAIL, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[6], DRMAA_START_TIME, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[7], DRMAA_JOB_NAME, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[8], DRMAA_INPUT_PATH, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[9], DRMAA_OUTPUT_PATH, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[10], DRMAA_ERROR_PATH, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[11], DRMAA_JOIN_FILES, DRMAA_ATTR_BUFFER-1);

    // TODO: add optional scalar job attributes
    //strncpy((*values)->attrs[12], DRMAA_TRANSFER_FILES, DRMAA_ATTR_BUFFER-1);
    //strncpy((*values)->attrs[13], DRMAA_DEADLINE_TIME, DRMAA_ATTR_BUFFER-1);
    //strncpy((*values)->attrs[14], DRMAA_WCT_HLIMIT, DRMAA_ATTR_BUFFER-1);
    //strncpy((*values)->attrs[15], DRMAA_WCT_SLIMIT, DRMAA_ATTR_BUFFER-1);
    //strncpy((*values)->attrs[16], DRMAA_DURATION_HLIMIT, DRMAA_ATTR_BUFFER-1);
    //strncpy((*values)->attrs[17], DRMAA_DURATION_SLIMIT, DRMAA_ATTR_BUFFER-1);

    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_get_vector_attribute_names(drmaa_attr_names_t **values,
				 char *error_diagnosis, size_t error_diag_len)
{
    int i;

    if(!is_lib_init()) {
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    
    // allocate memory
    if ( (*values = (drmaa_attr_names_t*)malloc(sizeof(drmaa_attr_names_t)))
	 == NULL)
	return DRMAA_ERRNO_NO_MEMORY;	
    (*values)->index = 0;
    (*values)->size = NUM_SUPP_VECTOR_ATTR;
    if (((*values)->attrs = (char**)malloc(sizeof(char*) * NUM_SUPP_VECTOR_ATTR))
	== NULL)
	return DRMAA_ERRNO_NO_MEMORY;
    for (i=0; i < NUM_SUPP_VECTOR_ATTR; i++){
	(*values)->attrs[i] = (char*)malloc(DRMAA_ATTR_BUFFER);
	if ((*values)->attrs[i] == NULL)
	    return DRMAA_ERRNO_NO_MEMORY;
	// null terminate all attribute name strings
	(*values)->attrs[i][DRMAA_ATTR_BUFFER-1] = '\0';
    }

    // copy attribute names into place
    strncpy((*values)->attrs[0], DRMAA_V_ARGV, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[1], DRMAA_V_ENV, DRMAA_ATTR_BUFFER-1);
    strncpy((*values)->attrs[2], DRMAA_V_EMAIL, DRMAA_ATTR_BUFFER-1);

    return DRMAA_ERRNO_SUCCESS;
}


/* ------------------- job submission routines ------------------- */
int
drmaa_run_job(char *job_id, size_t job_id_len, drmaa_job_template_t *jt,
              char *error_diagnosis, size_t error_diag_len)
{
    int result = DRMAA_ERRNO_TRY_LATER;
    char* submit_file_name;
    condor_drmaa_job_info_t* job;

    // 1. Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }    
    else if (!is_valid_job_template(jt, error_diagnosis, error_diag_len))
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    else if ((int)job_id_len < MIN_JOBID_LEN){
	snprintf(error_diagnosis, error_diag_len, "job_id_len must be a "\
		 "minimum of %d characters", MIN_JOBID_LEN);
	return DRMAA_ERRNO_INVALID_ARGUMENT;	
    }

    // 2. Create submit file
    if ((result = create_submit_file(&submit_file_name, jt, error_diagnosis,
				     error_diag_len)) != DRMAA_ERRNO_SUCCESS)
	return result;

    // 3. Submit job
    if ((result = submit_job(job_id, job_id_len, submit_file_name, 
			     error_diagnosis, error_diag_len)) 
	!= DRMAA_ERRNO_SUCCESS){
	free(submit_file_name);
	return result;
    }
    free(submit_file_name);

    // 4. Add job_id to list
#ifdef WIN32
    EnterCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
    if (pthread_mutex_lock(&info_list_lock) == 0){
#endif
#endif
	if ((job = create_job_info(job_id)) == NULL){
	    snprintf(error_diagnosis, error_diag_len, 
		     "Unable to create job info");
	    result = DRMAA_ERRNO_NO_MEMORY;
	}
	else {
	    job->state = SUBMITTED;
	    job->next = job_info_list;
	    job_info_list = job;
	    num_info_jobs++;
	    result = DRMAA_ERRNO_SUCCESS;
	}
	unlock_info_list_lock();
#if (!defined(WIN32) && HAS_PTHREADS)
    }
    else {
        snprintf(error_diagnosis, error_diag_len, 
		 "Problem acquiring job id list lock");
	result = DRMAA_ERRNO_TRY_LATER;
    }
#endif
    return result;
}

int 
drmaa_run_bulk_jobs(drmaa_job_ids_t **jobids, drmaa_job_template_t *jt, 
		    int start, int end, int incr, char *error_diagnosis, 
		    size_t error_diag_len)
{
    int result = DRMAA_ERRNO_TRY_LATER;
    // 1. Validation
    // validate loop bounds

    // allocate space for jobids

    // 2. Submit jobs one by one - exit at the first error encountered
    // 2.1 Create submit file
    // 2.2 Submit job
    // 2.3 Acquire lock; Add job ID to internal job ID list; release lock
    // 2.4 add job IS to jobIds[]

    snprintf(error_diagnosis, error_diag_len, "Feature not yet supported.");

    return result;
}


/* ------------------- job control routines ------------------- */
int 
drmaa_control(const char *jobid, int action, char *error_diagnosis, 
	      size_t error_diag_len)
{
    int result = DRMAA_ERRNO_INVALID_JOB;
    int proceed = 0;
    condor_drmaa_job_info_t* cur_res, *cur_info, *last;   

    // 1. Argument validation and library state check
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }      
    else if (jobid == NULL || 
	     (!is_valid_job_id(jobid) && 
	      (strcmp(jobid, DRMAA_JOB_IDS_SESSION_ALL) != 0))){
	snprintf(error_diagnosis, error_diag_len, "Invalid job id");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }     
    else if (action < DRMAA_CONTROL_SUSPEND ||
	     action > DRMAA_CONTROL_TERMINATE){
	snprintf(error_diagnosis, error_diag_len, "Invalid action");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    // Condor doesn't support SUSPEND or RESUME 
    //  though "condor_hold" may hold a currently running process...
    if (action == DRMAA_CONTROL_SUSPEND)
	return DRMAA_ERRNO_SUSPEND_INCONSISTENT_STATE;
    else if (action == DRMAA_CONTROL_RESUME)
	return DRMAA_ERRNO_RESUME_INCONSISTENT_STATE;

    // 2. Verify existence and status of job
    if (strcmp(jobid, DRMAA_JOB_IDS_SESSION_ALL) == 0){
	snprintf(error_diagnosis, error_diag_len, "Feature not supported");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else {
#ifdef WIN32
	EnterCriticalSection(&reserved_list_lock);
	EnterCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
	if (pthread_mutex_lock(&reserved_list_lock) == 0 &&
	    pthread_mutex_lock(&info_list_lock) == 0){   
#endif
#endif
	    // A. Verify job not on reserved list
	    cur_res = reserved_job_list;
	    while (cur_res != NULL){
		if (strcmp(jobid, cur_res->id) == 0){
		    if (action == DRMAA_CONTROL_HOLD)
			return rel_locks(DRMAA_ERRNO_HOLD_INCONSISTENT_STATE);
		    else if (action == DRMAA_CONTROL_RELEASE)
			return rel_locks(DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE);
		    else if (action == DRMAA_CONTROL_TERMINATE)
			return rel_locks(DRMAA_ERRNO_TRY_LATER);
		}
		else 
		    cur_res = cur_res->next;
	    }

	    // B. Verify job on info list & in proper state
	    cur_info = job_info_list;
	    last = cur_info;
	    while (cur_info != NULL){
		if (strcmp(jobid, cur_info->id) == 0){
		    if (cur_info->state == CONTROLLED){
			if (action == DRMAA_CONTROL_HOLD)
			    return 
				rel_locks(DRMAA_ERRNO_HOLD_INCONSISTENT_STATE);
			else if (action == DRMAA_CONTROL_RELEASE)
			    return rel_locks(DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE);
			else if (action == DRMAA_CONTROL_TERMINATE)
			    return rel_locks(DRMAA_ERRNO_TRY_LATER);
		    }
		    else if (cur_info->state == HELD){
			if (action == DRMAA_CONTROL_HOLD)
			    return
				rel_locks(DRMAA_ERRNO_HOLD_INCONSISTENT_STATE);
			else {
			    proceed = 1;
			    break;
			}
		    }
		    else if (cur_info->state == FINISHED){
			snprintf(error_diagnosis, error_diag_len,
				 "Job has finished.");
			if (action == DRMAA_CONTROL_HOLD)
			    return 
				rel_locks(DRMAA_ERRNO_HOLD_INCONSISTENT_STATE);
			else if (action == DRMAA_CONTROL_RELEASE)
			    return rel_locks(DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE);
			else if (action == DRMAA_CONTROL_TERMINATE)
			    return rel_locks(DRMAA_ERRNO_INVALID_JOB);
		    }
		    else if (cur_info->state == SUBMITTED){
			if (action == DRMAA_CONTROL_RELEASE)
			    return rel_locks(DRMAA_ERRNO_RELEASE_INCONSISTENT_STATE);			
			else {			
			    proceed = 1;
			    break;
			}
		    }

		}
		else {
		    last = cur_info;
		    cur_info = cur_info->next;		
		}
	    }

	    if (!proceed){  // jobid not found
		snprintf(error_diagnosis, error_diag_len,
			 "Jobid not valid this session");
		return rel_locks(DRMAA_ERRNO_INVALID_JOB);
	    }

	    // C. Ensure sole access to job
	    if (action == DRMAA_CONTROL_TERMINATE){
		if (last == cur_info)
		    job_info_list = cur_info->next;
		else
		    last->next = cur_info->next;
		cur_info->next = reserved_job_list;
		reserved_job_list = cur_info;
		num_info_jobs--;
		num_reserved_jobs++;		       
	    }
	    else if (action == DRMAA_CONTROL_HOLD ||
		     action == DRMAA_CONTROL_RELEASE){
#ifdef WIN32
		EnterCriticalSection(&(cur_info->lock));
#else
#if HAS_PTHREADS
		if (pthread_mutex_lock(&(cur_info->lock)) != 0){
		    snprintf(error_diagnosis, error_diag_len,
			     "Problem acquiring job lock");
		    return rel_locks(DRMAA_ERRNO_TRY_LATER);    
		}
		else
#endif
#endif    
		    cur_info->state = CONTROLLED;
	    }

	    rel_locks(0);
#if (!defined(WIN32) && HAS_PTHREADS)
	}
	else {
	    snprintf(error_diagnosis, error_diag_len, 
		     "Problem acquiring job list locks");
	    return rel_locks(DRMAA_ERRNO_TRY_LATER);
	}
#endif
    }

    // 2. Perform action
    if (strcmp(jobid, DRMAA_JOB_IDS_SESSION_ALL) == 0){
	// TODO
	// if jobid == DMRAA_JOB_IDS_SESSION_ALL  (currently hold both locks)
	//   for every job_id in list that is not in reserved_list
	//      if HOLD v RELEASE
	//        acquire jobId lock
	//        perform action, exiting upon first error
	//        release jobID lock
	//      else TERMINATE
	//        perform action (including removing files related to this jobid)
	//        if success, remove job from job_id_list
	//    release job_id_list lock
	//    release reserved_list lock
    }
    else {
	if (action == DRMAA_CONTROL_HOLD || action == DRMAA_CONTROL_RELEASE){
	    // we hold lock to this job's entry in info list
	    // and job is in valid state for action
	    if (action == DRMAA_CONTROL_HOLD)
		result = hold_job(jobid, error_diagnosis, error_diag_len);
	    else
		result = release_job(jobid, error_diagnosis, error_diag_len);

	    // update cur_info status
	    if (result != DRMAA_ERRNO_SUCCESS || action == DRMAA_CONTROL_RELEASE)
		cur_info->state = SUBMITTED;
	    else
		cur_info->state = HELD;

	    unlock_job_info(cur_info);
	}
	else if (action == DRMAA_CONTROL_TERMINATE){
	    // job info is on reserved_job_list
	    result = terminate_job(jobid, error_diagnosis, error_diag_len);

	    if (result == DRMAA_ERRNO_SUCCESS)
		rm_reslist(jobid);
	    else {
		cur_info->state = SUBMITTED;
		mv_job_res_to_info(jobid);
	    }
	}
    }

    return result;
}

int 
drmaa_synchronize(const drmaa_job_ids_t* jobids, signed long timeout, 
		  int dispose, char* error_diagnosis, size_t error_diag_len)
{
    int result;
    int i, proceed;
    int sync_all_jobs = 0;
    time_t start;
    condor_drmaa_job_info_t* cur_info, *cur_res, *last;

    // 1. Validation of Lib status and args
    if (!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (timeout < 0 && timeout != DRMAA_TIMEOUT_WAIT_FOREVER){
	snprintf(error_diagnosis, error_diag_len, "Invalid wait quantity");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    } 
    else if (jobids == NULL || jobids->size < 1){
	snprintf(error_diagnosis, error_diag_len, "Jobids is NULL or empty");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }   
    for (i = 0; i < jobids->size; i++){
	if (strcmp(jobids->values[i], DRMAA_JOB_IDS_SESSION_ANY) == 0){
	    sync_all_jobs = 1;
	    break;
	}
	else if (!is_valid_job_id(jobids->values[i])){
	    snprintf(error_diagnosis, error_diag_len, "Invalid job id: %s",
		     jobids->values[i]);
	    return DRMAA_ERRNO_INVALID_ARGUMENT;
	}
    }

    // 2. Manage locks and lib jobid lists
    if (sync_all_jobs){

	snprintf(error_diagnosis, error_diag_len, "DRMAA_JOB_IDS_SESSION_ANY "\
		 "not currently supported");
	return DRMAA_ERRNO_INVALID_ARGUMENT;

	// verify all jobs submitted this session are "syncable"
	/*
	if (pthread_mutex_lock(&reserved_list_lock) == 0){
	    if (num_reserved_jobs > 0){
		snprintf(error_diagnosis, error_diag_len,
		"Not all jobs in syncable state");
		pthread_mutex_unlock(&reserved_list_lock);
		return DRMAA_ERRNO_TRY_LATER;
	    }

	    //   acquire job_id_list lock
	    if (pthread_mutex_lock(&info_list_lock) == 0){

		// method break ---|
		// TODO
    //       for every jobId to wait on
    //         if locked, wait
    //         then lock it (?)
    //         move it to reserved_list
		// method end --|
    //       release job_id_list lock   // all jobs synchronized on are locked
    //       release reserved_list lock
	    }
	    else {
		pthread_mutex_unlock(&reserved_list_lock);
		snprintf(error_diagnosis, error_diag_len, 
		"Problem acquiring info list lock");
		return DRMAA_ERRNO_TRY_LATER;	    
	    }
	}
	else {
	    snprintf(error_diagnosis, error_diag_len, 
	    "Problem acquiring reserved list lock");
	    return DRMAA_ERRNO_TRY_LATER;	    
	}
	*/
    }
    else {
	// not syncing all jobs
#ifdef WIN32
	EnterCriticalSection(&reserved_list_lock);
	EnterCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
	if (pthread_mutex_lock(&reserved_list_lock) == 0 &&
	    pthread_mutex_lock(&info_list_lock) == 0){	
#endif
#endif
	    // A. Verify no sync-desired jobs are on reserved list	    
	    i = 0;		    
	    while (i < jobids->size){
		cur_res = reserved_job_list;
		while (cur_res != NULL){
		    if (strcmp(jobids->values[i], cur_res->id) == 0){
			snprintf(error_diagnosis, error_diag_len, 
				 "Job Id %s is not in a sychronizable state.",
				 jobids->values[i]);
			return rel_locks(DRMAA_ERRNO_TRY_LATER);
		    }
		    cur_res = cur_res->next;
		}
		i++;
	    }

	    // B. Verify all synch-desired jobids are on job_info list
	    i = 0;
	    proceed = 0;
	    while (i < jobids->size){
		cur_info = job_info_list;
		while (cur_info != NULL){
		    if (strcmp(jobids->values[i], cur_info->id) == 0){
			proceed = 1;
			break;
		    }
		    else 
			cur_info = cur_info->next;
		}

		if (!proceed){
		    snprintf(error_diagnosis, error_diag_len, "Job Id %s is " \
			     "not a valid job id", jobids->values[i]);
		    return rel_locks(DRMAA_ERRNO_INVALID_JOB);		    
		}
		else 
		    i++;
	    }

	    // C. Move all jobs from job_info to reserved list
	    i = 0;
	    while (i < jobids->size){
		cur_info = job_info_list;
		last = cur_info;
		while (cur_info != NULL){
		    if (strcmp(jobids->values[i], cur_info->id) == 0){
			// lock condor_drmaa_job_info_t - prevents sync-ing a job
			//  being control()ed
#ifdef WIN32
			EnterCriticalSection(&cur_info->lock);
#else
#if HAS_PTHREADS
			if (pthread_mutex_lock(&cur_info->lock) == 0){
#endif
#endif    
			    if (cur_info == last)
				job_info_list = cur_info->next;
			    else
				last->next = cur_info->next;
			    cur_info->next = reserved_job_list;
			    reserved_job_list = cur_info;
			    num_info_jobs--;
			    num_reserved_jobs++;
			    unlock_job_info(cur_info);
			    cur_info = NULL;  // break
#if (!defined(WIN32) && HAS_PTHREADS)
			}
			else {
			    // undo all other condor_drmaa_job_info_t*'s moved
			    mv_jobs_res_to_info(jobids);
			    snprintf(error_diagnosis, error_diag_len, 
				     "Unable to lock job %s", jobids->values[i]);
			    return rel_locks(DRMAA_ERRNO_TRY_LATER);
			}
#endif
		    }
		    else {
			last = cur_info;
			cur_info = cur_info->next;
		    } 
		}
		i++;
	    }

	    rel_locks(0);
#if (!defined(WIN32) && HAS_PTHREADS)
	}
	else {
	    snprintf(error_diagnosis, error_diag_len, 
		     "Problem acquiring proper locks");
	    return rel_locks(DRMAA_ERRNO_TRY_LATER);
	}
#endif
    }

    // 3. Wait jobs one-by-one
    if (sync_all_jobs){
	// TODO
    }
    else {
	i = 0;
	start = time(NULL);
	while (i < jobids->size){
	    result = wait_job(jobids->values[i], dispose, 0, NULL, timeout,
			      start, NULL, error_diagnosis, error_diag_len);

	    if (result != DRMAA_ERRNO_SUCCESS) {		
		// remove all sync targets not yet completed from reserved list
		// back to job_info_list
#ifdef WIN32
		EnterCriticalSection(&reserved_list_lock);
		EnterCriticalSection(&info_list_lock);
#else
#if HAS_PTHREADS
		if (pthread_mutex_lock(&reserved_list_lock) == 0 &&
		    pthread_mutex_lock(&info_list_lock) == 0){
#endif
#endif
		    mv_jobs_res_to_info(jobids);
		    return rel_locks(result);
#if (!defined(WIN32) && HAS_PTHREADS)
		}
		else {
		    // this is serious!
		    snprintf(error_diagnosis, error_diag_len, 
			     "Problem acquiring proper locks."\
			    " Library in inconsistent state!");
		    return rel_locks(DRMAA_ERRNO_INTERNAL_ERROR);
		}
#endif
	    }
	    else {
		if (dispose)
		    // remove job_id from reserved list on job-by-job basis
		    rm_reslist(jobids->values[i]);
		else 
		    // set job status to FINISHED - moved to info list later
		    mark_res_job_finished(jobids->values[i]);

		i++;
	    }
	}
    }

    // 4. Move all waited jobs back to info list
    mv_jobs_res_to_info(jobids);

    return result;
}

int 
drmaa_wait(const char *job_id, char *job_id_out, size_t job_id_out_len,
	   int *stat, signed long timeout, drmaa_attr_values_t **rusage,
	   char *error_diagnosis, size_t error_diag_len)
{
    int result = DRMAA_ERRNO_INVALID_JOB;
    char term_job_id[MAX_JOBID_LEN];

    // Validation of lib state and args
    if (!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }
    else if (!is_valid_job_id(job_id) && 
	     strcmp(job_id, DRMAA_JOB_IDS_SESSION_ANY) != 0){
	snprintf(error_diagnosis, error_diag_len, "Invalid job id");
	return DRMAA_ERRNO_INVALID_JOB;
    }
    else if (rusage == NULL){
	snprintf(error_diagnosis, error_diag_len, "Invalid rusage value");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }
    else if (timeout < 0 && timeout != DRMAA_TIMEOUT_WAIT_FOREVER){
	snprintf(error_diagnosis, error_diag_len, "Invalid wait quantity");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    // Determine name of job_id to wait on
    if (strcmp(job_id, DRMAA_JOB_IDS_SESSION_ANY) == 0){
	// 1) condor_drmaa_job_info_t* last_job_to_complete?
	// 2) scan through all log files for any job that has completed?
	// 3) condor_drmaa_job_info_t* last_job_submitted?
	snprintf(error_diagnosis, error_diag_len, 
		 "Feature not currently supported");
	return DRMAA_ERRNO_INVALID_JOB;
    }
    else 
	snprintf(term_job_id, sizeof(term_job_id), "%s", job_id);
    
    result = wait_job(term_job_id, 1, 1, stat, timeout, time(NULL), rusage, 
		      error_diagnosis, error_diag_len);
    
    // remove job info from job_info list
    if (result == DRMAA_ERRNO_SUCCESS)
	rm_infolist(term_job_id);

    return result;
}

int 
drmaa_wifexited(int *exited, int stat, char *error_diagnosis, 
		size_t error_diag_len)
{
    // Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }   
    else if (!is_valid_stat(stat)){
        snprintf(error_diagnosis, error_diag_len, "Invalid stat code");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    // Did process terminate? 
    if (stat != STAT_UNKNOWN)
	*exited = 1;
    else
	*exited = 0;

    return DRMAA_ERRNO_SUCCESS;
}

int
drmaa_wexitstatus(int *exit_status, int stat, char *error_diagnosis, 
		  size_t error_diag_len)
{
    // Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }   
    else if (!is_valid_stat(stat) || stat == STAT_UNKNOWN){
        snprintf(error_diagnosis, error_diag_len, "Invalid stat code");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    if (stat >= STAT_NOR_BASE)
	*exit_status = stat - STAT_NOR_BASE;
    else 
	*exit_status = 0;

    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_wifsignaled(int *signaled, int stat, char *error_diagnosis, 
		  size_t error_diag_len)
{
    // Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }   
    else if (!is_valid_stat(stat) ||
	     stat <= STAT_UNKNOWN || stat >= STAT_NOR_BASE){
        snprintf(error_diagnosis, error_diag_len, "Invalid stat code");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    *signaled = 1;
    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_wtermsig(char *signal, size_t signal_len, int stat, 
	       char *error_diagnosis, size_t error_diag_len)
{
    // Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }   
    else if (!is_valid_stat(stat) || stat <= STAT_UNKNOWN || 
	     stat >= STAT_NOR_BASE) {
        snprintf(error_diagnosis, error_diag_len, "Invalid stat code");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }
    else if (signal == NULL || signal_len <= MIN_SIGNAL_NM_LEN){
	snprintf(error_diagnosis, error_diag_len, "signal buffer too small");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    if (stat-1 < (sizeof(SIGNAL_NAMES) / sizeof(char*)))
	snprintf(signal, signal_len, "%s", SIGNAL_NAMES[stat-1]);

    return DRMAA_ERRNO_SUCCESS;
}

int
drmaa_wcoredump(int *core_dumped, int stat, char *error_diagnosis, 
		size_t error_diag_len)
{
    // Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }   
    else if (!is_valid_stat(stat) ||
	     stat <= STAT_UNKNOWN || stat >= STAT_NOR_BASE){
        snprintf(error_diagnosis, error_diag_len, "Invalid stat code");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }
    
    // TODO: Must be linked to the term via signal
    *core_dumped = 0;

    return DRMAA_ERRNO_SUCCESS;
}

int
drmaa_wifaborted(int *aborted, int stat, char *error_diagnosis, 
		 size_t error_diag_len)
{
    // Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }   
    else if (!is_valid_stat(stat)){
        snprintf(error_diagnosis, error_diag_len, "Invalid stat code");
	return DRMAA_ERRNO_INVALID_ARGUMENT;
    }

    if (stat == STAT_ABORTED)
	*aborted = 1;
    else
	*aborted = 0;

    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_job_ps(const char *job_id, int *remote_ps, char *error_diagnosis, 
	     size_t error_diag_len)
{
    FILE* logFS;
    char line[MAX_LOG_FILE_LINE_LEN];
    char state[100];

    // 1. Perform Initialization and Validation checks
    if(!is_lib_init()){
        snprintf(error_diagnosis, error_diag_len, "Library not initialized");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }    
    else if (!is_valid_job_id(job_id)){
	snprintf(error_diagnosis, error_diag_len, "Invalid job id");
	return DRMAA_ERRNO_INVALID_JOB;
    }

    // 2. Read log file of this job_id to determine the program status
    logFS = open_log_file(job_id);

    if (logFS == NULL){
	snprintf(error_diagnosis, error_diag_len, "Unable to open log file");
	return DRMAA_ERRNO_INTERNAL_ERROR;
    }

    // scan through entire file for last occurrance of one of following
    while (fgets(line, sizeof(line), logFS) != NULL){

	if (strstr(line, "Job terminated") != NULL){
	    strcpy(state, "term");
	    break;
	}
	else if (strstr(line, "Job was aborted by the user") != NULL){
	    strcpy(state, "fail");
	    break;
	}
	else if (strstr(line, "Job submitted from host") != NULL)
	    strcpy(state, "q_active");
	else if (strstr(line, "Job was held") != NULL)
	    strcpy(state, "user_hold");
	else if (strstr(line, "Job was released") != NULL ||
		 strstr(line, "Job executing on host") != NULL)
	    strcpy(state, "running");
    }
    
    if (strcmp(state, "term") == 0)
	*remote_ps = DRMAA_PS_DONE;
    else if (strcmp(state, "fail") == 0)
	*remote_ps = DRMAA_PS_FAILED;
    else if (strcmp(state, "q_active") == 0)
	*remote_ps = DRMAA_PS_QUEUED_ACTIVE;
    else if (strcmp(state, "user_hold") == 0)
	*remote_ps = DRMAA_PS_USER_ON_HOLD;
    else if (strcmp(state, "running") == 0)
	*remote_ps = DRMAA_PS_RUNNING;
    else 
	*remote_ps = DRMAA_PS_UNDETERMINED;

    return DRMAA_ERRNO_SUCCESS;
}


/* ------------------- auxiliary routines ------------------- */
const char* 
drmaa_strerror(int drmaa_errno)
{
    const char *result = NULL;
    int numErrors = sizeof(DRMAA_ERR_MSGS) / sizeof(char*);

    if (drmaa_errno >= 0 && drmaa_errno < numErrors)
	result = DRMAA_ERR_MSGS[drmaa_errno];

    return result;
}

int 
drmaa_get_contact(char *contact, size_t contact_len, char *error_diagnosis,
		  size_t error_diag_len)
{ 
    snprintf(contact, contact_len, "Condor");
    return DRMAA_ERRNO_SUCCESS;;
}

int 
drmaa_version(unsigned int *major, unsigned int *minor,
	      char *error_diagnosis, size_t error_diag_len)
{
    *major = 1;
    *minor = 0;
    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_get_DRM_system(char *drm_system, size_t drm_system_len,
		     char *error_diagnosis, size_t error_diag_len)
{  
    snprintf(drm_system, drm_system_len, "Condor");
    return DRMAA_ERRNO_SUCCESS;
}

int 
drmaa_get_DRMAA_implementation(char *impl, size_t impl_len, 
			       char *error_diagnosis, size_t error_diag_len)
{
    snprintf(impl, impl_len, "Condor");
    return DRMAA_ERRNO_SUCCESS;
}


/** Helper Methods (that must be private) **/
int
is_lib_init()
{
    int result = 0;
#ifdef WIN32
    if (is_init_lock_initialized){
	EnterCriticalSection(&is_init_lock);
	if (is_init)
	    result = 1;
	LeaveCriticalSection(&is_init_lock);
    }
#else
    #if HAS_PTHREADS
        if (pthread_mutex_lock(&is_init_lock) == 0){
            if (is_init)
	      result = 1;
	    pthread_mutex_unlock(&is_init_lock);
        }
    #endif
#endif
    return result;
}

/** Converts a Condor log file signal number to this DRMAA
    implementation's number.
    @return signal number or 0 if no matching signal found
*/
/*
int 
condor_sig_to_drmaa(const int condor_sig)
{
    int result = 0;

    // TODO

    return result;
}*/

void 
unlock_is_init_lock()
{
#ifdef WIN32
    LeaveCriticalSection(&is_init_lock);
#else
    #if HAS_PTHREADS
        pthread_mutex_unlock(&is_init_lock);
    #endif
#endif
}
