/* Copied from Globus 5.0.0 for use with the NorduGrid GAHP server. */
/*
 * Copyright 1999-2006 University of Chicago
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file
 *
 * $RCSfile: globus_ftp_client_exists.c,v $
 * $Revision: 1.10 $
 * $Date: 2006/10/14 07:21:56 $
 */
#endif

#include "globus_i_ftp_client.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

#define GLOBUS_L_FTP_CLIENT_EXIST_BUFFER_LENGTH 4096

/* Module specific data types */
/**
 * Existence check state enumeration.
 * @internal
 * 
 * Existence checking may require a few commands on the FTP control
 * channel. This enumeration shows which state we are currently processing.
 */
typedef enum
{
    GLOBUS_FTP_CLIENT_EXIST_SIZE,
    GLOBUS_FTP_CLIENT_EXIST_MLST,
    GLOBUS_FTP_CLIENT_EXIST_STAT,
    GLOBUS_FTP_CLIENT_EXIST_LIST
}
globus_l_ftp_client_existence_state_t;

/**
 * Existence check status structure.
 * @internal
 *
 * All information needed for a file or directory existence check
 * is contained here. This state is used to track the progress of
 * the existence check operation.
 *
 * @see globus_l_ftp_client_existence_info_init(),
 *      globus_l_ftp_client_existence_info_destroy()
 */
typedef struct
{
    /** The URL we are checking for existence */
    char *					url_string;

    /** Parsed representation of url_string */
    globus_url_t				parsed_url;

    /** Data channel buffer */
    globus_byte_t *				buffer;

    /** Length of our data channel buffer */
    globus_size_t				buffer_length;

    /** Copy of attributes we will use for the FTP operations needed to
     * implement the existence check.
     */
    globus_ftp_client_operationattr_t 		attr;

    /** True once we've determined that the file exists. */
    globus_bool_t				exists;

    /** Size of the file, used for one of the existence checks */
    globus_off_t				size;
    
    /** stat/mlst buffer, used for one of the existence checks */
    globus_byte_t *                             mlst_buffer;
    globus_size_t                               mlst_buffer_len;
        
    /** Error object containing the reason for why the existence check
     * failed, or GLOBUS_SUCCESS.
     */
    globus_object_t *				error;

    /** User-supplied completion callback for when we've determined
     * if the file exists or not
     */
    globus_ftp_client_complete_callback_t	callback;

    /** User-supplied callback argument */
    void *					callback_arg;

    /** Current state of the existence check machine. */
    globus_l_ftp_client_existence_state_t	state;
}
globus_l_ftp_client_existence_info_t;

/* Module specific prototypes */
static
globus_result_t
globus_l_ftp_client_existence_info_init(
    globus_l_ftp_client_existence_info_t **	existence_info,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				rfc1738_url,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

static
globus_result_t
globus_l_ftp_client_existence_info_destroy(
    globus_l_ftp_client_existence_info_t **	existence_info);

static
void
globus_l_ftp_client_exist_callback(
    void *					user_arg,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error);

static
void
globus_l_ftp_client_exist_data_callback(
    void *					user_arg,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    globus_byte_t *				buffer,
    globus_size_t				length,
    globus_off_t				offset,
    globus_bool_t				eof);

#endif

/**
 * @name File or Directory Existence
 */
/* @{ */
/**
 * Check for the existence of a file or directory on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function attempts to determine whether the specified URL points to
 * a valid file or directory. The @a complete_callback will be invoked
 * with the result of the existence check passed as a globus error object,
 * or GLOBUS_SUCCESS.
 *
 * @param u_handle
 *        An FTP Client handle to use for the existence check operation.
 * @param url
 *        The URL of the directory or file to check. The URL may be an
 *        ftp or gsiftp URL.
 * @param attr
 *        Attributes to use for this operation.
 * @param complete_callback
 *        Callback to be invoked once the existence operation is completed.
 * @param callback_arg
 *        Argument to be passed to the complete_callback.
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - u_handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_exists(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_result_t result;
    globus_l_ftp_client_existence_info_t *	existence_info;

    result = globus_l_ftp_client_existence_info_init(&existence_info,
	                                             url,
						     attr,
						     (*u_handle)->attr.rfc1738_url,
						     complete_callback,
						     callback_arg);
    if(result != GLOBUS_SUCCESS)
    {
	goto result_exit;
    }
    
    existence_info->state = GLOBUS_FTP_CLIENT_EXIST_SIZE;
    result = globus_ftp_client_size(
        u_handle,
        url,
        attr,
        &existence_info->size,
        globus_l_ftp_client_exist_callback,
        existence_info);

    if(result != GLOBUS_SUCCESS)
    {
	goto info_destroy_exit;
    }
    return GLOBUS_SUCCESS;

  info_destroy_exit:
    globus_l_ftp_client_existence_info_destroy(&existence_info);
  result_exit:
    return result;
}
/* globus_ftp_client_exists() */
/* @} */

/* Local/internal functions */
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * Initialize an existence info structure.
 *
 * This function allocates and initializes an existence information
 * structure.
 *
 * @param existence_info
 *        Pointer to be set to point to the address of a freshly allocated and
 *        initialized existence information structure.
 * @param url
 *        The URL which this existence info structure will be used to
 *        check the existence of.
 * @param attr
 *        FTP operation attributes which will be used to process this
 *        existence check.
 * @param complete_callback
 *        User callback to be invoked once the existence check is complete.
 * @param callback_arg
 *        User argument to above.
 *
 * @see globus_l_ftp_client_existence_info_t,
 *      globus_l_ftp_client_existence_info_destroy()
 */
static
globus_result_t
globus_l_ftp_client_existence_info_init(
    globus_l_ftp_client_existence_info_t **	existence_info,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				rfc1738_url,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err = GLOBUS_SUCCESS;
    globus_result_t				result;
    int						rc;
    GlobusFuncName(globus_l_ftp_client_existence_info_init);

    *existence_info =
	globus_libc_calloc(1, sizeof(globus_l_ftp_client_existence_info_t));

    if(*existence_info == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto error_exit;
    }
    
    if(rfc1738_url)
    {
        rc = globus_url_parse_rfc1738(url, &(*existence_info)->parsed_url);
    }
    else
    {   
	rc = globus_url_parse(url, &(*existence_info)->parsed_url);
    }
	    

    if(rc != GLOBUS_SUCCESS)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("url");

	goto free_info_exit;
    }

    (*existence_info)->url_string = globus_libc_strdup(url);

    if((*existence_info)->url_string == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto free_parsed_url_exit;
    }


    result = globus_ftp_client_operationattr_copy(
	    &(*existence_info)->attr,
	    attr);

    if(result != GLOBUS_SUCCESS)
    {
	err = globus_error_get(result);

	goto free_url_string_exit;
    }

    (*existence_info)->callback = complete_callback;
    (*existence_info)->callback_arg = callback_arg;
    (*existence_info)->buffer =
	globus_libc_malloc(GLOBUS_L_FTP_CLIENT_EXIST_BUFFER_LENGTH);

    if((*existence_info)->buffer == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto free_attr_exit;
    }

    (*existence_info)->buffer_length = GLOBUS_L_FTP_CLIENT_EXIST_BUFFER_LENGTH;

    return GLOBUS_SUCCESS;

free_attr_exit:
    globus_ftp_client_operationattr_destroy(&(*existence_info)->attr);
free_url_string_exit:
    globus_libc_free((*existence_info)->url_string);
free_parsed_url_exit:
    globus_url_destroy(&(*existence_info)->parsed_url);
free_info_exit:
    globus_libc_free(*existence_info);
error_exit:
    return globus_error_put(err);
}
/* globus_l_ftp_client_existence_info_init() */

/**
 * Destroy an existence info structure.
 *
 * This function frees all memory allocated by
 * globus_l_ftp_client_existence_info_init().
 *
 * @param existence_info
 *        Pointer to the pointer returned from the
 *        globus_l_ftp_client_existence_info_init() function in the
 *        existence_info parameter.
 *
 * @see globus_l_ftp_client_existence_info_t,
 *      globus_l_ftp_client_existence_info_init()
 */
static
globus_result_t
globus_l_ftp_client_existence_info_destroy(
    globus_l_ftp_client_existence_info_t **	existence_info)
{
    globus_libc_free((*existence_info)->url_string);
    globus_url_destroy(&(*existence_info)->parsed_url);
    globus_libc_free((*existence_info)->buffer);
    if((*existence_info)->error)
    {
	globus_object_free((*existence_info)->error);
    }
    if((*existence_info)->mlst_buffer)
    {
	globus_free((*existence_info)->mlst_buffer);
    }

    globus_ftp_client_operationattr_destroy(&(*existence_info)->attr);

    globus_libc_free(*existence_info);

    *existence_info = GLOBUS_NULL;

    return GLOBUS_SUCCESS;
}
/* globus_l_ftp_client_existence_info_destroy() */

/**
 * Existence check state machine.
 * @internal
 *
 * This function contains the logic for proceding through the existence
 * check state machine. It is a globus_ftp_client_complete_callback_t
 * which is passed to various ftp client operations to process the 
 * existence check.
 *
 * @param user_arg
 *        A void * pointing to the globus_l_ftp_client_existence_info_t
 *        structure with the existence check state.
 * @param handle
 *        The FTP client handle that this existence check is being
 *        processed on.
 * @param error
 *        Error status from the FTP client operations.
 */
static
void
globus_l_ftp_client_exist_callback(
    void *					user_arg,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error)
{
    globus_l_ftp_client_existence_info_t *	info;
    globus_result_t				result;
    globus_bool_t				myerr = GLOBUS_FALSE;
    globus_bool_t				try_again = GLOBUS_FALSE;
    
    info = user_arg;

    switch(info->state)
    {
	case GLOBUS_FTP_CLIENT_EXIST_SIZE:
	    if(error == GLOBUS_SUCCESS)
	    {
		info->exists = GLOBUS_TRUE;
	    }
	    else
	    {
                info->state = GLOBUS_FTP_CLIENT_EXIST_MLST;
		result = globus_ftp_client_mlst(
			handle,
			info->url_string,
			&info->attr,
			&info->mlst_buffer,
			&info->mlst_buffer_len,
			globus_l_ftp_client_exist_callback,
			info);

		if(result != GLOBUS_SUCCESS)
		{
		    error = globus_error_get(result);
		    myerr = GLOBUS_TRUE;
		}
		else
		{
		    try_again = GLOBUS_TRUE;
		}
	    }
	    break;
	case GLOBUS_FTP_CLIENT_EXIST_MLST:
	    if(error == GLOBUS_SUCCESS)
	    {
		info->exists = GLOBUS_TRUE;
	    }
	    else
	    {
                info->state = GLOBUS_FTP_CLIENT_EXIST_STAT;
		result = globus_ftp_client_stat(
			handle,
			info->url_string,
			&info->attr,
			&info->mlst_buffer,
			&info->mlst_buffer_len,
			globus_l_ftp_client_exist_callback,
			info);

		if(result != GLOBUS_SUCCESS)
		{
		    error = globus_error_get(result);
		    myerr = GLOBUS_TRUE;
		}
		else
		{
		    try_again = GLOBUS_TRUE;
		}
	    }
	    break;
	case GLOBUS_FTP_CLIENT_EXIST_STAT:
	    if(error == GLOBUS_SUCCESS)
	    {
		info->exists = GLOBUS_TRUE;
	    }
	    else
	    {                   
             /* some servers reply successfully to a LIST even if the path
             doesn't exist.  CWD to the path first will work around this 
             * -- if we got this far, we can be fairly sure it is a dir */
                info->attr->cwd_first = GLOBUS_TRUE;
                info->state = GLOBUS_FTP_CLIENT_EXIST_LIST;
		result = globus_ftp_client_verbose_list(
			handle,
			info->url_string,
			&info->attr,
			globus_l_ftp_client_exist_callback,
			info);

		if(result != GLOBUS_SUCCESS)
		{
		    error = globus_error_get(result);
		    myerr = GLOBUS_TRUE;
		}
		else
		{
		    result = globus_ftp_client_register_read(
			handle,
			info->buffer,
			info->buffer_length,
			globus_l_ftp_client_exist_data_callback,
			info);
		    if(result != GLOBUS_SUCCESS)
		    {
			error = globus_error_get(result);
			myerr = GLOBUS_TRUE;
		    }
		    else
		    {
			try_again = GLOBUS_TRUE;
		    }
		}
	    }
	    break;
	case GLOBUS_FTP_CLIENT_EXIST_LIST:
            if(error != GLOBUS_SUCCESS)
	    {
		info->exists = GLOBUS_FALSE;
	    }
	    try_again = GLOBUS_FALSE;
	    break;
    }
    if(!try_again)
    {
	if(error == GLOBUS_SUCCESS && !info->exists)
	{
	    error = GLOBUS_I_FTP_CLIENT_ERROR_NO_SUCH_FILE(info->url_string);

	    myerr = GLOBUS_TRUE;
	}
	info->callback(info->callback_arg, handle, error);

	globus_l_ftp_client_existence_info_destroy(&info);

	if(myerr)
	{
	    globus_object_free(error);
	}
    }
}
/* globus_l_ftp_client_exist_callback() */

/**
 * Existence check state machine data callback.
 * @internal
 *
 * This function handles the data which arrives as part of the LIST
 * check of the existence check state machine. It just discards the
 * data which arrives until EOF.
 *
 * @param user_arg
 *        A void * pointing to the globus_l_ftp_client_existence_info_t
 *        structure with the existence check state.
 * @param handle
 *        The FTP client handle that this existence check is being
 *        processed on.
 * @param error
 *        Error status from the FTP client operations.
 * @param buffer
 *        A pointer to the existence state structure's buffer member.
 * @param length
 *        The amount of date read on the data channel (ignored).
 * @param offset
 *        The offset of the data in the list (ignored).
 * @param eof
 *        End-of-file flag.
 */
static
void
globus_l_ftp_client_exist_data_callback(
    void *					user_arg,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    globus_byte_t *				buffer,
    globus_size_t				length,
    globus_off_t				offset,
    globus_bool_t				eof)
{
    globus_l_ftp_client_existence_info_t *	info;
    globus_result_t				result;

    info = user_arg;

    if(error != GLOBUS_SUCCESS && !info->error)
    {
	info->error = globus_object_copy(error);
    }
    if(!error)
    {
        info->exists = GLOBUS_TRUE;
    }
    if(! eof)
    {
	result =
	    globus_ftp_client_register_read(
		    handle,
		    info->buffer,
		    info->buffer_length,
		    globus_l_ftp_client_exist_data_callback,
		    info);

	if(result != GLOBUS_SUCCESS && !info->error)
	{
	    info->error = globus_error_get(result);
	}
    }
}
/* globus_l_ftp_client_exist_data_callback() */
#endif
