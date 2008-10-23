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
 * @file globus_ftp_client_state.c
 * Globus FTP Client Library State Machine
 *
 * $RCSfile: globus_ftp_client_state.c,v $
 * $Revision: 1.59 $
 * $Date: 2008/04/04 01:51:47 $
 */
#endif

#include "globus_i_ftp_client.h"

#include <string.h>		/* strstr(), strncmp() */
#include <ctype.h>		/* isupper() */
#include <time.h>

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

#define CRLF "\r\n"

/* Module-specific prototypes */
static
globus_object_t *
globus_l_ftp_client_parse_site_help(
    globus_i_ftp_client_target_t *		target,
    globus_ftp_control_response_t *		response);

static
void
globus_l_ftp_client_parse_feat(
    globus_i_ftp_client_target_t *		target,
    globus_ftp_control_response_t *		response);

static
void
globus_l_ftp_client_parse_pasv(
    globus_ftp_control_handle_t *               handle,
    globus_ftp_control_response_t *		response,
    globus_ftp_control_host_port_t **		host_port,
    int *					num_pasv_addresses);

static
void
globus_l_ftp_client_parse_restart_marker(
    globus_i_ftp_client_handle_t *		handle,
    globus_ftp_control_response_t *		response);

static
char *
globus_l_ftp_client_parallelism_string(
    globus_i_ftp_client_target_t *		target);

static
char *
globus_l_ftp_client_layout_string(
    globus_i_ftp_client_target_t *		target);

static
void
globus_l_ftp_client_connection_error(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_i_ftp_client_target_t *		target,
    globus_object_t *				error,
    globus_ftp_control_response_t *		response);

static
const char *
globus_l_ftp_client_guess_buffer_command(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target);

static
void
globus_l_ftp_client_update_buffer_feature(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target,
    globus_ftp_client_tristate_t		ok);

static
globus_bool_t
globus_l_ftp_client_can_cache_data_connection(
    globus_i_ftp_client_target_t *		target);

static
void
globus_l_ftp_client_data_force_close_callback(
    void *					callback_arg,
    globus_ftp_control_handle_t *		control_handle,
    globus_object_t *				error);

static
void
globus_l_ftp_client_parse_mdtm(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response);

static
void
globus_l_ftp_client_parse_cksm(
    globus_i_ftp_client_handle_t *              client_handle,
    globus_ftp_control_response_t *             response);

static
void
globus_l_ftp_client_parse_mlst(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response);

static
void
globus_l_ftp_client_parse_cwd(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response);

static
void
globus_l_ftp_client_parse_stat(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response);

static
globus_result_t
globus_l_ftp_client_pp_src_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added);

static
globus_result_t
globus_l_ftp_client_pp_dst_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added);

static
globus_result_t
globus_l_ftp_client_pp_xfer_dst_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added);

static
globus_result_t
globus_l_ftp_client_pp_xfer_src_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added);

static
globus_result_t
globus_l_ftp_client_use_gridftp2_getput(
    globus_i_ftp_client_target_t *		target,
    globus_bool_t *                             getput);

static
globus_result_t
globus_l_ftp_client_send_get(
    globus_i_ftp_client_target_t *		target,
    char *                                      pathname,
    globus_bool_t                               pasv);

static
globus_result_t
globus_l_ftp_client_send_put(
    globus_i_ftp_client_target_t *		target,
    char *                                      pathname,
    globus_bool_t                               pasv);

/**
 * Buffer size command applicability information.
 * @internal
 * 
 * This structure is associated with each of the TCP buffer
 * setting commands to help the client library decide whether
 * a command is compatible with the operation being done.
 * For example, some like <b>SITE RETRBUFSIZE</b> will not affect the
 * functioning of a <b>STOR</b> command.
 * @see globus_l_ftp_client_buffer_cmd_info,
 *      globus_l_ftp_client_guess_buffer_command()
 */
typedef struct 
{
    /**
     * The command string. It should match the server's HELP response if
     * applicable.
     */
    char *					string;
    /** True if this command is usable for STOR */
    globus_bool_t				stor_ok;
    /** True if this command is usable for RETR */
    globus_bool_t				retr_ok;
}
globus_l_ftp_client_buffer_cmd_info_t;

/**
 * Table of buffer size commands.
 * @internal
 *
 * All known buffer size commands have an entry in this table, indicating
 * in which situations they can be used.
 *
 * @see globus_l_ftp_client_guess_buffer_command()
 */
static
globus_l_ftp_client_buffer_cmd_info_t globus_l_ftp_client_buffer_cmd_info[] =
{
    {"SITE RETRBUFSIZE", GLOBUS_FALSE, GLOBUS_TRUE },
    {"SITE RBUFSZ", GLOBUS_FALSE, GLOBUS_TRUE },
    {"SITE RBUFSIZ", GLOBUS_FALSE, GLOBUS_TRUE },
    {"SITE STORBUFSIZE", GLOBUS_TRUE, GLOBUS_FALSE },
    {"SITE SBUFSZ", GLOBUS_TRUE, GLOBUS_FALSE },
    {"SITE SBUFSIZ", GLOBUS_TRUE, GLOBUS_FALSE },
    {"SITE BUFSIZE", GLOBUS_TRUE, GLOBUS_TRUE },
    {"SBUF", GLOBUS_TRUE, GLOBUS_TRUE },
    {"ABUF", GLOBUS_TRUE, GLOBUS_TRUE }
};

#define GLOBUS_L_ERET_FORMAT_STRING \
    "ERET P %"GLOBUS_OFF_T_FORMAT" %"GLOBUS_OFF_T_FORMAT" %s"CRLF


/* Internal/Local Functions */

/**
 * FTP response callback.
 *
 * This function is invoked whenever an FTP response is received on
 * an FTP control connection. It processes the FTP state machine to
 * figure out what to do next.
 *
 * @param user_arg
 *        A void * which is set to the globus_i_ftp_client_target_t
 *        associated with this response.
 * @param handle
 *        The control handle associated with this response.
 * @param error
 *        A Globus error object or the value GLOBUS_SUCCESS if the
 *	  response was received and parsed without error.
 * @param response
 *        The parsed response string from the server. This is
 *        interpreted based on the command currently being processed
 *        by the control library.
 */
void
globus_i_ftp_client_response_callback(
    void *					user_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				in_error,
    globus_ftp_control_response_t *		response)
{
    globus_i_ftp_client_target_t *		target;
    globus_object_t *                           error;
    globus_i_ftp_client_handle_t *		client_handle;
    globus_result_t				result;
    globus_bool_t				registered=GLOBUS_FALSE;
    char *					tmpstr = GLOBUS_NULL;
    const char *				buffer_cmd = GLOBUS_NULL;
    char *					parallelism_opt = GLOBUS_NULL;
    char *					layout_opt = GLOBUS_NULL;
    char *                                      list_str = GLOBUS_NULL;    
    unsigned long				pbsz = 0;
    int						rc, oldrc, i;
    char *                                      pathname;
    globus_bool_t			        gridftp2_getput;
    GlobusFuncName(globus_i_ftp_client_response_callback);
    
    target = (globus_i_ftp_client_target_t *) user_arg;
    client_handle = target->owner;
    
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_i_ftp_client_response_callback() entering\n"));
    globus_i_ftp_client_debug_states(2, client_handle);

    globus_assert(! GLOBUS_I_FTP_CLIENT_BAD_MAGIC(&client_handle));
    
    if(in_error)
    {
        error = globus_object_copy(in_error);
    }
    else
    {
        error = GLOBUS_NULL;
    }
   
    globus_i_ftp_client_handle_lock(client_handle);
    
    if(response)
    {
        globus_i_ftp_client_plugin_notify_response(
            client_handle,
            target->url_string,
            target->mask,
            error,
            response);
    }

    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
        client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto finish;
    }        

    /* The behaviour of several states depends on whether to use the
     * GFD.47 (a.k.a GridFTP 2) GETPUT extension.
     */
    result = globus_l_ftp_client_use_gridftp2_getput(target, &gridftp2_getput);
    if (result != GLOBUS_SUCCESS)
    {
        goto result_fault;
    }

    /* This redo is used to make a second run through the state
     * machine, which a few states will require.
     */
    
    pathname = target->url.url_path ? target->url.url_path : "/";
    
redo:
    switch(target->state)
    {
    case GLOBUS_FTP_CLIENT_TARGET_CONNECT:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    /* Successfully connected. Begin authentication */
	    target->state = GLOBUS_FTP_CLIENT_TARGET_AUTHENTICATE;

	    globus_i_ftp_client_plugin_notify_authenticate(
		client_handle,
		target->url_string,
		&target->auth_info);

	    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	    {
		break;
	    }

	    globus_assert(
		client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT
		||
		client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	    result =
		globus_ftp_control_authenticate(
		    handle,
		    &target->attr->auth_info,
		    target->url.scheme_type==GLOBUS_URL_SCHEME_GSIFTP,
		    globus_i_ftp_client_response_callback,
		    user_arg);
                                                        
	    if(result != GLOBUS_SUCCESS)
	    {
		/*
		 * If authentication fails without registration, notify the
		 * plugins, then deal with the fault.
		 */
		goto result_fault;

	    }
	}
	else if(error || response->response_class
		!= GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY)
	{
	    /* Connection failed, deal with it (connect_error
	     * unlocks the handle)
	     */
	    if(!error && response && response->response_buffer)
	    {
		error = GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response);
	    }
	    goto notify_fault;
	}
	/* Else, (success + 1yz response), which just means: server will
	 * get to you in a moment)
	 */
	break;
    case GLOBUS_FTP_CLIENT_TARGET_AUTHENTICATE:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
#           if BUILD_DEBUG
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_FAULT;
	    }
#           else
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_HELP;
	    }
#	    endif
	    goto redo;
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}
	break;
# if BUILD_DEBUG
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_FAULT:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	target->state = GLOBUS_FTP_CLIENT_TARGET_SITE_FAULT;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_INFORMATION;

	tmpstr = globus_libc_getenv("GLOBUS_FTP_CLIENT_FAULT_MODE");

	if(! tmpstr)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_HELP;

	    goto redo;
	}

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "SITE FAULT %s" CRLF,
	    tmpstr);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT
	    ||
	    client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	result =
	    globus_ftp_control_send_command(
		handle,
		"SITE FAULT %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		tmpstr);

	globus_libc_unsetenv("GLOBUS_FTP_CLIENT_FAULT_MODE");

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SITE_FAULT:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_HELP;
	    goto redo;
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}
	break;

# endif
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_HELP:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	target->state = GLOBUS_FTP_CLIENT_TARGET_SITE_HELP;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_INFORMATION;
	
        target->features = globus_i_ftp_client_features_init();
        if(!target->features)
        {
            error = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
	    goto notify_fault;
        }

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "SITE HELP" CRLF);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT
	    ||
	    client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	result =
	    globus_ftp_control_send_command(
		handle,
		"SITE HELP" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;
    case GLOBUS_FTP_CLIENT_TARGET_SITE_HELP:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	if(error != GLOBUS_SUCCESS)
	{
	    goto notify_fault;
	}
	else if(response->response_class==GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    /*
	     * Parse the reply to find out what is implemented by this
	     * server
	     */
	    error = globus_l_ftp_client_parse_site_help(target, response);
	    if(error != GLOBUS_SUCCESS)
            {
                goto notify_fault;
            }
	}

	target->state = GLOBUS_FTP_CLIENT_TARGET_FEAT;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_INFORMATION;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "FEAT" CRLF);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	result =
	    globus_ftp_control_send_command(
		handle,
		"FEAT" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;
    case GLOBUS_FTP_CLIENT_TARGET_FEAT:
	globus_assert(
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT);

	if(error != GLOBUS_SUCCESS)
	{
	    goto notify_fault;
	}
	if(response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    /*
	     * Parse the reply to find out what is implemented by this
	     * server
	     */
	    globus_l_ftp_client_parse_feat(target, response);
	}

	if (client_handle->op == GLOBUS_FTP_CLIENT_FEAT)
	{
            target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
	}	
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_TYPE;
	}

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT)
	{
	    client_handle->state =
		GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION;
	}
	else
	{
	    client_handle->state =
		GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION;
	}
	goto redo;
    
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_TYPE:
	target->state = GLOBUS_FTP_CLIENT_TARGET_TYPE;

	if(target->attr->type == target->type)
	{
	    goto skip_type;
	}

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "TYPE %c",
	    (char) target->attr->type);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION
	    ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "TYPE %c" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    (char) target->attr->type);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_TYPE:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    target->type = target->attr->type;
	    result = globus_ftp_control_local_type(target->control_handle,
						   target->type,
						   8);
	    if(result != GLOBUS_SUCCESS)
	    {
		goto result_fault;
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

    skip_type:
        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_AUTHZ_ASSERT;
        goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_AUTHZ_ASSERT:
        target->state = GLOBUS_FTP_CLIENT_TARGET_AUTHZ_ASSERT;

        if (!target->attr->authz_assert)
        {
            goto skip_authz_assert;
        }
        if(target->authz_assert &&
           !strcmp(target->attr->authz_assert, target->authz_assert) &&
           target->attr->cache_authz_assert)
        {
            goto skip_authz_assert;
        }

        target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_MISC;

        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            target->url_string,
            target->mask,
            "SITE AUTHZ_ASSERT %s",
            target->attr->authz_assert);
        
        if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT || 
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
        {   
            break;
        }
        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION
            ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);
        
        result = globus_ftp_control_send_command(
            target->control_handle,
            "SITE AUTHZ_ASSERT %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            target->attr->authz_assert);
        
        if(result != GLOBUS_SUCCESS)
        {   
            goto result_fault;
        }

        break;

    case GLOBUS_FTP_CLIENT_TARGET_AUTHZ_ASSERT:
        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        if((!error) &&
           response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
        {
            if (target->authz_assert)
            {
                if (strcmp(target->authz_assert, target->attr->authz_assert))
                {
                    globus_free(target->authz_assert);
                    target->authz_assert = globus_libc_strdup(
                                            target->attr->authz_assert);
                }
            }
            else
            {
                target->authz_assert = 
                    globus_libc_strdup(target->attr->authz_assert);
            }
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

            goto notify_fault;
        }

    skip_authz_assert:
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SETNETSTACK;
	goto redo;


    case GLOBUS_FTP_CLIENT_TARGET_SETUP_SETNETSTACK:
        target->state = GLOBUS_FTP_CLIENT_TARGET_SETNETSTACK;
        if(target->attr->net_stack_str == NULL ||  
            (target->net_stack_str != NULL && 
            strcmp(target->attr->net_stack_str, target->net_stack_str) == 0))
        {
            goto skip_setnetstack;
        }
        
        memset(&target->cached_data_conn,
               '\0',
               sizeof(globus_i_ftp_client_data_target_t));

        target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            target->url_string,
            target->mask,
            "SITE SETNETSTACK %s" CRLF,
            target->attr->net_stack_str);

        if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
        {
            break;
        }

        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        result = globus_ftp_control_send_command(
            target->control_handle,
            "SITE SETNETSTACK %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            target->attr->net_stack_str);
        if(result != GLOBUS_SUCCESS)
        {
            goto result_fault;
        }

        break;

    case GLOBUS_FTP_CLIENT_TARGET_SETNETSTACK:
        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        if((!error) &&
           response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
        {
            globus_xio_stack_t          stack;
            
            if(target->net_stack_str)
            {
                globus_free(target->net_stack_str);
            }
            target->net_stack_str = 
                globus_libc_strdup(target->attr->net_stack_str);
            
            if(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER)
            {
                if(target->net_stack_list)
                {
                    globus_i_ftp_control_unload_xio_drivers(
                        target->net_stack_list);
                    target->net_stack_list = GLOBUS_NULL;
                }
                        
                result = globus_i_ftp_control_load_xio_drivers(
                    target->net_stack_str, &target->net_stack_list);
                if(result != GLOBUS_SUCCESS)
                {
                    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
        
                    goto notify_fault;
                }
                result = globus_i_ftp_control_create_stack(
                    target->control_handle, target->net_stack_list, &stack);
                if(result != GLOBUS_SUCCESS)
                {
                    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
        
                    goto notify_fault;
                }
            
                result = globus_i_ftp_control_data_set_stack(
                    target->control_handle, stack);
                if(result != GLOBUS_SUCCESS)
                {
                    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
        
                    goto notify_fault;
                }
                globus_xio_stack_destroy(stack);
            }      
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

            goto notify_fault;
        }

    skip_setnetstack:
        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SETDISKSTACK;
        goto redo;


    case GLOBUS_FTP_CLIENT_TARGET_SETUP_SETDISKSTACK:
        target->state = GLOBUS_FTP_CLIENT_TARGET_SETDISKSTACK;
        if(target->attr->disk_stack_str == NULL ||  
            (target->disk_stack_str != NULL && 
            strcmp(target->attr->disk_stack_str, target->disk_stack_str) == 0))
        {
            goto skip_setdiskstack;
        }
        
        memset(&target->cached_data_conn,
               '\0',
               sizeof(globus_i_ftp_client_data_target_t));

        target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            target->url_string,
            target->mask,
            "SITE SETDISKSTACK %s" CRLF,
            target->attr->disk_stack_str);

        if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
        {
            break;
        }

        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        result = globus_ftp_control_send_command(
            target->control_handle,
            "SITE SETDISKSTACK %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            target->attr->disk_stack_str);
        if(result != GLOBUS_SUCCESS)
        {
            goto result_fault;
        }

        break;

    case GLOBUS_FTP_CLIENT_TARGET_SETDISKSTACK:
        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        if((!error) &&
           response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
        {
            if(target->disk_stack_str)
            {
                globus_free(target->disk_stack_str);
            }
            target->disk_stack_str = 
                globus_libc_strdup(target->attr->disk_stack_str);
            
            if(client_handle->op == GLOBUS_FTP_CLIENT_GET)
            {
                /* if not a 3pt or put gotta set stack on local deal 
                for now this will eb an error */
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

                goto notify_fault;
            }      
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

            goto notify_fault;
        }

    skip_setdiskstack:
        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_MODE;
        goto redo;


    case GLOBUS_FTP_CLIENT_TARGET_SETUP_MODE:
	target->state = GLOBUS_FTP_CLIENT_TARGET_MODE;
	if(target->attr->mode == target->mode)
	{
	    goto skip_mode;
	}
	memset(&target->cached_data_conn,
	       '\0',
	       sizeof(globus_i_ftp_client_data_target_t));

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "MODE %c" CRLF,
	    (char) target->attr->mode);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "MODE %c" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    (char) target->attr->mode);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_MODE:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    target->mode = target->attr->mode;
	    result = globus_ftp_control_local_mode(target->control_handle,
						   target->mode);
	    if(result != GLOBUS_SUCCESS)
	    {
		goto result_fault;
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

    skip_mode:
	if(client_handle->op == GLOBUS_FTP_CLIENT_CKSM)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CKSM;
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_SIZE;
	}
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_CKSM:


	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
	
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_INFORMATION;

	globus_i_ftp_client_plugin_notify_command(
            client_handle,
            target->url_string,
            target->mask,  
            "CKSM %s %" GLOBUS_OFF_T_FORMAT " %" GLOBUS_OFF_T_FORMAT " %s" CRLF,
	    client_handle->checksum_alg,
	    client_handle->checksum_offset,
	    client_handle->checksum_length,
            pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
        {
            break;
        }

        globus_assert(client_handle->state ==
                      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
                      client_handle->state ==
                      GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        result = globus_ftp_control_send_command(
            target->control_handle,
            "CKSM %s %" GLOBUS_OFF_T_FORMAT " %" GLOBUS_OFF_T_FORMAT " %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
	    client_handle->checksum_alg,
	    client_handle->checksum_offset,
	    client_handle->checksum_length,
            pathname);

        if(result != GLOBUS_SUCCESS)
        {
            goto result_fault;
        }
        break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_SIZE:
	/*
	 * Doing a SIZE isn't necessary but is nice for
	 * - resuming  stream mode 3rd party transfers
	 *
	 * Skip if
	 * - server doesn't do SIZE
	 * - not interesting
	 *   where interesting is
	 *   - destination of stream mode 3rd party transfer w/ resume
	 *     attr set to true
	 */
	if(globus_i_ftp_client_feature_get(
		target->features, 
		GLOBUS_FTP_CLIENT_FEATURE_SIZE) == GLOBUS_FALSE
	   ||
	   (!
	       (
		   (client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
		    target == client_handle->dest &&
		    target->attr->resume_third_party &&
		    target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
		   ||
                   (client_handle->op == GLOBUS_FTP_CLIENT_SIZE)
	       )
	   )
	  )
	{
	    if(client_handle->op == GLOBUS_FTP_CLIENT_SIZE)
	    {
		error = GLOBUS_I_FTP_CLIENT_ERROR_UNSUPPORTED_FEATURE("SIZE");

		goto notify_fault;

	    }
	    goto skip_size;
	}

	if(client_handle->op == GLOBUS_FTP_CLIENT_SIZE)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SIZE;
	}

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_INFORMATION;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "SIZE %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
		      client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "SIZE %s" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SIZE:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION
	    ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if(error != GLOBUS_SUCCESS)
	{
	    goto notify_fault;
	}
	if(response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    globus_i_ftp_client_feature_set(
	        target->features,
	        GLOBUS_FTP_CLIENT_FEATURE_SIZE,
	        GLOBUS_FTP_CLIENT_TRUE);

	    if(client_handle->source == target)
	    {
		globus_libc_scan_off_t((char *) response->response_buffer+4,
				       &client_handle->source_size,
				       GLOBUS_NULL);
	    }
	    else
	    {
		globus_byte_t * size;
		const globus_byte_t * p;
		globus_byte_t *q;

		size = globus_libc_malloc(
		    strlen((char *) response->response_buffer+3));
		for(p = response->response_buffer+4, q = size;
		    isdigit(*p);
		    p++,q++)
		{
		    *q = *p;
		}
		*q = '\0';

		if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM &&
		   client_handle->restart_marker.type ==
		   GLOBUS_FTP_CLIENT_RESTART_NONE)
		{
		    client_handle->restart_marker.type =
			GLOBUS_FTP_CLIENT_RESTART_STREAM;

		    globus_libc_scan_off_t(
			(char *) size,
			&client_handle->restart_marker.stream.offset,
			GLOBUS_NULL);

		    client_handle->restart_marker.stream.ascii_offset =
			client_handle->restart_marker.stream.offset;
		}
		globus_libc_free(size);
	    }

	}
	else if(response->code / 10 == 50)
	{
	    /* A 500 response means the server doesn't know about SIZE */
	    globus_i_ftp_client_feature_set(
	        target->features,
	        GLOBUS_FTP_CLIENT_FEATURE_SIZE,
	        GLOBUS_FTP_CLIENT_FALSE);

	}
	else if(response->code == 550 && /* file unavailable */
		client_handle->dest == target)
	{
	    /*
	     * The file may not exist on the remote side if we're
	     * restarting.
	     */
	    ;
	}
	else
	{
	    /* Any other response is not good. */
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
	    if((!client_handle->err) && (!error))
	    {
		error = GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response);
	    }
	    goto connection_error;
	}

    skip_size:
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_BUFSIZE;
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_BUFSIZE:
	target->state = GLOBUS_FTP_CLIENT_TARGET_BUFSIZE;

	/*
	 * TODO: compare buffer mode/size properly for all instances
	 * of union components
	 */
	if(target->attr->buffer.mode == target->tcp_buffer.mode &&
	   target->attr->buffer.fixed.size == target->tcp_buffer.fixed.size)
	{
	    goto skip_bufsize;
	}
    if(!(client_handle->op == GLOBUS_FTP_CLIENT_GET ||
        client_handle->op == GLOBUS_FTP_CLIENT_PUT ||
        client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
        client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
        client_handle->op == GLOBUS_FTP_CLIENT_MLSD ||
        client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER))
    {
        goto skip_bufsize;
    }
    
	buffer_cmd = globus_l_ftp_client_guess_buffer_command(client_handle,
							      target);
	if(buffer_cmd == GLOBUS_NULL)
	{
	    error = GLOBUS_I_FTP_CLIENT_ERROR_UNSUPPORTED_FEATURE(
	        "Adjust socket buffer");

	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

	/* Choose the proper string to pass along with the chosen
	 * buffer size command.
	 */
	switch(target->attr->buffer.mode)
	{
	case GLOBUS_FTP_CONTROL_TCPBUFFER_DEFAULT:
	    target->attr->buffer.fixed.size = 0UL;
	case GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED:
	    break;
	case GLOBUS_FTP_CONTROL_TCPBUFFER_AUTOMATIC:
	    error = GLOBUS_I_FTP_CLIENT_ERROR_UNSUPPORTED_FEATURE("ABUF");

	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "%s %lu" CRLF,
	    buffer_cmd,
	    target->attr->buffer.fixed.size);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
		      client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "%s %lu" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    buffer_cmd,
	    target->attr->buffer.fixed.size);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_BUFSIZE:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    globus_l_ftp_client_update_buffer_feature(
	        client_handle, target, GLOBUS_FTP_CLIENT_TRUE);

	    target->tcp_buffer = target->attr->buffer;

	    result = globus_ftp_control_local_tcp_buffer(
		target->control_handle,
		&target->tcp_buffer);

	    if(result != GLOBUS_SUCCESS)
	    {
		goto result_fault;
	    }
	}
	else if(error)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}
	else
	{
	    globus_l_ftp_client_update_buffer_feature(
	        client_handle, target, GLOBUS_FTP_CLIENT_FALSE);

	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_BUFSIZE;
	    goto redo;
	}

    skip_bufsize:
	if(target->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
	    (client_handle->op == GLOBUS_FTP_CLIENT_GET ||
	     client_handle->op == GLOBUS_FTP_CLIENT_MLSD ||
	     client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
	     client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
	     (client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
	      target == client_handle->source)))
	{
	    /* Only send OPTS RETR if the source control handle will
	     * be used.
	     */
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REMOTE_RETR_OPTS;
	}
	else if(target->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
		client_handle->op == GLOBUS_FTP_CLIENT_PUT)
	{
	    /* Only do local_layout and local_parallelism if we are
	     * receiving the data (not 3rd party)
	     */
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_LOCAL_RETR_OPTS;
	}
	else
	{
	    goto skip_opts_retr;
	}

	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_REMOTE_RETR_OPTS:

	parallelism_opt = globus_l_ftp_client_parallelism_string(target);
	layout_opt = globus_l_ftp_client_layout_string(target);

	if((!parallelism_opt) && (!layout_opt))
	{
	    goto skip_opts_retr;
	}

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;

	globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"OPTS RETR %s%s" CRLF,
		layout_opt ? layout_opt : "",
		parallelism_opt ? parallelism_opt : "");

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    if(parallelism_opt)
	    {
	        globus_libc_free(parallelism_opt);
	    }
	    if(layout_opt)
	    {
	        globus_libc_free(layout_opt);
	    }
	    break;
	}
	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "OPTS RETR %s%s" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    layout_opt ? layout_opt : "",
	    parallelism_opt ? parallelism_opt : "");

	if(parallelism_opt)
	{
	    globus_libc_free(parallelism_opt);
	}
	if(layout_opt)
	{
	    globus_libc_free(layout_opt);
	}
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	target->state = GLOBUS_FTP_CLIENT_TARGET_REMOTE_RETR_OPTS;
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_LOCAL_RETR_OPTS:
        
        if(target->attr->parallelism.mode == target->parallelism.mode &&
            (target->parallelism.mode != GLOBUS_FTP_CONTROL_PARALLELISM_FIXED 
                || target->attr->parallelism.fixed.size ==
	            target->parallelism.fixed.size) &&
	    target->attr->layout.mode == target->layout.mode &&
	        (target->layout.mode !=
	            GLOBUS_FTP_CONTROL_STRIPING_BLOCKED_ROUND_ROBIN
	            || target->attr->layout.round_robin.block_size ==
	                target->layout.round_robin.block_size))
        {
            goto skip_opts_retr;
        }
        
	result = globus_ftp_control_local_parallelism(
	    target->control_handle,
	    &target->attr->parallelism);

	if(result)
	{
	    goto result_fault;
	}
	result = globus_ftp_control_local_layout(target->control_handle,
						 &target->attr->layout,
						 0);
	if(result)
	{
	    goto result_fault;
	}

	memcpy(&target->parallelism,
	       &target->attr->parallelism,
	       sizeof(globus_ftp_control_parallelism_t));

	memcpy(&target->parallelism,
	       &target->attr->parallelism,
	       sizeof(globus_ftp_control_parallelism_t));

	goto skip_opts_retr;

    case GLOBUS_FTP_CLIENT_TARGET_REMOTE_RETR_OPTS:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION
	    ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    memcpy(&target->parallelism,
		   &target->attr->parallelism,
		   sizeof(globus_ftp_control_parallelism_t));
	    memcpy(&target->layout,
		   &target->attr->layout,
		   sizeof(globus_ftp_control_layout_t));
	}
	else if((!error) && response->code / 10 == 50)
	{
	    globus_i_ftp_client_feature_set(
                target->features,
                GLOBUS_FTP_CLIENT_FEATURE_PARALLELISM,
                GLOBUS_FTP_CLIENT_FALSE);

	    if(target->attr->parallelism.mode
	       == GLOBUS_FTP_CONTROL_PARALLELISM_NONE &&
	       target->attr->layout.mode == GLOBUS_FTP_CONTROL_STRIPING_NONE)
	    {
		memcpy(&target->parallelism,
		       &target->attr->parallelism,
		       sizeof(globus_ftp_control_parallelism_t));
		memcpy(&target->layout,
		       &target->attr->layout,
		       sizeof(globus_ftp_control_layout_t));
	    }
	    else
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

		goto connection_error;
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

    skip_opts_retr:
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_DCAU;
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_DCAU:
	target->state = GLOBUS_FTP_CLIENT_TARGET_DCAU;

	if(target->attr->dcau.mode == target->dcau.mode &&
	   target->dcau.mode != GLOBUS_FTP_CONTROL_DCAU_DEFAULT)
	{
	    goto skip_dcau;
	}
	if(target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_DEFAULT &&
	   globus_i_ftp_client_feature_get(
	       target->features, 
	       GLOBUS_FTP_CLIENT_FEATURE_DCAU) != GLOBUS_FTP_CLIENT_TRUE)
	{
	    goto skip_dcau;
	}
	if(target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_DEFAULT &&
	   (target->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SELF ||
	    target->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_DEFAULT))
	{
	    goto finish_dcau;
	}

	/* changing DCAU forces us to trash our old data connections */
	memset(&target->cached_data_conn,
	       '\0',
	       sizeof(globus_i_ftp_client_data_target_t));

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "DCAU %c%s%s" CRLF,
	    (char) target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_DEFAULT
	        ? GLOBUS_FTP_CONTROL_DCAU_SELF : target->attr->dcau.mode,
	    target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT
		? " " : "",
	    target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT
		? target->attr->dcau.subject.subject : "");

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "DCAU %c%s%s" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    (char) target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_DEFAULT
	        ? GLOBUS_FTP_CONTROL_DCAU_SELF : target->attr->dcau.mode,
	    target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT
		? " " : "",
	    target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT
		? target->attr->dcau.subject.subject : "");

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_DCAU:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{

	    if(target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_SUBJECT)
	    {
		char * tmp_subj;
		tmp_subj = target->dcau.subject.subject;

		target->dcau.subject.subject =
		    globus_libc_strdup(target->attr->dcau.subject.subject);
		if(! target->dcau.subject.subject)
		{
		    error = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
		    target->dcau.subject.subject = tmp_subj;

		    goto notify_fault;
		}
		else if(tmp_subj)
		{
		    globus_libc_free(tmp_subj);
		}
	    }
	finish_dcau:
	    if(target->attr->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_DEFAULT)
	    {
	        if(!globus_i_ftp_client_feature_get(
	            target->features, GLOBUS_FTP_CLIENT_FEATURE_DCAU))
		{
		    target->dcau.mode = GLOBUS_FTP_CONTROL_DCAU_NONE;
		}
		else
		{
		    target->dcau.mode = GLOBUS_FTP_CONTROL_DCAU_SELF;
		}
	    }
	    else
	    {
		target->dcau.mode = target->attr->dcau.mode;
	    }

	    result = globus_ftp_control_local_dcau(target->control_handle,
						   &target->dcau,
                          target->control_handle->cc_handle.auth_info.credential_handle);
	    if(result != GLOBUS_SUCCESS)
	    {
		goto result_fault;
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

    skip_dcau:
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PBSZ;
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_PBSZ:
	target->state = GLOBUS_FTP_CLIENT_TARGET_PBSZ;

	if(target->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_NONE)
	{
	    goto skip_pbsz;
	}

	result = globus_ftp_control_get_pbsz(
		    target->control_handle,
		    &pbsz);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	if(target->pbsz == pbsz)
	{
	    goto skip_pbsz;
	}
	
	/* changing PBSZ forces us to trash our old data connections */
	memset(&target->cached_data_conn,
	       '\0',
	       sizeof(globus_i_ftp_client_data_target_t));

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "PBSZ %lu" CRLF,
	    pbsz);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "PBSZ %lu" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    pbsz);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_PBSZ:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    char *                      s;
            
            pbsz = 0;
            s = strstr((char *) response->response_buffer, "PBSZ=");
            if(s)
            {
                sscanf(s, "PBSZ=%lu", &pbsz);
            }
            
            if(pbsz == 0)
            {
                result = globus_ftp_control_get_pbsz(
                    target->control_handle, &pbsz);
            }
            else
            {
                result = globus_ftp_control_local_pbsz(
                    target->control_handle, pbsz);
            }
            
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            target->pbsz = pbsz;
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

    skip_pbsz:
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PROT;
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_PROT:
	target->state = GLOBUS_FTP_CLIENT_TARGET_PROT;

	if(target->dcau.mode == GLOBUS_FTP_CONTROL_DCAU_NONE)
	{
	    goto skip_prot;
	}
	if(target->attr->data_prot == target->data_prot)
	{
	    goto skip_prot;
	}
	/* changing PROT forces us to trash our old data connections [true?] */
	memset(&target->cached_data_conn,
	       '\0',
	       sizeof(globus_i_ftp_client_data_target_t));

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "PROT %c" CRLF,
	    (char) target->attr->data_prot);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    target->control_handle,
	    "PROT %c" CRLF,
	    globus_i_ftp_client_response_callback,
	    target,
	    (char) target->attr->data_prot);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_PROT:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    target->data_prot = target->attr->data_prot;

	    result = globus_ftp_control_local_prot(target->control_handle,
						   target->data_prot);
	    if(result != GLOBUS_SUCCESS)
	    {
		goto result_fault;
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

    skip_prot:
	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION:
	/* for operations which don't use a data connection,
	 * skip PASV/PORT */
	if(client_handle->op == GLOBUS_FTP_CLIENT_CHMOD)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CHMOD;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_DELETE)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_DELETE;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_MKDIR)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_MKDIR;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_RMDIR)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_RMDIR;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_CWD)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CWD;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_MOVE)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_RNFR;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_MDTM)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_MDTM;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_MLST)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_MLST;
	}
	else if(client_handle->op == GLOBUS_FTP_CLIENT_STAT)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_STAT;
	}
	/* Prefer PASV data connections for most operations */
	else if(client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION ||
		(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER &&
		 target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM))
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PASV;
	}
	/* In extended block mode, we MUST have RETR/LIST in PORT mode */
	else
	{
	    globus_assert(client_handle->state ==
			  GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT;
	}

	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_OPTS_PASV_DELAYED:
        globus_assert(
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        if((!error) &&
            response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
        {
            target->delayed_pasv = target->attr->delayed_pasv;
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PASV;
            
            goto redo;
        }
        /* XXX need to ignore error */
        
        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
        goto notify_fault;       

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_PASV:
        if(gridftp2_getput == GLOBUS_TRUE)
        {
            goto skip_pasv;
        }

	if(globus_i_ftp_client_can_reuse_data_conn(client_handle))
	{
	    goto skip_pasv;
	}
	
        if(target->delayed_pasv != target->attr->delayed_pasv && 
            globus_i_ftp_client_feature_get(target->features, 
            GLOBUS_FTP_CLIENT_FEATURE_DELAYED_PASV) == GLOBUS_FTP_CLIENT_TRUE)
        {
	    target->state = GLOBUS_FTP_CLIENT_TARGET_OPTS_PASV_DELAYED;
            
            target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS;
            globus_i_ftp_client_plugin_notify_command(
                client_handle,
                target->url_string,
                target->mask,
                "OPTS PASV AllowDelayed=%c;" CRLF,
                target->attr->delayed_pasv ? '1' : '0');
                
            if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
                client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
            {
                break;
            }
            globus_assert(
                client_handle->state ==
                GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
                client_handle->state ==
                GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);
            
            result =
                globus_ftp_control_send_command(
                target->control_handle,
                "OPTS PASV AllowDelayed=%c;" CRLF,
                globus_i_ftp_client_response_callback,
                target,
                target->attr->delayed_pasv ? '1' : '0');
            
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            break;
        }

	if((client_handle->op == GLOBUS_FTP_CLIENT_PUT ||
	   client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER) &&
	   (target->attr->layout.mode != GLOBUS_FTP_CONTROL_STRIPING_NONE ||
	    target->attr->force_striped))
	{
	    if(target->attr->allow_ipv6)
	    {
	        tmpstr = "SPAS 2";
	    }
	    else
	    {
	        tmpstr = "SPAS";
	    }
	}
	else if(target->attr->allow_ipv6)
	{
	    tmpstr = "EPSV";
	}
	else
	{
	    tmpstr = "PASV";
	}

	target->state = GLOBUS_FTP_CLIENT_TARGET_PASV;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_DATA_ESTABLISHMENT;
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "%s" CRLF,
	    tmpstr);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		target->control_handle,
		"%s" CRLF,
		globus_i_ftp_client_response_callback,
		target,
		tmpstr);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT:
	if(globus_i_ftp_client_can_reuse_data_conn(client_handle))
	{
	    goto skip_port;
	}
	
	/* turn off delayed pasv now that we're going to use PORT */
	target->delayed_pasv = GLOBUS_FALSE;
	
	if(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER)
	{
	    if(client_handle->pasv_address)
	    {
	        globus_free(client_handle->pasv_address);
	    }
	    client_handle->pasv_address =
	        globus_libc_malloc(sizeof(globus_ftp_control_host_port_t));
	    client_handle->num_pasv_addresses = 1;
            
	    memset(client_handle->pasv_address, 0,
	        sizeof(globus_ftp_control_host_port_t));

	    result = globus_ftp_control_local_pasv(target->control_handle,
					           client_handle->pasv_address);
            if(result == GLOBUS_SUCCESS && !target->attr->allow_ipv6 &&
                client_handle->pasv_address[0].hostlen == 16)
            {
                char *                  cs;
                globus_sockaddr_t       addr;
                
                /* control channel was made with ipv6, but we must have
                 * disabled ipv6 since and need an ipv4 address now.
                 * just use localhost
                 */
                GlobusLibcSockaddrSetFamily(addr, AF_INET);
                GlobusLibcSockaddrSetPort(addr,
                    client_handle->pasv_address[0].port);
                
                result = globus_libc_addr_to_contact_string(
                    &addr,
                    GLOBUS_LIBC_ADDR_LOCAL | GLOBUS_LIBC_ADDR_NUMERIC |
                        GLOBUS_LIBC_ADDR_IPV4,
                    &cs);
                if(result == GLOBUS_SUCCESS)
                {
                    globus_libc_contact_string_to_ints(
                        cs,
                        client_handle->pasv_address[0].host,
                        &client_handle->pasv_address[0].hostlen,
                        &client_handle->pasv_address[0].port);
                }
            }
            
	    if(result != GLOBUS_SUCCESS)
	    {
		goto result_fault;
	    }
	}

        if(gridftp2_getput == GLOBUS_TRUE)
        {
            goto skip_port;
        }

	tmpstr = globus_libc_malloc(56 * client_handle->num_pasv_addresses
				   + 7 /*SPOR|PORT|EPRT\r\n\0*/);
	                             /* ' |2|<45>|<5>|' == 56 */
	if(tmpstr == GLOBUS_NULL)
	{
	    error = GLOBUS_ERROR_NO_INFO;

	    goto notify_fault;
	}
	else
	{
	    rc = oldrc = 0;
	    if(client_handle->num_pasv_addresses == 1)
	    {
	        if(target->attr->allow_ipv6 &&
	            client_handle->pasv_address[0].hostlen == 16)
	        {
	            rc += sprintf(tmpstr, "EPRT");
	        }
	        else
	        {
		    rc += sprintf(tmpstr, "PORT");
		}
	    }
	    else
	    {
		rc += sprintf(tmpstr, "SPOR");
	    }

	    if(rc == oldrc)
	    {
		error = GLOBUS_ERROR_NO_INFO;

	        goto notify_fault;
	    }

	    for(i = 0; i < client_handle->num_pasv_addresses; i++)
	    {
	        oldrc = rc;
	        if(target->attr->allow_ipv6 &&
	            (client_handle->num_pasv_addresses > 1 ||
	            client_handle->pasv_address[0].hostlen == 16))
	        {
	            char                buf[50];
	            
	            globus_ftp_control_host_port_get_host(
                        &client_handle->pasv_address[i], buf);
    
	            rc += sprintf(&tmpstr[oldrc],
                                 " |%d|%s|%d|",
                                 client_handle->pasv_address[i].hostlen == 16
                                    ? 2 : 1,
                                 buf,
                                 (int) client_handle->pasv_address[i].port);
	        }
	        else
	        {
                    rc += sprintf(&tmpstr[oldrc],
                                 " %d,%d,%d,%d,%d,%d",
                                 client_handle->pasv_address[i].host[0],
                                 client_handle->pasv_address[i].host[1],
                                 client_handle->pasv_address[i].host[2],
                                 client_handle->pasv_address[i].host[3],
                                 (client_handle->pasv_address[i].port >> 8)
                                 & 0xff,
                                 client_handle->pasv_address[i].port & 0xff);
                }
                if(rc == oldrc)
                {
                    error = GLOBUS_ERROR_NO_INFO;

                    goto notify_fault;
                }
	    }

	    target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_DATA_ESTABLISHMENT;
	    globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"%s" CRLF,
		tmpstr);

	    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	    {
		globus_libc_free(tmpstr);
		break;
	    }
	    globus_assert( client_handle->state ==
		       GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
		       client_handle->state ==
		       GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	    target->state = GLOBUS_FTP_CLIENT_TARGET_PORT;

	    result =
		globus_ftp_control_send_command(
			target->control_handle,
			"%s" CRLF,
			globus_i_ftp_client_response_callback,
			target,
			tmpstr);

	    globus_libc_free(tmpstr);
	}

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	break;

    case GLOBUS_FTP_CLIENT_TARGET_PASV:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    if(target->delayed_pasv)
	    {
	        goto skip_pasv;	    
	    }
	    
	    globus_l_ftp_client_parse_pasv(
	        handle,
	        response, 
	        &client_handle->pasv_address, 
	        &client_handle->num_pasv_addresses);
            
	    if(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER)
	    {
	        
		if(client_handle->num_pasv_addresses == 1)
		{
		    result =
			globus_ftp_control_local_port(
			    handle,
			    client_handle->pasv_address);
		}
		else
		{
		    result =
			globus_ftp_control_local_spor(
			    handle,
			    client_handle->pasv_address,
			    client_handle->num_pasv_addresses);
		}
		if(result != GLOBUS_SUCCESS)
		{
		    goto result_fault;
		}
	    }

	    /* Store the current data connection in the cache for
	     * the target associated with this transfer, if the server
	     * will support it.
	     */
	    if(globus_l_ftp_client_can_cache_data_connection(target))
	    {
                if(client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
                   client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
                   client_handle->op == GLOBUS_FTP_CLIENT_MLSD)
                {
        	    target->cached_data_conn.operation = GLOBUS_FTP_CLIENT_GET;
                }
                else
                {
                    target->cached_data_conn.operation = client_handle->op;
                }
                      
		target->cached_data_conn.source = client_handle->source;
		target->cached_data_conn.dest = client_handle->dest;
	    }
	    /* In a 3rd party transfer, we need to clear the peer's
	     * data connection cache if we've called passive on the
	     * destination server.
	     */
	    if(client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER)
	    {
		memset(&client_handle->source->cached_data_conn,
		       '\0',
		       sizeof(globus_i_ftp_client_data_target_t));
	    }

	skip_pasv:
            if((client_handle->op == GLOBUS_FTP_CLIENT_PUT ||
                (client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER && 
                client_handle->dest == target)) && 
                target->attr->allocated_size > 0)
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_ALLO;
            }
	    else if(client_handle->restart_marker.type !=
	       GLOBUS_FTP_CLIENT_RESTART_NONE)
	    {
		if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
		{
		    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_STREAM;
		}
		else
		{
		    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_EB;
		}
	    }
	    else
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION;
	    }
	}
        else if(!error)
	{
	    if(target->attr->allow_ipv6)
	    {
	        /* lets try without ipv6 */
	        target->attr->allow_ipv6 = GLOBUS_FALSE;
	        globus_ftp_control_ipv6_allow(handle, GLOBUS_FALSE);
	        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PASV;
	    }
	    else if(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER &&
	       target->mode != GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK )
	    {
	        /* Try doing a PORT in some cases */
	        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT;
	    }
	    else
	    {
	        target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
	        goto notify_fault;
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
	    goto notify_fault;
	}

	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_PORT:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	    if(globus_l_ftp_client_can_cache_data_connection(target))
	    {
                if(client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
                   client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
                   client_handle->op == GLOBUS_FTP_CLIENT_MLSD)
                {
        	    target->cached_data_conn.operation = GLOBUS_FTP_CLIENT_GET;
                }
                else
                {
                    target->cached_data_conn.operation = client_handle->op;
                }
                      
		target->cached_data_conn.source = client_handle->source;
		target->cached_data_conn.dest = client_handle->dest;
	    }

	skip_port:
            if((client_handle->op == GLOBUS_FTP_CLIENT_PUT ||
                (client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER && 
                client_handle->dest == target)) && 
                target->attr->allocated_size > 0)
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_ALLO;
            }
	    else if(client_handle->restart_marker.type !=
	       GLOBUS_FTP_CLIENT_RESTART_NONE)
	    {
		if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
		{
		    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_STREAM;
		}
		else
		{
		    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_EB;
		}
	    }
	    else
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION;
	    }
	}
	else if(!error && target->attr->allow_ipv6 &&
	    client_handle->op == GLOBUS_FTP_CLIENT_GET)
	{
	    target->attr->allow_ipv6 = GLOBUS_FALSE;
	    globus_ftp_control_ipv6_allow(handle, GLOBUS_FALSE);
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT;
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}
	goto redo;
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_STREAM:
	/*
	 * REST must be the last thing done on the control connection
	 * before sending the RETR, STOR, ERET, or ESTO command.
	 */
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if(!globus_i_ftp_client_feature_get( 
	    target->features, 
	    GLOBUS_FTP_CLIENT_FEATURE_REST_STREAM))
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto connection_error;
	}
	target->state = GLOBUS_FTP_CLIENT_TARGET_REST;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_MODIFIERS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
            target->url_string,
	    target->mask,
	    "REST %" GLOBUS_OFF_T_FORMAT CRLF,
	    target->type == GLOBUS_FTP_CONTROL_TYPE_ASCII
	        ? client_handle->restart_marker.stream.ascii_offset
	        : client_handle->restart_marker.stream.offset);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    handle,
	    "REST %" GLOBUS_OFF_T_FORMAT CRLF,
	    globus_i_ftp_client_response_callback,
	    user_arg,
	    target->type == GLOBUS_FTP_CONTROL_TYPE_ASCII
	        ? client_handle->restart_marker.stream.ascii_offset
	        : client_handle->restart_marker.stream.offset);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;
	
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_ALLO:
	/*
	 * ALLO is sent before a STOR, REST or STOR is next.
	 */
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	target->state = GLOBUS_FTP_CLIENT_TARGET_ALLO;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_MODIFIERS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
            target->url_string,
	    target->mask,
	    "ALLO %" GLOBUS_OFF_T_FORMAT CRLF,
	    target->attr->allocated_size);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	result = globus_ftp_control_send_command(
	    handle,
	    "ALLO %" GLOBUS_OFF_T_FORMAT CRLF,
	    globus_i_ftp_client_response_callback,
	    user_arg,
	    target->attr->allocated_size);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_EB:
	/*
	 * REST must be the last thing done on the control connection
	 * before sending the RETR, STOR, ERET, or ESTO command.
	 */
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	globus_ftp_client_restart_marker_to_string(
            &client_handle->restart_marker,
            &tmpstr);

	if(tmpstr)
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_REST;
	    target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_MODIFIERS;

	    globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"REST %s" CRLF,
		tmpstr);

	    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	    {
		break;
	    }

	    globus_assert(
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	    result = globus_ftp_control_send_command(
		handle,
		"REST %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		tmpstr);

	    globus_libc_free(tmpstr);
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION;
	    goto redo;
	}
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_REST:
	globus_assert(
	    client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
            client_handle->state ==
            GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_INTERMEDIATE_REPLY)
	{
	    if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
	    {
	        globus_i_ftp_client_feature_set(
                    target->features,
                    GLOBUS_FTP_CLIENT_FEATURE_REST_STREAM,
                    GLOBUS_FTP_CLIENT_TRUE);
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	}

	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION;
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_ALLO:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
	}
	else
	{
	    /*  Don't think we really care about error response here
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

	    goto notify_fault;
	    */
	}
        if(client_handle->restart_marker.type !=
            GLOBUS_FTP_CLIENT_RESTART_NONE)
        {
            if(target->mode == GLOBUS_FTP_CONTROL_MODE_STREAM)
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_STREAM;
            }
            else
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_EB;
            }
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION;
        }
	goto redo;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION:
	switch(client_handle->op)
	{
	  case GLOBUS_FTP_CLIENT_LIST:
	  case GLOBUS_FTP_CLIENT_NLST:
	  case GLOBUS_FTP_CLIENT_MLSD:
	    if(target->attr->cwd_first)
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST_CWD;
            }
            else
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST;
            }
	    goto redo;
	  case GLOBUS_FTP_CLIENT_GET:
            if (gridftp2_getput == GLOBUS_TRUE)
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_GET;
            }
            else
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_GET;
            }
            goto redo;
	  case GLOBUS_FTP_CLIENT_PUT:
            if (gridftp2_getput == GLOBUS_TRUE)
            {
                 target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_PUT;
            }
            else
            {
                 target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_PUT;
            }
	    goto redo;
	  case GLOBUS_FTP_CLIENT_TRANSFER:
	    if(client_handle->state ==
	       GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION)
	    {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_DEST;
	    }
	    else
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_SOURCE;
	    }
	    goto redo;
	  case GLOBUS_FTP_CLIENT_IDLE:
	    globus_assert(client_handle->op != GLOBUS_FTP_CLIENT_IDLE);
	    goto finish;

	  default:/* No other states should occur */
	    globus_assert(0 && "Unexpected state");
	}

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST_CWD:
	/*
	 * CWD before a listing operation.
	 */
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST;
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
            target->url_string,
	    target->mask,
	    "CWD %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	result = globus_ftp_control_send_command(
	    handle,
	    "CWD %s" CRLF,
	    globus_i_ftp_client_response_callback,
	    user_arg,
	    pathname);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

        /* if there is a response before this it is probably from a
          CWD or NOOP */
        if(response != NULL)
        {
            if((!error) &&
              response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
            {
            }
            else
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
    
                goto notify_fault;
            }
        }

        if(!target->delayed_pasv)
        {
            result =
                globus_ftp_control_data_connect_read(target->control_handle,
                                                     GLOBUS_NULL,
                                                     GLOBUS_NULL);
        }
        else
        {
            result = GLOBUS_SUCCESS;
        }
        
	target->state = GLOBUS_FTP_CLIENT_TARGET_LIST;

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	client_handle->state =
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	switch(client_handle->op)
	{
	  case GLOBUS_FTP_CLIENT_LIST:
	    list_str = "LIST";
	    break;

	  case GLOBUS_FTP_CLIENT_NLST:
	    list_str = "NLST";
	    break;

	  case GLOBUS_FTP_CLIENT_MLSD:
	    list_str = "MLSD";
	    break;

          default:
            globus_assert(0 && "Unexpected list op");
            break;
	}
	
	
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "%s %s" CRLF,
	    list_str,
	    target->attr->cwd_first ? "" : pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST);

	result =
	    globus_ftp_control_send_command(
		handle,
		"%s %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		list_str,
		target->attr->cwd_first ? "" : pathname);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_MLST:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "MLST %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"MLST %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_STAT:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "STAT %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"STAT %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_CHMOD:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "SITE CHMOD %04o %s" CRLF,
	    client_handle->chmod_file_mode,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"SITE CHMOD %04o %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		client_handle->chmod_file_mode,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;
	
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_DELETE:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "DELE %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"DELE %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_RNFR:

	target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_RNTO;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "RNFR %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"RNFR %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_RNTO:
    {
        globus_url_t                    dest_url;
	globus_bool_t			rfc1738_url;
	globus_ftp_client_handleattr_t  handle_attr;
	
	handle_attr = &client_handle->attr;

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	result = globus_ftp_client_handleattr_get_rfc1738_url(&handle_attr,
						     &rfc1738_url);
	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	
        if(rfc1738_url==GLOBUS_TRUE)
	{
	    result = (globus_result_t) globus_url_parse_rfc1738(client_handle->dest_url,
                &dest_url);
	}
	else
	{
	    result = (globus_result_t) globus_url_parse(client_handle->dest_url, 
	        &dest_url);
	}

        if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "RNTO %s" CRLF,
	    dest_url.url_path);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
            globus_url_destroy(&dest_url);
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	{
	    result =
		globus_ftp_control_send_command(
		    handle,
		    "RNTO %s" CRLF,
		    globus_i_ftp_client_response_callback,
		    user_arg,
		    dest_url.url_path);
	}

        globus_url_destroy(&dest_url);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;
    }
    case GLOBUS_FTP_CLIENT_TARGET_SETUP_MKDIR:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "MKD %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"MKD %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_RMDIR:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "RMD %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"RMD %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_CWD:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

/*
	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "MLST %s" CRLF,
	    pathname);
*/

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"CWD %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_MDTM:

	target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	globus_i_ftp_client_plugin_notify_command(
	    client_handle,
	    target->url_string,
	    target->mask,
	    "MDTM %s" CRLF,
	    pathname);

	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	result =
	    globus_ftp_control_send_command(
		handle,
		"MDTM %s" CRLF,
		globus_i_ftp_client_response_callback,
		user_arg,
		pathname);

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_GET:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

        if(!target->delayed_pasv)
        {
            result =
                globus_ftp_control_data_connect_read(target->control_handle,
                                                     GLOBUS_NULL,
                                                     GLOBUS_NULL);
        }
        else
        {
            result = GLOBUS_SUCCESS;
        }

	target->state = GLOBUS_FTP_CLIENT_TARGET_RETR;

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	client_handle->state =
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

        if(target->attr->module_alg_str != GLOBUS_NULL)
	{
	    globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"ERET %s %s" CRLF,
                target->attr->module_alg_str,
		pathname);
	}
	else
	{
	    globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"RETR %s" CRLF,
		pathname);
	}
	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET);


        if(!0)
        {
            if(target->attr->module_alg_str != GLOBUS_NULL)
            {
                result =
                    globus_ftp_control_send_command(
                        handle,
                        "ERET %s %s" CRLF,
                        globus_i_ftp_client_response_callback,
                        user_arg,
                        target->attr->module_alg_str,
                        pathname);
            }
            else
            {
                result =
                    globus_ftp_control_send_command(
                        handle,
                        "RETR %s" CRLF,
                        globus_i_ftp_client_response_callback,
                        user_arg,
                        pathname);
            }
    
            if(result != GLOBUS_SUCCESS)
            {
                    goto result_fault;
            }
        }

        if(client_handle->attr.pipeline_callback)
        {
            globus_bool_t                   added = GLOBUS_TRUE;
                        
            while(added)
            {
                result = globus_l_ftp_client_pp_src_add(
                    client_handle,
                    handle, 
                    target,
                    &added);
                if(result != GLOBUS_SUCCESS)
                {
                    goto result_fault;
                }
            }
        }
        break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_PUT:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        if(!target->delayed_pasv)       
        {
            result =
                globus_ftp_control_data_connect_write(target->control_handle,
                                                      GLOBUS_NULL,
                                                      GLOBUS_NULL);
        }
        else
        {
            result = GLOBUS_SUCCESS;
        }

	target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;

	if(result != GLOBUS_SUCCESS)
	{
	    goto result_fault;
	}

	client_handle->state =
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

	if(target->attr->module_alg_str != GLOBUS_NULL)
	{
	    globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"ESTO %s %s" CRLF,
                target->attr->module_alg_str,
		pathname);
	}
	else
	{
	    globus_i_ftp_client_plugin_notify_command(
		client_handle,
		target->url_string,
		target->mask,
		"%s %s" CRLF,
		target->attr->append ? "APPE" : "STOR",
		pathname);
	}
	if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	{
	    break;
	}

	globus_assert(client_handle->state ==
		      GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO);

        if(!0)
        {
            if(target->attr->module_alg_str != GLOBUS_NULL)
            {
                result =
                    globus_ftp_control_send_command(
                            handle,
                            "ESTO %s %s" CRLF,
                            globus_i_ftp_client_response_callback,
                            user_arg,
                            target->attr->module_alg_str,
                            pathname);
            }
            else if(target->attr->append)
            {
                result =
                    globus_ftp_control_send_command(
                        handle,
                        "APPE %s" CRLF,
                        globus_i_ftp_client_response_callback,
                        user_arg,
                        pathname);
            }
            else
            {
                result =
                    globus_ftp_control_send_command(
                        handle,
                        "STOR %s" CRLF,
                        globus_i_ftp_client_response_callback,
                        user_arg,
                        pathname);
            }
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
        }

        if(client_handle->attr.pipeline_callback)
        {
            globus_bool_t                   added = GLOBUS_TRUE;
                        
            while(added)
            {
                result = globus_l_ftp_client_pp_dst_add(
                    client_handle,
                    handle,
                    target,
                    &added);
                if(result != GLOBUS_SUCCESS)
                {
                    goto result_fault;
                }
            }
        }

	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_DEST:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

	/* The destination is prepared first. We send all
	 * of the commands we need to, including the STOR
	 * or ESTO, and then turn our attention to the
	 * source.
	 */
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

        if(gridftp2_getput)
        {
            result = 
                globus_l_ftp_client_send_put(target, pathname, GLOBUS_TRUE);
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
               client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
               client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
            {
                break;
            }
            
            if(globus_i_ftp_client_can_reuse_data_conn(client_handle))
            {
                /* In this case we do not expect a 127 reply.
                 */
                target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;
            }
            else
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_TRANSFER;
            
                /* In this case we have to wait for the 127 reply
                 * before we can setup the transfer at the source.
                 */
                break;
            }
        }
        else
        {
            if(target->attr->module_alg_str != GLOBUS_NULL)
            {
                globus_i_ftp_client_plugin_notify_command(
                    client_handle,
                    target->url_string,
                    target->mask,
                    "ESTO %s %s" CRLF,
                    target->attr->module_alg_str,
                    pathname);
            }
            else
            {
                globus_i_ftp_client_plugin_notify_command(
                    client_handle,
                    target->url_string,
                    target->mask,
                    "STOR %s" CRLF,
                    pathname);
            }
            
            if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
                client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
            {
                break;
            }
            
            globus_assert(client_handle->state ==
                GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);
            
            target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;
            
            if(target->attr->module_alg_str != GLOBUS_NULL)
            {
                result = globus_ftp_control_send_command(
                    handle,
                    "ESTO %s %s" CRLF,
                    globus_i_ftp_client_response_callback,
                    user_arg,
                    target->attr->module_alg_str,
                    pathname);
            }
            else
            {
                result = globus_ftp_control_send_command(
                    handle,
                    "STOR %s" CRLF,
                    globus_i_ftp_client_response_callback,
                    user_arg,
                    pathname);
            }
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }

            if(client_handle->attr.pipeline_callback)
            {
                int                             i;
                globus_bool_t                   added = GLOBUS_TRUE;
                globus_i_ftp_client_url_ent_t * url_ent;
    
                /* add the one we just sent to the response queue */
                url_ent = globus_malloc(sizeof(globus_i_ftp_client_url_ent_t));
                url_ent->source_url = NULL;
                url_ent->dest_url = globus_libc_strdup(target->url_string);
                globus_l_ftp_client_url_parse(
                    url_ent->dest_url,
                    &url_ent->dst_url,
                    client_handle->attr.rfc1738_url);
                globus_fifo_enqueue(
                    &client_handle->dst_response_pending_queue, url_ent);
    
                client_handle->no_callback_count++;
                client_handle->attr.pipeline_done = GLOBUS_FALSE;
                
                for(
                    i = 0; 
                    added && i < client_handle->attr.outstanding_commands;
                    i++)
                {
                    added = GLOBUS_FALSE;
                    result = globus_l_ftp_client_pp_xfer_dst_add(
                        client_handle,
                        handle,
                        target,
                        &added);
                    if(result != GLOBUS_SUCCESS)
                    {
                        goto result_fault;
                    }
                }
            }
        }
        
        if(!target->delayed_pasv)
        {
            target = client_handle->source;
    
            error =
                globus_i_ftp_client_target_activate(client_handle,
                                                    target,
                                                    &registered);
            if(registered == GLOBUS_FALSE)
            {
                if(client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                   client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_RESTART)
                {
                    break;
                }
                else
                {
                    goto connection_error;
                }
            }
        }
	break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_SOURCE:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

	client_handle->state =
	    GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER;

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;

        if(gridftp2_getput)
        {
            result = 
                globus_l_ftp_client_send_get(target, pathname, GLOBUS_FALSE);
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
        
            if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
               client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
               client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
            {
                break;
            }
            
            target->state = GLOBUS_FTP_CLIENT_TARGET_RETR;
        }
        else
        {
            if(target->attr->module_alg_str)
            {
                globus_i_ftp_client_plugin_notify_command(
                    client_handle,
                    target->url_string,
                    target->mask,
                    "ERET %s %s" CRLF,
                    target->attr->module_alg_str,
                    pathname);
            }
            else
            {
                globus_i_ftp_client_plugin_notify_command(
                    client_handle,
                    target->url_string,
                    target->mask,
                    "RETR %s" CRLF,
                    pathname);
            }
    
            if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
                client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
            {
                break;
            }
            globus_assert(client_handle->state ==
                          GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER);
    
            target->state = GLOBUS_FTP_CLIENT_TARGET_RETR;
    
            if(target->attr->module_alg_str)
            {
                result = globus_ftp_control_send_command(
                    handle,
                    "ERET %s %s\r\n",
                    globus_i_ftp_client_response_callback,
                    user_arg,
                    target->attr->module_alg_str,
                    pathname);
            }
            else
            {
                result = globus_ftp_control_send_command(
                    handle,
                    "RETR %s" CRLF,
                    globus_i_ftp_client_response_callback,
                    user_arg,
                    pathname);
            }
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            if(client_handle->attr.pipeline_callback)
            {
                int                             i;
                globus_bool_t                   added = GLOBUS_TRUE;
                globus_i_ftp_client_url_ent_t * url_ent;
    
                /* add the one we just sent to the response queue */
                url_ent = globus_malloc(sizeof(globus_i_ftp_client_url_ent_t));
                url_ent->source_url = globus_libc_strdup(target->url_string);
                url_ent->dest_url = NULL;
                globus_l_ftp_client_url_parse(
                    url_ent->source_url,
                    &url_ent->src_url,
                    client_handle->attr.rfc1738_url);
                globus_fifo_enqueue(
                    &client_handle->src_response_pending_queue, url_ent);
    
                client_handle->no_callback_count++;
                client_handle->attr.pipeline_done = GLOBUS_FALSE;
                
                for(
                    i = 0; 
                    added && i < client_handle->attr.outstanding_commands;
                    i++)
                {
                    result = globus_l_ftp_client_pp_xfer_src_add(
                        client_handle,
                        handle,
                        target,
                        &added);
                    if(result != GLOBUS_SUCCESS)
                    {
                        goto result_fault;
                    }
                }
            }
        }
        
        break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_GET:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION);

        client_handle->state =
             GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET;
        
        if(target->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK ||
           globus_i_ftp_client_can_reuse_data_conn(client_handle))
        {
            /* In extended block mode, we will be the passive party
             * and do not expect a 127 reply.
             *
             * If a connection was cached, we do not expect a 127 reply
             * either.
             */
            target->state = GLOBUS_FTP_CLIENT_TARGET_RETR;

            /* Setup data connection. Since we do not expect a 127
             * reply, we need to do this before sending GET.
             */
            result =
                globus_ftp_control_data_connect_read(target->control_handle,
                                                     GLOBUS_NULL,
                                                     GLOBUS_NULL);
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
        }
        else
        {
            /* We require a 127 reply and will setup the connection once
             * it has been received.
             */
            target->state = GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_GET;
        }

	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;
        if(target->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK) 
        {
            /* In extended block mode, we will be the passive
             * party. GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT has already
             * filled in client_handle->pasv_address.
             */
            result = 
                globus_l_ftp_client_send_get(target, pathname, GLOBUS_FALSE);
        }
        else
        {
            /* In all other cases we prefer to be active.
             */
            result = 
                globus_l_ftp_client_send_get(target, pathname, GLOBUS_TRUE);
        }
        if(result != GLOBUS_SUCCESS)
        {
            goto result_fault;
        }
        break;

    case GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_PUT:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);

        client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO;

        if(globus_i_ftp_client_can_reuse_data_conn(client_handle))
        {
            /* In this case we do not expect a 127 reply.
             */
            target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;

            /* Create the data connection.
             */
            result =
                globus_ftp_control_data_connect_write(target->control_handle,
                                                      GLOBUS_NULL,
                                                      GLOBUS_NULL);
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_PUT;
        }
 
	target->mask = GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS;
        result = globus_l_ftp_client_send_put(target, pathname, GLOBUS_TRUE);
        if(result != GLOBUS_SUCCESS)
        {
            goto result_fault;
        }           
        break;

    case GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_GET:
	if((!error) &&
	   response->code == 127)
        {
            /* Setup our side of PASV.
             */
	    globus_l_ftp_client_parse_pasv(
	        handle,
	        response, 
	        &client_handle->pasv_address, 
	        &client_handle->num_pasv_addresses);

            if(client_handle->num_pasv_addresses == 1)
            {
                result =
                    globus_ftp_control_local_port(
                        handle,
                        client_handle->pasv_address);
            }
            else
            {
                result =
                    globus_ftp_control_local_spor(
                        handle,
                        client_handle->pasv_address,
                        client_handle->num_pasv_addresses);
            }
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            /* Create the data connection.
             */
            result =
                globus_ftp_control_data_connect_read(target->control_handle,
                                                     GLOBUS_NULL,
                                                     GLOBUS_NULL);
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            target->state = GLOBUS_FTP_CLIENT_TARGET_RETR;                
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
            goto notify_fault;
        }
        break;

    case GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_PUT:        
	if((!error) &&
	   response->code == 127)
        {
            /* Setup our side of PASV.
             */
	    globus_l_ftp_client_parse_pasv(
	        handle,
	        response, 
	        &client_handle->pasv_address, 
	        &client_handle->num_pasv_addresses);

            if(client_handle->num_pasv_addresses == 1)
            {
                result =
                    globus_ftp_control_local_port(
                        handle,
                        client_handle->pasv_address);
            }
            else
            {
                result =
                    globus_ftp_control_local_spor(
                        handle,
                        client_handle->pasv_address,
                        client_handle->num_pasv_addresses);
            }
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            /* Create the data connection.
             */
            result =
                globus_ftp_control_data_connect_write(target->control_handle,
                                                      GLOBUS_NULL,
                                                      GLOBUS_NULL);
            if(result != GLOBUS_SUCCESS)
            {
                goto result_fault;
            }
            
            target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
            goto notify_fault;
        }
        break;

    case GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_TRANSFER:
	if((!error) &&
	   response->code == 127)
        {
	    globus_l_ftp_client_parse_pasv(
	        handle,
	        response, 
	        &client_handle->pasv_address, 
	        &client_handle->num_pasv_addresses);

            /* The passive end of a 3rd party transfer is always the
             * destination.
             */
            target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;

            /* Now continue setting up the source of the transfer.
             */
            target = client_handle->source;

            error =
                globus_i_ftp_client_target_activate(client_handle,
                                                    target,
                                                    &registered);
            if(registered == GLOBUS_FALSE)
            {
                if(client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                   client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_RESTART)
                {
                    break;
                }
                else
                {
                    goto connection_error;
                }
            }
        }
        else
        {
            target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
            goto notify_fault;
        }
        break;

    case GLOBUS_FTP_CLIENT_TARGET_LIST:
    case GLOBUS_FTP_CLIENT_TARGET_RETR:
    case GLOBUS_FTP_CLIENT_TARGET_STOR:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE ||
	    (client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
	     client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION) ||
	    (client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
	     client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT) ||
	    (target->delayed_pasv && client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
	     client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION));

	if((!error) &&
	   response->response_class == GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY)
	{
	    if(response->code == 127 || response->code == 129)
            {
/* XXX START copy from GLOBUS_FTP_CLIENT_TARGET_PASV */    
                globus_l_ftp_client_parse_pasv(
                    handle,
                    response, 
                    &client_handle->pasv_address, 
                    &client_handle->num_pasv_addresses);
                
                if(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER)
                {
                    
                    if(client_handle->num_pasv_addresses == 1)
                    {
                        result =
                            globus_ftp_control_local_port(
                                handle,
                                client_handle->pasv_address);
                    }
                    else
                    {
                        result =
                            globus_ftp_control_local_spor(
                                handle,
                                client_handle->pasv_address,
                                client_handle->num_pasv_addresses);
                    }
                    if(result != GLOBUS_SUCCESS)
                    {
                        goto result_fault;
                    }
                }
        
                /* Store the current data connection in the cache for
                 * the target associated with this transfer, if the server
                 * will support it.
                 */
                if(globus_l_ftp_client_can_cache_data_connection(target))
                {
                    if(client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
                       client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
                       client_handle->op == GLOBUS_FTP_CLIENT_MLSD)
                    {
                        target->cached_data_conn.operation = GLOBUS_FTP_CLIENT_GET;
                    }
                    else
                    {
                        target->cached_data_conn.operation = client_handle->op;
                    }
                          
                    target->cached_data_conn.source = client_handle->source;
                    target->cached_data_conn.dest = client_handle->dest;
                }
                /* In a 3rd party transfer, we need to clear the peer's
                 * data connection cache if we've called passive on the
                 * destination server.
                 */
                if(client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER)
                {
                    memset(&client_handle->source->cached_data_conn,
                           '\0',
                           sizeof(globus_i_ftp_client_data_target_t));
                }       
/* XXX END copy from GLOBUS_FTP_CLIENT_TARGET_PASV */    
            }

            if((target->delayed_pasv && response->code == 125) || 
                response->code == 127 || response->code == 129)
            {
                if(client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER)
                {
                    if(target->state == GLOBUS_FTP_CLIENT_TARGET_RETR ||
                        target->state == GLOBUS_FTP_CLIENT_TARGET_LIST)
                    {
                        result = globus_ftp_control_data_connect_read(
                            handle,
                            GLOBUS_NULL,
                            GLOBUS_NULL);
                    }
                    else
                    {
                        result = globus_ftp_control_data_connect_write(
                            handle,
                            GLOBUS_NULL,
                            GLOBUS_NULL);
                    }
                    if(result != GLOBUS_SUCCESS)
                    {
                        goto result_fault;
                    }
                }
                else
                {
                    target = client_handle->source;
            
                    error =
                        globus_i_ftp_client_target_activate(client_handle,
                                                            target,
                                                            &registered);
                    if(registered == GLOBUS_FALSE)
                    {
                        if(client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                           client_handle->state==GLOBUS_FTP_CLIENT_HANDLE_RESTART)
                        {
                            break;
                        }
                        else
                        {
                            goto connection_error;
                        }
                    }
                } 
            } 
            
	    if(response->code == 127 || response->code == 129)
            {
                break;
            }

	    /*
	     * this should be a "connected" or "using existing
	     * data connection" response
	     */
	    if(client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
	       client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
	       client_handle->op == GLOBUS_FTP_CLIENT_MLSD ||
	       client_handle->op == GLOBUS_FTP_CLIENT_GET  ||
	       client_handle->op == GLOBUS_FTP_CLIENT_PUT)
	    {
		target->state =
		    GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA;

		error =
		    globus_i_ftp_client_data_dispatch_queue(client_handle);

		if(error != GLOBUS_SUCCESS)
		{
		    globus_i_ftp_client_plugin_notify_fault(
			client_handle,
			target->url_string,
			error);
                    
                    if(client_handle->err)
                    {
		        globus_object_free(error);
                error = NULL;
		    }
		    else
		    {
		        client_handle->err = error;
		        error = GLOBUS_NULL;
		    }

		    if(client_handle->state == 
		        GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		        client_handle->state == 
		            GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	                client_handle->state == 
	                    GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
		    {
			break;
		    }
		    globus_assert(
			client_handle->state ==
			GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
			client_handle->state ==
			GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
			client_handle->state ==
			GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET);

		    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
		    target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
		    globus_ftp_control_force_close(
			target->control_handle,
			globus_i_ftp_client_force_close_callback,
			target);
		}
	    }
	    else
	    {
		/* performance or restart markers are ok for 3rd party */
		if(response->code == 111)
		{
		    globus_l_ftp_client_parse_restart_marker(client_handle,
							     response);
		}
	    }
	}
	else if((!error) &&
		response->response_class
		== GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
	{
            if(client_handle->attr.pipeline_callback)
            {                
                globus_i_ftp_client_url_ent_t * url_ent = NULL;
                globus_bool_t                   added = GLOBUS_FALSE;
                
                client_handle->no_callback_count--;
                if(target == client_handle->source)
                {
                    result = globus_l_ftp_client_pp_xfer_src_add(
                        client_handle, handle, target, &added);

                    url_ent = globus_fifo_dequeue(
                        &client_handle->src_response_pending_queue);
                    if(target->url_string)
                    {
                        globus_free(target->url_string);
                    }
                    target->url_string = globus_libc_strdup(url_ent->source_url);
                    if(client_handle->source_url)
                    {
                        globus_free(client_handle->source_url);
                    }
                    client_handle->source_url = url_ent->source_url;
                    globus_url_destroy(&url_ent->src_url);
                    globus_free(url_ent);
            
                    target->state = GLOBUS_FTP_CLIENT_TARGET_RETR;                        
                }
                else if(target == client_handle->dest)
                {
                    result = globus_l_ftp_client_pp_xfer_dst_add(
                        client_handle, handle, target, &added);

                    url_ent = globus_fifo_dequeue(
                        &client_handle->dst_response_pending_queue);
                    if(target->url_string)
                    {
                        globus_free(target->url_string);
                    }
                    target->url_string = globus_libc_strdup(url_ent->dest_url);
                    if(client_handle->dest_url)
                    {
                        globus_free(client_handle->dest_url);
                    }
                    client_handle->dest_url = url_ent->dest_url;
                    globus_url_destroy(&url_ent->dst_url);
                    globus_free(url_ent);
            
                    target->state = GLOBUS_FTP_CLIENT_TARGET_STOR;
                }
            }
	    if(client_handle->state ==
                GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER)
	    {
                client_handle->state =
                    GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE;
            }
	    else if(client_handle->state ==
                GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE)
	    {
                if(client_handle->no_callback_count == 0)
                {
                    client_handle->source->state = 
                        GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION;
                    client_handle->dest->state = 
                        GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION;

                    globus_i_ftp_client_transfer_complete(client_handle);

                    goto do_return;
                }
                else
                {
                    goto finish;
                }
	    }
	    else
	    {
	        /* this shouldnt really be possible, but handle it */
	        target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE;
	        globus_i_ftp_client_data_flush(client_handle);
	        memset(&target->cached_data_conn,
                    '\0', sizeof(globus_i_ftp_client_data_target_t));
                globus_ftp_control_data_force_close(
		    target->control_handle,
		    globus_l_ftp_client_data_force_close_callback,
		    GLOBUS_NULL);
	        goto redo;
	    }
	}
	else
	{
	    if((!client_handle->err) && (!error))
	    {
		client_handle->err = 
		    GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response);

		globus_ftp_control_data_force_close(
		    target->control_handle,
		    globus_l_ftp_client_data_force_close_callback,
		    GLOBUS_NULL);
	    }
	    else if(!client_handle->err)
	    {
		client_handle->err = globus_object_copy(error);
	    }
	    globus_i_ftp_client_plugin_notify_fault(
		client_handle,
		target->url_string,
		client_handle->err);

	    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
	    {
		break;
	    }

	    globus_assert(
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST ||
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET ||
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO ||
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER ||
		client_handle->state ==
		GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE ||
		(client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
		 client_handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION) ||
		(client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER &&
		 client_handle->state ==
		 GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT));
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
            
            goto connection_error;
	}

	break;
    case GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA:
    case GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE:
	/*
	 * We've received the callback for this operation, so now we
	 * just need to wait for the final data callbacks
	 */
	if(error)
	{
	    if(client_handle->err == GLOBUS_SUCCESS)
	    {
		client_handle->err = globus_object_copy(error);
	    }
	    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;

	    goto notify_fault;
	}
	else if(response->response_class ==
		GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY)
	{
	    if(response->code == 111)
	    {
		globus_l_ftp_client_parse_restart_marker(client_handle,
							 response);
	    }
	    break;
	}
        else
        {
            if(response->response_class != GLOBUS_FTP_POSITIVE_COMPLETION_REPLY
                && client_handle->err == GLOBUS_SUCCESS)
            {
                /* any other response must be a transient error such as 
                 * 426 data timeout
                 */
                client_handle->err = 
                    GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response);
            }
            
            if(target->state == GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE)
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE;
            }
            else
            {
                target->state = GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK;
            }
        }
	
	break;

    case GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE:
	/* Reset the state to setup_connection, so that the url
	 * caching code knows to keep this one around.
	 */
	if(!error)
	{
	    if(response->response_class ==
	       GLOBUS_FTP_POSITIVE_PRELIMINARY_REPLY)
	    {
		if(response->code == 111)
		{
		    globus_l_ftp_client_parse_restart_marker(client_handle,
							     response);
		}
		break;
	    }
	    else
	    {
		target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;

		if(response->response_class !=
		   GLOBUS_FTP_POSITIVE_COMPLETION_REPLY)
		{
		    if(client_handle->err == GLOBUS_SUCCESS)
		    {
			client_handle->err =
			    GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response);
		    }
		    if(client_handle->op != GLOBUS_FTP_CLIENT_MDTM &&
		       client_handle->op != GLOBUS_FTP_CLIENT_SIZE &&
		       client_handle->op != GLOBUS_FTP_CLIENT_FEAT &&
		       client_handle->op != GLOBUS_FTP_CLIENT_CKSM &&
		       client_handle->op != GLOBUS_FTP_CLIENT_MLST &&
		       client_handle->op != GLOBUS_FTP_CLIENT_CWD &&
		       client_handle->op != GLOBUS_FTP_CLIENT_STAT)
		    {
			client_handle->state =
			    GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
		    }
		}
		if(client_handle->op == GLOBUS_FTP_CLIENT_MDTM &&
		   response->code == 213)
		{
		    globus_l_ftp_client_parse_mdtm(client_handle,
			                           response);
		}
		else if(client_handle->op == GLOBUS_FTP_CLIENT_SIZE &&
			response->code == 213)
		{
		    globus_libc_scan_off_t((char *) response->response_buffer+4,
					   client_handle->size_pointer,
					   GLOBUS_NULL);
		}
		else if(client_handle->op == GLOBUS_FTP_CLIENT_CKSM &&
			response->code == 213)
		{
		    globus_l_ftp_client_parse_cksm(client_handle,
				    		   response);
				    		   
		}
		else if(client_handle->op == GLOBUS_FTP_CLIENT_FEAT)
		{
		    for(i = 0; i < GLOBUS_FTP_CLIENT_FEATURE_MAX; i++)
                    {
                        globus_i_ftp_client_feature_set(
                            client_handle->features_pointer,
                            i,
                            globus_i_ftp_client_feature_get(
                                target->features, i));
                    }
		}
		else if(client_handle->op == GLOBUS_FTP_CLIENT_CWD &&
			response->code == 250)
		{
		    globus_l_ftp_client_parse_cwd(client_handle,
						  response);
		}
		else if(client_handle->op == GLOBUS_FTP_CLIENT_MLST &&
			response->code == 250)
		{
		    globus_l_ftp_client_parse_mlst(client_handle,
			                           response);
		}
		else if(client_handle->op == GLOBUS_FTP_CLIENT_STAT)
		{
		    globus_l_ftp_client_parse_stat(client_handle,
			                           response);
		}
	    }
	}
	else
	{
	    target->state = GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION;
	    if(client_handle->err == GLOBUS_SUCCESS)
	    {
		client_handle->err = globus_object_copy(error);
	    }
	}
	
	globus_i_ftp_client_transfer_complete(client_handle);
	
	goto do_return;

	break;

    case GLOBUS_FTP_CLIENT_TARGET_FAULT:
    case GLOBUS_FTP_CLIENT_TARGET_CLOSED:
	/*
	 * This state only happens if this is a callback which was a
	 * result of a force_close. We do nothing, and the force_close
	 * callback deals with the failure.
	 */
	break;
    case GLOBUS_FTP_CLIENT_TARGET_NOOP:
	globus_assert(
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION ||
	    client_handle->state ==
	    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION);
        
        /* response will be NULL if the NOOP was faked */
	if((!error) && (!response ||
	   response->response_class == GLOBUS_FTP_POSITIVE_COMPLETION_REPLY))
	{
	    /* NOOP successful, we can re-use this target */
	    target->state =
		GLOBUS_FTP_CLIENT_TARGET_SETUP_TYPE;
	    goto redo;
	}
	else
	{
	    globus_i_ftp_client_target_t *		new_target;
	    /*
	     * NOOP failed---This means that the cached target went
	     * bad. We'll discard this target, and find a new
	     * one.
	     */
	    globus_assert(client_handle->source == target ||
			  client_handle->dest == target);

	    if(client_handle->source == target)
	    {
		error =
		    globus_i_ftp_client_target_find(client_handle,
						    client_handle->source_url,
						    target->attr,
						    &new_target);
		if(error != GLOBUS_SUCCESS)
		{
		    target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

		    goto notify_fault;
		}
		client_handle->source = new_target;
	    }
	    else if(client_handle->dest == target)
	    {
		error =
		    globus_i_ftp_client_target_find(client_handle,
						    client_handle->dest_url,
						    target->attr,
						    &new_target);

		if(error != GLOBUS_SUCCESS)
		{
		    target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;

		    goto notify_fault;
		}
		client_handle->dest = new_target;
	    }

	    /* Mark the old target for destruction... */
	    target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
	    /* And release it */
	    globus_i_ftp_client_target_release(client_handle,
					       target);

	    /* Start this new target off */
	    error = globus_i_ftp_client_target_activate(client_handle,
							new_target,
							&registered);

	    if(registered == GLOBUS_FALSE)
	    {
		globus_assert(error ||
		    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE);
		/*
		 * A restart or abort happened during activation, before any
		 * callbacks were registered. We must deal with them here.
		 */
		if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
		    client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
	            client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
		{
		    break;
		}
		else if(error != GLOBUS_SUCCESS)
		{
		    globus_i_ftp_client_plugin_notify_fault(
			client_handle,
			target->url_string,
			error);
                    
		    goto connection_error;
		}
	    }
	}
	break;
    default:
	globus_assert(0 && "Invalid state");
    }
 finish:
    globus_i_ftp_client_handle_unlock(client_handle);
 do_return:   
    
    if(error)
    {
        globus_object_free(error);
    }
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_i_ftp_client_response_callback() exiting\n"));
    globus_i_ftp_client_debug_states(2, client_handle);

    return;

 result_fault:
    if(error)
    {
        globus_object_free(error);
    }
    error = globus_error_get(result);
 notify_fault:
    globus_i_ftp_client_plugin_notify_fault(
	client_handle,
	target->url_string,
	error);
 connection_error:
    globus_l_ftp_client_connection_error(client_handle,
					 target,
					 error,
					 response);
    
    globus_object_free(error);
    globus_i_ftp_client_debug_printf(1, (stderr, 
        "globus_i_ftp_client_response_callback() exiting with error\n"));
    globus_i_ftp_client_debug_states(2, client_handle);

    return;
}
/* globus_i_ftp_client_response_callback() */

/**
 * Parse the response to the "SITE HELP" FTP command.
 *
 * This command response might list all of the SITE commands
 * implemented by the server. This function parses the list to see if
 * any of the known methods of changing the TCP buffer size for the
 * transfer are included in this site response. The list of SITE
 * commands is based on similar code in the ncftp package. These
 * include
 * - RETRBUFSIZE
 * - RBUFSZ
 * - RBUFSIZ
 * - STORBUFSIZE
 * - SBUFSZ
 * - SBUFSIZ
 * - BUFISZE
 *
 * @param target
 *        The target structure associated with this ftp response.
 * @param response
 *        The response structure returned from the ftp control library.
 */
static
globus_object_t *
globus_l_ftp_client_parse_site_help(
    globus_i_ftp_client_target_t *		target,
    globus_ftp_control_response_t *		response)
{
    char * p;

    if(strstr((char *) response->response_buffer, "RETRBUFSIZE") != 0)
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_RETRBUFSIZE,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(strstr((char *) response->response_buffer, "RBUFSZ") != 0)
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_RBUFSZ,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(((p = strstr((char *) response->response_buffer, "RBUFSIZ")) != 0) &&
	!isupper(*(p-1)))
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_RBUFSIZ,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(strstr((char *) response->response_buffer, "STORBUFSIZE") != 0)
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_STORBUFSIZE,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(strstr((char *) response->response_buffer, "SBUFSZ") != 0)
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_SBUFSIZ,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(((p = strstr((char *) response->response_buffer, "SBUFSIZ")) != 0) &&
	!isupper(*(p-1)))
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_SBUFSIZ,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(((p = strstr((char *) response->response_buffer, "BUFSIZE")) != 0) &&
	!isupper(*(p-1)))
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_BUFSIZE,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(((p = strstr((char *) response->response_buffer, "CHMOD")) != 0) &&
	!isupper(*(p-1)))
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_CHMOD,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    if(((p = strstr((char *) response->response_buffer, "AUTHZ_ASSERT")) != 0) 
        && !isupper(*(p-1)))
    {
        globus_i_ftp_client_feature_set(
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_AUTHZ_ASSERT,
            GLOBUS_FTP_CLIENT_TRUE);
    }
    
    
    return GLOBUS_SUCCESS;
}
/* globus_l_ftp_client_parse_site_help() */

/**
 * Parse the response to a FEAT command, to figure out what the
 * extensions the server implements.
 *
 * We are currently interested in this code for extensions described
 * in our GSI FTP extensions document.
 *
 * @param target
 *        The target structure associated with this ftp response.
 * @param response
 *        The response structure returned from the ftp control library.
 */
static
void
globus_l_ftp_client_parse_feat(
    globus_i_ftp_client_target_t *		target,
    globus_ftp_control_response_t *		response)
{
    char *					p;
    char *					pstart;
    globus_bool_t				first = GLOBUS_TRUE;

    if(response->code != 211)
    {
	return;
    }
    p = globus_libc_strdup((char *) response->response_buffer);
    pstart = p;
    while(1)
    {
	char *				eol;
	eol = strstr(p, CRLF);
	if(eol == 0)
	{
	    int i;
	    /* Last line should contain "211 End" CRLF ONLY */
	    globus_libc_free(pstart);

	    /*
	     * If there are any features which are in the unknown state,
	     * set them to false now.
	     */
	    for(i = GLOBUS_FTP_CLIENT_FIRST_FEAT_FEATURE;
		i <= GLOBUS_FTP_CLIENT_LAST_FEAT_FEATURE;
		i++)
	    {
		if(globus_i_ftp_client_feature_get(
		    target->features, i) == GLOBUS_FTP_CLIENT_MAYBE)
		{
		    globus_i_ftp_client_feature_set(
		        target->features, i, GLOBUS_FTP_CLIENT_FALSE);
		}
	    }
	    return;
	}
	else if(first)
	{
	    /* First line contains no feature. */
	    p = eol + 2;
	    first = GLOBUS_FALSE;
	    continue;
	}
	else
	{
	    /* An actual feature! */
	    char *			feature_label;
	    char *			feature_parms;

	    *eol = '\0';

	    feature_label = p + 1;
	    feature_parms = feature_label;

	    /* VCHAR (%x21-%7E) */
	    while((*feature_parms) >= ((char)0x21) &&
		  (*feature_parms) <= ((char)0x7e))
	    {
		feature_parms++;
	    }
	    if(strncmp(feature_label, "REST", 4) == 0)
	    {
		if(strstr(feature_parms, "STREAM"))
		{
		    globus_i_ftp_client_feature_set(
		        target->features,
		        GLOBUS_FTP_CLIENT_FEATURE_REST_STREAM,
		        GLOBUS_FTP_CLIENT_TRUE);
		}
	    }
	    else if(strncmp(feature_label, "PARALLEL", 8) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_PARALLELISM,
	            GLOBUS_FTP_CLIENT_TRUE);
	    }
	    else if(strncmp(feature_label, "DCAU", 4) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_DCAU,
	            GLOBUS_FTP_CLIENT_TRUE);
		/* Per our extensions document, if server publishes
		 * DCAU feature, it must default to DCAU S(elf)
		 * if we used RFC 2228 authentication.
		 *
		 * gsi-wuftpd 0.5 and below are broken in this regard.
		 */
		if(target->url.scheme_type == GLOBUS_URL_SCHEME_GSIFTP)
		{
		    target->dcau.mode = GLOBUS_FTP_CONTROL_DCAU_DEFAULT;
		}
	    }
	    else if(strncmp(feature_label, "ESTO", 4) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_ESTO,
	            GLOBUS_FTP_CLIENT_TRUE);
	    }
	    else if(strncmp(feature_label, "ERET", 4) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_ERET,
	            GLOBUS_FTP_CLIENT_TRUE);
	    }
	    else if(strncmp(feature_label, "SBUF", 4) == 0)
	    {
		int i;
		globus_i_ftp_client_feature_set(
		    target->features,
		    GLOBUS_FTP_CLIENT_FEATURE_SBUF,
		    GLOBUS_FTP_CLIENT_TRUE);
		/* If SBUF is supported, then don't bother with other
		 * buffer size commands
		 */

		for(i = 0; i < GLOBUS_FTP_CLIENT_FEATURE_SBUF; i++)
		{
		    if(globus_i_ftp_client_feature_get(
		        target->features, i) == GLOBUS_FTP_CLIENT_MAYBE)
		    {
		        globus_i_ftp_client_feature_set(
		            target->features, i, GLOBUS_FTP_CLIENT_FALSE);
		    }
		}
	    }
	    else if(strncmp(feature_label, "ABUF", 4) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_ABUF,
	            GLOBUS_FTP_CLIENT_TRUE);
	    }
	    else if(strncmp(feature_label, "SIZE", 4) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_SIZE,
	            GLOBUS_FTP_CLIENT_TRUE);
	    }
            else if(strncmp(feature_label, "GETPUT", 6) == 0)
            {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_GETPUT,
	            GLOBUS_FTP_CLIENT_TRUE);                
            }
	    else if(strncmp(feature_label, "MLST", 4) == 0)
	    {
	        globus_i_ftp_client_feature_set(
	            target->features,
	            GLOBUS_FTP_CLIENT_FEATURE_MLST,
	            GLOBUS_FTP_CLIENT_TRUE);
	    }
            else if(strncmp(feature_label, "PASV", 4) == 0)
	    {
		if(strstr(feature_parms, "AllowDelayed"))
		{
		    globus_i_ftp_client_feature_set(
		        target->features,
		        GLOBUS_FTP_CLIENT_FEATURE_DELAYED_PASV,
		        GLOBUS_FTP_CLIENT_TRUE);
		}
	    }
	    p = eol + 2;

	}
    }
}
/* globus_l_ftp_client_parse_feat() */

/**
 * Parse the response to the "PASV" FTP command.
 *
 * The command response contains the IP address of a TCP socket
 * listening for a connection. This function parses the response, and
 * sets the host_port structure to contain the parsed value of the
 * address.
 *
 * If the response could not be parsed, the port is set to 0.
 *
 * @param response
 *        The response structure returned from the ftp control
 *        library.
 * @param host_port
 *        A pointer to a structure to contain the socket address of the
 *        listener.
 */
static
void
globus_l_ftp_client_parse_pasv(
    globus_ftp_control_handle_t *               handle,
    globus_ftp_control_response_t *		response,
    globus_ftp_control_host_port_t **		host_port,
    int *					num_pasv_addresses)
{
    char *					p;
    int						port[2] = {0,0};
    int						rc;
    int						i;
    int						consumed;
    char                                        delim;
    char                                        buf[100];
    
    p = strchr((char *) response->response_buffer, '(');
    if(response->code == 229)
    {
        if(!p)
        {
            /* spas */
            (*num_pasv_addresses) = -2;
            
            p = (char *) response->response_buffer;
            while(GLOBUS_NULL != (p = strchr(p, '\n')))
            {
                (*num_pasv_addresses)++;
                p++;
            }
    
            /* skip the first line of the 229 response */
            p = strchr((char *) response->response_buffer, '\n') + 1;
            while(isspace(*p)) p++;
        }
        else
        {
            /* epsv */
            (*num_pasv_addresses) = 1;
            p++;
        }
    }
    else
    {
	(*num_pasv_addresses) = 1;

	/* skip the initial 227 in the response */
	if(!p)
	{
	    p = (char *) response->response_buffer + 3;
	    while(isspace(*p)) p++;
	}
	else
	{
	    p++;
	}
    }
    (*host_port) = globus_libc_calloc((*num_pasv_addresses),
				   sizeof(globus_ftp_control_host_port_t));

    if(isdigit(delim = *p))
    {
        delim = 0;
    }
    
    for(i = 0; i < (*num_pasv_addresses) && *p; i++)
    {
        if(delim)
        {
            /* |prt|ip|port| */
            while(*p && *p != delim) p++;
            if(*p)
            {
                p++;
            }
            while(*p && *p != delim) p++;
            if(*p)
            {
                int                     j = 0;
                char *                  s;
                char *                  c;
                
                p++;
                c = strchr(p, ':');
                s = strchr(p, delim);
                
                if(*p != delim)
                {
                    if(c && c < s)
                    {
                        buf[j++] = '[';
                    }
                
                    while(j < sizeof(buf) - 1 && p < s)
                    {
                        buf[j++] = *(p++);
                    }
                    
                    if(*p == delim && j + 7 < sizeof(buf))
                    {
                        p++;
                        if(*buf == '[')
                        {
                            buf[j++] = ']';
                        }
                        
                        buf[j++] = ':';
                        while(j < sizeof(buf) - 2 && *p && *p != delim)
                        {
                            buf[j++] = *(p++);
                        }
                        if(*p == delim)
                        {
                            p++;
                        }
                    }
                    
                    buf[j] = 0;
                    
                    globus_libc_contact_string_to_ints(buf,
                        (*host_port)[i].host,
                        &(*host_port)[i].hostlen,
                        &(*host_port)[i].port);
                }
                else if((*num_pasv_addresses) == 1)
                {
                    /* only accept missing ip for epsv responses */
                    p++;
                    globus_ftp_control_client_get_connection_info_ex(
                        handle, GLOBUS_NULL, &(*host_port)[i]);
                    (*host_port)[i].port = atoi(p);
                }
            }
        }
        else
        {
            while(*p && !isdigit(*p)) p++;
    
            rc = sscanf(p,
                        "%d,%d,%d,%d,%d,%d%n",
                        &(*host_port)[i].host[0],
                        &(*host_port)[i].host[1],
                        &(*host_port)[i].host[2],
                        &(*host_port)[i].host[3],
                        &port[0],
                        &port[1],
                        &consumed);
            if(rc >= 6)
            {
                (*host_port)[i].port = (port[0] * 256) + port[1];
            }
            else
            {
                host_port[i]->port = 0;
            }
            p += consumed;
        }
    }
}

/**
 * Construct a string suitable for use as an argument to OPTS RETR.
 *
 * @param target
 *        The target desired containing the layout parameters.
 *
 * @return This function returns the string representation of the
 *         layout parameter, if it is different from the current
 *         setting on the control handle associated with the
 *         target. If the string is non-NULL, it must be freed by the
 *         caller.
 */
static
char *
globus_l_ftp_client_layout_string(
    globus_i_ftp_client_target_t *		target)
{
    char *					ptr = GLOBUS_NULL;
    globus_size_t				length;

    length = 16;		/* " StripeLayout=;\0" */

    switch(target->attr->layout.mode)
    {
    case GLOBUS_FTP_CONTROL_STRIPING_PARTITIONED:
	if(target->layout.mode != GLOBUS_FTP_CONTROL_STRIPING_PARTITIONED)
	{
	    length += 11;	/* "Partitioned" */
	    ptr = globus_libc_malloc(length);
	    sprintf(ptr, "StripeLayout=Partitioned;");
	}
	break;
    case GLOBUS_FTP_CONTROL_STRIPING_BLOCKED_ROUND_ROBIN:
	if((target->layout.mode !=
	        GLOBUS_FTP_CONTROL_STRIPING_BLOCKED_ROUND_ROBIN) ||
	   (target->attr->layout.round_robin.block_size !=
	        target->layout.round_robin.block_size))
	{
	    length += 18;	/* "Blocked;BlockSize=" */
	    length +=
		globus_i_ftp_client_count_digits(
		    target->attr->layout.round_robin.block_size);
	    ptr = globus_libc_malloc(length);
	    sprintf(ptr, "StripeLayout=Blocked;BlockSize=%d;",
		    (int) target->attr->layout.round_robin.block_size);
	}
	break;
    case GLOBUS_FTP_CONTROL_STRIPING_NONE:
	break;
    }
    return ptr;
}

/**
 * Construct a string suitable for use as an argument to OPTS RETR.
 *
 * @param target
 *        The target desired containing the parallelism parameters.
 *
 * @return This function returns the string representation of the
 *         parallelism parameter, if it is different from the current
 *         setting on the control handle associated with the target. If the
 *         string is non-NULL, it must be freed by the caller.
 */
static
char *
globus_l_ftp_client_parallelism_string(
    globus_i_ftp_client_target_t *		target)
{
    char *					ptr = GLOBUS_NULL;
    globus_size_t				length;

    length = 17;		/* " Parallelism=,,;\0" */

    switch(target->attr->parallelism.mode)
    {
    case GLOBUS_FTP_CONTROL_PARALLELISM_FIXED:
	if((target->parallelism.mode !=
	        GLOBUS_FTP_CONTROL_PARALLELISM_FIXED) ||
	   (target->attr->parallelism.fixed.size !=
	        target->parallelism.fixed.size))
	{
	    length += 3 *
		globus_i_ftp_client_count_digits(
		    target->attr->parallelism.fixed.size);
	    ptr = globus_libc_malloc(length);
	    sprintf(ptr, "Parallelism=%d,%d,%d;",
		    (int) target->attr->parallelism.fixed.size,
		    (int) target->attr->parallelism.fixed.size,
		    (int) target->attr->parallelism.fixed.size);
	}
	break;
    case GLOBUS_FTP_CONTROL_PARALLELISM_NONE:
        if((target->parallelism.mode !=
	        GLOBUS_FTP_CONTROL_PARALLELISM_NONE) &&
	   (target->parallelism.fixed.size != 1))
	{
	    ptr = globus_libc_strdup("Parallelism=1,1,1;");
	}
	break;
    }
    return ptr;
}

/**
 * faked force close oneshot wrapper
 */
static
void
globus_i_ftp_client_faked_force_close_callback(
    void *                              user_args)
{
    globus_i_ftp_client_target_t *      target;
    
    target = (globus_i_ftp_client_target_t *) user_args;
    
    globus_i_ftp_client_force_close_callback(
        target,
        target->control_handle,
        GLOBUS_SUCCESS /* don't care */,
        GLOBUS_NULL);
}

/**
 * Handle a connection fault.
 *
 * This function is called from the FTP state machine when a fault
 * occurs. A fault in this scope is one of the following:
 * - callback registration failure
 * - unexpected or invalid response to connect or authenticate
 * - error passed to response callback
 *
 * This code handles the fault by closing the control connection as
 * nicely as possible, and then calling the user's "complete"
 * callback.
 *
 * There aren't any restart or abort points in this code, but if a
 * restart happened already, in addition to th fault, then we can deal
 * with it.
 *
 * handle is locked on entry, unlocked on exit
 */
static
void
globus_l_ftp_client_connection_error(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_i_ftp_client_target_t *		target,
    globus_object_t *				error,
    globus_ftp_control_response_t *		response)
{
    globus_result_t				result;
    GlobusFuncName(globus_l_ftp_client_connection_error);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_connection_error() entering\n"));
    globus_i_ftp_client_debug_states(2, client_handle);

    if(client_handle->err == GLOBUS_NULL)
    {
	if(error)
	{
	    client_handle->err = globus_object_copy(error);
	}
	else if(response && response->response_buffer)
	{
	    client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response);
	}
	else
	{
	    client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_PROTOCOL_ERROR();
	}
    }
    
    /*
     * Now, let's figure out to cope with this fault. Get/Put are
     * similar, but 3rd party transfer is rather different
     */
    if(client_handle->op == GLOBUS_FTP_CLIENT_GET  ||
       client_handle->op == GLOBUS_FTP_CLIENT_LIST ||
       client_handle->op == GLOBUS_FTP_CLIENT_NLST ||
       client_handle->op == GLOBUS_FTP_CLIENT_MLSD ||
       client_handle->op == GLOBUS_FTP_CLIENT_PUT)
    {
        if(error)
        {
            /* Mark the target for destruction, since we faulted on it. */
            target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
        }
        
	/* If we haven't been restarted yet, then we will deal with
	 * the fault here.
	 */
	if(client_handle->state != GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;

	    /*
	     * If we're in this function, then whatever buffers the
	     * user has registered will not yet be passed to the
	     * control library. That means we've got to call back for all
	     * of the data buffers which have been registered.
	     *
	     * Because we are in the FAILURE state, the plugins may
	     * *not* do a restart now.
	     */
	    globus_i_ftp_client_data_flush(client_handle);

	    /*
	     * If there are no more blocks registered with the control
	     * library, we're done, otherwise, the callbacks for those
	     * blocks will pick this up and call complete.
	     */

	    if(client_handle->num_active_blocks == 0)
	    {
		globus_i_ftp_client_transfer_complete(client_handle);
                globus_i_ftp_client_debug_printf(1, 
                    (stderr, "globus_l_ftp_client_connection_error() exiting\n"));
                globus_i_ftp_client_debug_states(2, client_handle);
                
		return;
	    }
	}
    }
    else if(client_handle->op == GLOBUS_FTP_CLIENT_TRANSFER)
    {
	if(client_handle->state != GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    globus_i_ftp_client_target_t *		other_target = 0;

	    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
	    /* In a third party transfer, this fault can happen on
	     * either the source or destination target's control
	     * handle.
	     */
	    if(client_handle->source == target)
	    {
		other_target = client_handle->dest;
	    }
	    else if(client_handle->dest == target)
	    {
		other_target = client_handle->source;
	    }
	    globus_assert(other_target != GLOBUS_NULL);

	    /*
	     * if the other target is in the start or setup_connection
	     * state, then we can release complete w/failure
	     * immediately. Otherwise, we need to either abort the
	     * active command on it, or force it to close.
	     */
	    if(other_target->state == GLOBUS_FTP_CLIENT_TARGET_START ||
	       other_target->state == GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION)
	    {
	        if(target->state != GLOBUS_FTP_CLIENT_TARGET_FAULT &&
	            target->state != GLOBUS_FTP_CLIENT_TARGET_CLOSED)
	        {
	            target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
	            
                    /*
                     * Kill the current target---the other one is idle
                     */
                    result = globus_ftp_control_force_close(
                        target->control_handle,
                        globus_i_ftp_client_force_close_callback,
                        target);
                        
                    if(result != GLOBUS_SUCCESS)
                    {
                        /* Shoot, that didn't work. Fake it. */
                        globus_callback_register_oneshot(
                            GLOBUS_NULL,
                            &globus_i_reltime_zero,
                            globus_i_ftp_client_faked_force_close_callback,
                            target);
                    }
                }
	    }
	    else
	    {
	        if(other_target->state != GLOBUS_FTP_CLIENT_TARGET_FAULT &&
	            other_target->state != GLOBUS_FTP_CLIENT_TARGET_CLOSED)
	        {
                    /*
                     * If the target is doing something else, then we will
                     * just kill it.
                     */
                    other_target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
        
                    result = globus_ftp_control_force_close(
                        other_target->control_handle,
                        globus_i_ftp_client_force_close_callback,
                        other_target);
                    if(result)
                    {
                        /* Shoot, that didn't work. Fake it */
                        globus_callback_register_oneshot(
                            GLOBUS_NULL,
                            &globus_i_reltime_zero,
                            globus_i_ftp_client_faked_force_close_callback,
                            other_target);
                    }
                }
                
                if(target->state != GLOBUS_FTP_CLIENT_TARGET_FAULT &&
	            target->state != GLOBUS_FTP_CLIENT_TARGET_CLOSED)
	        {
    		    /*
                     * Kill the current target---the other one is idle now.
                     */
                    target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
    
                    result = globus_ftp_control_force_close(
                        target->control_handle,
                        globus_i_ftp_client_force_close_callback,
                        target);
                    
                    if(result != GLOBUS_SUCCESS)
                    {
                        /* Shoot, that didn't work. Fake it. */
                        globus_callback_register_oneshot(
                            GLOBUS_NULL,
                            &globus_i_reltime_zero,
                            globus_i_ftp_client_faked_force_close_callback,
                            target);
                    }
                }
	    }
	}
	else if(error)
        {
            /* Mark the target for destruction, since we faulted on it. */
            target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
        }
    }
    else /* commands that don't involve a data channel*/
    {
        if(error)
        {
            /* Mark the target for destruction, since we faulted on it. */
            target->state = GLOBUS_FTP_CLIENT_TARGET_FAULT;
        }
        
	/* If we haven't been restarted yet, then we will deal with
	 * the fault here.
	 */
	if(client_handle->state != GLOBUS_FTP_CLIENT_HANDLE_RESTART)
	{
	    client_handle->state = GLOBUS_FTP_CLIENT_HANDLE_FAILURE;
	    globus_i_ftp_client_transfer_complete(client_handle);
            
            globus_i_ftp_client_debug_printf(1, 
                (stderr, "globus_l_ftp_client_connection_error() exiting\n"));

	    return;
	}
    }

    globus_i_ftp_client_handle_unlock(client_handle);
    
    globus_i_ftp_client_debug_printf(1, 
        (stderr, "globus_l_ftp_client_connection_error() exiting\n"));
    globus_i_ftp_client_debug_states(2, client_handle);
    
    return;
}
/* globus_l_ftp_client_connection_error() */

static
const char *
globus_l_ftp_client_guess_buffer_command(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target)
{
    int						i;
    globus_bool_t				stor_desired = GLOBUS_FALSE;
    globus_bool_t				retr_desired = GLOBUS_FALSE;
    int                                         first_maybe;

    if(handle->op == GLOBUS_FTP_CLIENT_GET ||
       handle->op == GLOBUS_FTP_CLIENT_LIST ||
       handle->op == GLOBUS_FTP_CLIENT_NLST ||
       handle->op == GLOBUS_FTP_CLIENT_MLSD ||
       (handle->op == GLOBUS_FTP_CLIENT_TRANSFER && handle->source == target))
    {
	retr_desired = GLOBUS_TRUE;
    }
    if(handle->op == GLOBUS_FTP_CLIENT_PUT ||
       (handle->op == GLOBUS_FTP_CLIENT_TRANSFER && handle->dest == target))
    {
	stor_desired = GLOBUS_TRUE;
    }
    
    first_maybe = -1;
    for(i = 0; i < GLOBUS_FTP_CLIENT_LAST_BUFFER_COMMAND; i++)
    {
        globus_ftp_client_tristate_t            is_supported;
        
        is_supported = globus_i_ftp_client_feature_get(target->features, i);
        
	if(is_supported &&
	   ((globus_l_ftp_client_buffer_cmd_info[i].stor_ok && stor_desired) ||
	    (globus_l_ftp_client_buffer_cmd_info[i].retr_ok && retr_desired)))
	{
	    if(is_supported == GLOBUS_FTP_CLIENT_TRUE)
	    {
	        return globus_l_ftp_client_buffer_cmd_info[i].string;
	    }
	    else if(first_maybe < 0)
	    {
	        first_maybe = i;
	    }
	}
    }
    
    if(first_maybe >= 0)
    {
        return globus_l_ftp_client_buffer_cmd_info[first_maybe].string;
    }
    
    return NULL;
}
/* globus_l_ftp_client_guess_buffer_command() */

/**
 * Update the list of tcp buffer mode features based on the
 * success/failure of the last attempt.
 *
 * These function update the feature table located in a target handle,
 * based on whether the last ftp buffer-setting command succeeded or
 * failed.
 *
 * @param handle
 *        The client handle that the buffer-setting attempt happened
 *        on.
 * @param target
 *        The actual ftp target that we sent the command to.
 * @param ok
 *        If set to GLOBUS_TRUE, then the last ftp buffer command
 *	  worked. Otherwise, it failed.
 */
static
void
globus_l_ftp_client_update_buffer_feature(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target,
    globus_ftp_client_tristate_t		ok)
{
    int						i;
    globus_bool_t				stor_desired = GLOBUS_FALSE;
    globus_bool_t				retr_desired = GLOBUS_FALSE;

    if(handle->op == GLOBUS_FTP_CLIENT_GET ||
       handle->op == GLOBUS_FTP_CLIENT_LIST ||
       handle->op == GLOBUS_FTP_CLIENT_NLST ||
       handle->op == GLOBUS_FTP_CLIENT_MLSD ||
       (handle->op == GLOBUS_FTP_CLIENT_TRANSFER && handle->source == target))
    {
	retr_desired = GLOBUS_TRUE;
    }
    if(handle->op == GLOBUS_FTP_CLIENT_PUT ||
       (handle->op == GLOBUS_FTP_CLIENT_TRANSFER && handle->dest == target))
    {
	stor_desired = GLOBUS_TRUE;
    }

    for(i = 0; i < GLOBUS_FTP_CLIENT_LAST_BUFFER_COMMAND; i++)
    {
	if(globus_i_ftp_client_feature_get( target->features, i) &&
	   ((globus_l_ftp_client_buffer_cmd_info[i].stor_ok && stor_desired) ||
	    (globus_l_ftp_client_buffer_cmd_info[i].retr_ok && retr_desired)))
	{
	    if(globus_i_ftp_client_feature_get(target->features, i) != ok)
	    {
	        globus_i_ftp_client_feature_set(target->features, i, ok);
		break;
	    }
	}
    }
}
/* globus_l_ftp_client_update_buffer_feature() */

/**
 * Parse range marker responses and store them in the handle.
 *
 * The range markers which are stored in the handle allow the
 * auto_restart feature to automatically pick-up where the transfer
 * failed.
 *
 * @param handle
 *        The handle which we received the restart marker on.
 * @param response
 *        The response containing the range marker.
 */
static
void
globus_l_ftp_client_parse_restart_marker(
    globus_i_ftp_client_handle_t *		handle,
    globus_ftp_control_response_t *		response)
{
    globus_off_t				offset, end;
    char *					p;
    globus_result_t 				res;
    int						consumed;

    if(response->code != 111)
    {
	return;
    }
    p = (char *) response->response_buffer;

    /* skip 111 Range Marker */
    p += 4;
    while(!isdigit(*p)) p++;

    while( sscanf(p, "%"GLOBUS_OFF_T_FORMAT"-%"GLOBUS_OFF_T_FORMAT"%n",
		  &offset, &end, &consumed) >= 2)
    {
	res = globus_ftp_client_restart_marker_insert_range(
	    &handle->restart_marker,
	    offset,
	    end);

	if(res)
	{
	    break;
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
/* globus_l_ftp_client_parse_restart_marker() */

static
void
globus_l_ftp_client_parse_cksm(
    globus_i_ftp_client_handle_t *              client_handle,
    globus_ftp_control_response_t *             response)
{
    char *                                      p;
    int                                         rc;
    GlobusFuncName(globus_l_ftp_client_parse_cksm);

    if(response->code != 213)
    {
        return;
    }
    p = (char *) response->response_buffer;


    /* skip 213 <SP> */
    p += 4;

    rc=sscanf(p, "%s", client_handle->checksum);

}


static
void
globus_l_ftp_client_parse_mdtm(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response)
{
    char *					p;
    struct tm                                   tm;
    struct tm                                   gmt_now_tm;
    struct tm *                                 gmt_now_tm_p;
    time_t                                      offset;
    time_t                                      gmt_now;
    time_t                                      now;
    time_t                                      file_time;
    float					fraction;
    unsigned long				nsec = 0UL;
    int 					rc;
    int						i;
    GlobusFuncName(globus_l_ftp_client_parse_mdtm);

    if(response->code != 213)
    {
	return;
    }
    p = (char *) response->response_buffer;

    /* skip 213 <SP> */
    p += 4;
    while(!isdigit(*p)) p++;

    if(strlen(p) < 14)
	goto error_exit;

    for(i = 0; i < 14; i++)
    {
	if(!isdigit(*(p+i)))
	    goto error_exit;
    }
    memset(&tm, '\0', sizeof(struct tm));

    /* 4 digit year */
    rc = sscanf(p, "%04d", &tm.tm_year);
    if(rc != 1)
    {
	goto error_exit;
    }
    tm.tm_year -= 1900;
    p += 4;

    /* 2 digit month [01-12] */
    rc = sscanf(p, "%02d", &tm.tm_mon);
    if(rc != 1)
    {
	goto error_exit;
    }
    tm.tm_mon--;
    p += 2;

    /* 2 digit day/month [01-31] */
    rc = sscanf(p, "%02d", &tm.tm_mday);
    if(rc != 1)
    {
	goto error_exit;
    }
    p += 2;

    /* 2 digit hour [00-23] */
    rc = sscanf(p, "%02d", &tm.tm_hour);
    if(rc != 1)
    {
	goto error_exit;
    }
    p += 2;

    /* 2 digit minute [00-59] */
    rc = sscanf(p, "%02d", &tm.tm_min);
    if(rc != 1)
    {
	goto error_exit;
    }
    p += 2;

    /* 2 digit second [00-60] */
    rc = sscanf(p, "%02d", &tm.tm_sec);
    if(rc != 1)
    {
	goto error_exit;
    }
    p += 2;

    if(*p == '.')
    {
	sscanf(p, "%f", &fraction);
	nsec = fraction * 1000000000;
    }
    
    file_time = mktime(&tm);
    if(file_time == (time_t) -1)
    {
	goto error_exit;
    }
    
    now = time(&now);
    if(now == (time_t) -1)
    {
	goto error_exit;
    }
    
    memset(&gmt_now_tm, '\0', sizeof(struct tm));
    gmt_now_tm_p = globus_libc_gmtime_r(&now, &gmt_now_tm);
    if(gmt_now_tm_p == NULL)
    {
	goto error_exit;
    }
    
    gmt_now = mktime(&gmt_now_tm);
    if(gmt_now == (time_t) -1)
    {
	goto error_exit;
    }
    
    offset = now - gmt_now;

    client_handle->modification_time_pointer->tv_sec = file_time + offset;
    client_handle->modification_time_pointer->tv_nsec = nsec;

    return;

error_exit:
    if(client_handle->err == GLOBUS_SUCCESS)
    {
	client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_PROTOCOL_ERROR();
    }
}

/*
We expect the response buffer for MLST to look like this:    

250-Begin<CRLF>
<SPACE>type=dir;modify=20020101224853;UNIX.mode=0755<SPACE>/pub/kernel/software<CRLF>
250 End.<CRLF>

There may be multiple fact string lines, but the filenames must be the same. If
there are multiple fact string lines, we verify that the filenames are the same
and concatenate the fact lists into a single fact list.  The user will always 
get a single line consisting of "factlist<space>filename".

If the response buffer doesn't look like that we exit with a protocol error.
*/
static
void
globus_l_ftp_client_parse_mlst(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response)
{
    char *					p;
    globus_byte_t *                             buffer;
    char *                                      fact_list;
    char *                                      this_filename;
    char *                                      filename;
    int                                         fact_list_length = 0;
    int                                         total_fact_list_length = 0;
    int                                         filename_length = 0;
    int                                         data_length;

    GlobusFuncName(globus_l_ftp_client_parse_mlst);

    if(response->code != 250)
    {
	return;
    }
    
    buffer = (globus_byte_t *) globus_malloc(
        response->response_length * sizeof(globus_byte_t));
    if(buffer == GLOBUS_NULL)
    {
        goto nomem_exit;
    }
    
    p = (char *) response->response_buffer;

    /* advancing past the starting control response */   
    while(*p && *p != '\r' && *p != '\n')
    {
        p++;
    }
    while(*p && (*p == '\r' || *p == '\n'))
    {
        p++;
    }
    
    /* every fact line starts with a space */
    while(*p == ' ' && *(++p))
    {        
        fact_list = p;
        
        /* filename starts after the first space
           no spaces are allowed in the fact list */
        this_filename = strchr(p, ' ');
        if(this_filename == GLOBUS_NULL)
        {
            goto error_exit;
        }        
        fact_list_length = this_filename - p;
        
        /* copy current fact list to buffer */        
        memcpy(buffer + total_fact_list_length, fact_list, fact_list_length);
        
        total_fact_list_length += fact_list_length;

        /* advancing past the end of the filename */
        p = this_filename;
        while(*p && *p != '\r' && *p != '\n')
        {
            p++;
        }        

        /* if we have multiple fact strings for this file, check that the
           filenames are the same.  exit with protocol error if not */
        if(filename_length != 0)
        {
            if(filename_length != p - this_filename ||
               strncmp(filename, this_filename, filename_length))
            {
                goto error_exit;
            }
        }        
        else
        {    
            filename_length = p - this_filename;
            filename = this_filename;
        }
        
        /* advancing past the CRLF to the start of the next line */
        while(*p && (*p == '\r' || *p == '\n'))
        {
            p++;
        }        
    }    
    
    if(*p == '\0' || filename_length == 0)
    {
        goto error_exit;
    }
        
    memcpy(buffer + total_fact_list_length, filename, filename_length);
    
    data_length = total_fact_list_length + filename_length;
    
    /* null terminate the buffer since the user 
       will likely treat it as a string */
    buffer[data_length] = '\0';
    
    *client_handle->mlst_buffer_pointer = buffer;
    *client_handle->mlst_buffer_length_pointer = data_length; 
      
    return;

error_exit:
    if(client_handle->err == GLOBUS_SUCCESS)
    {
	client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_PROTOCOL_ERROR();
    }
    globus_free(buffer);
    return;

nomem_exit:
    if(client_handle->err == GLOBUS_SUCCESS)
    {
	client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
    }
    return;
} /* globus_l_ftp_client_parse_mlst() */


static
void
globus_l_ftp_client_parse_cwd(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response)
{
    globus_byte_t *                             buffer;

    GlobusFuncName(globus_l_ftp_client_parse_cwd);

    if(response->code != 250)
    {
	return;
    }
    
    if ( client_handle->mlst_buffer_pointer == NULL ||
	 client_handle->mlst_buffer_length_pointer == NULL ) {
	return;
    }

    buffer = (globus_byte_t *) globus_malloc(
        response->response_length * sizeof(globus_byte_t));
    if(buffer == GLOBUS_NULL)
    {
        goto nomem_exit;
    }

    memcpy( buffer, response->response_buffer, response->response_length );

    *client_handle->mlst_buffer_pointer = buffer;
    *client_handle->mlst_buffer_length_pointer = response->response_length; 

    return;

nomem_exit:
    if(client_handle->err == GLOBUS_SUCCESS)
    {
	client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
    }
    return;
} /* globus_l_ftp_client_parse_cwd() */

static
void
globus_l_ftp_client_parse_stat(
    globus_i_ftp_client_handle_t *		client_handle,
    globus_ftp_control_response_t *		response)
{
    char *					p;
    char *					listing_start;
    globus_byte_t *                             buffer = NULL;
    int                                         listing_length = 0;
    char                                        res_code[5];

    GlobusFuncName(globus_l_ftp_client_parse_stat);

    if(response->code != 211 &&
        response->code != 212 &&
        response->code != 213)
    {
	goto error_exit;
    }
    
    snprintf(res_code, 5, "%d ", response->code);
    
    buffer = (globus_byte_t *) globus_malloc(
        response->response_length * sizeof(globus_byte_t));
    if(buffer == GLOBUS_NULL)
    {
        goto nomem_exit;
    }
    
    p = (char *) response->response_buffer;

    /* advancing past the starting control response */   
    while(*p && *p != '\r' && *p != '\n')
    {
        p++;
    }
    while(*p && (*p == '\r' || *p == '\n'))
    {
        p++;
    }
    
    listing_start = p;

    /* check for the end control response */
    while(*p && strncmp(p, res_code, 4) != 0)
    {                
        /* advancing to the end of the listing */
        while(*p && *p != '\r' && *p != '\n')
        {
            p++;
        }        
        /* advancing past the CRLF to the start of the next line */
        while(*p && (*p == '\r' || *p == '\n'))
        {
            p++;
        }        
    }    
    listing_length = p - listing_start;
    
    if(*p == '\0' || listing_length == 0)
    {
        goto error_exit;
    }
        
    memcpy(buffer, listing_start, listing_length);
        
    /* null terminate the buffer since the user 
       will likely treat it as a string */
    buffer[listing_length] = '\0';
    
    *client_handle->mlst_buffer_pointer = buffer;
    *client_handle->mlst_buffer_length_pointer = listing_length; 
      
    return;

error_exit:
    if(client_handle->err == GLOBUS_SUCCESS)
    {
	client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_PROTOCOL_ERROR();
    }
    if(buffer)
    {
        globus_free(buffer);
    }
    return;

nomem_exit:
    if(client_handle->err == GLOBUS_SUCCESS)
    {
	client_handle->err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
    }
    return;
} /* globus_l_ftp_client_parse_stat() */


static
globus_bool_t
globus_l_ftp_client_can_cache_data_connection(
    globus_i_ftp_client_target_t *		target)
{
    return GLOBUS_TRUE;
}

static
void
globus_l_ftp_client_data_force_close_callback(
    void *					callback_arg,
    globus_ftp_control_handle_t *		control_handle,
    globus_object_t *				error)
{
}

static
globus_result_t
globus_l_ftp_client_pp_src_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added)
{
    globus_result_t                     result;
    char *                              pl_source_url;
    char *                              pl_dest_url;
    globus_i_ftp_client_url_ent_t *     url_ent;

    *added = GLOBUS_FALSE;
    (*client_handle->attr.pipeline_callback)(
        client_handle->handle,
        &pl_source_url,
        &pl_dest_url,
        client_handle->attr.pipeline_arg);

    if(pl_source_url == NULL)
    {
        return GLOBUS_SUCCESS;
    }

    url_ent = globus_malloc(sizeof(globus_i_ftp_client_url_ent_t));

    url_ent->source_url = globus_libc_strdup(pl_source_url);
    url_ent->dest_url = globus_libc_strdup(pl_dest_url);

    globus_l_ftp_client_url_parse(
        url_ent->source_url,
        &url_ent->src_url,
        client_handle->attr.rfc1738_url);

    if(target->attr->module_alg_str != NULL)
    {
        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            target->url_string,
            target->mask,
            "ERET %s %s" CRLF,
            target->attr->module_alg_str,
            url_ent->src_url.url_path);

        result = globus_ftp_control_send_command(
            handle,
            "ERET %s %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            target->attr->module_alg_str,
            url_ent->src_url.url_path);
    }
    else
    {
        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            target->url_string,
            target->mask,
            "RETR %s" CRLF,
            url_ent->src_url.url_path);

        result = globus_ftp_control_send_command(
            handle,
            "RETR %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            url_ent->src_url.url_path);
    }
    if(result != GLOBUS_SUCCESS)
    {
        goto result_fault;
    }

    client_handle->no_callback_count++;
    globus_fifo_enqueue(&client_handle->src_response_pending_queue, url_ent);

    *added = GLOBUS_TRUE;
    return GLOBUS_SUCCESS;

result_fault:

    return result;
}

static
globus_result_t
globus_l_ftp_client_pp_dst_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added)
{
    globus_result_t                     result;
    char *                              pl_source_url;
    char *                              pl_dest_url;
    globus_i_ftp_client_url_ent_t *     url_ent;

    *added = GLOBUS_FALSE;
    (*client_handle->attr.pipeline_callback)(
        client_handle->handle,
        &pl_source_url,
        &pl_dest_url,
        client_handle->attr.pipeline_arg);
    if(pl_dest_url == NULL)
    {
        return GLOBUS_SUCCESS;
    }
    url_ent = globus_malloc(sizeof(globus_i_ftp_client_url_ent_t));

    url_ent->source_url = globus_libc_strdup(pl_source_url);
    url_ent->dest_url = globus_libc_strdup(pl_dest_url);

    globus_l_ftp_client_url_parse(
        url_ent->dest_url,
        &url_ent->dst_url,
        client_handle->attr.rfc1738_url);

    if(target->attr->module_alg_str != GLOBUS_NULL)
    {
        result = globus_ftp_control_send_command(
            handle,
            "ESTO %s %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            target->attr->module_alg_str,
            url_ent->dst_url.url_path);
    }
    else if(target->attr->append)
    {
        result = globus_ftp_control_send_command(
            handle,
            "APPE %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            url_ent->dst_url.url_path);
    }
    else
    {
        result = globus_ftp_control_send_command(
            handle,
            "STOR %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            url_ent->dst_url.url_path);
    }
    if(result != GLOBUS_SUCCESS)
    {
        goto result_fault;
    }

    client_handle->no_callback_count++;
    globus_fifo_enqueue(&client_handle->dst_response_pending_queue, url_ent);

    *added = GLOBUS_TRUE;
    return GLOBUS_SUCCESS;

result_fault:

    return result;
}

static
globus_result_t
globus_l_ftp_client_pp_get_next(
    globus_i_ftp_client_handle_t *      client_handle)
{
    char *                              pl_source_url = NULL;
    char *                              pl_dest_url = NULL;
    globus_i_ftp_client_url_ent_t *     src_url_ent;
    globus_i_ftp_client_url_ent_t *     url_ent;

    if(!client_handle->attr.pipeline_done)
    {
        (*client_handle->attr.pipeline_callback)(
            client_handle->handle,
            &pl_source_url,
            &pl_dest_url,
            client_handle->attr.pipeline_arg);
    }

    if(pl_source_url == NULL && pl_dest_url == NULL)
    {
        client_handle->attr.pipeline_done = GLOBUS_TRUE;
    }
    else
    {
        url_ent = globus_malloc(sizeof(globus_i_ftp_client_url_ent_t));

        url_ent->source_url = globus_libc_strdup(pl_source_url);
        url_ent->dest_url = globus_libc_strdup(pl_dest_url);

        if(url_ent->source_url)
        {
            globus_l_ftp_client_url_parse(
                url_ent->source_url,
                &url_ent->src_url,
                client_handle->attr.rfc1738_url);
        }
        if(url_ent->dest_url)
        {
            globus_l_ftp_client_url_parse(
                url_ent->dest_url,
                &url_ent->dst_url,
                client_handle->attr.rfc1738_url);
        }

        globus_fifo_enqueue(&client_handle->dst_op_queue, url_ent);
        src_url_ent = globus_malloc(sizeof(globus_i_ftp_client_url_ent_t));
        memcpy(src_url_ent, url_ent, sizeof(globus_i_ftp_client_url_ent_t));
        globus_fifo_enqueue(&client_handle->src_op_queue, src_url_ent);
    }
    
    return GLOBUS_SUCCESS;
}



static
globus_result_t
globus_l_ftp_client_pp_xfer_dst_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added)
{
    globus_result_t                     result;
    globus_i_ftp_client_url_ent_t *     url_ent;

    *added = GLOBUS_FALSE;

    globus_l_ftp_client_pp_get_next(client_handle);

    if(globus_fifo_empty(&client_handle->dst_op_queue))
    {
        *added = GLOBUS_FALSE;
        return GLOBUS_SUCCESS;
    }
    url_ent = (globus_i_ftp_client_url_ent_t *)
        globus_fifo_dequeue(&client_handle->dst_op_queue);
    
    if(target->attr->module_alg_str != GLOBUS_NULL)
    {
        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            url_ent->dest_url,
            target->mask,
            "ESTO %s %s" CRLF,
            target->attr->module_alg_str,
            url_ent->dst_url.url_path);
    }
    else
    {
        globus_i_ftp_client_plugin_notify_command(
            client_handle,
            url_ent->dest_url,
            target->mask,
            "STOR %s" CRLF,
            url_ent->dst_url.url_path);
    }
    if(target->attr->module_alg_str != GLOBUS_NULL)
    {
        result = globus_ftp_control_send_command(
            handle,
            "ESTO %s %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            target->attr->module_alg_str,
            url_ent->dst_url.url_path);
    }
    else
    {
        result = globus_ftp_control_send_command(
            handle,
            "STOR %s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            url_ent->dst_url.url_path);
    }
    if(result != GLOBUS_SUCCESS)
    {
        goto result_fault;
    }

    client_handle->no_callback_count++;
    globus_fifo_enqueue(&client_handle->dst_response_pending_queue, url_ent);

    *added = GLOBUS_TRUE;
    return GLOBUS_SUCCESS;

result_fault:

    return result;
}


static
globus_result_t
globus_l_ftp_client_pp_xfer_src_add(
    globus_i_ftp_client_handle_t *      client_handle,
    globus_ftp_control_handle_t *       handle,
    globus_i_ftp_client_target_t *      target,
    globus_bool_t *                     added)
{
    globus_result_t                     result;
    globus_i_ftp_client_url_ent_t *     url_ent;

    *added = GLOBUS_FALSE;

    globus_l_ftp_client_pp_get_next(client_handle);

    if(globus_fifo_empty(&client_handle->src_op_queue))
    {
        return GLOBUS_SUCCESS;
    }
    url_ent = (globus_i_ftp_client_url_ent_t *)
        globus_fifo_dequeue(&client_handle->src_op_queue);

        if(target->attr->module_alg_str != GLOBUS_NULL)
        {
            globus_i_ftp_client_plugin_notify_command(
                client_handle,
                url_ent->source_url,
                target->mask,
                "ERET %s %s" CRLF,
                target->attr->module_alg_str,
                url_ent->src_url.url_path);

            result = globus_ftp_control_send_command(
                handle,
                "ERET %s %s\r\n",
                globus_i_ftp_client_response_callback,
                target,
                target->attr->module_alg_str,
                url_ent->src_url.url_path);
        }
        else
        {
            globus_i_ftp_client_plugin_notify_command(
                client_handle,
                url_ent->source_url,
                target->mask,
                "RETR %s" CRLF,
                url_ent->src_url.url_path);

            result = globus_ftp_control_send_command(
                handle,
                "RETR %s" CRLF,
                globus_i_ftp_client_response_callback,
                target,
                url_ent->src_url.url_path);
        }
        if(result != GLOBUS_SUCCESS)
        {
            goto result_fault;
        }

        globus_fifo_enqueue(
            &client_handle->src_response_pending_queue, url_ent);
    client_handle->no_callback_count++;

    *added = GLOBUS_TRUE;

    return GLOBUS_SUCCESS;

result_fault:

    return result;

}

static
globus_result_t
globus_l_ftp_client_use_gridftp2_getput(
    globus_i_ftp_client_target_t *		target,
    globus_bool_t *                             getput)
{
    globus_i_ftp_client_handle_t *		client_handle;
    globus_ftp_client_handleattr_t              handle_attr;
    globus_result_t				result;

    client_handle = target->owner;
    handle_attr = &client_handle->attr;

    /* Does target support GFD.47? 
     */
    result = 
        globus_ftp_client_handleattr_get_gridftp2(&handle_attr, getput);
    if(result != GLOBUS_SUCCESS || *getput == GLOBUS_FALSE)
    {
        return result;
    }

    /* Does requested operation allow the use of GETPUT?
     */
    if (client_handle->op != GLOBUS_FTP_CLIENT_TRANSFER
        && client_handle->op != GLOBUS_FTP_CLIENT_GET
        && client_handle->op != GLOBUS_FTP_CLIENT_PUT)
    {
        *getput = GLOBUS_FALSE;
        return GLOBUS_SUCCESS;
    }

    /* Are GFD.47 extensions enabled?
     */
    if (globus_i_ftp_client_feature_get( 
            target->features,
            GLOBUS_FTP_CLIENT_FEATURE_GETPUT) == GLOBUS_FTP_CLIENT_FALSE)
    {
        *getput = GLOBUS_FALSE;
        return GLOBUS_SUCCESS;
    }

    /* GFD.47 style GETPUT does not support extended server side
     * processing.
     */
    if (target->attr->module_alg_str != GLOBUS_NULL)
    {
        *getput = GLOBUS_FALSE;
        return GLOBUS_SUCCESS;
    }

    /* In principal, GFD.47 supports striping, however the exact
     * details for configuring striping using GETPUT are currently
     * unknown.
     */
    if (target->attr->layout.mode != GLOBUS_FTP_CONTROL_STRIPING_NONE)
    {
        *getput = GLOBUS_FALSE;
        return GLOBUS_SUCCESS;
    }

    /* GFD.47 doesn't tell us how to do IPv6.
     */
    if(target->attr->allow_ipv6)
    {
        *getput = GLOBUS_FALSE;
        return GLOBUS_SUCCESS;
    }

    return GLOBUS_SUCCESS;
}

/**
 * Sends a GFD.47 compliant GET command.
 */
static
globus_result_t
globus_l_ftp_client_send_get(
    globus_i_ftp_client_target_t *		target,
    char *                                      pathname,
    globus_bool_t                               pasv)
{
    char *					tmpstr = GLOBUS_NULL;
    globus_i_ftp_client_handle_t *		client_handle;
    int						rc, oldrc;
    globus_result_t				result;

    client_handle = target->owner;

    tmpstr = globus_libc_malloc(56 * client_handle->num_pasv_addresses + 6);

    if(tmpstr == GLOBUS_NULL)
    {
        return globus_error_put(GLOBUS_ERROR_NO_INFO);
    }

    if(globus_i_ftp_client_can_reuse_data_conn(client_handle))
    {
        tmpstr[0] = 0;
    }
    else if(pasv == GLOBUS_TRUE)
    {
        sprintf(tmpstr, "pasv;");
    }
    else
    {
        rc = oldrc = 0;
        globus_assert(!target->attr->allow_ipv6);
        globus_assert(client_handle->num_pasv_addresses == 1);
        globus_assert(client_handle->pasv_address[0].hostlen != 16);

        rc += sprintf(&tmpstr[oldrc],
                      "port=%d,%d,%d,%d,%d,%d;",
                      client_handle->pasv_address[0].host[0],
                      client_handle->pasv_address[0].host[1],
                      client_handle->pasv_address[0].host[2],
                      client_handle->pasv_address[0].host[3],
                      (client_handle->pasv_address[0].port >> 8)
                      & 0xff,
                      client_handle->pasv_address[0].port & 0xff);
        if(rc == oldrc)
        {
            globus_libc_free(tmpstr); 
            return globus_error_put(GLOBUS_ERROR_NO_INFO);
        }
    }

    globus_i_ftp_client_plugin_notify_command(
        client_handle,
        target->url_string,
        target->mask,
        "GET path=%s;%s" CRLF,
        pathname,
        tmpstr);
        
    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
    {
        globus_libc_free(tmpstr);
        return GLOBUS_SUCCESS;
    }
        
    result =
        globus_ftp_control_send_command(
            target->control_handle,
            "GET path=%s;%s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            pathname,
            tmpstr);
        
    globus_libc_free(tmpstr);

    return result;
}

/**
 * Sends a GFD.47 compliant PUT command.
 */
static
globus_result_t
globus_l_ftp_client_send_put(
    globus_i_ftp_client_target_t *		target,
    char *                                      pathname,
    globus_bool_t                               pasv)
{
    char *					tmpstr = GLOBUS_NULL;
    globus_i_ftp_client_handle_t *		client_handle;
    int						rc, oldrc;
    globus_result_t				result;

    client_handle = target->owner;

    tmpstr = globus_libc_malloc(56 * client_handle->num_pasv_addresses + 6);

    if(tmpstr == GLOBUS_NULL)
    {
        return globus_error_put(GLOBUS_ERROR_NO_INFO);
    }

    if(globus_i_ftp_client_can_reuse_data_conn(client_handle))
    {
        tmpstr[0] = 0;
    }
    else if(pasv == GLOBUS_TRUE)
    {
        sprintf(tmpstr, "pasv;");
    }
    else
    {
        rc = oldrc = 0;
        globus_assert(!target->attr->allow_ipv6);
        globus_assert(client_handle->num_pasv_addresses == 1);
        globus_assert(client_handle->pasv_address[0].hostlen != 16);

        rc += sprintf(&tmpstr[oldrc],
                      "port=%d,%d,%d,%d,%d,%d;",
                      client_handle->pasv_address[0].host[0],
                      client_handle->pasv_address[0].host[1],
                      client_handle->pasv_address[0].host[2],
                      client_handle->pasv_address[0].host[3],
                      (client_handle->pasv_address[0].port >> 8)
                      & 0xff,
                      client_handle->pasv_address[0].port & 0xff);
        if(rc == oldrc)
        {
            globus_libc_free(tmpstr); 
            return globus_error_put(GLOBUS_ERROR_NO_INFO);
        }
    }

    globus_i_ftp_client_plugin_notify_command(
        client_handle,
        target->url_string,
        target->mask,
        "PUT path=%s;%s" CRLF,
        pathname,
        tmpstr);
        
    if(client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
       client_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
    {
        globus_libc_free(tmpstr);
        return GLOBUS_SUCCESS;
    }
        
    result =
        globus_ftp_control_send_command(
            target->control_handle,
            "PUT path=%s;%s" CRLF,
            globus_i_ftp_client_response_callback,
            target,
            pathname,
            tmpstr);
        
    globus_libc_free(tmpstr);

    return result;
}

#endif
