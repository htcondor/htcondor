/* Modified from Globus 4.2.0 for use with the NorduGrid GAHP server. */
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
 * $RCSfile: globus_ftp_client_transfer.c,v $
 * $Revision: 1.40 $
 * $Date: 2007/12/05 21:53:33 $
 */
#endif

#include "globus_i_ftp_client.h"

#include <string.h>		/* strstr(), strncmp() */
#include <ctype.h>		/* isupper() */



#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

#define CRLF "\r\n"

/* Module-specific prototypes */
static
void
globus_l_ftp_client_abort_callback(
    void *					user_arg);
#endif

#define GLOBUS_L_DATA_ERET_FORMAT_STRING \
    "P %"GLOBUS_OFF_T_FORMAT" %"GLOBUS_OFF_T_FORMAT""

#define GLOBUS_L_DATA_ESTO_FORMAT_STRING \
    "A %"GLOBUS_OFF_T_FORMAT""

static
globus_result_t
globus_l_ftp_client_list_op(
    globus_ftp_client_handle_t *                u_handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_i_ftp_client_operation_t             op,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);


static
globus_result_t
globus_l_ftp_client_extended_get(
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                eret_alg_str,
    globus_off_t                                partial_offset,
    globus_off_t                                partial_end_offset,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);

static
globus_result_t
globus_l_ftp_client_extended_put(
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                esto_alg_name,
    globus_off_t                                partial_offset,
    globus_off_t                                partial_end_offset,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);

static
globus_result_t
globus_l_ftp_client_extended_third_party_transfer(
    globus_ftp_client_handle_t *                handle,
    const char *                                source_url,
    globus_ftp_client_operationattr_t *         source_attr,
    const char *                                eret_alg_str,
    const char *                                dest_url,
    globus_ftp_client_operationattr_t *         dest_attr,
    const char *                                esto_alg_str,
    globus_ftp_client_restart_marker_t *        restart,
    globus_off_t                                partial_offset,
    globus_off_t                                partial_end_offset,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);

static
globus_object_t *
globus_l_ftp_client_transfer_normalize_attrs(
    const char *			source_url,
    globus_ftp_client_operationattr_t *	source_attr,
    globus_ftp_client_operationattr_t * normalized_source_attr,
    const char *			dest_url,
    globus_ftp_client_operationattr_t *	dest_attr,
    globus_ftp_client_operationattr_t * normalized_dest_attr);


typedef struct globus_i_ftp_client_operation_ent_s
{
    const char *			    source_url;
    globus_ftp_client_operationattr_t *     source_attr;
    const char *                            dest_url;
    globus_ftp_client_operationattr_t *     dest_attr;
    const char *                            eret_alg_str;
    const char *                            esto_alg_str;
    globus_off_t                            partial_offset;
    globus_off_t                            partial_end_offset;
    globus_ftp_client_restart_marker_t *    restart;
    globus_ftp_client_complete_callback_t   complete_callback;
    void *                                  callback_arg;
    globus_byte_t **                        stat_buffer_out;
    globus_size_t *                         stat_buffer_length;
    int                                     chmod_mode;
    globus_off_t *                          size_out;
    globus_abstime_t *                      mdtm_out;
    char *                                  cksm_out;
    globus_off_t                            cksm_offset;
    globus_off_t                            cksm_length;
    const char *                            cksm_alg;
} globus_i_ftp_client_operation_ent_t;
    


/**
 * @name Make Directory
 */
/*@{*/
/**
 * Make a directory on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a mkdir operation on a FTP server. 
 *
 * When the response to the mkdir request has been received the
 * complete_callback will be invoked with the result of the mkdir
 * operation. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the mkdir operation.
 * @param url
 *	  The URL for the directory to create. The URL may be an ftp or
 *        gsiftp URL.
 * @param attr
 *	  Attributes for this operation.
 * @param complete_callback
 *        Callback to be invoked once the mkdir is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
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
globus_ftp_client_mkdir(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_mkdir);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the mkdir operation */
    handle->op = GLOBUS_FTP_CLIENT_MKDIR;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the mkdir is happening */
    globus_i_ftp_client_plugin_notify_mkdir(handle,
					    handle->source_url,
					    handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this mkdir. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_mkdir() */
/*@}*/

/**
 * @name Remove Directory
 */
/*@{*/
/**
 * Make a directory on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a rmdir operation on a FTP server. 
 *
 * When the response to the rmdir request has been received the
 * complete_callback will be invoked with the result of the rmdir
 * operation. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the rmdir operation.
 * @param url
 *	  The URL for the directory to create. The URL may be an ftp or
 *        gsiftp URL.
 * @param attr
 *	  Attributes for this operation.
 * @param complete_callback
 *        Callback to be invoked once the rmdir is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
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
globus_ftp_client_rmdir(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_rmdir);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the rmdir operation */
    handle->op = GLOBUS_FTP_CLIENT_RMDIR;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the rmdir is happening */
    globus_i_ftp_client_plugin_notify_rmdir(handle,
					    handle->source_url,
					    handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this rmdir. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_rmdir() */
/*@}*/

/**
 * @name Change Working Directory
 */
/*@{*/
/**
 * Make a directory on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a cwd operation on a FTP server. 
 *
 * When the response to the cwd request has been received the
 * complete_callback will be invoked with the result of the cwd
 * operation. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the cwd operation.
 * @param url
 *	  The URL for the directory to cd into. The URL may be an ftp or
 *        gsiftp URL.
 * @param attr
 *	  Attributes for this operation.
 * @param mlst_buffer
 *        A pointer to a globus_byte_t * to be allocated and filled with the 
 *	  directory name returned by the CWD, if it succeeds. Otherwise, the
 *	  value pointed to by it is undefined.  It is up the user to free
 *	  this memory.
 * @param mlst_buffer_length
 *        A pointer to a globus_size_t to be filled with the length of the data 
 *        in mlst_buffer.
 * @param complete_callback
 *        Callback to be invoked once the cwd is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - u_handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - mlst_buffer is GLOBUS_NULL
 *        - mlst_buffer_length is GLOBUS_NULL
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_cwd(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_byte_t **    			mlst_buffer,
    globus_size_t *                             mlst_buffer_length,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_cwd);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }
/*
    else if(mlst_buffer_length == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("mlst_buffer_length");

	goto error_exit;
    }

    else if(mlst_buffer == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("mlst_buffer");

	goto error_exit;
    }
*/

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the cwd operation */
    handle->op = GLOBUS_FTP_CLIENT_CWD;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->mlst_buffer_pointer = mlst_buffer;
    handle->mlst_buffer_length_pointer = mlst_buffer_length;
    if ( mlst_buffer ) {
	*mlst_buffer = GLOBUS_NULL;
    }

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the cwd is happening */
/*
    globus_i_ftp_client_plugin_notify_rmdir(handle,
					    handle->source_url,
					    handle->source->attr);
*/

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this cwd. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->mlst_buffer_pointer = GLOBUS_NULL;
    handle->mlst_buffer_length_pointer = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);
    if(mlst_buffer) {
	*mlst_buffer = GLOBUS_NULL;
    }
    if(mlst_buffer_length) {
	*mlst_buffer_length = GLOBUS_NULL;
    }

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_cwd() */
/*@}*/

/**
 * @name Delete
 */
/*@{*/
/**
 * Delete a file on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a delete operation on a FTP server. Note that
 * this functions will only delete files, not directories.
 *
 * When the response to the delete request has been received the
 * complete_callback will be invoked with the result of the delete
 * operation. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the delete operation.
 * @param url
 *	  The URL for the file to delete. The URL may be an ftp or
 *        gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param complete_callback
 *        Callback to be invoked once the delete is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
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
globus_ftp_client_delete(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_delete);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);
    
    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the delete operation */
    handle->op = GLOBUS_FTP_CLIENT_DELETE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the delete is happening */
    globus_i_ftp_client_plugin_notify_delete(handle,
					     handle->source_url,
					     handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this delete. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_delete() */
/*@}*/

/**
 * @name List
 */
/*@{*/
/**
 * Get a file listing from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "NLST" transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this listing.
 *
 * When all of the data associated with the listing is retrieved, and all 
 * of the data callbacks have been called, or if the list request is
 * aborted, the complete_callback will be invoked with the final
 * status of the list.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param complete_callback
 *        Callback to be invoked once the "list" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 *
 * @see globus_ftp_client_register_read()
 */
globus_result_t
globus_ftp_client_list(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    GlobusFuncName(globus_ftp_client_list);
    
    return globus_l_ftp_client_list_op(
                u_handle,
                url,
                attr,
                GLOBUS_FTP_CLIENT_NLST,
                complete_callback,
                callback_arg);
}
/* globus_ftp_client_list() */


/**
 * Get a file listing from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "LIST" transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this listing.
 *
 * When all of the data associated with the listing is retrieved, and all 
 * of the data callbacks have been called, or if the list request is
 * aborted, the complete_callback will be invoked with the final
 * status of the list.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param complete_callback
 *        Callback to be invoked once the "list" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 *
 * @see globus_ftp_client_register_read()
 */
globus_result_t
globus_ftp_client_verbose_list(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    GlobusFuncName(globus_ftp_client_verbose_list);
    
    return globus_l_ftp_client_list_op(
                u_handle,
                url,
                attr,
                GLOBUS_FTP_CLIENT_LIST,
                complete_callback,
                callback_arg);
}
/* globus_ftp_client_verbose_list() */

/**
 * @name STAT
 */
/*@{*/
/**
 * Get information about a file or directory from a FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function requests the STAT listing of a file or directory from 
 * an FTP server.
 *
 * When the STAT request is completed or aborted, the complete_callback will 
 * be invoked with the final status of the operation.
 * If the callback is returns without an error, the STAT fact string will
 * be stored in the globus_byte_t * pointed to by the stat_buffer parameter 
 * to this function.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL of a file or directory to list. The URL may be an ftp or 
 *        gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param stat_buffer
 *        A pointer to a globus_byte_t * to be allocated and filled with the 
 *        STAT listing of the file, if it exists. Otherwise, the value 
 *        pointed to by it is undefined.  It is up to the user to free this
 *        memory.
 * @param stat_buffer_length
 *        A pointer to a globus_size_t to be filled with the length of the data 
 *        in stat_buffer.
 * @param complete_callback
 *        Callback to be invoked once the STAT command is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - stat_buffer is GLOBUS_NULL
 *        - stat_buffer_length is GLOBUS_NULL
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_stat(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_byte_t **    			stat_buffer,
    globus_size_t *                             stat_buffer_length,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_stat);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }
    else if(stat_buffer_length == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("stat_buffer_length");

	goto error_exit;
    }

    else if(stat_buffer == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("stat_buffer");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the STAT */
    handle->op = GLOBUS_FTP_CLIENT_STAT;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->mlst_buffer_pointer = stat_buffer;
    handle->mlst_buffer_length_pointer = stat_buffer_length;
    *stat_buffer = GLOBUS_NULL;
    
    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the STAT is happening */
    globus_i_ftp_client_plugin_notify_stat(handle,
					   handle->source_url,
					   handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this list. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

 free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->mlst_buffer_pointer = GLOBUS_NULL;
    handle->mlst_buffer_length_pointer = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);
    *stat_buffer = GLOBUS_NULL;
    *stat_buffer_length = GLOBUS_NULL;
    
    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_stat() */
/*@}*/

/**
 * Get a machine parseable file listing from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "MLSD" transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this listing.
 *
 * When all of the data associated with the listing is retrieved, and all 
 * of the data callbacks have been called, or if the list request is
 * aborted, the complete_callback will be invoked with the final
 * status of the list.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param complete_callback
 *        Callback to be invoked once the "list" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 *
 * @see globus_ftp_client_register_read()
 */
globus_result_t
globus_ftp_client_machine_list(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    GlobusFuncName(globus_ftp_client_machine_list);
    
    return globus_l_ftp_client_list_op(
                u_handle,
                url,
                attr,
                GLOBUS_FTP_CLIENT_MLSD,
                complete_callback,
                callback_arg);
}
/* globus_ftp_client_machine_list() */
/*@}*/


/**
 * @name MLST
 */
/*@{*/
/**
 * Get information about a file or directory from a FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function requests the MLST fact string of a file or directory from 
 * an FTP server.
 *
 * When the MLST request is completed or aborted, the complete_callback will 
 * be invoked with the final status of the operation.
 * If the callback is returns without an error, the MLST fact string will
 * be stored in the globus_byte_t * pointed to by the mlst_buffer parameter 
 * to this function.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL of a file or directory to list. The URL may be an ftp or 
 *        gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param mlst_buffer
 *        A pointer to a globus_byte_t * to be allocated and filled with the 
 *        MLST fact string of the file, if it exists. Otherwise, the value 
 *        pointed to by it is undefined.  It is up to the user to free this
 *        memory.
 * @param mlst_buffer_length
 *        A pointer to a globus_size_t to be filled with the length of the data 
 *        in mlst_buffer.
 * @param complete_callback
 *        Callback to be invoked once the MLST command is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - mlst_buffer is GLOBUS_NULL
 *        - mlst_buffer_length is GLOBUS_NULL
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_mlst(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_byte_t **    			mlst_buffer,
    globus_size_t *                             mlst_buffer_length,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_mlst);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }
    else if(mlst_buffer_length == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("mlst_buffer_length");

	goto error_exit;
    }

    else if(mlst_buffer == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("mlst_buffer");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the MLST */
    handle->op = GLOBUS_FTP_CLIENT_MLST;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->mlst_buffer_pointer = mlst_buffer;
    handle->mlst_buffer_length_pointer = mlst_buffer_length;
    *mlst_buffer = GLOBUS_NULL;
    
    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the MLST is happening */
    globus_i_ftp_client_plugin_notify_mlst(handle,
					   handle->source_url,
					   handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this list. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

 free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->mlst_buffer_pointer = GLOBUS_NULL;
    handle->mlst_buffer_length_pointer = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);
    *mlst_buffer = GLOBUS_NULL;
    *mlst_buffer_length = GLOBUS_NULL;
    
    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_mlst() */
/*@}*/

/**
 * @name Move
 */
/*@{*/
/**
 * Move a file on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a move (rename) operation on an FTP
 * server. Note that this function does not move files between FTP
 * servers and that the host:port part of the destination url is
 * ignored.
 *
 * When the response to the move request has been received the
 * complete_callback will be invoked with the result of the move
 * operation. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the move operation.
 * @param source_url
 *	  The URL for the file to move.
 * @param dest_url
 *	  The URL for the target of the move. The host:port part of
 *        this URL is ignored. 
 * @param attr
 *	  Attributes for this operation.
 * @param complete_callback
 *        Callback to be invoked once the move is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - source_url is GLOBUS_NULL
 *        - source_url cannot be parsed
 *        - source_url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_move(
    globus_ftp_client_handle_t *		u_handle,
    const char *				source_url,
    const char *				dest_url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    int                                         rc;
    globus_bool_t				registered;
    globus_bool_t  				rfc1738_url;
    globus_url_t                                url;
    globus_i_ftp_client_handle_t *		handle;
    globus_ftp_client_handleattr_t		handleattr;
    GlobusFuncName(globus_ftp_client_move);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("source_url");

	goto error_exit;
    }
    else if(dest_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("dest_url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the get operation */
    handle->op = GLOBUS_FTP_CLIENT_MOVE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(source_url);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    handle->dest_url = globus_libc_strdup(dest_url);

    if(handle->dest_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto free_source_url_exit;
    }

    handleattr=&(handle->attr);
    globus_ftp_client_handleattr_get_rfc1738_url(&handleattr,
		    				    &rfc1738_url);

    if(rfc1738_url)
    {
	 rc = globus_url_parse_rfc1738(dest_url,
			 	       &url);
    }
    else
    {
         rc = globus_url_parse(dest_url,
                               &url);
    }
    
    if(rc != GLOBUS_SUCCESS)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("dest_url");

	goto free_urls_exit;
    }

    if(url.url_path == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("dest_url");

        globus_url_destroy(&url);
	goto free_urls_exit;
    }

    globus_url_destroy(&url);

    
    /* Obtain a connection to the source FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  source_url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_urls_exit;
    }

    /* Notify plugins that the GET is happening */
    globus_i_ftp_client_plugin_notify_move(handle,
					   handle->source_url,
					   handle->dest_url,
					   handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this get. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }
free_urls_exit:
    globus_libc_free(handle->dest_url);
free_source_url_exit:
    globus_libc_free(handle->source_url);
reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
unlock_exit:
    /* Release the lock */
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_move() */
/*@}*/

/**
 * @name chmod
 */
/*@{*/
/**
 * Change permissions on a file.
 * @ingroup globus_ftp_client_operations
 *
 * This function changes file permissions
 * 
 * 
 * 
 *
 * When the response to the move request has been received the
 * complete_callback will be invoked with the result of the
 * operation. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the move operation.
 * @param url
 *	  The URL of the file to modify permissions. 
 * @param mode
 *	  The integer file mode to change to.  Be sure that the integer is 
 *        in the proper base, i.e. 0777 (octal) vs 777 (decimal).           
 * @param attr
 *	  Attributes for this operation.
 * @param complete_callback
 *        Callback to be invoked once the move is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_chmod(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    int         				mode,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_chmod);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the get operation */
    handle->op = GLOBUS_FTP_CLIENT_CHMOD;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->chmod_file_mode = mode;

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    
    /* Obtain a connection to the source FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
	        			  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the CHMOD is happening */
    globus_i_ftp_client_plugin_notify_chmod(handle,
					   handle->source_url,
					   handle->chmod_file_mode,
					   handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this op. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }
free_url_exit:
    globus_libc_free(handle->source_url);
reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
unlock_exit:
    /* Release the lock */
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_chmod() */
/*@}*/


/**
 * @name Get
 */
/*@{*/
/**
 * Get a file from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "get" file transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this URL.
 *
 * When all of the data associated with this URL is retrieved, and all 
 * of the data callbacks have been called, or if the get request is
 * aborted, the complete_callback will be invoked with the final
 * status of the get.
 *
 * @param u_handle
 *        An FTP Client handle to use for the get operation.
 * @param url
 *	  The URL to download. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param restart
 *        Pointer to a restart marker.
 * @param complete_callback
 *        Callback to be invoked once the "get" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 *
 * @see globus_ftp_client_register_read()
 */
globus_result_t
globus_ftp_client_get(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_get() entering\n"));
        
    result = globus_l_ftp_client_extended_get(handle,
					 url,
					 attr,
					 restart,
                                         GLOBUS_NULL,
					 -1,
					 -1,
					 complete_callback,
					 callback_arg);
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_get() exiting\n"));
        
    return result;
}
/* globus_ftp_client_get() */

/**
 * Get a file from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "get" file transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this URL.
 *
 * When all of the data associated with this URL is retrieved, and all 
 * of the data callbacks have been called, or if the get request is
 * aborted, the complete_callback will be invoked with the final
 * status of the get.
 *
 * @param handle
 *        An FTP Client handle to use for the get operation.
 * @param url
 *	  The URL to download. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param restart
 *        Pointer to a restart marker.
 * @param partial_offset
 *        Starting offset for a partial file get.
 * @param partial_end_offset
 *        Ending offset for a partial file get. Use -1 for EOF.
 * @param complete_callback
 *        Callback to be invoked once the "get" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_partial_get(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    char                                        alg_str_buf[128];
    globus_result_t                             result;
    globus_ftp_client_restart_marker_t          tmp_restart;
    globus_object_t *                            err;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_partial_get() entering\n"));

    if(partial_offset < 0)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("partial_offset");
        goto error_param;
    }

    if(partial_end_offset != -1 && partial_end_offset < partial_offset)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("partial_end_offset");
        goto error_param;
    }


    if(partial_end_offset != -1)
    { 
        sprintf(
            alg_str_buf, GLOBUS_L_DATA_ERET_FORMAT_STRING,
            partial_offset, 
            partial_end_offset - partial_offset);
        
        result = globus_l_ftp_client_extended_get(
            handle,
            url,
            attr,
            restart,
            alg_str_buf,
            partial_offset,
            partial_end_offset,
            complete_callback,
            callback_arg);
    }
    /* if partial_end_offset == -1 use a restart at offset to get the full
        transfer starting at offset */
    else
    {
        if(restart)
        {
            globus_ftp_client_restart_marker_copy(&tmp_restart, restart);
        }
        else
        {
            globus_ftp_client_restart_marker_init(&tmp_restart);
        }
        
        if(tmp_restart.type == GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK ||
            (attr && *attr &&  
            (*attr)->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK))
        {
            globus_ftp_client_restart_marker_insert_range(
                &tmp_restart,
                0,
                partial_offset);
        }
        else if(tmp_restart.type == GLOBUS_FTP_CLIENT_RESTART_NONE ||
            (tmp_restart.type == GLOBUS_FTP_CLIENT_RESTART_STREAM && 
            tmp_restart.stream.offset < partial_offset))
        {
            globus_ftp_client_restart_marker_set_offset(
                &tmp_restart,
                partial_offset);
        }
        
        result = globus_ftp_client_get(
            handle,
            url,
            attr,
            &tmp_restart,
            complete_callback,
            callback_arg);
            
        globus_ftp_client_restart_marker_destroy(&tmp_restart);
    }
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_partial_get() exiting\n"));
        
    return result;

error_param:
    return globus_error_put(err);
        
}
/* globus_ftp_client_partial_get() */

/**
 * Get a file from an FTP server with server-side processing.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "get" file transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this URL.
 *
 * When all of the data associated with this URL is retrieved, and all 
 * of the data callbacks have been called, or if the get request is
 * aborted, the complete_callback will be invoked with the final
 * status of the get.
 *
 * This function differs from the globus_ftp_client_get() function by allowing
 * the user to invoke server-side data processing algorithms.  GridFTP servers
 * may support support algorithms for data reduction or other customized data
 * storage requirements. There is no client-side verification done on the
 * algorithm string provided by the user. If the server does not understand the
 * requested algorithm, the transfer will fail.
 *
 * @param handle
 *        An FTP Client handle to use for the get operation.
 * @param url
 *        The URL to download. The URL may be an ftp or gsiftp URL.
 * @param attr
 *        Attributes for this file transfer.
 * @param restart
 *        Pointer to a restart marker.
 * @param eret_alg_str
 *        The ERET algorithm string. This string contains information needed
 *        to invoke a server-specific data reduction algorithm on the file
 *        being retrieved.
 * @param complete_callback
 *        Callback to be invoked once the "get" is completed.
 * @param callback_arg
 *        Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 *
 * @see globus_ftp_client_register_read()
 */
globus_result_t
globus_ftp_client_extended_get(
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                eret_alg_str,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg)
{
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_extended_get() entering\n"));
        
    result = globus_l_ftp_client_extended_get(
               handle,
               url,
               attr,
               restart,
               eret_alg_str,
               -1,
               -1,
               complete_callback,
               callback_arg);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_extended_get() exiting\n"));
        
    return result;
}
/* globus_ftp_client_extended_get() */

static
globus_result_t
globus_l_ftp_client_extended_get(
    globus_ftp_client_handle_t *                u_handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                eret_alg_str,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_get);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_get() entering\n"));
        
    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the get operation */
    handle->op = GLOBUS_FTP_CLIENT_GET;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;

    handle->partial_offset = partial_offset;
    handle->partial_end_offset = partial_end_offset;

    /* In stream mode, we need to keep track of the base offset to
     * adjust the offsets returned from the control library.
     * (In extended block mode, the data blocks have headers which have
     * the offset in them, so we can trust the control library.)
     */
    if((!attr) || (! *attr ) ||
	    ((*attr) && (*attr)->mode == GLOBUS_FTP_CONTROL_MODE_STREAM))
    {
	if(restart && restart->type == GLOBUS_FTP_CLIENT_RESTART_STREAM)
	{
	   if(((!attr) || (!*attr) ||
		       (*attr)->type == GLOBUS_FTP_CONTROL_TYPE_IMAGE) &&
	      restart->stream.offset > handle->base_offset)
	    {
	        handle->base_offset = restart->stream.offset;
	    }
            else if(attr &&
		    (*attr) &&
		    (*attr)->type == GLOBUS_FTP_CONTROL_TYPE_ASCII &&
	       restart->stream.ascii_offset > handle->base_offset)
	    {
	        handle->base_offset = restart->stream.ascii_offset;
	    }
        }
    }

    handle->callback_arg = callback_arg;
    if(restart)
    {
	globus_ftp_client_restart_marker_copy(&handle->restart_marker,
						restart);
    }
    else
    {
	globus_ftp_client_restart_marker_init(&handle->restart_marker);
    }
    
    handle->source_url = globus_libc_strdup(url);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the source FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }
    /* set new string in handle */
    if(eret_alg_str != NULL)
    {
        if(handle->source->attr->module_alg_str != NULL)
        {
            free(handle->source->attr->module_alg_str);
        }
        handle->source->attr->module_alg_str = strdup(eret_alg_str);
    }

    /* Notify plugins that the GET is happening */
    globus_i_ftp_client_plugin_notify_get(handle,
					  handle->source_url,
					  handle->source->attr,
					  &handle->restart_marker);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_get() exiting\n"));
       
    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this get. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }
free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    globus_ftp_client_restart_marker_destroy(&handle->restart_marker);
    
unlock_exit:
    /* Release the lock */
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_get() exiting with error\n"));

    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
        globus_i_ftp_client_debug_printf(1, (stderr, 
            "globus_l_ftp_client_extended_get() exiting after restart\n"));

	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->partial_offset = -1;
    handle->partial_end_offset = -1;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_get() exiting after abort\n"));

    return globus_error_put(err);
}
/* globus_l_ftp_client_extended_get() */
/*@}*/

/**
 * @name Put
 */
/*@{*/
/**
 * Store a file on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "put" file transfer to an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_write() to send the data
 * associated with this URL.
 *
 * When all of the data associated with this URL is sent, and all 
 * of the data callbacks have been called, or if the put request is
 * aborted, the complete_callback will be invoked with the final
 * status of the put.
 *
 * @param handle
 *        An FTP Client handle to use for the put operation.
 * @param url
 *	  The URL to store the data to. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param restart
 *        Pointer to a restart marker.
 * @param complete_callback
 *        Callback to be invoked once the "put" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @see globus_ftp_client_register_write()
 */
globus_result_t
globus_ftp_client_put(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_put() entering\n"));
        
    result = globus_l_ftp_client_extended_put(handle,
				  	    url,
					    attr,
					    restart,
                                            GLOBUS_NULL,
					    -1,
					    -1,
					    complete_callback,
					    callback_arg);
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_put() exiting\n"));
        
    return result;
}
/* globus_ftp_client_put() */

/**
 * Store a file on an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "put" file transfer to an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_write() to send the data
 * associated with this URL.
 *
 * When all of the data associated with this URL is sent, and all 
 * of the data callbacks have been called, or if the put request is
 * aborted, the complete_callback will be invoked with the final
 * status of the put.
 *
 * @param handle
 *        An FTP Client handle to use for the put operation.
 * @param url
 *	  The URL to store the data to. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param restart
 *        Pointer to a restart marker.
 * @param partial_offset
 *        Starting offset for a partial file put.
 * @param partial_end_offset
 *        Ending offset for a partial file put.
 * @param complete_callback
 *        Callback to be invoked once the "put" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @see globus_ftp_client_register_write()
 */
globus_result_t
globus_ftp_client_partial_put(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    char                                        esto_str_buf[128];
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_partial_put() entering\n"));
        
    sprintf(esto_str_buf, GLOBUS_L_DATA_ESTO_FORMAT_STRING, partial_offset);

    result = globus_l_ftp_client_extended_put(handle,
					    url,
					    attr,
					    restart,
                                            esto_str_buf,
					    partial_offset,
					    partial_end_offset,
					    complete_callback,
					    callback_arg);
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_partial_put() exiting\n"));
        
    return result;
}
/* globus_ftp_client_partial_put() */


/**
 * Store a file on an FTP server with server-side processing.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a "put" file transfer to an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_write() to send the data
 * associated with this URL.
 *
 * When all of the data associated with this URL is sent, and all 
 * of the data callbacks have been called, or if the put request is
 * aborted, the complete_callback will be invoked with the final
 * status of the put.
 *
 * This function differs from the globus_ftp_client_put() function by allowing
 * the user to invoke server-side data processing algorithms.  GridFTP servers
 * may support algorithms for data reduction or other customized data storage
 * requirements. There is no client-side verification done on the alogirhtm
 * string provided by the user. if the server does not understand the requested
 * algorithm, the transfer will fail.
 *
 * @param handle
 *        An FTP Client handle to use for the put operation.
 * @param url
 *	  The URL to store the data to. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param restart
 *        Pointer to a restart marker.
 * @param complete_callback
 *        Callback to be invoked once the "put" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @see globus_ftp_client_register_write()
 */
globus_result_t
globus_ftp_client_extended_put(
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                esto_alg_str,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg)
{
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_extended_put() entering\n"));
        
    result = globus_l_ftp_client_extended_put(handle,
               url,
               attr,
               restart,
               esto_alg_str,
               -1,
               -1,
               complete_callback,
               callback_arg);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_extended_put() exiting\n"));
        
    return result;
}
/* globus_ftp_client_extended_put() */

static
globus_result_t
globus_l_ftp_client_extended_put(
    globus_ftp_client_handle_t *                u_handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                esto_alg_str,
    globus_off_t                                partial_offset,
    globus_off_t                                partial_end_offset,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_put);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_put() entering\n"));
        
    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }

    /* Setup handle for the put operation */
    handle->op = GLOBUS_FTP_CLIENT_PUT;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->partial_offset = partial_offset;
    handle->partial_end_offset = partial_end_offset;

    /* In stream mode, we need to keep track of the base offset to
     * adjust the offsets returned from the control library.
     * (In extended block mode, the data blocks have headers which have
     * the offset in them, so we can trust the control library.)
     */
    if((!attr) || (!*attr) ||
	    ((*attr) && (*attr)->mode == GLOBUS_FTP_CONTROL_MODE_STREAM))
    {
	if(restart && restart->type == GLOBUS_FTP_CLIENT_RESTART_STREAM)
	{
	   if(((!attr) || (!*attr) ||
		       (*attr)->type == GLOBUS_FTP_CONTROL_TYPE_IMAGE) &&
	      restart->stream.offset > handle->base_offset)
	    {
	        handle->base_offset = restart->stream.offset;
	    }
            else if(attr && *attr &&
		    (*attr)->type == GLOBUS_FTP_CONTROL_TYPE_ASCII &&
	       restart->stream.ascii_offset > handle->base_offset)
	    {
	        handle->base_offset = restart->stream.ascii_offset;
	    }
	}
    }

    if(restart)
    {
	globus_ftp_client_restart_marker_copy(&handle->restart_marker,
					      restart);
    }
    else
    {
	globus_ftp_client_restart_marker_init(&handle->restart_marker);
    }

    handle->dest_url = globus_libc_strdup(url);
    if(handle->dest_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the dest FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->dest);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    if(esto_alg_str != NULL)
    {
        if(handle->dest->attr->module_alg_str != NULL)
        {
            free(handle->dest->attr->module_alg_str);
        }
        handle->dest->attr->module_alg_str = strdup(esto_alg_str);
    }

    /* Notify plugins that the PUT is happening */
    globus_i_ftp_client_plugin_notify_put(handle,
					  handle->dest_url,
					  handle->dest->attr,
					  restart);
    /*
     * We must check our handle state before continuing.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->dest,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto dest_problem_exit;
	}
    }
    globus_i_ftp_client_handle_unlock(handle);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_put() exiting\n"));
    
    return GLOBUS_SUCCESS;

    /* Error handling */
dest_problem_exit:
    if(handle->dest != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->dest);
    }
free_url_exit:
    globus_libc_free(handle->dest_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->dest_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->partial_offset = -1;
    handle->partial_end_offset = -1;
    globus_ftp_client_restart_marker_destroy(&handle->restart_marker);

unlock_exit:
    /* Release the lock */
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_extended_put() exiting with error\n"));

    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->dest);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	globus_i_ftp_client_debug_printf(1, (stderr, 
            "globus_l_ftp_client_extended_put() exiting after restart\n"));

	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */

abort:
    if(handle->dest)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->dest);
    }
    /* Reset the state of the handle */
    globus_libc_free(handle->dest_url);
    handle->dest_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;

    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
            "globus_l_ftp_client_extended_put() exiting after abort\n"));
    
    return globus_error_put(err);
}
/* globus_l_ftp_client_extended_put() */
/*@}*/

/**
 * @name 3rd Party Transfer
 */
/*@{*/
/**
 * Transfer a file between two FTP servers.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts up a third-party file transfer between FTP server.
 * This function returns immediately. 
 *
 * When the transfer is completed or if the transfer is
 * aborted, the complete_callback will be invoked with the final
 * status of the transfer.
 *
 * @param handle
 *        An FTP Client handle to use for the get operation.
 * @param source_url
 *	  The URL to transfer. The URL may be an ftp or gsiftp URL.
 * @param source_attr
 *	  Attributes for the souce URL.
 * @param dest_url
 *        The destination URL for the transfer. The URL may be an ftp
 *        or gsiftp URL.
 * @param dest_attr
 *        Attributes for the destination URL.
 * @param restart
 *        Pointer to a restart marker.
 * @param complete_callback
 *        Callback to be invoked once the "put" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @note The source_attr and dest_attr MUST be compatible. For
 *       example, the MODE and TYPE should match for both the source
 *       and destination. 
 */
globus_result_t
globus_ftp_client_third_party_transfer(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    globus_ftp_client_operationattr_t *		source_attr,
    const char *				dest_url,
    globus_ftp_client_operationattr_t *		dest_attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_third_party_transfer() entering\n"));
    
    result = globus_l_ftp_client_extended_third_party_transfer(handle,
						       source_url,
						       source_attr,
                                                       GLOBUS_NULL,
						       dest_url,
						       dest_attr,
                                                       GLOBUS_NULL,
						       restart,
						       -1,
						       -1,
						       complete_callback,
						       callback_arg);
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_third_party_transfer() exiting\n"));
    
    return result;
}
/* globus_ftp_client_third_party_transfer() */

/**
 * Transfer a file between two FTP servers.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts up a third-party file transfer between FTP server.
 * This function returns immediately. 
 *
 * When the transfer is completed or if the transfer is
 * aborted, the complete_callback will be invoked with the final
 * status of the transfer.
 *
 * @param handle
 *        An FTP Client handle to use for the get operation.
 * @param source_url
 *	  The URL to transfer. The URL may be an ftp or gsiftp URL.
 * @param source_attr
 *	  Attributes for the souce URL.
 * @param dest_url
 *        The destination URL for the transfer. The URL may be an ftp
 *        or gsiftp URL.
 * @param dest_attr
 *        Attributes for the destination URL.
 * @param restart
 *        Pointer to a restart marker.
 * @param partial_offset
 *        Starting offset for a partial file get.
 * @param partial_end_offset
 *        Ending offset for a partial file get. Use -1 for EOF.
 * @param complete_callback
 *        Callback to be invoked once the "put" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @note The source_attr and dest_attr MUST be compatible. For
 *       example, the MODE and TYPE should match for both the source
 *       and destination. 
 */
globus_result_t
globus_ftp_client_partial_third_party_transfer(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    globus_ftp_client_operationattr_t *		source_attr,
    const char *				dest_url,
    globus_ftp_client_operationattr_t *		dest_attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    char                                        eret_alg_buf[128]; 
    char                                        esto_alg_buf[128]; 
    globus_result_t                             result;
    globus_ftp_client_restart_marker_t          tmp_restart;
    globus_object_t *                            err;
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_ftp_client_partial_third_party_transfer() entering\n"));
    
    if(partial_offset < 0)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("partial_offset");
        goto error_param;
    }

    if(partial_end_offset != -1 && partial_end_offset < partial_offset)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("partial_end_offset");
        goto error_param;
    }

    if(partial_end_offset != -1)
    { 
        sprintf(
            eret_alg_buf, GLOBUS_L_DATA_ERET_FORMAT_STRING,
            partial_offset, 
            partial_end_offset - partial_offset);
        sprintf(esto_alg_buf, GLOBUS_L_DATA_ESTO_FORMAT_STRING, partial_offset);
    
        result = globus_l_ftp_client_extended_third_party_transfer(
            handle, 
            source_url,
            source_attr,
            eret_alg_buf,
            dest_url,
            dest_attr,
            esto_alg_buf,
            restart,
            partial_offset,
            partial_end_offset,
            complete_callback,
            callback_arg);

    }
    /* if partial_end_offset == -1 use a restart at offset to get the full
        transfer starting at offset */
    else
    {
        if(restart)
        {
            globus_ftp_client_restart_marker_copy(&tmp_restart, restart);
        }
        else
        {
            globus_ftp_client_restart_marker_init(&tmp_restart);
        }
        
        if(tmp_restart.type == GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK ||
            (source_attr && *source_attr && 
            (*source_attr)->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK))
        {
            globus_ftp_client_restart_marker_insert_range(
                &tmp_restart,
                0,
                partial_offset);
        }
        else if(tmp_restart.type == GLOBUS_FTP_CLIENT_RESTART_NONE ||
            (tmp_restart.type == GLOBUS_FTP_CLIENT_RESTART_STREAM && 
            tmp_restart.stream.offset < partial_offset))
        {
            globus_ftp_client_restart_marker_set_offset(
                &tmp_restart,
                partial_offset);
        }
        
        result = globus_ftp_client_third_party_transfer(
            handle, 
            source_url,
            source_attr,
            dest_url,
            dest_attr,
            &tmp_restart,
            complete_callback,
            callback_arg);
           
        globus_ftp_client_restart_marker_destroy(&tmp_restart);
    }

    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_ftp_client_partial_third_party_transfer() exiting\n"));
    
    return result;
    
error_param:
    return globus_error_put(err);

}
/* globus_ftp_client_partial_third_party_transfer() */

/**
 * Transfer a file between two FTP servers with server-side processing.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts up a third-party file transfer between FTP server.
 * This function returns immediately. 
 *
 * When the transfer is completed or if the transfer is
 * aborted, the complete_callback will be invoked with the final
 * status of the transfer.
 *
 * This function differes from the globus_ftp_client_third_party_transfer()
 * funciton by allowing the user to invoke server-side data processing
 * algorithms.  GridFTP servers may support algorithms for data reduction or
 * other customized data storage requirements. There is no client-side
 * verification done on the alogirhtm string provided by the user. if the
 * server does not understand * the requested algorithm, the transfer will
 * fail.
 *
 * @param handle
 *        An FTP Client handle to use for the get operation.
 * @param source_url
 *	  The URL to transfer. The URL may be an ftp or gsiftp URL.
 * @param source_attr
 *	  Attributes for the souce URL.
 * @param dest_url
 *        The destination URL for the transfer. The URL may be an ftp
 *        or gsiftp URL.
 * @param dest_attr
 *        Attributes for the destination URL.
 * @param restart
 *        Pointer to a restart marker.
 * @param complete_callback
 *        Callback to be invoked once the "put" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @note The source_attr and dest_attr MUST be compatible. For
 *       example, the MODE and TYPE should match for both the source
 *       and destination. 
 */
globus_result_t
globus_ftp_client_extended_third_party_transfer(
    globus_ftp_client_handle_t *                handle,
    const char *                                source_url,
    globus_ftp_client_operationattr_t *         source_attr,
    const char *                                eret_alg_str,
    const char *                                dest_url,
    globus_ftp_client_operationattr_t *         dest_attr,
    const char *                                esto_alg_str,
    globus_ftp_client_restart_marker_t *        restart,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg)
{
    globus_result_t                             result;
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_ftp_client_extended_third_party_transfer() entering\n"));
    
    result = globus_l_ftp_client_extended_third_party_transfer(
               handle, 
               source_url,
               source_attr,
               eret_alg_str,
               dest_url,
               dest_attr,
               esto_alg_str,
               restart,
               -1,
               -1,
               complete_callback,
               callback_arg);
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_ftp_client_extended_third_party_transfer() exiting\n"));
    
    return result;
}
/* globus_ftp_client_extended_third_party_transfer() */

static
globus_result_t
globus_l_ftp_client_extended_third_party_transfer(
    globus_ftp_client_handle_t *                u_handle,
    const char *                                source_url,
    globus_ftp_client_operationattr_t *         source_attr,
    const char *                                eret_alg_str,
    const char *                                dest_url,
    globus_ftp_client_operationattr_t *         dest_attr,
    const char *                                esto_alg_str,
    globus_ftp_client_restart_marker_t *        restart,
    globus_off_t                                partial_offset,
    globus_off_t                                partial_end_offset,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    globus_ftp_client_operationattr_t 		normalized_source_attr;
    globus_ftp_client_operationattr_t 		normalized_dest_attr;
    globus_i_ftp_client_operationattr_t *	use_attr;

    GlobusFuncName(globus_ftp_client_partial_third_party_transfer);
    
    globus_i_ftp_client_debug_printf(1, (stderr,
        "globus_l_ftp_client_extended_third_party_transfer() entering\n"));

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("source_url");

	goto error_exit;
    }
    else if(dest_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("dest_url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
            
    globus_i_ftp_client_handle_is_active(u_handle);
    
    globus_i_ftp_client_handle_lock(handle);
    
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

        goto unlock_exit;
    }
    /* Make sure the attributes are compatible for a 3rd party operation,
     * tweaking things like default DCAU -> no DCAU when mixing GSI
     * and non-GSI servers
     */
    err = globus_l_ftp_client_transfer_normalize_attrs(
            source_url,
            source_attr,
            &normalized_source_attr,
            dest_url,
            dest_attr,
            &normalized_dest_attr);

    if(err != GLOBUS_SUCCESS)
    {
        goto unlock_exit;
    }

    /* Setup handle for third-party transfer operation */
    handle->op = GLOBUS_FTP_CLIENT_TRANSFER;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->partial_offset = partial_offset;
    handle->partial_end_offset = partial_end_offset;

    if(restart)
    {
        globus_ftp_client_restart_marker_copy(&handle->restart_marker,
                                              restart);
    }
    else
    {
        globus_ftp_client_restart_marker_init(&handle->restart_marker);
    }
    handle->source_url = globus_libc_strdup(source_url);
    if(handle->source_url == GLOBUS_NULL)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

        goto reset_handle_exit;
    }
    handle->dest_url = globus_libc_strdup(dest_url);
    if(handle->dest_url == GLOBUS_NULL)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

        goto free_source_url_exit;
    }

    if(normalized_source_attr)
    {
        use_attr = normalized_source_attr;
    }
    else if(source_attr)
    {
        use_attr = *source_attr;
    }
    else
    {
        use_attr = GLOBUS_NULL;
    }
    /* Obtain a connection to the source FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
                                          source_url,
                                          use_attr,
                                          &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
        goto free_dest_url_exit;
    }

    /* Obtain a connection to the destination FTP server, maybe cached */
    if(normalized_dest_attr)
    {
        use_attr = normalized_dest_attr;
    }
    else if(dest_attr)
    {
        use_attr = *dest_attr;
    }
    else
    {
        use_attr = GLOBUS_NULL;
    }
    err = globus_i_ftp_client_target_find(handle,
                                          dest_url,
                                          use_attr,
                                          &handle->dest);

    if(err != GLOBUS_SUCCESS)
    {
        goto src_problem_exit;
    }
    if(esto_alg_str != NULL)
    {
        if(handle->dest->attr->module_alg_str != NULL)
        {
            free(handle->dest->attr->module_alg_str);
        }
        handle->dest->attr->module_alg_str = strdup(esto_alg_str);
    }
    if(eret_alg_str != NULL)
    {
        if(handle->source->attr->module_alg_str != NULL)
        {
            free(handle->source->attr->module_alg_str);
        }
        handle->source->attr->module_alg_str = strdup(eret_alg_str);
    }



    /* Notify plugins that the TRANSFER is happening */
    globus_i_ftp_client_plugin_notify_transfer(
        handle,
        handle->source_url,
        handle->source->attr,
        handle->dest_url,
        handle->dest->attr,
        restart);

    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
        err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

        goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
        goto restart;
    }

    /*
     * We will prepare the destination handle before we do the source
     * handle; this way we can do everything including the STOR/RETR on
     * both connections without having to switch between sending
     * commands on the two
     */
    err = globus_i_ftp_client_target_activate(handle,
                                              handle->dest,
                                              &registered);


    if(registered == GLOBUS_FALSE)
    {
        /* 
         * A restart or abort happened during activation, before any
         * callbacks were registered. We must deal with them here.
         */
        globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
                      err != GLOBUS_SUCCESS);

        if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
        {
            err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
            
            goto abort;
        }
        else if (handle->state ==
                 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
        {
            goto restart;
        }
        else if(err != GLOBUS_SUCCESS)
        {
            goto dest_problem_exit;
        }
    }

    if(normalized_source_attr)
    {
        globus_ftp_client_operationattr_destroy(&normalized_source_attr);
    }
    if(normalized_dest_attr)
    {
        globus_ftp_client_operationattr_destroy(&normalized_dest_attr);
    }

    globus_i_ftp_client_handle_unlock(handle);
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_l_ftp_client_extended_third_party_transfer() exiting\n"));

    return GLOBUS_SUCCESS;

    /* Error handling */
dest_problem_exit:
    globus_i_ftp_client_target_release(handle,
				       handle->dest);
src_problem_exit:
    globus_i_ftp_client_target_release(handle,
				       handle->source);
free_dest_url_exit:
    globus_libc_free(handle->dest_url);
free_source_url_exit:
    globus_libc_free(handle->source_url);
reset_handle_exit:
    /* Free normalized attrs, if created */
    if(normalized_source_attr)
    {
	globus_ftp_client_operationattr_destroy(&normalized_source_attr);
    }
    if(normalized_dest_attr)
    {
	globus_ftp_client_operationattr_destroy(&normalized_dest_attr);
    }

    /* Reset the state of the handle. */
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->source_url = GLOBUS_NULL;
    handle->dest_url = GLOBUS_NULL;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->partial_offset = -1;
    handle->partial_end_offset = -1;
    globus_ftp_client_restart_marker_destroy(&handle->restart_marker);

unlock_exit:
    /* release the lock */
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_l_ftp_client_extended_third_party_transfer() exiting with error\n"));

    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);
    globus_i_ftp_client_target_release(handle,
				       handle->dest);
    err = globus_i_ftp_client_restart_register_oneshot(handle);
    
    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	
	globus_i_ftp_client_debug_printf(1, (stderr, 
            "globus_l_ftp_client_extended_third_party_transfer() exiting after restart\n"));

	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */

abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }
    if(handle->dest)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->dest);
    }
    
    /* Reset the state of the handle */
    globus_libc_free(handle->source_url);
    globus_libc_free(handle->dest_url);
    handle->source_url = GLOBUS_NULL;
    handle->dest_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;

    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_l_ftp_client_extended_third_party_transfer() exiting after abort\n"));

    return globus_error_put(err);
}
/* globus_l_ftp_client_extended_third_party_transfer() */
/*@}*/

/**
 * @name Modification Time
 */
/*@{*/
/**
 * Get a file's modification time from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function requests the modification time of a file from an FTP
 * server.
 *
 * When the modification time request is completed or aborted, the
 * complete_callback will be invoked with the final status of the operation.
 * If the callback is returns without an error, the modification time will
 * be stored in the globus_abstime_t value pointed to by the modification_time
 * parameter to this function.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param modification_time
 *        A pointer to a globus_abstime_t to be filled with the modification
 *        time of the file, if it exists. Otherwise, the value pointed to by it
 *        is undefined.
 * @param complete_callback
 *        Callback to be invoked once the modification time check is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - modification time is GLOBUS_NULL
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_modification_time(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_abstime_t *				modification_time,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_modification_time);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }
    else if(modification_time == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("modification_time");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the MDTM*/
    handle->op = GLOBUS_FTP_CLIENT_MDTM;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->modification_time_pointer = modification_time;

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the LIST is happening */
    globus_i_ftp_client_plugin_notify_modification_time(handle,
						        handle->source_url,
						        handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this list. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

 free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->modification_time_pointer = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_modification_time() */
/*@}*/

/**
 * @name Size
 */
/*@{*/
/**
 * Get a file's size from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function requests the size of a file from an FTP
 * server.
 *
 * When the size request is completed or aborted, the complete_callback
 * will be invoked with the final status of the operation.
 * If the callback is returns without an error, the size will be stored in
 * the globus_off_t value pointed to by the size parameter to this function.
 *
 * @note In ASCII mode, the size will be the size of the file after conversion
 * to ASCII mode. The actual amount of data which is returned in the data
 * callbacks may be less than this amount.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param size
 *        A pointer to a globus_off_t to be filled with the total size of the
 *        file, if it exists. Otherwise, the value pointed to by it
 *        is undefined.
 * @param complete_callback
 *        Callback to be invoked once the size check is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - size is GLOBUS_NULL
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_size(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_off_t *				size,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_size);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }
    else if(size == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("size");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the SIZE */
    handle->op = GLOBUS_FTP_CLIENT_SIZE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->size_pointer = size; 

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the SIZE is happening */
    globus_i_ftp_client_plugin_notify_size(handle,
					   handle->source_url,
					   handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this list. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

 free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->size_pointer = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_size() */
/*@}*/

/**
 * @name Cksm
 */
/*@{*/
/**
 * Get a file's checksum from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function requests the checksum of a file from an FTP
 * server.
 *
 * When the request is completed or aborted, the complete_callback
 * will be invoked with the final status of the operation.
 * If the callback is returns without an error, the checksum will be stored in
 * the char * buffer provided in the 'checksum' parameter to this function. 
 * The buffer must be large enough to hold the expected checksum result. 
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param cksm
 *        A pointer to a buffer to be filled with the checksum of the file.
 *        On error the buffer contents are undefined.  
 * @param offset
 *        File offset to start calculating checksum.    
 * @param length
 *        Length of data to read from the starting offset.  Use -1 to read the
 *        entire file.
 * @param algorithm
 *        A pointer to a string to specifying the desired algorithm
 *        Currently, GridFTP servers only support the MD5 algorithm.      
 * @param complete_callback
 *        Callback to be invoked once the checksum is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - size is GLOBUS_NULL
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_cksm(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    char *					cksm,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_bool_t				registered;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_cksm);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }
    else if(cksm == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("cksm");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the CKSM */
    handle->op = GLOBUS_FTP_CLIENT_CKSM;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);
    handle->checksum = cksm;
    handle->checksum_length = length;
    handle->checksum_offset = offset;
    handle->checksum_alg = globus_libc_strdup(algorithm);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  attr ? *attr : GLOBUS_NULL,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto free_url_exit;
    }

    /* Notify plugins that the CKSM is happening */
    globus_i_ftp_client_plugin_notify_cksm(handle,
					   handle->source_url,
					   handle->checksum_offset,
					   handle->checksum_length,
					   handle->checksum_alg,
					   handle->source->attr);

    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this list. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

 free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    handle->checksum = GLOBUS_NULL;
    handle->checksum_length = -1;
    handle->checksum_offset = 0;
    handle->checksum_alg = GLOBUS_NULL;

    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_ftp_client_cksm() */
/*@}*/

/**
 *
 * @name Abort
 */
/*@{*/
/**
 * Abort the operation currently in progress.
 * @ingroup globus_ftp_client_operations
 *
 * @param u_handle 
 *        Handle which to abort.
 */
globus_result_t
globus_ftp_client_abort(
    globus_ftp_client_handle_t *		u_handle)
{
    globus_bool_t                   active;
    globus_object_t *				err;
    globus_result_t				result;
    globus_i_ftp_client_handle_t *		handle;
    GlobusFuncName(globus_ftp_client_abort);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_abort() entering\n"));
    
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error;
    }
    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error;
    }

    handle = *u_handle;
    globus_i_ftp_client_handle_lock(handle);
    
    globus_i_ftp_client_debug_states(2, handle);
        
    if(handle->op == GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_NOT_IN_USE("handle");

	goto unlock_error;
    }

    switch(handle->state)
    {
    case GLOBUS_FTP_CLIENT_HANDLE_START:
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;
	handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
	
	globus_i_ftp_client_plugin_notify_abort(handle);
	break;
	
    case GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT:
    case GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION:
    case GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO:
	
	result = globus_ftp_control_force_close(
	    handle->dest->control_handle,
	    globus_i_ftp_client_force_close_callback,
	    handle->dest);

	if(result == GLOBUS_SUCCESS)
	{
	    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;
	    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

	    globus_i_ftp_client_plugin_notify_abort(handle);
	    
	    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
	}
	else
	{
	    err = globus_error_get(result);
	    goto unlock_error;
	}
	break;

    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT:
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION:
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST:
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET:
    case GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER:
	if(handle->op != GLOBUS_FTP_CLIENT_TRANSFER)
	{
	    result = globus_ftp_control_force_close(
		handle->source->control_handle,
		globus_i_ftp_client_force_close_callback,
		handle->source);
	    if(result == GLOBUS_SUCCESS)
	    {
		handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;
		handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
		handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		
		globus_i_ftp_client_plugin_notify_abort(handle);
	    }
	    else
	    {
		err = globus_error_get(result);
		goto unlock_error;
	    }
	}
	else
	{
	    result = globus_ftp_control_force_close(
		handle->source->control_handle,
		globus_i_ftp_client_force_close_callback,
		handle->source);
	    if(result == GLOBUS_SUCCESS)
	    {
		result = globus_ftp_control_force_close(
		    handle->dest->control_handle,
		    globus_i_ftp_client_force_close_callback,
		    handle->dest);

		if(result == GLOBUS_SUCCESS)
		{
		    handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;

		    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

		    globus_i_ftp_client_plugin_notify_abort(handle);
		}
		else
		{
		    handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_CLOSED;
		    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;

		    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
		}
	    }
	    else
	    {
		err = globus_error_get(result);

		goto error;
	    }
	}
	break;
    case GLOBUS_FTP_CLIENT_HANDLE_FINALIZE:
    case GLOBUS_FTP_CLIENT_HANDLE_ABORT:
    case GLOBUS_FTP_CLIENT_HANDLE_FAILURE:
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_NOT_IN_USE("handle");
	goto unlock_error;

    case GLOBUS_FTP_CLIENT_HANDLE_RESTART:
	globus_callback_unregister(
	    handle->restart_info->callback_handle,
	    GLOBUS_NULL,
	    GLOBUS_NULL,
	    &active);

	if(active)
	{
	    /* 
	     * The callback is about to start, but needs the lock. We will just
	     * set the handle state and then return, the callback will
	     * notify the plugins about the abort.
	     */
	    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;
	    if(handle->err)
	    {
		globus_object_free(handle->err);
	    }
	    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
	}
	else
	{
	    /*
	     * We killed the callback before it could happen.
	     * Now we can safely register a oneshot to terminate
	     * this transfer for good.
	     */
	    if(handle->err)
	    {
		globus_object_free(handle->err);
	    }
	    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    result = globus_callback_register_oneshot(
		GLOBUS_NULL,
		&globus_i_reltime_zero,
		globus_l_ftp_client_abort_callback,
		handle);

	    if(result != GLOBUS_SUCCESS)
	    {
		goto unlock_error;
	    }
	    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;
	}
	break;
	    
    case GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE:
	/* This is called when one side of a third-party transfer has sent
	 * it's positive or negative response to the transfer, but the
	 * other hasn't yet done the same.
	 */
	globus_assert(handle->op == GLOBUS_FTP_CLIENT_TRANSFER);
	if(handle->source->state == GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION)
	{
	    result = globus_ftp_control_force_close(
		handle->dest->control_handle,
		globus_i_ftp_client_force_close_callback,
		handle->dest);
	    
	    if(result == GLOBUS_SUCCESS)
	    {
		handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;

		handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

		globus_i_ftp_client_plugin_notify_abort(handle);
	    }
	    else
	    {
		err = globus_error_get(result);
		goto unlock_error;
	    }
	}
	else if(handle->dest->state == 
	    GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION)
	{
	    result = globus_ftp_control_force_close(
		handle->source->control_handle,
		globus_i_ftp_client_force_close_callback,
		handle->source);
	    
	    if(result == GLOBUS_SUCCESS)
	    {
		handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;

		handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

		globus_i_ftp_client_plugin_notify_abort(handle);
	    }
	    else
	    {
		err = globus_error_get(result);
		goto unlock_error; 
	    }
	}
        else
        {
            result = globus_ftp_control_force_close(
                handle->source->control_handle,
                globus_i_ftp_client_force_close_callback,
                handle->source);
            if(result == GLOBUS_SUCCESS)
            {
                result = globus_ftp_control_force_close(
                    handle->dest->control_handle,
                    globus_i_ftp_client_force_close_callback,
                    handle->dest);

                if(result == GLOBUS_SUCCESS)
                {
                    handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
                    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
                    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;

                    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

                    globus_i_ftp_client_plugin_notify_abort(handle);
                }
                else
                {
                    handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
                    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_CLOSED;
                    handle->state = GLOBUS_FTP_CLIENT_HANDLE_ABORT;

                    handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
                }
            }
            else
            {
                err = globus_error_get(result);

                goto unlock_error;
            }
        }           

    }
    globus_i_ftp_client_handle_unlock(handle);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_abort() exiting\n"));
    globus_i_ftp_client_debug_states(2, handle);
    
    return GLOBUS_SUCCESS;
unlock_error:
    globus_i_ftp_client_handle_unlock(handle);
error:
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_ftp_client_abort() exiting with error\n"));
    globus_i_ftp_client_debug_states(2, handle);
    
    return globus_error_put(err);
}
/* globus_ftp_client_abort() */
/*@}*/

/* Internal/Local Functions */
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

/**
 * Get a file listing from an FTP server.
 * @ingroup globus_ftp_client_operations
 *
 * This function starts a list transfer from an FTP server. If
 * this function returns GLOBUS_SUCCESS, then the user may immediately
 * begin calling globus_ftp_client_register_read() to retrieve the data
 * associated with this listing.
 *
 * When all of the data associated with the listing is retrieved, and all 
 * of the data callbacks have been called, or if the list request is
 * aborted, the complete_callback will be invoked with the final
 * status of the list.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *	  The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *	  Attributes for this file transfer.
 * @param op
 *	  Specific list operation to perform.
 * @param complete_callback
 *        Callback to be invoked once the "list" is completed.
 * @param callback_arg
 *	  Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - handle is GLOBUS_NULL
 *        - url is GLOBUS_NULL
 *        - url cannot be parsed
 *        - url is not a ftp or gsiftp url
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 *
 * @see globus_ftp_client_register_read()
 */

static
globus_result_t
globus_l_ftp_client_list_op(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_i_ftp_client_operation_t             op,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_result_t                             result;
    globus_bool_t				registered;
    globus_ftp_client_operationattr_t  		local_attr;
    globus_i_ftp_client_handle_t *		handle;

    GlobusFuncName(globus_l_ftp_client_list_op);

    /* Check arguments for validity */
    if(u_handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    else if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	goto error_exit;
    }
    else if(complete_callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

	goto error_exit;
    }

    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    globus_i_ftp_client_handle_is_active(u_handle);

    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");

	goto unlock_exit;
    }
    /* Setup handle for the list operation */
    handle->op = op;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->source_url = globus_libc_strdup(url);

    if(handle->source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto reset_handle_exit;
    }

    if(attr != GLOBUS_NULL)
    {
        result = globus_ftp_client_operationattr_copy(&local_attr,
						  attr);
    }
    else
    {
	result = globus_ftp_client_operationattr_init(&local_attr);
    }
    
    if(result != GLOBUS_SUCCESS)
    {
	err = globus_error_get(result);
	goto free_url_exit;
    }
    
    /* Obtain a connection to the FTP server, maybe cached */
    err = globus_i_ftp_client_target_find(handle,
					  url,
					  local_attr,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	goto destroy_local_attr_exit;
    }
    
    /* force stream/ASCII/no parallelism, 
        unless we are allowing lists in the current data mode */
    if(!handle->source->attr->list_uses_data_mode)
    {
        handle->source->attr->mode = GLOBUS_FTP_CONTROL_MODE_STREAM;
        handle->source->attr->type = GLOBUS_FTP_CONTROL_TYPE_ASCII;
        handle->source->attr->parallelism.mode = 
            GLOBUS_FTP_CONTROL_PARALLELISM_NONE;
    }

    /* Notify plugins that a list operation is happening */
    switch(op)
    {
      case GLOBUS_FTP_CLIENT_NLST:
        globus_i_ftp_client_plugin_notify_list(handle,
					   handle->source_url,
					   handle->source->attr);
        break;

      case GLOBUS_FTP_CLIENT_LIST:
        globus_i_ftp_client_plugin_notify_verbose_list(handle,
					   handle->source_url,
					   handle->source->attr);
        break;
        
      case GLOBUS_FTP_CLIENT_MLSD:
        globus_i_ftp_client_plugin_notify_machine_list(handle,
					   handle->source_url,
					   handle->source->attr);
        break;
        
      default:
        globus_assert(0 && "unknown ftp client list operation");
        break;
        
    }
        
    /* 
     * check our handle state before continuing, because we just unlocked.
     */
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }

    err = globus_i_ftp_client_target_activate(handle, 
					      handle->source,
					      &registered);
    if(registered == GLOBUS_FALSE)
    {
	/* 
	 * A restart or abort happened during activation, before any
	 * callbacks were registered. We must deal with them here.
	 */
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
		      err != GLOBUS_SUCCESS);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();

	    goto abort;
	}
	else if (handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto source_problem_exit;
	}
    }

    globus_i_ftp_client_handle_unlock(handle);

    globus_ftp_client_operationattr_destroy(&local_attr);

    return GLOBUS_SUCCESS;

    /* Error handling */
source_problem_exit:
    /* Release the target associated with this list. */
    if(handle->source != GLOBUS_NULL)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

 destroy_local_attr_exit:
    globus_ftp_client_operationattr_destroy(&local_attr);

 free_url_exit:
    globus_libc_free(handle->source_url);

reset_handle_exit:
    /* Reset the state of the handle. */
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    /* Release the lock */
unlock_exit:
    globus_i_ftp_client_handle_unlock(handle);

    globus_i_ftp_client_handle_is_not_active(u_handle);

    /* And return our error */
error_exit:
    return globus_error_put(err);

restart:
    globus_i_ftp_client_target_release(handle,
				       handle->source);

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
	return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;
    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

    return globus_error_put(err);
}
/* globus_l_ftp_client_list_op() */




/**
 * Complete processing a get, put, or 3rd party transfer request.
 *
 * This function is called when the FTP Client API has completed
 * all operations associated with a given get, put, or 3rd party
 * transfer. This function calls the user's completion callback, and
 * resets the state of the client handle to be able to process a new
 * operation.
 *
 * @param client_handle
 *        The handle which is completing the processing of the
 *        operation. 
 *
 * @note Call this function with the handle's mutex locked. It will
 *       return unlocked, as the caller MUST NOT try to touch the handle
 *       once this function has returned.
 */
void
globus_i_ftp_client_transfer_complete(
    globus_i_ftp_client_handle_t *		client_handle)
{
    globus_ftp_client_complete_callback_t	callback;
    globus_ftp_client_handle_t *		handle;
    void *					callback_arg;
    globus_object_t *				error;

    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_transfer_complete() entering\n"));

    /* 
     * Switch state before calling into plugins, to prevent them
     * from doing anything on this handle.
     */
    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FINALIZE;
    globus_i_ftp_client_plugin_notify_complete(client_handle);
    
    /*
     * cache data connection info if possible.
     */
    if(!globus_i_ftp_client_can_reuse_data_conn(client_handle))
    {
        if(client_handle->source)
        {
            memset(&client_handle->source->cached_data_conn,
                   '\0',
                   sizeof(globus_i_ftp_client_data_target_t));
        }
        if(client_handle->dest)
        {
            memset(&client_handle->dest->cached_data_conn,
                   '\0',
                   sizeof(globus_i_ftp_client_data_target_t));
        }
    }
    /* Release any ftp targets back to the client handle's cache,
     * if requested by the user.
     *
     * If one of the targets went bad, it should be in such a state
     * that the call to release will destroy it.
     */
    if(client_handle->source)
    {
        globus_i_ftp_client_target_release(client_handle,
                                           client_handle->source);
        client_handle->source = GLOBUS_NULL;
    }
    if(client_handle->dest)
    {
        globus_i_ftp_client_target_release(client_handle,
                                           client_handle->dest);
        client_handle->dest = GLOBUS_NULL;
    }

    /*
     * Save information from the handle, and then reset it to it's default
     * condition for after this callback completes.
     */
    callback = client_handle->callback;
    client_handle->callback = GLOBUS_NULL;
    
    callback_arg = client_handle->callback_arg;
    client_handle->callback_arg = GLOBUS_NULL;
    
    error = client_handle->err;
    client_handle->err = GLOBUS_NULL;

    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    client_handle->op = GLOBUS_FTP_CLIENT_IDLE;
    if(client_handle->source_url)
    {
        globus_libc_free(client_handle->source_url);
        client_handle->source_url = GLOBUS_NULL;
    }
    if(client_handle->dest_url)
    {
        globus_libc_free(client_handle->dest_url);
        client_handle->dest_url = GLOBUS_NULL;
    }
    client_handle->source_size = 0;

    client_handle->read_all_biggest_offset = 0;
    client_handle->base_offset = 0;

    client_handle->partial_offset = -1;
    client_handle->partial_end_offset = -1;

    if(client_handle->pasv_address != GLOBUS_NULL)
    {
        globus_libc_free(client_handle->pasv_address);
        client_handle->pasv_address = GLOBUS_NULL;
    }
    client_handle->num_pasv_addresses = 0;

    globus_ftp_client_restart_marker_destroy(&client_handle->restart_marker);
    
    /*
     * Call back to the user. After this callback, we can not
     * touch the handle from this callback scope.
     */
    handle = client_handle->handle;
    globus_i_ftp_client_handle_unlock(client_handle);

    globus_i_ftp_client_handle_is_not_active(handle);

    (*callback)(callback_arg,
                    handle,
                    error);
    
    if(error)
    {
        globus_object_free(error);
    }
        
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_transfer_complete() exiting\n"));
}
/* globus_i_ftp_client_transfer_complete() */

static
void
globus_i_ftp_client_faked_response_callback(
    void *                              user_arg)
{
    globus_i_ftp_client_target_t *      target;
    
    target = (globus_i_ftp_client_target_t *) user_arg;
    
    globus_i_ftp_client_response_callback(
        user_arg, target->control_handle, NULL, NULL);
}

/**
 * Cause the target to be eligible for callbacks.
 *
 * When a target is returned from globus_i_ftp_client_target_find() it
 * doesn't have any active command associated. This function registers
 * a command for this new target, to activate the state machine for
 * this target.
 *
 * @param handle
 *        The handle associated with this target.
 * @param target
 *        The target to activate.
 * @param registered
 *        Return flag, set to GLOBUS_TRUE if an event handler was
 *        successfully registered with the FTP Control library. If
 *        this is GLOBUS_FALSE, then the caller must deal with any 
 *        abort/restart which happened during unlocked time.
 */
globus_object_t *
globus_i_ftp_client_target_activate(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target,
    globus_bool_t *				registered)
{
    globus_ftp_client_handle_state_t		desired_state;
    globus_result_t				result = GLOBUS_SUCCESS;
    globus_object_t *				err;

    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_target_activate() entering\n"));

    *registered = GLOBUS_FALSE;

    if(target->state == GLOBUS_FTP_CLIENT_TARGET_START)
    {
    	/* New control structure, make the connection */
	desired_state = handle->state;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_CONTROL_ESTABLISHMENT;
	globus_i_ftp_client_plugin_notify_connect(handle,
						  target->url_string);
	
	if(handle->state == desired_state)
	{
	    result = globus_ftp_control_connect(
		target->control_handle,
		target->url.host,
		target->url.port,
		globus_i_ftp_client_response_callback,
		target);
	    
	    if(result != GLOBUS_SUCCESS)
	    {
		err = globus_error_get(result);
		if(handle->err == GLOBUS_SUCCESS)
		{
		    handle->err = globus_object_copy(err);
		}

		globus_i_ftp_client_plugin_notify_fault(
		    handle,
		    target->url_string,
		    err);

		goto error_exit;
	    }

	    if(handle->source == target)
	    {
		handle->state = GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT;
	    }
	    else
	    {
		handle->state = GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT;
	    }
	    target->state = GLOBUS_FTP_CLIENT_TARGET_CONNECT;
	    (*registered) = GLOBUS_TRUE;
	}
    }
    else if(target->state == GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION)
    {
        globus_abstime_t                expire_time;
        globus_bool_t                   do_noop = GLOBUS_FALSE;
        
	/* Old control structure, send a NOOP to make sure it's still good. */
	if(handle->source == target)
	{
	    handle->state = GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION;
	}
	else
	{
	    handle->state = GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION;
	}
	desired_state = handle->state;

	target->state = GLOBUS_FTP_CLIENT_TARGET_NOOP;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_MISC;
        
        GlobusTimeAbstimeGetCurrent(expire_time);
        GlobusTimeAbstimeDec(expire_time, globus_i_ftp_client_noop_idle);
        if(globus_abstime_cmp(&target->last_access, &expire_time) < 0)
        {
            do_noop = GLOBUS_TRUE;
            
            globus_i_ftp_client_plugin_notify_command(
                handle,
                target->url_string,
                target->mask,
                "NOOP" CRLF);
        }

	if(handle->state == desired_state)
	{
	    if(do_noop)
	    {
                result = globus_ftp_control_send_command(
                    target->control_handle,
                    "NOOP" CRLF,
                    globus_i_ftp_client_response_callback,
                    target);
            }
            else
            {
                result = globus_callback_register_oneshot(
                    NULL,
                    NULL,
                    globus_i_ftp_client_faked_response_callback,
                    target);
            }
             
	    if (result != GLOBUS_SUCCESS)
	    {
		err = globus_error_get(result);
		if(handle->err == GLOBUS_SUCCESS)
		{
		    handle->err = globus_object_copy(err);
		}

		globus_i_ftp_client_plugin_notify_fault(
		    handle,
		    target->url_string,
		    err);

		goto error_exit;
	    }
	    (*registered) = GLOBUS_TRUE;
	}
    }
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_target_activate() exiting\n"));

    return GLOBUS_SUCCESS;

error_exit:
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_target_activate() exiting with error\n"));

    return err;
}
/* globus_i_ftp_client_target_activate() */

void
globus_i_ftp_client_force_close_callback(
    void *					user_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_ftp_control_response_t *		response)
{
    globus_i_ftp_client_target_t *		target;
    globus_i_ftp_client_handle_t *		client_handle;
    globus_object_t *				err;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_force_close_callback() entering\n"));
        
    target = (globus_i_ftp_client_target_t *) user_arg;
    client_handle = target->owner;

    globus_assert(! GLOBUS_I_FTP_CLIENT_BAD_MAGIC(&client_handle));

    globus_i_ftp_client_handle_lock(client_handle);

    target->state = GLOBUS_FTP_CLIENT_TARGET_CLOSED;

    if(client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
        !(client_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_CLOSED &&
        client_handle->dest->state == GLOBUS_FTP_CLIENT_TARGET_CLOSED))
    {
	if((client_handle->source->state != GLOBUS_FTP_CLIENT_TARGET_CLOSED &&
	    client_handle->source->state != GLOBUS_FTP_CLIENT_TARGET_START &&
	    client_handle->source->state != GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION &&
	    client_handle->source->state != GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION) ||
	   (client_handle->dest->state != GLOBUS_FTP_CLIENT_TARGET_CLOSED &&
	    client_handle->dest->state != GLOBUS_FTP_CLIENT_TARGET_START &&
	    client_handle->dest->state != GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION &&
	    client_handle->dest->state != GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION))
	{
	    globus_i_ftp_client_handle_unlock(client_handle);
	    
            globus_i_ftp_client_debug_printf(1, (stderr, 
                "globus_i_ftp_client_force_close_callback() exiting\n"));

	    return;
	}
    }
    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	if(client_handle->source)
	{
	    globus_i_ftp_client_target_release(client_handle,
		                               client_handle->source);
	}
	if(client_handle->dest)
	{
	    globus_i_ftp_client_target_release(client_handle,
		                               client_handle->dest);
	}
	if(client_handle->pasv_address)
	{
	    globus_libc_free(client_handle->pasv_address);
	    client_handle->pasv_address = GLOBUS_NULL;

	    client_handle->num_pasv_addresses = 0;
	}
	if(client_handle->err)
	{
	    globus_object_free(client_handle->err);
	    client_handle->err = GLOBUS_NULL;
	}

	err = globus_i_ftp_client_restart_register_oneshot(client_handle);

	if(!err)
	{
	    globus_i_ftp_client_handle_unlock(client_handle);
	}
	else
	{
	    globus_i_ftp_client_data_flush(client_handle);
	    if(client_handle->err == GLOBUS_NULL)
	    {
		client_handle->err = err;
	    }
	    
	    globus_i_ftp_client_transfer_complete(client_handle);
	}
    }
    else
    {
	globus_i_ftp_client_data_flush(client_handle);

	/* This function unlocks and potentially frees the client_handle */
	globus_i_ftp_client_transfer_complete(client_handle);
    }
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_force_close_callback() exiting\n"));

}
/* globus_i_ftp_client_force_close_callback() */

/**
 * Internal callback to handle aborts.
 *
 * This callback is invoked if, when the FTP client is waiting for the
 * restart delay to expire, the transfer is aborted. Since all control
 * and data channel is completed before the delay, this just has to
 * complete the abort, and let the user know that the transfer is done.
 */
static
void
globus_l_ftp_client_abort_callback(
    void *					user_arg)
{
    globus_i_ftp_client_handle_t *		handle;
    handle = (globus_i_ftp_client_handle_t *) user_arg;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_abort_callback() entering\n"));
        
    globus_assert(! GLOBUS_I_FTP_CLIENT_BAD_MAGIC(&handle));
    globus_i_ftp_client_handle_lock(handle);

    globus_i_ftp_client_plugin_notify_abort(handle);

    if(handle->restart_info)
    {
	globus_i_ftp_client_restart_info_delete(handle->restart_info);
	handle->restart_info = GLOBUS_NULL;
    }
    globus_i_ftp_client_transfer_complete(handle);

    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_abort_callback() exiting\n"));
}
/* globus_l_ftp_client_abort_callback() */

static
globus_object_t *
globus_l_ftp_client_transfer_normalize_attrs(
    const char *			source_url,
    globus_ftp_client_operationattr_t *	source_attr,
    globus_ftp_client_operationattr_t * normalized_source_attr,
    const char *			dest_url,
    globus_ftp_client_operationattr_t *	dest_attr,
    globus_ftp_client_operationattr_t * normalized_dest_attr)
{
    globus_result_t			result;
    globus_ftp_control_dcau_t		normalized_dcau;
    globus_object_t *			err;

    *normalized_source_attr = GLOBUS_NULL;
    *normalized_dest_attr = GLOBUS_NULL;

    normalized_dcau.mode = GLOBUS_FTP_CONTROL_DCAU_NONE;

    if(strncmp(source_url, dest_url, 3) != 0)
    {
	/* Different URL schemes---need to normalize */
	if(source_attr && *source_attr)
	{
	    result = globus_ftp_client_operationattr_copy(
		    normalized_source_attr,
		    source_attr);
	    if(result != GLOBUS_SUCCESS)
	    {
		err = globus_error_get(result);

		goto error_exit;
	    }
	    result = globus_ftp_client_operationattr_set_dcau(
		    normalized_source_attr,
		    &normalized_dcau);
	    if(result != GLOBUS_SUCCESS)
	    {
		err = globus_error_get(result);

		goto destroy_source_attr;
	    }
	}
	if(dest_attr && *dest_attr)
	{
	    result = globus_ftp_client_operationattr_copy(
		    normalized_dest_attr,
		    dest_attr);
	    if(result != GLOBUS_SUCCESS)
	    {
		err = globus_error_get(result);

		goto destroy_source_attr;
	    }
	    result = globus_ftp_client_operationattr_set_dcau(
		    normalized_dest_attr,
		    &normalized_dcau);
	    if(result != GLOBUS_SUCCESS)
	    {
		err = globus_error_get(result);

		goto destroy_dest_attr;
	    }
	}
    }

    if(*normalized_source_attr && *normalized_dest_attr)
    {
	/* Another potentional incompatibility: Mode */
	if((*normalized_source_attr)->mode !=
	   (*normalized_dest_attr)->mode)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("attr");

	    goto destroy_dest_attr;
	}
    }

    return GLOBUS_SUCCESS;

destroy_dest_attr:
    if(*normalized_dest_attr)
    {
	globus_ftp_client_operationattr_destroy(normalized_dest_attr);
    }
destroy_source_attr:
    if(*normalized_source_attr)
    {
	globus_ftp_client_operationattr_destroy(normalized_source_attr);
    }
error_exit:
    return err;
}
/* globus_l_ftp_client_transfer_normalize_attrs() */

#endif
