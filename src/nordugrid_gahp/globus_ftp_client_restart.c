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
 * @file globus_ftp_client_restart.c
 * Globus FTP Client Library Restart Support.
 *
 * $RCSfile: globus_ftp_client_restart.c,v $
 * $Revision: 1.14 $
 * $Date: 2006/10/14 07:21:56 $
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
globus_l_ftp_client_restart_get_callback(
    void *				user_arg);

static
void
globus_l_ftp_client_restart_put_callback(
    void *				user_arg);

static
void
globus_l_ftp_client_restart_transfer_callback(
    void *				user_arg);

static
void
globus_l_ftp_client_restart_no_connection(
    void *			        user_arg);


/**
 * Register the oneshot event which will restart the current transfer 
 * after a delay.
 *
 * @param handle
 *        The handle whose operation we are restarting.
 *
 * @retval GLOBUS_TRUE
 *         The oneshot was successfully registered. 
 * @retval GLOBUS_FALSE
 *         The oneshot was not registered. 
 */
globus_object_t *
globus_i_ftp_client_restart_register_oneshot(
    globus_i_ftp_client_handle_t *		handle)
{
    globus_abstime_t				now;
    globus_reltime_t				when;
    globus_reltime_t				zero;
    globus_result_t                             result = GLOBUS_SUCCESS;
    GlobusFuncName(globus_l_ftp_client_restart_register_oneshot);

    /* Update the restart marker in the handle */
    globus_ftp_client_restart_marker_destroy(&handle->restart_marker);
    globus_ftp_client_restart_marker_copy(&handle->restart_marker,
					  &handle->restart_info->marker);
   
    GlobusTimeReltimeSet(zero, 0, 0);
    GlobusTimeAbstimeGetCurrent(now);
    GlobusTimeAbstimeDiff(when, handle->restart_info->when, now);
    
    if(globus_abstime_cmp(&handle->restart_info->when, &now) < 0)
    {
	GlobusTimeReltimeSet(when, 0, 0);
    }

    if(handle->op == GLOBUS_FTP_CLIENT_GET    ||
       handle->op == GLOBUS_FTP_CLIENT_CHMOD  ||
       handle->op == GLOBUS_FTP_CLIENT_CKSM   ||
       handle->op == GLOBUS_FTP_CLIENT_DELETE ||
       handle->op == GLOBUS_FTP_CLIENT_MDTM   ||
       handle->op == GLOBUS_FTP_CLIENT_SIZE   ||
       handle->op == GLOBUS_FTP_CLIENT_FEAT   ||
       handle->op == GLOBUS_FTP_CLIENT_MKDIR  ||
       handle->op == GLOBUS_FTP_CLIENT_RMDIR  ||
       handle->op == GLOBUS_FTP_CLIENT_CWD    ||
       handle->op == GLOBUS_FTP_CLIENT_MOVE   ||
       handle->op == GLOBUS_FTP_CLIENT_NLST   ||
       handle->op == GLOBUS_FTP_CLIENT_MLSD   ||
       handle->op == GLOBUS_FTP_CLIENT_MLST   ||
       handle->op == GLOBUS_FTP_CLIENT_STAT   ||
       handle->op == GLOBUS_FTP_CLIENT_LIST)
    {
	result = globus_callback_register_oneshot(
	    &handle->restart_info->callback_handle,
	    &when,
	    globus_l_ftp_client_restart_get_callback,
	    handle);
    }
    else if(handle->op == GLOBUS_FTP_CLIENT_PUT)
    {
	result = globus_callback_register_oneshot(
	    &handle->restart_info->callback_handle,
	    &when,
	    globus_l_ftp_client_restart_put_callback,
	    handle);
    }
    else if(handle->op == GLOBUS_FTP_CLIENT_TRANSFER)
    {
	result = globus_callback_register_oneshot(
	    &handle->restart_info->callback_handle,
	    &when,
	    globus_l_ftp_client_restart_transfer_callback,
	    handle);
    }
    else
    {
        globus_assert(0 && "Unexpected operation");
    }
    
    return globus_error_get(result);
}
/* globus_i_ftp_client_restart_register_oneshot() */

/**
 * Process a restart of a get after the pre-restart delay has
 * completed.
 *
 * We need to check to see if the transfer has been aborted before
 * actually processing the restart.
 *
 * @param time_stop
 *        Time when this callback should return. We currently ignore
 *        this, since we do not block in this function.
 * @param user_arg
 *        A void * pointing to the handle which contains the restarted
 *        get information.
 * @return
 *        This function always returns GLOBUS_TRUE, as it always
 *        processed the event which was registered.
 */
static
void
globus_l_ftp_client_restart_get_callback(
    void *				user_arg)
{
    globus_i_ftp_client_handle_t *	handle;
    globus_i_ftp_client_restart_t *	restart_info;
    globus_object_t *			err = GLOBUS_SUCCESS;
    globus_bool_t			registered;

    handle = (globus_i_ftp_client_handle_t *) user_arg;

    globus_assert(! GLOBUS_I_FTP_CLIENT_BAD_MAGIC(&handle));
    globus_i_ftp_client_handle_lock(handle);

    restart_info = handle->restart_info;
    handle->restart_info = GLOBUS_NULL;

    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	/*
	 * We need to process the abort here, since the unregistration
	 * of this callback would have failed.
	 */
	globus_i_ftp_client_plugin_notify_abort(handle);

	goto error_exit;
    }
    else
    {
        globus_i_ftp_client_operationattr_t * attr;
	attr = restart_info->dest_attr;

        /* If stream mode, update the base offset */
	if((!attr) || (attr && attr->mode == GLOBUS_FTP_CONTROL_MODE_STREAM))
        {
	    globus_i_ftp_client_operationattr_t * attr;

	    attr = restart_info->source_attr;

            if(((!attr) || attr->type == GLOBUS_FTP_CONTROL_TYPE_IMAGE) &&
	       handle->restart_marker.stream.offset > handle->base_offset)
	    {
                handle->base_offset = handle->restart_marker.stream.offset;
	    }
	    else if((attr && attr->type == GLOBUS_FTP_CONTROL_TYPE_ASCII) &&
		   handle->restart_marker.stream.ascii_offset >
		       handle->base_offset)
	    {
                handle->base_offset = handle->restart_marker.stream.offset;
	    }
        }
 
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
	err = globus_i_ftp_client_target_find(handle,
					      restart_info->source_url,
					      restart_info->source_attr,
					      &handle->source);
	if(err != GLOBUS_SUCCESS)
	{
	    goto error_exit;
	}
	
	err = globus_i_ftp_client_target_activate(handle,
						  handle->source,
						  &registered);

	if(registered == GLOBUS_FALSE)
	{
	    globus_assert(err ||
			  handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
			  handle->state ==
			  GLOBUS_FTP_CLIENT_HANDLE_RESTART);

	    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	    {
		goto error_exit;
	    }
	    else if (handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	    {
		goto restart;
	    }
	    else if(err != GLOBUS_SUCCESS)
	    {
		goto src_problem_exit;
	    }
	}
    }
    globus_i_ftp_client_restart_info_delete(restart_info);

    globus_i_ftp_client_handle_unlock(handle);

    return;
src_problem_exit:
    globus_i_ftp_client_target_release(handle,
				       handle->source);
error_exit:
    globus_i_ftp_client_restart_info_delete(restart_info);
    
    if(handle->err == GLOBUS_SUCCESS)
    {
	handle->err = globus_object_copy(err);
    
    }

    globus_i_ftp_client_plugin_notify_fault(
	handle,
	0,
	err);
    
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }
    if(handle->state != GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
    }
    globus_i_ftp_client_data_flush(handle);

    /* This function unlocks and potentially frees the client_handle */
    globus_i_ftp_client_transfer_complete(handle);

    return;
restart:
    globus_i_ftp_client_restart_info_delete(restart_info);
    if(handle->source)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->source);
    }

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
    }
    else
    {
	globus_object_free(err);

	globus_i_ftp_client_data_flush(handle);
	
	/* This function unlocks and potentially frees the client_handle */
	globus_i_ftp_client_transfer_complete(handle);
    }
}
/* globus_l_ftp_client_restart_get_callback() */

/**
 * Process a restart of a put after the pre-restart delay has
 * completed.
 *
 * We need to check to see if the transfer has been aborted before
 * actually processing the restart.
 *
 * @param time_stop
 *        Time when this callback should return. We currently ignore
 *        this, since we do not block in this function.
 * @param user_arg
 *        A void * pointing to the handle which contains the restarted
 *        put information.
 * @return
 *        This function always returns GLOBUS_TRUE, as it always
 *        processed the event which was registered.
 */
static
void
globus_l_ftp_client_restart_put_callback(
    void *				user_arg)
{
    globus_i_ftp_client_handle_t *	handle;
    globus_i_ftp_client_restart_t *	restart_info;
    globus_object_t *			err = GLOBUS_SUCCESS;
    globus_bool_t			registered;

    handle = (globus_i_ftp_client_handle_t *) user_arg;

    globus_assert(! GLOBUS_I_FTP_CLIENT_BAD_MAGIC(&handle));
    globus_i_ftp_client_handle_lock(handle);

    restart_info = handle->restart_info;
    handle->restart_info = GLOBUS_NULL;

    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	/*
	 * We need to process the abort here, since the unregistration
	 * of this callback would have failed.
	 */
	globus_i_ftp_client_plugin_notify_abort(handle);

	goto error_exit;
    }
    else
    {
	globus_i_ftp_client_operationattr_t * attr;
	attr = restart_info->dest_attr;

        /* If stream mode, update the base offset */
	if((!attr) || (attr && attr->mode == GLOBUS_FTP_CONTROL_MODE_STREAM))
        {

	    if(((!attr) || attr->type == GLOBUS_FTP_CONTROL_TYPE_IMAGE) &&
	       handle->restart_marker.stream.offset > handle->base_offset)
	    {
	        handle->base_offset = handle->restart_marker.stream.offset;
	    }
            else if((attr && attr->type == GLOBUS_FTP_CONTROL_TYPE_ASCII) &&
	            handle->restart_marker.stream.ascii_offset > handle->base_offset)
	    {
	        handle->base_offset = handle->restart_marker.stream.ascii_offset;
	    }
        }
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
	err = globus_i_ftp_client_target_find(handle,
					      restart_info->dest_url,
					      restart_info->dest_attr,
					      &handle->dest);
	if(err != GLOBUS_SUCCESS)
	{
	    goto error_exit;
	}
	err = globus_i_ftp_client_target_activate(handle,
						  handle->dest,
						  &registered);

	if(registered == GLOBUS_FALSE)
	{
	    globus_assert(err ||
			  handle->state ==
			  GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
			  handle->state ==
			  GLOBUS_FTP_CLIENT_HANDLE_RESTART);

	    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	    {
		goto dest_problem_exit;
	    }
	    else if (handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	    {
		goto restart;
	    }
	    else if(err != GLOBUS_SUCCESS)
	    {
		goto error_exit;
	    }
	}
    }
    globus_i_ftp_client_restart_info_delete(restart_info);

    globus_i_ftp_client_handle_unlock(handle);

    return;

dest_problem_exit:
    globus_i_ftp_client_target_release(handle,
				       handle->dest);
error_exit:
    globus_i_ftp_client_restart_info_delete(restart_info);
    if(handle->err == GLOBUS_SUCCESS)
    {
	handle->err = globus_object_copy(err);
    }

    globus_i_ftp_client_plugin_notify_fault(
	handle,
	0,
	err);
    
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }
    if(handle->state != GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
    }
    globus_i_ftp_client_data_flush(handle);

    /* This function unlocks and potentially frees the client_handle */
    globus_i_ftp_client_transfer_complete(handle);

    return;

restart:
    globus_i_ftp_client_restart_info_delete(restart_info);
    if(handle->dest)
    {
	globus_i_ftp_client_target_release(handle,
					   handle->dest);
    }

    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
    }
    else
    {
	globus_object_free(err);
	globus_i_ftp_client_data_flush(handle);
	
	/* This function unlocks and potentially frees the client_handle */
	globus_i_ftp_client_transfer_complete(handle);
    }
}
/* globus_l_ftp_client_restart_put_callback() */

/**
 * Process a restart of a transfer after the pre-restart delay has
 * completed.
 *
 * We need to check to see if the transfer has been aborted before
 * actually processing the restart.
 *
 * @param time_stop
 *        Time when this callback should return. We currently ignore
 *        this, since we do not block in this function.
 * @param user_arg
 *        A void * pointing to the handle which contains the restarted
 *        transfer information.
 * @return
 *        This function always returns GLOBUS_TRUE, as it always
 *        processed the event which was registered.
 */
static
void
globus_l_ftp_client_restart_transfer_callback(
    void *				user_arg)
{
    globus_i_ftp_client_handle_t *	handle;
    globus_i_ftp_client_restart_t *	restart_info;
    globus_object_t *			err = GLOBUS_SUCCESS;
    globus_bool_t			registered;

    handle = (globus_i_ftp_client_handle_t *) user_arg;

    globus_assert(! GLOBUS_I_FTP_CLIENT_BAD_MAGIC(&handle));
    globus_i_ftp_client_handle_lock(handle);

    restart_info = handle->restart_info;
    handle->restart_info = GLOBUS_NULL;

    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	/*
	 * We need to process the abort here, since the unregistration
	 * of this callback would have failed.
	 */
	globus_i_ftp_client_plugin_notify_abort(handle);

	goto error_exit;
    }
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    err = globus_i_ftp_client_target_find(handle,
					  restart_info->source_url,
					  restart_info->source_attr,
					  &handle->source);
    if(err != GLOBUS_SUCCESS)
    {
	if(handle->err == GLOBUS_SUCCESS)
	{
	    handle->err = globus_object_copy(err);
	}
	    
	globus_i_ftp_client_plugin_notify_fault(
	    handle,
	    0,
	    err);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	goto error_exit;
    }

    err = globus_i_ftp_client_target_find(handle,
					  restart_info->dest_url,
					  restart_info->dest_attr,
					  &handle->dest);
    if(err != GLOBUS_SUCCESS)
    {
	if(handle->err == GLOBUS_SUCCESS)
	{
	    handle->err = globus_object_copy(err);
	}
	    
	globus_i_ftp_client_plugin_notify_fault(
	    handle,
	    0,
	    err);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	goto src_problem_exit;
    }
	
    err = globus_i_ftp_client_target_activate(handle,
					      handle->dest,
					      &registered);

    if(registered == GLOBUS_FALSE)
    {
	globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		      handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART);

	if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
	{
	    goto error_exit;
	}
	else if (handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    goto restart;
	}
	else if(err != GLOBUS_SUCCESS)
	{
	    goto dest_problem_exit;
	}
    }

    globus_i_ftp_client_restart_info_delete(restart_info);

    globus_i_ftp_client_handle_unlock(handle);

    return;

dest_problem_exit:
    globus_i_ftp_client_target_release(handle,
				       handle->dest);
src_problem_exit:
    globus_i_ftp_client_target_release(handle,
				       handle->source);
error_exit:
    globus_i_ftp_client_restart_info_delete(restart_info);

    if(handle->err == GLOBUS_SUCCESS)
    {
	handle->err = globus_object_copy(err);
    }
    if(handle->state != GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	globus_i_ftp_client_plugin_notify_fault(
	    handle,
	    0,
	    err);
    }

    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }
    if(handle->state != GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
    }

    /* This function unlocks and potentially frees the client_handle */
    globus_i_ftp_client_transfer_complete(handle);

    return;
restart:
    globus_i_ftp_client_restart_info_delete(restart_info);
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
    globus_i_ftp_client_handle_unlock(handle);
    
    err = globus_i_ftp_client_restart_register_oneshot(handle);

    if(!err)
    {
	globus_i_ftp_client_handle_unlock(handle);
    }
    else
    {
	globus_object_free(err);
	/* This function unlocks and potentially frees the client_handle */
	globus_i_ftp_client_transfer_complete(handle);
    }
}
/* globus_l_ftp_client_restart_transfer_callback() */

/**
 * Start the restart process in the handle.
 *
 * This is called by the individual restart functions (get, put, and
 * transfer) after all of the parameters have been parsed and the
 * restart info is all set up to go.
 *
 * We shut down the current connections. When the callbacks for that
 * happens, we will pick up and actually restart the transfer.
 *
 * The handle is locked when this function is called, and when it
 * returns.
 *
 * @param handle
 *        The handle being restarted.
 * @param restart_info
 *        Information on the new parameters for the restart.
 */
globus_object_t *
globus_i_ftp_client_restart(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_restart_t *		restart_info)
{
    globus_object_t *				err = GLOBUS_SUCCESS;
    globus_result_t				result;
    GlobusFuncName(globus_i_ftp_client_restart);

    switch(handle->state)
    {
    case GLOBUS_FTP_CLIENT_HANDLE_START:
	handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
	handle->restart_info = restart_info;

	globus_i_ftp_client_plugin_notify_restart(handle);
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
	    handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
	    handle->restart_info = restart_info;
	    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

	    globus_i_ftp_client_plugin_notify_restart(handle);
	}
	else
	{
	    err = globus_error_get(result);
	}
	
	break;
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT:
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION:
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET:
    case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST:
	if(handle->op == GLOBUS_FTP_CLIENT_GET    ||
	   handle->op == GLOBUS_FTP_CLIENT_CHMOD  ||
	   handle->op == GLOBUS_FTP_CLIENT_CKSM   ||
	   handle->op == GLOBUS_FTP_CLIENT_DELETE ||
	   handle->op == GLOBUS_FTP_CLIENT_FEAT   ||
	   handle->op == GLOBUS_FTP_CLIENT_MKDIR  ||
	   handle->op == GLOBUS_FTP_CLIENT_RMDIR  ||
	   handle->op == GLOBUS_FTP_CLIENT_CWD    ||
	   handle->op == GLOBUS_FTP_CLIENT_MOVE   ||
	   handle->op == GLOBUS_FTP_CLIENT_NLST   ||
	   handle->op == GLOBUS_FTP_CLIENT_MLSD   ||
	   handle->op == GLOBUS_FTP_CLIENT_MLST   ||
	   handle->op == GLOBUS_FTP_CLIENT_STAT   ||
	   handle->op == GLOBUS_FTP_CLIENT_LIST   ||
	   handle->op == GLOBUS_FTP_CLIENT_SIZE   ||
	   handle->op == GLOBUS_FTP_CLIENT_MDTM
	   )
	{
	    result = globus_ftp_control_force_close(
		handle->source->control_handle,
		globus_i_ftp_client_force_close_callback,
		handle->source);

	    if(result == GLOBUS_SUCCESS)
	    {
		handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
		handle->restart_info = restart_info;
		handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

		globus_i_ftp_client_plugin_notify_restart(handle);
	    }
	    else if(handle->source->state == GLOBUS_FTP_CLIENT_TARGET_CONNECT)
	    {
		globus_result_t             rc;
		err = globus_error_get(result);

		handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
		handle->restart_info = restart_info;
		handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

		globus_i_ftp_client_plugin_notify_restart(handle);

		rc = globus_callback_register_oneshot(
			GLOBUS_NULL,
			&globus_i_reltime_zero,
			globus_l_ftp_client_restart_no_connection,
			handle->source);

		if(rc == GLOBUS_SUCCESS)
		{
		    globus_object_free(err);
		    err = GLOBUS_NULL;
		}
	    }
	    else
	    {
		err = globus_error_get(result);
	    }
	}
	else
	{
	    globus_assert(handle->op == GLOBUS_FTP_CLIENT_TRANSFER);

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
		    handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
		    handle->restart_info = restart_info;
	    
		    globus_i_ftp_client_plugin_notify_restart(handle);
		}
		else
		{
		    handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		    handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_CLOSED;
		    handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
		    handle->restart_info = restart_info;
	    
		    globus_i_ftp_client_plugin_notify_restart(handle);
		}
	    }
	    else
	    {
		err = globus_error_get(result);
	    }
	}
	break;
    case GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER:
    case GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE:
	globus_assert(handle->op == GLOBUS_FTP_CLIENT_TRANSFER);

	handle->state = GLOBUS_FTP_CLIENT_HANDLE_RESTART;
	handle->restart_info = restart_info;
	handle->source->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
	handle->dest->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

	globus_i_ftp_client_plugin_notify_restart(handle);
	
	globus_ftp_control_force_close(handle->source->control_handle,
				       globus_i_ftp_client_force_close_callback,
				       handle->source);
	    
	globus_ftp_control_force_close(handle->dest->control_handle,
				       globus_i_ftp_client_force_close_callback,
				       handle->dest);
	break;
    case GLOBUS_FTP_CLIENT_HANDLE_FINALIZE:
    case GLOBUS_FTP_CLIENT_HANDLE_ABORT:
    case GLOBUS_FTP_CLIENT_HANDLE_FAILURE:
    case GLOBUS_FTP_CLIENT_HANDLE_RESTART:
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_NOT_IN_USE("handle");
	break;
    }
    return err;
}
/* globus_i_ftp_client_restart() */

static
void
globus_l_ftp_client_restart_no_connection(
    void *			        user_arg)
{
    globus_i_ftp_client_target_t *		target;

    target = user_arg;

    globus_i_ftp_client_force_close_callback(
	    target,
	    target->control_handle,
	    GLOBUS_NULL,
	    GLOBUS_NULL);
}

#endif
