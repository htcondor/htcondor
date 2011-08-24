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
 * @file globus_ftp_client_restart_marker.c
 *
 * $RCSfile: globus_ftp_client_restart_marker.c,v $
 * $Revision: 1.9 $
 * $Date: 2009/11/05 22:43:41 $
 */
#endif

#include "globus_i_ftp_client.h"
#include <string.h>

/**
 * Initialize a restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * @param marker
 *        New restart marker.
 * @see globus_ftp_client_restart_marker_t,
 * globus_ftp_client_restart_marker_destroy()
 */
globus_result_t
globus_ftp_client_restart_marker_init(
    globus_ftp_client_restart_marker_t *	marker)
{
    GlobusFuncName(globus_ftp_client_restart_marker_init);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }

    memset(marker, '\0', sizeof(globus_ftp_client_restart_marker_t));
    marker->type = GLOBUS_FTP_CLIENT_RESTART_NONE;

    return GLOBUS_SUCCESS;
}

/**
 * Create a copy of a restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * This function copies the contents of marker to new_marker.
 *
 * @param new_marker
 *        A pointer to a new restart marker.
 * @param marker
 *        The marker to copy.
 *
 * @see globus_ftp_client_restart_marker_init(),
 * globus_ftp_client_restart_marker_destroy()
 */
globus_result_t
globus_ftp_client_restart_marker_copy(
    globus_ftp_client_restart_marker_t *	new_marker,
    globus_ftp_client_restart_marker_t *	marker)
{
    globus_fifo_t * tmp;
    GlobusFuncName(globus_ftp_client_restart_marker_copy);

    if(new_marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("new_marker"));
    }
    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }

    globus_ftp_client_restart_marker_init(new_marker);

    new_marker->type = marker->type;

    switch(new_marker->type)
    {
    case GLOBUS_FTP_CLIENT_RESTART_NONE:
	break;
    case GLOBUS_FTP_CLIENT_RESTART_STREAM:
	new_marker->stream.offset = marker->stream.offset;
	break;
    case GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK:

	globus_fifo_init(&new_marker->extended_block.ranges);

	if(globus_fifo_empty(&marker->extended_block.ranges))
	{
	    break;
	}
	tmp = globus_fifo_copy(&marker->extended_block.ranges);

	while(!globus_fifo_empty(tmp))
	{
	    globus_i_ftp_client_range_t *	range;

	    range = (globus_i_ftp_client_range_t *) globus_fifo_dequeue(tmp);

	    globus_ftp_client_restart_marker_insert_range(new_marker,
							  range->offset,
							  range->end_offset);
	}
	
	globus_fifo_destroy(tmp);
	globus_free(tmp);
	break;
    }
    return GLOBUS_SUCCESS;
}
/* globus_iftp_client_restart_marker_copy() */

/**
 * Destroy a restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * @param marker
 *        Restart marker. This marker must be initialized by either
 *        calling globus_ftp_client_restart_marker_init() or
 *        globus_ftp_client_restart_marker_copy()
 *
 * @see globus_ftp_client_restart_marker_t,
 * globus_ftp_client_restart_marker_init(),
 * globus_ftp_client_restart_marker_copy()
 */
globus_result_t
globus_ftp_client_restart_marker_destroy(
    globus_ftp_client_restart_marker_t *	marker)
{
    GlobusFuncName(globus_ftp_client_restart_marker_destroy);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }

    switch(marker->type)
    {
    case GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK:
	while(!globus_fifo_empty(&marker->extended_block.ranges))
	{
	    globus_i_ftp_client_range_t *	range;

	    range = (globus_i_ftp_client_range_t *)
		globus_fifo_dequeue(&marker->extended_block.ranges);

	    globus_libc_free(range);
	}
	globus_fifo_destroy(&marker->extended_block.ranges);
    /* FALLSTHROUGH */
    case GLOBUS_FTP_CLIENT_RESTART_NONE:
    case GLOBUS_FTP_CLIENT_RESTART_STREAM:
	memset(marker, '\0', sizeof(globus_ftp_client_restart_marker_t));
	marker->type = GLOBUS_FTP_CLIENT_RESTART_NONE;

	break;
    }
    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_restart_marker_destroy() */

/**
 * Insert a range into a restart marker
 * @ingroup globus_ftp_client_restart_marker
 *
 * This function updates a restart marker with a new byte range,
 * suitable for using to restart an extended block mode transfer.
 * Adjacent ranges within the marker will be combined into a single
 * entry in the marker.
 *
 * The marker must first be initialized by calling
 * globus_ftp_client_restart_marker_init() or
 * globus_ftp_client_restart_marker_copy().
 *
 * A marker can only hold a range list or a stream offset. Calling
 * this function after calling
 * globus_ftp_client_restart_marker_set_offset() will result in a marker
 * suitable only for use restarting an extended block mode transfer.
 *
 * @param marker
 *        A restart marker
 * @param offset
 *        The starting offset of the range.
 * @param end_offset
 *        The ending offset of the range.
 *
 * @see globus_ftp_client_restart_marker_set_offset()
 * globus_ftp_client_operationattr_set_mode()
 */
globus_result_t
globus_ftp_client_restart_marker_insert_range(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t				offset,
    globus_off_t				end_offset)
{
    globus_fifo_t				tmp;
    globus_i_ftp_client_range_t *		range;
    globus_i_ftp_client_range_t *		newrange;
    globus_object_t *				err = GLOBUS_SUCCESS;
    GlobusFuncName(globus_ftp_client_insert_range);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }
    if(marker->type != GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK)
    {
	memset(marker,
	       '\0',
	       sizeof(globus_ftp_client_restart_extended_block_t));

	marker->type = GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK;
	globus_fifo_init(&marker->extended_block.ranges);
    }
    globus_fifo_move(&tmp, &marker->extended_block.ranges);

    while(!globus_fifo_empty(&tmp))
    {
	range = globus_fifo_dequeue(&tmp);
	if(offset <= range->offset)
	{
	    if(end_offset+1 < range->offset)
	    {
		newrange = globus_malloc(sizeof(globus_i_ftp_client_range_t));
		if(newrange == NULL)
		{
		    err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
		    if(!err)
			err = GLOBUS_ERROR_NO_INFO;

		    goto copy_rest;
		}
		newrange->offset = offset;
		newrange->end_offset = end_offset;

		globus_fifo_enqueue(&marker->extended_block.ranges, newrange);
		globus_fifo_enqueue(&marker->extended_block.ranges, range);
		goto copy_rest;
	    }
	    else if(end_offset+1 == range->offset)
	    {
		end_offset = range->end_offset;
		globus_libc_free(range);
	    }
	    else
	    {
		/* weird.... overlapping data */
		if(end_offset < range->end_offset)
		{
		    end_offset = range->end_offset;
		}
		globus_libc_free(range);
	    }
	}
	else
	{
	    if(range->end_offset < offset - 1)
	    {
		globus_fifo_enqueue(&marker->extended_block.ranges, range);
	    }
	    else if(range->end_offset >= offset - 1)
	    {
		offset = range->offset;
		if(end_offset < range->end_offset)
		{
		    end_offset = range->end_offset;
		}
		globus_libc_free(range);
	    }
	    else
	    {
		globus_fifo_enqueue(&marker->extended_block.ranges, range);
	    }
	}
    }

    newrange = globus_malloc(sizeof(globus_i_ftp_client_range_t));
    if(newrange == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
	if(!err)
	    err = GLOBUS_ERROR_NO_INFO;

	goto copy_rest;
    }
    newrange->offset = offset;
    newrange->end_offset = end_offset;
    globus_fifo_enqueue(&marker->extended_block.ranges, newrange);
copy_rest:
    while(! globus_fifo_empty(&tmp))
    {
	globus_fifo_enqueue(&marker->extended_block.ranges,
			    globus_fifo_dequeue(&tmp));
    }
    globus_fifo_destroy(&tmp);
    
    return err ? globus_error_put(err) : GLOBUS_SUCCESS;
}
/* globus_ftp_client_insert_range() */

/**
 * Set the offset for a restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * This function modifies a restart marker to contain a stream offset,
 * suitable for using to restart a steam mode transfer.
 *
 * The marker must first be initialized by calling
 * globus_ftp_client_restart_marker_init() or
 * globus_ftp_client_restart_marker_copy().
 *
 * A marker can only hold a range list or a stream offset. Calling
 * this function after calling
 * globus_ftp_client_restart_marker_insert_range() will delete the
 * ranges associated with the marker, and replace it with a marker
 * suitable only for use restarting a stream mode transfer.
 *
 * When restarting an ASCII type transfer, use
 * globus_ftp_client_restart_marker_set_ascii_offset() to set both
 * the offset used in the local representation of an ACSII file, and
 * the network representation of the ASCII file. For UNIX systems, the
 * former includes counts newlines as one character towards the file
 * offset, and the latter counts them as 2 characters (CRLF).
 *
 * @param marker
 *        A restart marker
 * @param offset
 *        The local stream offset.
 * @param ascii_offset
 *        The network ascii representation of the offset.
 *
 * @see globus_ftp_client_restart_marker_insert_range(),
 * globus_ftp_client_restart_marker_set_offset(),
 * globus_ftp_client_operationattr_set_mode(),
 * globus_ftp_client_operationattr_set_type()
 */
globus_result_t
globus_ftp_client_restart_marker_set_ascii_offset(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t				offset,
    globus_off_t				ascii_offset)
{
    GlobusFuncName(globus_ftp_client_restart_marker_set_ascii_offset);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }
    if(marker->type != GLOBUS_FTP_CLIENT_RESTART_STREAM)
    {
        globus_ftp_client_restart_marker_destroy(marker);
	marker->type = GLOBUS_FTP_CLIENT_RESTART_STREAM;
    }
    marker->stream.offset = offset;
    marker->stream.ascii_offset = ascii_offset;

    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_restart_marker_set_offset() */

/**
 * Set the offset for a restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * This function modifies a restart marker to contain a stream offset,
 * suitable for using to restart a steam mode transfer.
 *
 * The marker must first be initialized by calling
 * globus_ftp_client_restart_marker_init() or
 * globus_ftp_client_restart_marker_copy().
 *
 * A marker can only hold a range list or a stream offset. Calling
 * this function after calling
 * globus_ftp_client_restart_marker_insert_range() will delete the
 * ranges associated with the marker, and replace it with a marker
 * suitable only for use restarting a stream mode transfer.
 *
 * When restarting an ASCII type transfer, the offset must take into
 * account the additional carriage return characters added to the data
 * stream.
 *
 * @param marker
 *        A restart marker
 * @param offset
 *        The stream offset
 *
 * @see globus_ftp_client_restart_marker_insert_range(),
 * globus_ftp_client_operationattr_set_mode(),
 * globus_ftp_client_operationattr_set_type()
 */
globus_result_t
globus_ftp_client_restart_marker_set_offset(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t				offset)
{
    GlobusFuncName(globus_ftp_client_restart_marker_set_offset);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }
    if(marker->type != GLOBUS_FTP_CLIENT_RESTART_STREAM)
    {
        globus_ftp_client_restart_marker_destroy(marker);
	marker->type = GLOBUS_FTP_CLIENT_RESTART_STREAM;
    }
    marker->stream.offset = marker->stream.ascii_offset = offset;

    return GLOBUS_SUCCESS;
}
/* globus_ftp_client_restart_marker_set_offset() */


/**
 * Get total bytes accounted for in restart marker
 * @ingroup globus_ftp_client_restart_marker
 *
 * This funtion will return the sum of all bytes accounted for in
 * a restart marker.  If this restart marker contains a stream offset
 * then this value is the same as the offset (not the ascii offset)
 * that it was set with.  If it is a range list, it a sum of all the
 * bytes in the ranges.
 *
 * @param marker
 *        A previously initialized or copied restart marker
 *
 * @param total_bytes
 *        pointer to storage for total bytes in marker
 *
 * @return
 *        - Error on NULL marker or total bytes
 *        - <possible return>
 */
globus_result_t
globus_ftp_client_restart_marker_get_total(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t *				total_bytes)
{
    GlobusFuncName(globus_ftp_client_restart_marker_get_total);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }

    if(total_bytes == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("total_bytes"));
    }

    *total_bytes = 0;

    if(marker->type == GLOBUS_FTP_CLIENT_RESTART_STREAM)
    {
        *total_bytes = marker->stream.offset;
    }
    else if(marker->type == GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK &&
            !globus_fifo_empty(&marker->extended_block.ranges))
    {
        globus_fifo_t *			        tmp;
        globus_off_t			        total;
        globus_i_ftp_client_range_t *           range;

        tmp = globus_fifo_copy(&marker->extended_block.ranges);
        total = 0;

        while((!globus_fifo_empty(tmp)))
        {
	    range = (globus_i_ftp_client_range_t *) globus_fifo_dequeue(tmp);

            total += range->end_offset - range->offset;
        }

        *total_bytes = total;
        globus_fifo_destroy(tmp);
        globus_libc_free(tmp);
    }

    return GLOBUS_SUCCESS;

}

globus_result_t
globus_ftp_client_restart_marker_get_first_block(
    globus_ftp_client_restart_marker_t *        marker,
    globus_off_t *                              start_offset,
    globus_off_t *                              end_offset)
{
    GlobusFuncName(globus_ftp_client_restart_marker_get_first_block);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
                GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }

    if(start_offset == GLOBUS_NULL)
    {
        return globus_error_put(
                GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("start_offset"));
    }

    if(end_offset == GLOBUS_NULL)
    {
        return globus_error_put(
                GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("end_offset"));
    }

    *start_offset = 0;
    *end_offset = 0;
    
    if(marker->type == GLOBUS_FTP_CLIENT_RESTART_STREAM)
    {
        *end_offset = marker->stream.offset;
    }
    else if(marker->type == GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK &&
            !globus_fifo_empty(&marker->extended_block.ranges))
    {
        globus_i_ftp_client_range_t *           range;

        range = (globus_i_ftp_client_range_t *) 
            globus_fifo_peek(&marker->extended_block.ranges);
        
        *start_offset = range->offset;
        *end_offset = range->end_offset;
    }

    return GLOBUS_SUCCESS;

}

/**
 * Create a string representation of a restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * This function sets the @a marker_string parameter to point to
 * a freshly allocated string suitable for sending as an argument to
 * the FTP REST command, or for a later call to
 * globus_ftp_client_restart_marker_from_string().
 *
 * The string pointed to by marker_string must be freed by the caller.
 *
 * @param marker
 *        An initialized FTP client restart marker.
 * @param marker_string
 *        A pointer to a char * to be set to a freshly allocated marker
 *        string.
 *
 * @see globus_ftp_client_restart_marker
 */
globus_result_t
globus_ftp_client_restart_marker_to_string(
    globus_ftp_client_restart_marker_t *	marker,
    char **					marker_string)
{
    int					length = 0, mylen;
    char *				buf = GLOBUS_NULL;
    char *				tbuf;
    globus_i_ftp_client_range_t *	range;
    globus_fifo_t *			tmp;
    globus_off_t			offset;
    globus_size_t			digits;
    globus_object_t *			err;
    GlobusFuncName(globus_ftp_client_restart_marker_to_string);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }
    else if(marker_string == GLOBUS_NULL)
    {
        return globus_error_put(
		GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker_string"));
    }

    (*marker_string) = GLOBUS_NULL;

    if(marker->type == GLOBUS_FTP_CLIENT_RESTART_NONE)
    {
	return GLOBUS_SUCCESS;
    }
    else if(marker->type == GLOBUS_FTP_CLIENT_RESTART_STREAM)
    {
        if(marker->stream.ascii_offset > marker->stream.offset)
	{
	    offset = marker->stream.ascii_offset;
	}
	else
	{
	    offset = marker->stream.offset;
	}
        digits = globus_i_ftp_client_count_digits(offset);

	(*marker_string) = globus_libc_malloc(digits+1);

	if(!(*marker_string))
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	    if(!err)
	    {
	        err = GLOBUS_ERROR_NO_INFO;
	    }

	    goto error_exit;
	}

	globus_libc_sprintf((*marker_string),
	                    "%lu",
			    (unsigned long) offset);
    }
    else if(marker->type == GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK &&
            !globus_fifo_empty(&marker->extended_block.ranges))
    {
        tmp = globus_fifo_copy(&marker->extended_block.ranges);

        while((! globus_fifo_empty(tmp)))
        {
	    range = (globus_i_ftp_client_range_t *) globus_fifo_dequeue(tmp);

	    mylen = globus_i_ftp_client_count_digits(range->offset);
	    mylen++;
	    mylen += globus_i_ftp_client_count_digits(range->end_offset);
	    mylen++;
            
            if(buf)
            {
	        tbuf = realloc(buf, length + mylen + 1);
            }
            else
            {
                tbuf = malloc(length + mylen + 1);
            }
            
	    if(!tbuf)
	    {
		err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	        if(!err)
	        {
	            err = GLOBUS_ERROR_NO_INFO;
	        }
		goto buf_err;
	    }
	    else
	    {
	        buf = tbuf;
	    }
	    length += globus_libc_sprintf(
	        buf + length,
	        "%"GLOBUS_OFF_T_FORMAT"-%"GLOBUS_OFF_T_FORMAT",",
	        range->offset,
	        range->end_offset);
        }
        buf[strlen(buf)-1] = '\0';
	(*marker_string) = buf;
	
	globus_fifo_destroy(tmp);
        globus_libc_free(tmp);
    }

    return GLOBUS_SUCCESS;

  buf_err:
	globus_fifo_destroy(tmp);
    globus_libc_free(buf);
  error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_restart_marker_to_string() */

/**
 * Initialize a restart marker from a string.
 * @ingroup globus_ftp_client_restart_marker
 *
 * This function initializes a new restart, @a marker, based on the
 * @a marker_string parameter. The string may be either a single offset
 * for a stream-mode restart marker, or a comma-separated list of start-end
 * ranges.
 *
 * @param marker
 *        The restart marker to be unitialized.
 * @param marker_string
 *        The string containing a textual representation of a restart marker.
 * @see globus_ftp_client_restart_marker
 */
globus_result_t
globus_ftp_client_restart_marker_from_string(
    globus_ftp_client_restart_marker_t *	marker,
    const char *				marker_string)
{
    globus_off_t				offset;
    globus_off_t				end;
    globus_size_t				consumed;
    globus_object_t *				err;
    globus_result_t				res;
    const char *				p;
    GlobusFuncName(globus_ftp_client_restart_marker_from_string);

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
	    GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }
    else if(marker_string == GLOBUS_NULL)
    {
        return globus_error_put(
	    GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker_string"));
    }

    res = globus_ftp_client_restart_marker_init(marker);
    if(res != GLOBUS_SUCCESS)
    {
        goto res_exit;
    }

    if(strchr(marker_string, '-') != GLOBUS_NULL)
    {
        /* Looks like an extended block mode restart marker */
	if(marker->type == GLOBUS_FTP_CLIENT_RESTART_NONE)
	{
	    marker->type = GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK;
	}
	if(marker->type != GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("marker");

	    goto error_exit;
	}

        p = marker_string;
	while( sscanf(p, "%"GLOBUS_OFF_T_FORMAT"-%"GLOBUS_OFF_T_FORMAT"%n",
	              &offset,
		      &end,
		      &consumed) >= 2)
	{
	    res = globus_ftp_client_restart_marker_insert_range(marker,
	                                                        offset,
								end);
	    if(res != GLOBUS_SUCCESS)
	    {
	        goto res_exit;
	    }

	    p += consumed;
	    if(*p == ',')
	    {
	        p++;
	    }
	    else
	    {
	        break;
	    }
	}
    }
    else /* assume stream mode */
    {
        if(marker->type == GLOBUS_FTP_CLIENT_RESTART_NONE)
	{
	    marker->type = GLOBUS_FTP_CLIENT_RESTART_STREAM;
	}
	if(marker->type != GLOBUS_FTP_CLIENT_RESTART_STREAM)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("marker");

	    goto error_exit;
	}
	if(sscanf(marker_string, "%"GLOBUS_OFF_T_FORMAT, &offset) != 1)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("marker_string");

	    goto error_exit;
	}
	else
	{
	    marker->stream.ascii_offset = marker->stream.offset = offset;
	}
    }

    return GLOBUS_SUCCESS;

  error_exit:
    res = globus_error_put(err);
  res_exit:
    return res;
}
/* globus_ftp_client_restart_marker_from_string() */
