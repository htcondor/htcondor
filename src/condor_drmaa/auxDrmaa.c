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

int 
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

job_attr_t* 
create_job_attribute()
{
    job_attr_t* result = (job_attr_t*)malloc(sizeof(job_attr_t));

    if (result != NULL){
	result->num_values = 0;
	result->next = NULL;
    }

    return result;
}

condor_drmaa_job_info_t*
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

void
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

void
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

int 
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


int 
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

int 
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

int 
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

int 
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

void 
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

drmaa_job_template_t* 
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

int 
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

job_attr_t* 
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

int 
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

void
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

int 
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

int 
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

int 
write_job_attr(FILE* fs, const job_attr_t* ja)
{
    int result = FAILURE;
    int num_bw;

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
	// TODO: $drmaa_incr_ph$, $drmaa_hd_ph$, $drmaa_wd_ph$
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Input", ja->val.value);
    }
    else if (strcmp(ja->name, DRMAA_OUTPUT_PATH) == 0){
	// TODO: remove this quick and dirty implementation
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Output", ja->val.value);
    }
    else if (strcmp(ja->name, DRMAA_ERROR_PATH) == 0){
	// TODO: $drmaa_incr_ph$, $drmaa_hd_ph$, $drmaa_wd_ph$
	num_bw = fprintf(fs, "%-*s= %s\n", SUBMIT_FILE_COL_SIZE, 
			       "Error", ja->val.value);
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

int 
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
	
	if (result > 0)
	    result = fprintf(fs, "\n");
    }

    return result;
}

int 
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

int 
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

int
is_valid_stat(const int stat)
{
    return stat >= STAT_ABORTED;
}

int
is_valid_job_id(const char* job_id)
{
    return (job_id != NULL && 
	    strlen(job_id) >= MIN_JOBID_LEN &&
	    strlen(job_id)+1 < MAX_JOBID_LEN );
}

FILE* 
open_log_file(const char* job_id)
{
    char log_file_nm[MAX_FILE_NAME_LEN];

    snprintf(log_file_nm, MAX_FILE_NAME_LEN-1, "%s%s%s%s", file_dir, 
	     LOG_FILE_DIR, job_id, LOG_FILE_EXTN);
    return fopen(log_file_nm, "r");
}

int 
rm_log_file(const char* job_id)
{
    char log_file_nm[2000];
    sprintf(log_file_nm, "%s%s%s%s", file_dir, LOG_FILE_DIR, job_id, 
	    LOG_FILE_EXTN);
    return remove(log_file_nm)? 0 : 1;
}

int 
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

drmaa_attr_values_t*
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

int 
mv_jobs_res_to_info(const drmaa_job_ids_t* jobids)
{
    int i = 0;
    condor_drmaa_job_info_t* cur_res, *last;

    while (i < jobids->size){
	cur_res = reserved_job_list;
	last = cur_res;
	while (cur_res != NULL){
	    if (strcmp(cur_res->id, jobids->values[i]) == 0){
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

int 
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

int
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

int
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

int 
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

int 
rel_locks(const int drmaa_err_code)
{
    unlock_info_list_lock();
    unlock_reserved_list_lock();
    return drmaa_err_code;
}

int 
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
    if (dir[strlen(dir)-1) == '\\')
#else
    if (dir[strlen(dir)-1] == '/'){
#endif
	*buf = (char*)malloc(strlen(dir) + strlen(DRMAA_DIR) + 1);
	strcpy(*buf, dir);
    }
    else {
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

int 
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

int 
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

int 
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

void
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

void
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

void 
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

void
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

void 
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

void 
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

int 
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
