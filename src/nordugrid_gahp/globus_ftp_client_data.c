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
 * @file globus_ftp_client_data.c Read/Write functions
 *
 * $RCSfile: globus_ftp_client_data.c,v $
 * $Revision: 1.24 $
 * $Date: 2008/04/04 01:51:47 $
 */
#endif

#include "globus_i_ftp_client.h"

#include <string.h>

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * Data callback information structure.
 *
 * This structure is used to hold the data associated with a read or
 * write operation. This structure will be stored in one of the
 * stalled_blocks or active_blocks members of the control handle.
 *
 */
typedef struct
{
    /** The data buffer to be sent. */
    globus_byte_t *				buffer;
    /** The size of the buffer. */
    globus_size_t				buffer_length;
    /** The offset of the data block with respect to the entire file. */
    globus_off_t				offset;
    /** Is this block the end-of-file? */
    globus_bool_t				eof;
    /** The user's callback. */
    globus_ftp_client_data_callback_t		callback;
    /** The user-supplied parameter to the callback. */
    void *					callback_arg;
    /** handle this buffer is associated with (used for faking callbacks) */
    globus_i_ftp_client_handle_t *		i_handle;
}
globus_l_ftp_client_data_t;

static
void
globus_l_ftp_client_data_callback(
    void *					user_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_byte_t *				buffer,
    globus_size_t				length,
    globus_off_t				offset,
    globus_bool_t				eof);

static
void
globus_l_ftp_client_read_all_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               bytes_read,
    globus_off_t                                offset_read,
    globus_bool_t                               eof);

static
globus_l_ftp_client_data_t *
globus_l_ftp_client_data_new(
    globus_byte_t *				buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof,
    globus_ftp_client_data_callback_t		callback,
    void *					callback_arg);

static
void
globus_l_ftp_client_data_delete(
    globus_l_ftp_client_data_t *		data);


static
globus_size_t
globus_l_ftp_client_count_lf(const globus_byte_t * buf,
			     globus_size_t length);
#endif

/**
 * Register a data buffer to handle a part of the FTP data transfer.
 * @ingroup globus_ftp_client_data
 *
 * The data buffer will be associated with the current get being
 * performed on this client handle. 
 *
 * @param handle
 *        A pointer to a FTP Client handle which contains state information
 *        about the get operation.
 * @param buffer
 *        A user-supplied buffer into which data from the FTP server
 *        will be stored.
 * @param buffer_length
 *        The maximum amount of data that can be stored into the buffer.
 * @param callback
 *        The function to be called once the data has been read.
 * @param callback_arg
 *        Argument passed to the callback function
 *
 * @see globus_ftp_client_operationattr_set_read_all()
 */
globus_result_t
globus_ftp_client_register_read(
    globus_ftp_client_handle_t *		handle,
    globus_byte_t *				buffer,
    globus_size_t				buffer_length,
    globus_ftp_client_data_callback_t		callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_l_ftp_client_data_t *		data;
    globus_result_t				result;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_register_read);

    globus_i_ftp_client_debug_printf(3, 
        (stderr, "globus_ftp_client_register_read() entering\n"));

    if(handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error;
    }
    if(buffer == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("buffer");

	goto error;
    }
    if(callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("callback");

	goto error;
    }

    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error;
    }
    i_handle = *handle;

    globus_i_ftp_client_handle_lock(i_handle);

    if(i_handle->op != GLOBUS_FTP_CLIENT_GET  &&
       i_handle->op != GLOBUS_FTP_CLIENT_LIST &&
       i_handle->op != GLOBUS_FTP_CLIENT_NLST &&
       i_handle->op != GLOBUS_FTP_CLIENT_MLSD )
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_OPERATION(i_handle->op);

	goto unlock_error;
    }
    if(((i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
	i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
	i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE) &&
       !(i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA ||
        i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK) &&
        !(i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_RETR ||
         i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_LIST ||
         i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_GET)) ||
        i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FINALIZE)
    {
	/* We've already hit EOF on the data channel. We'll just
	 * return that information to the user.
	 */

	err = GLOBUS_I_FTP_CLIENT_ERROR_EOF();

	goto unlock_error;
    }

    data = globus_l_ftp_client_data_new(buffer,
					buffer_length,
					0,
					GLOBUS_FALSE,
					callback,
					callback_arg);
    if(data == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto unlock_error;
    }
    if((i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
        i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST) &&
        (i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA ||
        i_handle->source->state == GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK)
        && globus_priority_q_empty(&i_handle->stalled_blocks))
    {
	/*
	 * The data block can be sent to the control library immediately if there is
	 * nothing left in the stalled queue and we're in the proper state.
	 */
	globus_hashtable_insert(&i_handle->active_blocks,
				data->buffer,
				data);
	i_handle->num_active_blocks++;
	
	globus_i_ftp_client_plugin_notify_read(i_handle,
					       data->buffer,
					       data->buffer_length);

        result = globus_ftp_control_data_read(
            i_handle->source->control_handle,
            data->buffer,
            data->buffer_length,
            globus_l_ftp_client_data_callback,
            i_handle);

        if(result != GLOBUS_SUCCESS)
        {
            err = globus_error_get(result);

            globus_hashtable_remove(&i_handle->active_blocks,
                                    buffer);
            i_handle->num_active_blocks--;
            globus_l_ftp_client_data_delete(data);
            
            /* 
             * Our block may have caused the "complete" callback to
             * happen, as the data callback may have assumed that our
             * data block was successfully registered
             */
            if(i_handle->num_active_blocks == 0 &&
               (i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
                i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
                i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE))
            {
                if(i_handle->source->state == 
                    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE)
                {
                    /* We are finished, kick out a complete callback */
    
                    globus_reltime_t                         reltime;
                    
                    i_handle->source->state = 
                        GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION;
                    
                    GlobusTimeReltimeSet(reltime, 0, 0);
                    globus_callback_register_oneshot(
                        GLOBUS_NULL,
                        &reltime,
                        globus_l_ftp_client_complete_kickout,
                        (void *) i_handle);
                }
                else if(i_handle->source->state == 
                    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE)
                {
                    i_handle->source->state =
                        GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
                }
            }

            goto unlock_error;
        }
    }
    else
    {
	/* 
	 * Otherwise, enqueue the data in the block priority queue to
	 * be processed when the data channel is ready for it
	 */
	globus_priority_q_enqueue(&i_handle->stalled_blocks,
				  data,
				  &data->offset);
    }
    
    globus_i_ftp_client_handle_unlock(i_handle);
    
    globus_i_ftp_client_debug_printf(3, 
        (stderr, "globus_ftp_client_register_read() exiting\n"));

    return GLOBUS_SUCCESS;

unlock_error:
    globus_i_ftp_client_handle_unlock(i_handle);
error:
    globus_i_ftp_client_debug_printf(3, 
        (stderr, "globus_ftp_client_register_read() exiting with error\n"));

    return globus_error_put(err);
}
/* globus_ftp_client_register_read() */

/**
 * Register a data buffer to handle a part of the FTP data transfer.
 * @ingroup globus_ftp_client_data
 *
 * The data buffer will be associated with the current "put" being
 * performed on this client handle. Multiple data buffers may be
 * registered on a handle at once. There is no guaranteed ordering of
 * the data callbacks in extended block mode.
 *
 * @param handle
 *        A pointer to a FTP Client handle which contains state information
 *        about the put operation.
 * @param buffer
 *        A user-supplied buffer containing the data to write to the server.
 * @param buffer_length
 *        The size of the buffer to be written.
 * @param offset
 *        The offset of the buffer to be written. In extended-block
 *        mode, the data does not need to be sent in order. In stream
 *	  mode (the default), data must be sent in sequential
 *	  order. The behavior is undefined if multiple writes overlap.
 * @param callback
 *        The function to be called once the data has been written.
 * @param callback_arg
 *        Argument passed to the callback function
 */
globus_result_t
globus_ftp_client_register_write(
    globus_ftp_client_handle_t *		handle,
    globus_byte_t *				buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof,
    globus_ftp_client_data_callback_t		callback,
    void *					callback_arg)
{
    globus_object_t *				err;
    globus_l_ftp_client_data_t *		data;
    globus_result_t				result;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_register_write);
    
    globus_i_ftp_client_debug_printf(3, 
        (stderr, "globus_ftp_client_register_write() entering\n"));

    if(handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error;
    }
    if(buffer == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("buffer");

	goto error;
    }
    if(callback == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("callback");

	goto error;
    }

    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");
	
	goto error;
    }
    i_handle = *(globus_i_ftp_client_handle_t **) handle;

    globus_i_ftp_client_handle_lock(i_handle);

    if(i_handle->op != GLOBUS_FTP_CLIENT_PUT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_OPERATION(i_handle->op);

	goto unlock_error;
    }

    if((i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO &&
       !(i_handle->dest->state == GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA ||
        i_handle->dest->state == GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK) &&
        i_handle->dest->state != GLOBUS_FTP_CLIENT_TARGET_STOR &&
        i_handle->dest->state != GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_PUT) ||
        i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FINALIZE)
    {
	/* We've already sent EOF. We'll just return that information
	 * to the user. 
	 */

	err = GLOBUS_I_FTP_CLIENT_ERROR_EOF();

	goto unlock_error;
    }

    if(i_handle->partial_offset != -1)
    {
	offset -= i_handle->partial_offset;
    }

    data = globus_l_ftp_client_data_new(buffer,
					buffer_length,
					offset,
					eof,
					callback,
					callback_arg);
    if(data == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto unlock_error;
    }

    if(i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO &&
        i_handle->dest->state == GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA &&
        globus_priority_q_empty(&i_handle->stalled_blocks))
    {
	/*
	 * The data block can be processed immediately if there is
	 * nothing left in the stalled queue and we're in the proper state.
	 */
	globus_hashtable_insert(&i_handle->active_blocks,
				data->buffer,
				data);
	i_handle->num_active_blocks++;
	
	globus_i_ftp_client_plugin_notify_write(i_handle,
						data->buffer,
						data->buffer_length,
						data->offset,
						data->eof);
	/* We don't check for state changes here. Because we've
	 * already added this block to the hashtable and num_blocks,
	 * we don't need to, because the transfer can't finish without
	 * us.
	 */
	result = globus_ftp_control_data_write(
	    i_handle->dest->control_handle,
	    data->buffer,
	    data->buffer_length,
	    data->offset,
	    data->eof,
	    globus_l_ftp_client_data_callback,
	    i_handle);

	if(result != GLOBUS_SUCCESS)
	{
	    err = globus_error_get(result);

	    globus_hashtable_remove(&i_handle->active_blocks,
				    buffer);
	    i_handle->num_active_blocks--;
	    globus_l_ftp_client_data_delete(data);
	    
	    /* 
	     * Our block may have caused the "complete" callback to
	     * happen, as the data callback may have assumed that our
	     * data block was successfully registered
	     */
	    if(i_handle->num_active_blocks == 0 &&
	       (i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
		i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE))
            {
                if(i_handle->dest->state == 
                    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE)
                {
                    /* We are finished, kick out a complete callback */
    
                    globus_reltime_t                         reltime;
                    
                    i_handle->dest->state = 
                        GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION;
                        
                    GlobusTimeReltimeSet(reltime, 0, 0);
                    globus_callback_register_oneshot(
                        GLOBUS_NULL,
                        &reltime,
                        globus_l_ftp_client_complete_kickout,
                        (void *) i_handle);
                }
                else if(i_handle->dest->state == 
                    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE)
                {
                    i_handle->dest->state =
                        GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
                }
            }

	    goto unlock_error;
    	}
    }
    else
    {
	/* 
	 * Otherwise, enqueue the data in the block priority queue to
	 * be processed when the data channel is ready for it
	 */
	globus_priority_q_enqueue(&i_handle->stalled_blocks,
				  data,
				  &data->offset);

    }
    
    globus_i_ftp_client_handle_unlock(i_handle);
    
    globus_i_ftp_client_debug_printf(3, 
        (stderr, "globus_ftp_client_register_write() exiting\n"));

    return GLOBUS_SUCCESS;

unlock_error:
    globus_i_ftp_client_handle_unlock(i_handle);
error:
    globus_i_ftp_client_debug_printf(3, 
        (stderr, "globus_ftp_client_register_write() exiting with error\n"));

    return globus_error_put(err);
}
/* globus_ftp_client_register_write() */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/*-----------------------------------------------------------------------------
 * Local/Internal Functions.
 *-----------------------------------------------------------------------------
 */
/**
 * Priority queue comparison function.
 *
 * This function is called to decide where in the stalled_blocks
 * priority queue to insert a data block.
 *
 * @param priority_1
 * @param priority_2
 */
int
globus_i_ftp_client_data_cmp(
    void *					priority_1,
    void *					priority_2)
{
    globus_off_t *				offset1;
    globus_off_t *				offset2;

    offset1 = (globus_off_t *) priority_1;
    offset2 = (globus_off_t *) priority_2;

    return ((*offset1) > (*offset2));
}
/* globus_i_ftp_client_data_cmp() */

/**
 * Allocation and initialize a globus_l_ftp_client_data_t.
 *
 * @param buffer
 * @param buffer_length
 * @param offset
 * @param eof
 * @param callback
 *        User-supplied callback function.
 * @param callback_arg
 *        User-supplied callback argument.
 */
static
globus_l_ftp_client_data_t *
globus_l_ftp_client_data_new(
    globus_byte_t *				buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof,
    globus_ftp_client_data_callback_t		callback,
    void *					callback_arg)
{
    globus_l_ftp_client_data_t *		d;

    d = globus_libc_malloc(sizeof(globus_l_ftp_client_data_t));

    if(!d)
    {
	return GLOBUS_NULL;
    }
    d->buffer		= buffer;
    d->buffer_length	= buffer_length;
    d->offset		= offset;
    d->callback		= callback;
    d->callback_arg	= callback_arg;
    d->eof		= eof;

    return d;
}
/* globus_l_ftp_client_data_new() */

/**
 * Delete a globus_l_ftp_client_data_t.
 *
 * @param data
 *        The object to delete.
 */
static
void
globus_l_ftp_client_data_delete(
    globus_l_ftp_client_data_t *		data)
{
    globus_libc_free(data);
}
/* globus_l_ftp_client_data_delete() */

/**
 * Internal callback function to handle a data callback from the
 * control library. This function notifies the plugins that the data
 * block has been handled, and then calls the user callback
 * associated with this data block.
 *
 * If any error occurs during data block handling in the control
 * library, the data channels will be destroyed, so we should consider
 * any error passed to this function as moving the handle into the
 * failure state.
 *
 */
static
void
globus_l_ftp_client_data_callback(
    void *					user_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_byte_t *				buffer,
    globus_size_t				length,
    globus_off_t				offset,
    globus_bool_t				eof)
{
    globus_i_ftp_client_handle_t *		client_handle;
    globus_l_ftp_client_data_t *		data;
    globus_bool_t				dispatch_final = GLOBUS_FALSE;
    globus_i_ftp_client_target_t *		target;
    globus_i_ftp_client_target_t **		ptarget;
    globus_off_t				user_offset;
    globus_bool_t                               user_eof;
    
    globus_i_ftp_client_debug_printf(3, (stderr, 
        "globus_l_ftp_client_data_callback() entering\n"));
    
    client_handle = (globus_i_ftp_client_handle_t *) user_arg;

    globus_assert(!GLOBUS_I_FTP_CLIENT_BAD_MAGIC((&client_handle)));

    globus_i_ftp_client_handle_lock(client_handle);
    
    if(client_handle->op == GLOBUS_FTP_CLIENT_GET  ||
       client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
       client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
       client_handle->op == GLOBUS_FTP_CLIENT_MLSD)
    {
	ptarget = &client_handle->source;
    }
    else
    {
        ptarget = &client_handle->dest;
    }
    
    target = *ptarget;
    
    globus_i_ftp_client_debug_states(4, client_handle);

    if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
    {
        offset += client_handle->base_offset;
    }
    
    user_offset = (client_handle->partial_offset != -1)
        ? offset + client_handle->partial_offset
        : offset;

    /* Don't update the stream_offset on 0 length buffers; these may come in
     * with an offset of 0 when an error occurs or during restart,
     * which will clobber what we've already seen
     */
    if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM && length != 0)
    {
	(void) globus_ftp_client_restart_marker_set_ascii_offset(
	    &client_handle->restart_marker,
	    offset+length,
	    offset+length+globus_l_ftp_client_count_lf(buffer, length));
    }
    else if(target->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
            length != 0)
    {
	(void) globus_ftp_client_restart_marker_insert_range(
	    &client_handle->restart_marker,
	    offset,
	    offset+length);
    }
 
    /* Notify plugins that this data block has been processed. The plugin
     * gets the same (adjusted) view of the offset that the user gets. */
    globus_i_ftp_client_plugin_notify_data(client_handle,
					   error,
					   buffer,
					   length,
					   user_offset,
					   eof);

    data = (globus_l_ftp_client_data_t *) 
	globus_hashtable_remove(&client_handle->active_blocks,
				buffer);

    globus_assert(data);

    /*
     * Figure out if this was a fatal error. 
     * The plugins had a chance to restart when we notified them of the failure.
     */
    if(error && 
       (client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
	client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO))
    {
	if(!client_handle->err)
	{
	    client_handle->err = globus_object_copy(error);
	}
	client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
    }
    
    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART &&
        (client_handle->op == GLOBUS_FTP_CLIENT_GET  ||
        client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
        client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
        client_handle->op == GLOBUS_FTP_CLIENT_MLSD ))
    {
        user_eof = GLOBUS_FALSE;
    }
    else
    {
        user_eof = eof;
    }
    
    /* Call back to the user */
    globus_i_ftp_client_handle_unlock(client_handle);

    data->callback(
        data->callback_arg,
        client_handle->handle,
        error,
        buffer,
        length,
        user_offset,
        user_eof);
    
    globus_l_ftp_client_data_delete(data);
    
    globus_i_ftp_client_handle_lock(client_handle);
    
    /* gave up lock, target could have gone bad, reload */
    target = *ptarget;
    
    /* 
     * Figure out if we need to call the completion function
     * once this block's callback has been done
     */
    client_handle->num_active_blocks--;
    
    /* if eof now, or eof was received before */
    if(eof || 
        (target && (target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE ||
            target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE ||
            target->state == GLOBUS_FTP_CLIENT_TARGET_FAULT)))
    {
        if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
        {
            if(target->state == GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA ||
                target->state == 
                    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE)
            {
                if(client_handle->num_active_blocks == 0)
                {
                    /*
                     * This is the last data block, and the response to the RETR,
                     * ERET, STOR, or ESTO operation has not been received, 
                     * change state to prevent any subsequent data blocks being
                     * queued.
                     */
                    target->state =
                        GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
                }
                else
                {
                    target->state = 
                        GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE;
                }
            }
            else if(target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK ||
                target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE ||
                target->state == GLOBUS_FTP_CLIENT_TARGET_FAULT)
            {
                /* 
                 * If this is the last data block, and the response to the RETR,
                 * ERET, STOR, or ESTO operation has been received, then we
                 * must clean up after dispatching this callback.
                 */
                if(client_handle->num_active_blocks == 0)
                {
                    dispatch_final = GLOBUS_TRUE;
                    target->state = GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION;
                }
                else if(target->state != GLOBUS_FTP_CLIENT_TARGET_FAULT)
                {
                    target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE;
                }
            }
        }
    }
    
    if(dispatch_final)
    {
	/*
	 * This is the end of the transfer. We call into the transfer
	 * code to clean up things and call back to the user.
	 *
	 * This function is a bit weird in that you call it locked,
	 * and it returns unlocked.
	 */
	globus_i_ftp_client_transfer_complete(client_handle);
    }
    else
    {
        globus_i_ftp_client_handle_unlock(client_handle);
    }
    
    globus_i_ftp_client_debug_printf(3, (stderr, 
        "globus_l_ftp_client_data_callback() exiting\n"));
    globus_i_ftp_client_debug_states(4, client_handle);

    return;
}
/* globus_l_ftp_client_data_callback() */


/**
 * Internal callback function to handle a read_all data callback from the
 * control library. This function notifies the plugins that the data
 * block has been handled, and then calls the user callback
 * associated with this data block.
 *
 * When processing a read_all, intermediate callbacks may be issued if the 
 * user has requested it by setting the callback for the read_all attribute.
 *
 * If any error occurs during data block handling in the control
 * library, the data channels will be destroyed, so we should consider
 * any error passed to this function as moving the handle into the
 * failure state.
 *
 */
static
void
globus_l_ftp_client_read_all_callback(
    void *                                      callback_arg,
    globus_ftp_control_handle_t *               handle,
    globus_object_t *                           error,
    globus_byte_t *                             buffer,
    globus_size_t                               bytes_read,
    globus_off_t                                offset_read,
    globus_bool_t                               eof)
{
    globus_i_ftp_client_handle_t *		client_handle;
    globus_l_ftp_client_data_t *		data;
    globus_bool_t				dispatch_final = GLOBUS_FALSE;
    globus_i_ftp_client_target_t *		target;
    globus_ftp_client_data_callback_t		read_all_callback = GLOBUS_NULL;
    void *					read_all_callback_arg = GLOBUS_NULL;
    globus_off_t				total_bytes_read;
    globus_off_t				user_offset;

    client_handle = (globus_i_ftp_client_handle_t *) callback_arg;

    globus_assert(!GLOBUS_I_FTP_CLIENT_BAD_MAGIC((&client_handle)));

    globus_i_ftp_client_handle_lock(client_handle);

    target = client_handle->source;

    globus_assert(client_handle->op == GLOBUS_FTP_CLIENT_GET  ||
	          client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
              client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
	          client_handle->op == GLOBUS_FTP_CLIENT_MLSD );
    
    globus_i_ftp_client_debug_printf(3, (stderr, 
        "globus_l_ftp_client_read_all_callback() entering\n"));
    globus_i_ftp_client_debug_states(4, client_handle);

    if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
    {
        offset_read += client_handle->base_offset;
    }
    
    if(bytes_read > 0 &&
       offset_read + bytes_read > client_handle->read_all_biggest_offset)
    {
	client_handle->read_all_biggest_offset = offset_read + bytes_read;
    }
    
    user_offset =  (client_handle->partial_offset == -1)
	    ? offset_read
	    : offset_read + client_handle->partial_offset;

    /* Notify plugins that this data block has been processed. */
    globus_i_ftp_client_plugin_notify_data(client_handle,
					   error,
					   buffer + offset_read,
					   bytes_read,
					   user_offset,
					   eof);

    if(!eof)
    {
	data = (globus_l_ftp_client_data_t *) 
	    globus_hashtable_lookup(&client_handle->active_blocks,
				    buffer);
    }
    else
    {
	data = (globus_l_ftp_client_data_t *) 
	    globus_hashtable_remove(&client_handle->active_blocks,
				    buffer);
    }

    globus_assert(data);

    /*
     * Figure out if this was a fatal error. 
     * The plugins had a chance to restart when we notified them of the failure.
     */
    if(error && 
       (client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
	client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO))
    {
	if(!client_handle->err)
	{
	    client_handle->err = globus_object_copy(error);
	}
	client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
    }

    /* 
     * Figure out if we need to call the completion function
     * once this block's callback has been done
     */
     
    if(eof)
    {
        client_handle->num_active_blocks--;

        if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
        {
            if(target->state == GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA ||
                target->state == 
                    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE)
            {
                if(client_handle->num_active_blocks == 0)
                {
                    /*
                     * This is the last data block, and the response to the RETR,
                     * ERET, STOR, or ESTO operation has not been received, 
                     * change state to prevent any subsequent data blocks being
                     * queued.
                     */
                    target->state =
                        GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
                }
                else
                {
                    target->state = 
                        GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE;
                }
            }
            else if(target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK ||
                target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE ||
                target->state == GLOBUS_FTP_CLIENT_TARGET_FAULT)
            {
                /* 
                 * If this is the last data block, and the response to the RETR,
                 * ERET, STOR, or ESTO operation has been received, then we
                 * must clean up after dispatching this callback.
                 */
                if(client_handle->num_active_blocks == 0)
                {
                    dispatch_final = GLOBUS_TRUE;
                    target->state = GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION;
                }
                else if(target->state != GLOBUS_FTP_CLIENT_TARGET_FAULT)
                {
                    target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE;
                }
            }
        }
    }

    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	if(client_handle->op == GLOBUS_FTP_CLIENT_GET  ||
	   client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
       client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
	   client_handle->op == GLOBUS_FTP_CLIENT_MLSD)
	{
	    eof = GLOBUS_FALSE;
	    dispatch_final = GLOBUS_FALSE;
	}
    }

    globus_assert(
	client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET
	|| client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO 
	|| client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST
	|| client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE
	|| client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT 
	|| client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART);
    /* Don't update the stream_offset on 0 length buffers; these may come in
     * with an offset of 0 when an error occurs or during restart,
     * which will clobber what we've already seen
     */
    if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM && bytes_read != 0)
    {
	client_handle->restart_marker.stream.offset = bytes_read;
	client_handle->restart_marker.stream.ascii_offset = 
	    bytes_read + 
	    globus_l_ftp_client_count_lf(buffer + offset_read, bytes_read);
    }

    if(target->attr->read_all)
    {
	read_all_callback = target->attr->read_all_intermediate_callback;
	read_all_callback_arg =
	    target->attr->read_all_intermediate_callback_arg;
    }
    
    total_bytes_read = client_handle->read_all_biggest_offset;

    globus_i_ftp_client_handle_unlock(client_handle);

    if(read_all_callback)
    {
	(*read_all_callback)(read_all_callback_arg,
			     client_handle->handle,
			     error,
			     buffer + offset_read,
			     bytes_read,
			     user_offset,
			     eof);
    }
    
    user_offset = (client_handle->partial_offset == -1)
        ? 0
        : client_handle->partial_offset;
        
    if(eof)
    {
        data->callback(
            data->callback_arg,
            client_handle->handle,
            error,
            buffer,
            total_bytes_read,
            user_offset,
            eof);
        globus_l_ftp_client_data_delete(data);
    }
    
    if(dispatch_final)
    {
	/*
	 * This is the end of the transfer. We call into the transfer
	 * code to clean up things and call back to the user.
	 *
	 * This function is a bit weird in that you call it locked,
	 * and it returns unlocked.
	 */
	globus_i_ftp_client_handle_lock(client_handle);
	globus_i_ftp_client_transfer_complete(client_handle);
    }
    globus_i_ftp_client_debug_printf(3, (stderr, 
        "globus_l_ftp_client_read_all_callback() exiting\n"));
    globus_i_ftp_client_debug_states(4, client_handle);


    return;
}
/* globus_l_ftp_client_read_all_callback() */

/**
 * Dispatch buffers from the stalled_blocks queue to the control API.
 *
 * This function is called once we know that the data connection has
 * been established for a get or put request (via a 125 or 150
 * response). This code relies on the control library's being able to
 * deal with multiple blocks queued with it at once.
 *
 * The blocks are sent in the order of their starting offset. If
 * blocks overlap, then the behavior will probably not be correct.
 *
 * @param handle
 *        The client handle associated with this request.
 */
globus_object_t *
globus_i_ftp_client_data_dispatch_queue(
    globus_i_ftp_client_handle_t *		handle)
{
    globus_i_ftp_client_target_t *		target;
    globus_result_t				result;
    globus_object_t *				err;

    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_data_dispatch_queue() entering\n"));

    if(handle->op == GLOBUS_FTP_CLIENT_GET  ||
       handle->op == GLOBUS_FTP_CLIENT_LIST ||
       handle->op == GLOBUS_FTP_CLIENT_NLST ||
       handle->op == GLOBUS_FTP_CLIENT_MLSD)
    {
	target = handle->source;
    }
    else
    {
	target = handle->dest;
    }

    while(! globus_priority_q_empty(&handle->stalled_blocks) &&
	  handle->state != GLOBUS_FTP_CLIENT_HANDLE_RESTART &&
	  handle->state != GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	globus_l_ftp_client_data_t *		data;

	/* 
	 * Do not remove from the queue until the block is sent to the
	 * control library. This makes any block which is being
	 * handled by register_read or register_write in another
	 * thread get sent to the control library after the current
	 * head of the queue.
	 */
	data = (globus_l_ftp_client_data_t *)
		globus_priority_q_first(&handle->stalled_blocks);

	globus_hashtable_insert(&handle->active_blocks,
				data->buffer,
				data);
	handle->num_active_blocks++;

	globus_assert(handle->op == GLOBUS_FTP_CLIENT_LIST ||
		      handle->op == GLOBUS_FTP_CLIENT_NLST ||
              handle->op == GLOBUS_FTP_CLIENT_MLSD ||
		      handle->op == GLOBUS_FTP_CLIENT_GET  ||
		      handle->op == GLOBUS_FTP_CLIENT_PUT);

	switch(handle->op)
	{
	case GLOBUS_FTP_CLIENT_GET:
	case GLOBUS_FTP_CLIENT_LIST:
	case GLOBUS_FTP_CLIENT_NLST:
	case GLOBUS_FTP_CLIENT_MLSD:
	    globus_i_ftp_client_plugin_notify_read(handle,
						   data->buffer,
						   data->buffer_length);
	    if(target->attr->read_all)
	    {
		globus_i_ftp_client_target_t *		target;

		/* The base offset is used in stream mode to keep track of
		 * any restart marker's influence on the offset of the data
		 * coming on a data channel. In extended block mode, this
		 * is explicit in the extended block headers
		 */
		handle->base_offset = 0;
	
		target = handle->source;

		if(handle->restart_marker.type ==
		   GLOBUS_FTP_CLIENT_RESTART_STREAM &&
		   handle->restart_marker.stream.offset > handle->base_offset)
		{
		    handle->base_offset = handle->restart_marker.stream.offset;
		}

		result = globus_ftp_control_data_read_all(
		    target->control_handle,
		    data->buffer,
		    data->buffer_length,
		    globus_l_ftp_client_read_all_callback,
		    handle);
	    }
	    else
	    {
		result = globus_ftp_control_data_read(
		    handle->source->control_handle,
		    data->buffer,
		    data->buffer_length,
		    globus_l_ftp_client_data_callback,
		    handle);
	    }
	    break;
	case GLOBUS_FTP_CLIENT_PUT:
	    globus_i_ftp_client_plugin_notify_write(handle,
						    data->buffer,
						    data->buffer_length,
						    data->offset,
						    data->eof);
	    result = globus_ftp_control_data_write(
		handle->dest->control_handle,
		data->buffer,
		data->buffer_length,
		data->offset,
		data->eof,
		globus_l_ftp_client_data_callback,
		handle);

	    break;
	default: /* No other states should occur */
	  globus_assert(0 && "Unexpected state");
	}

	if(result == GLOBUS_SUCCESS)
	{
	    globus_priority_q_remove(&handle->stalled_blocks,
				     data);
	}
	else if((handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET || 
		 handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
		 handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE) &&
		(target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE ||
		 target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE ||
		 target->state == 
		    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE))
	{
	    /* 
	     * Registration can fail if the data connection has been
	     * closed. 
	     *
	     * In this case, the callback can be sent with no
	     * data and EOF set to true.
	     */
	    err = globus_error_get(result);

	    globus_priority_q_remove(&handle->stalled_blocks,
				     data);
	    globus_hashtable_remove(&handle->active_blocks,
				    data->buffer);
	    handle->num_active_blocks--;

	    globus_i_ftp_client_plugin_notify_data(handle,
						   err,
						   data->buffer,
						   0,
						   0,
						   GLOBUS_TRUE);

	    globus_i_ftp_client_handle_unlock(handle);

	    data->callback(data->callback_arg,
			   handle->handle,
			   err,
			   data->buffer,
			   0,
			   0,
			   GLOBUS_TRUE);

	    globus_object_free(err);
	    err = GLOBUS_SUCCESS;

	    globus_i_ftp_client_handle_lock(handle);
	}
	else
	{
	    globus_hashtable_remove(&handle->active_blocks,
				    data->buffer);
	    handle->num_active_blocks--;	  
	    err = globus_error_get(result);

	    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	    {
		globus_object_free(err);
		err = GLOBUS_SUCCESS;
	    }
            
            globus_i_ftp_client_debug_printf(1, (stderr, 
                "globus_i_ftp_client_data_dispatch_queue() exiting with error\n"));

	    return err;
	}
    }
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_i_ftp_client_data_dispatch_queue() exiting\n"));

    return GLOBUS_SUCCESS;
}
/* globus_i_ftp_client_data_dispatch_queue() */

/**
 * Flush any queued-but-not-processed data buffers from the client
 * handle.
 *
 * This function is called as part of the teardown after an operation
 * fails. If the failure occurs before the control connection is ready
 * for data blocks, then any data buffers from the user will be queued
 * in the stalled blocks queue.
 *
 * @param handle
 *        Client handle to process.
 *
 */
void
globus_i_ftp_client_data_flush(
    globus_i_ftp_client_handle_t *			handle)
{
    globus_fifo_t					tmp;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_data_flush() entering\n"));

    globus_fifo_init(&tmp);

    while(!globus_priority_q_empty(&handle->stalled_blocks))
    {
	globus_fifo_enqueue(&tmp,
			    globus_priority_q_dequeue(&handle->stalled_blocks));
    }

    while(!globus_fifo_empty(&tmp))
    {
	globus_l_ftp_client_data_t *			data;

	data = (globus_l_ftp_client_data_t *)
	    globus_fifo_dequeue(&tmp);

	globus_i_ftp_client_plugin_notify_data(handle,
					       handle->err,
					       data->buffer,
					       0/*length*/,
					       0/*offset*/,
					       GLOBUS_TRUE /*eof*/);
	globus_i_ftp_client_handle_unlock(handle);
	data->callback(data->callback_arg,
		       handle->handle,
		       handle->err,
		       data->buffer,
		       0,
		       0,
		       GLOBUS_TRUE);
	globus_i_ftp_client_handle_lock(handle);

	globus_libc_free(data);
    }
    
    globus_fifo_destroy(&tmp);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_i_ftp_client_data_flush() exiting\n"));
}
/* globus_i_ftp_client_data_flush() */

void
globus_l_ftp_client_complete_kickout(
    void *					user_arg)
{
    globus_i_ftp_client_handle_t *		handle;
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_complete_kickout() entering\n"));

    handle = (globus_i_ftp_client_handle_t *) user_arg;

    globus_assert(!GLOBUS_I_FTP_CLIENT_BAD_MAGIC((&handle)));

    globus_i_ftp_client_handle_lock(handle);
    globus_i_ftp_client_transfer_complete(handle);

    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_complete_kickout() exiting\n"));
}
/* globus_l_ftp_client_complete_kickout() */

/**
 * Count the number of newline characters in a buffer.
 */
static
globus_size_t
globus_l_ftp_client_count_lf(
    const globus_byte_t *			buf,
    globus_size_t				length)
{
    globus_size_t				i;
    globus_size_t				count = 0;

    for(i = 0; i < length; i++)
    {
	if(buf[i] == '\n') count++;
    }
    return count;
}
/* globus_l_ftp_client_count_lf() */

#endif
