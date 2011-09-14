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
 * @file globus_ftp_client.c
 *
 * $RCSfile: globus_ftp_client.c,v $
 * $Revision: 1.18 $
 * $Date: 2008/04/04 01:51:46 $
 */
#endif

#include "globus_i_ftp_client.h"
#include "version.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL

static int globus_l_ftp_client_activate(void);
static int globus_l_ftp_client_deactivate(void);

/**
 * Debugging level
 *
 * Currently this isn't terribly well defined. The idea is that 0 is no
 * debugging output, and 9 is a whole lot.
 */
int globus_i_ftp_client_debug_level = 0;

globus_xio_stack_t               ftp_client_i_popen_stack;
globus_xio_driver_t              ftp_client_i_popen_driver;
globus_bool_t                    ftp_client_i_popen_ready = GLOBUS_FALSE;

/**
 * Default authorization information for GSIFTP.
 */
globus_ftp_control_auth_info_t		globus_i_ftp_client_default_auth_info;


/* @{ */
/**
 * Thread-safety support for deactivation reference counting
 */
static
globus_mutex_t 				globus_l_ftp_client_active_list_mutex;

static
globus_mutex_t 				globus_l_ftp_client_control_list_mutex;

static
globus_cond_t 				globus_l_ftp_client_active_list_cond;

static
globus_cond_t 				globus_l_ftp_client_control_list_cond;
/* @} */

/**
 * List of active client handles.
 *
 * A handle is active if an operation's callback has been associated
 * with it, and the operation's processing has begun.
 */
static
globus_list_t *
globus_l_ftp_client_active_handle_list;

/**
 * List of active control handles.
 *
 * A handle from the time the initial connection callback has been
 * registered until the close or force_close callback has been called.
 */
static
globus_list_t *
globus_l_ftp_client_active_control_list;

globus_reltime_t                        globus_i_ftp_client_noop_idle =
{
    GLOBUS_I_FTP_CLIENT_NOOP_IDLE,
    0
};

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t		globus_i_ftp_client_module =
{
    "globus_ftp_client",
    globus_l_ftp_client_activate,
    globus_l_ftp_client_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};

/**
 * Module activation
 */
static
int
globus_l_ftp_client_activate(void)
{
    char *                              tmp_string;

    globus_module_activate(GLOBUS_FTP_CONTROL_MODULE);

    tmp_string = globus_module_getenv("GLOBUS_FTP_CLIENT_DEBUG_LEVEL");
    if(tmp_string != GLOBUS_NULL)
    {
	globus_i_ftp_client_debug_level = atoi(tmp_string);

	if(globus_i_ftp_client_debug_level < 0)
	{
	    globus_i_ftp_client_debug_level = 0;
	}
    }

    globus_i_ftp_client_debug_printf(1,
        (stderr, "globus_l_ftp_client_activate() entering\n"));

    globus_mutex_init(&globus_l_ftp_client_active_list_mutex, GLOBUS_NULL);
    globus_mutex_init(&globus_l_ftp_client_control_list_mutex, GLOBUS_NULL);
    globus_cond_init(&globus_l_ftp_client_active_list_cond, GLOBUS_NULL);
    globus_cond_init(&globus_l_ftp_client_control_list_cond, GLOBUS_NULL);
    globus_l_ftp_client_active_handle_list = GLOBUS_NULL;
    globus_l_ftp_client_active_control_list = GLOBUS_NULL;

    globus_ftp_control_auth_info_init(&globus_i_ftp_client_default_auth_info,
		                      GSS_C_NO_CREDENTIAL,
				      GLOBUS_TRUE,
				      ":globus-mapping:",
				      "",
				      0,
				      0);

    globus_i_ftp_client_debug_printf(1,
        (stderr, "globus_l_ftp_client_activate() exiting\n"));

    {
        globus_result_t res;

        res = globus_xio_driver_load("popen", &ftp_client_i_popen_driver);
        if(res == GLOBUS_SUCCESS)
        {
            globus_xio_stack_init(&ftp_client_i_popen_stack, NULL);
            globus_xio_stack_push_driver(
                ftp_client_i_popen_stack, ftp_client_i_popen_driver);
            ftp_client_i_popen_ready = GLOBUS_TRUE;
            globus_i_ftp_client_find_ssh_client_program();
        }
    }

    return GLOBUS_SUCCESS;
}

/**
 * Module deactivation
 *
 */
static
int
globus_l_ftp_client_deactivate(void)
{
    globus_i_ftp_client_debug_printf(1,
        (stderr, "globus_l_ftp_client_deactivate() entering\n"));

    globus_mutex_lock(&globus_l_ftp_client_active_list_mutex);

    /* Wait for all client library callbacks to complete.
     */
    while(! globus_list_empty(globus_l_ftp_client_active_handle_list))
    {
	globus_ftp_client_handle_t *tmp;

	tmp = globus_list_first(globus_l_ftp_client_active_handle_list);

	globus_ftp_client_abort(tmp);

	globus_cond_wait(&globus_l_ftp_client_active_list_cond,
			 &globus_l_ftp_client_active_list_mutex);
    }
    globus_mutex_unlock(&globus_l_ftp_client_active_list_mutex);
    /* TODO: Destroy all cached targets. */

    /* Wait for all detached target control library callbacks to
     * complete.
     */
    globus_mutex_lock(&globus_l_ftp_client_control_list_mutex);
    while(! globus_list_empty(globus_l_ftp_client_active_control_list))
    {
	globus_cond_wait(&globus_l_ftp_client_control_list_cond,
			 &globus_l_ftp_client_control_list_mutex);
    }
    globus_mutex_unlock(&globus_l_ftp_client_control_list_mutex);

    globus_mutex_destroy(&globus_l_ftp_client_active_list_mutex);
    globus_cond_destroy(&globus_l_ftp_client_active_list_cond);

    globus_mutex_destroy(&globus_l_ftp_client_control_list_mutex);
    globus_cond_destroy(&globus_l_ftp_client_control_list_cond);
    globus_module_deactivate(GLOBUS_FTP_CONTROL_MODULE);

    if(ftp_client_i_popen_ready)
    {
        globus_xio_driver_unload(ftp_client_i_popen_driver);
        globus_xio_stack_destroy(ftp_client_i_popen_stack);
        ftp_client_i_popen_ready = GLOBUS_FALSE;
    }

    globus_i_ftp_client_debug_printf(1,
        (stderr, "globus_l_ftp_client_deactivate() exiting\n"));

    return GLOBUS_SUCCESS;
}
/* globus_l_ftp_client_deactivate() */

/**
 *
 * Add a reference to a client handle to the shutdown count.
 *
 * When deactivating, we wait for all callbacks associated with
 * the FTP client library to be completed. This function adds the
 * specified handle to the active handle list, so that deactivation
 * will wait for it.
 *
 * @param handle
 *        The handle to add to the list.
 */
void
globus_i_ftp_client_handle_is_active(
    globus_ftp_client_handle_t *		handle)
{
    globus_mutex_lock(&globus_l_ftp_client_active_list_mutex);
    globus_list_insert(&globus_l_ftp_client_active_handle_list,
		       handle);
    globus_mutex_unlock(&globus_l_ftp_client_active_list_mutex);
}
/* globus_i_ftp_client_handle_is_active() */

/**
 * Remove a reference to a client handle to the shutdown count.
 *
 * When deactivating, we wait for all callbacks associated with
 * the FTP client library to be completed. This function removes the
 * specified handle to the active handle list, so that deactivation
 * will not wait for it any more.
 *
 * This funciton also wakes up the deactivation function if it is
 * waiting for this handle's callbacks to terminate.
 *
 * @param handle
 *        The handle to remove to the list.
 */
void
globus_i_ftp_client_handle_is_not_active(
    globus_ftp_client_handle_t *		handle)
{
    globus_list_t * node;

    globus_mutex_lock(&globus_l_ftp_client_active_list_mutex);
    node = globus_list_search(globus_l_ftp_client_active_handle_list,
			      handle);
    globus_assert(node);
    globus_list_remove(&globus_l_ftp_client_active_handle_list,
		       node);
    globus_cond_signal(&globus_l_ftp_client_active_list_cond);
    globus_mutex_unlock(&globus_l_ftp_client_active_list_mutex);
}
/* globus_i_ftp_client_handle_is_not_active() */


/**
 *
 * Add a reference to a control handle to the shutdown count.
 *
 * When deactivating, we wait for all callbacks associated with
 * the FTP client library to be completed. This function adds the
 * specified handle to the active control handle list, so that
 * deactivation will wait for it.
 *
 * @param handle
 *        The handle to add to the list.
 */
void
globus_i_ftp_client_control_is_active(
    globus_ftp_control_handle_t *		handle)
{
    globus_mutex_lock(&globus_l_ftp_client_control_list_mutex);
    globus_list_insert(&globus_l_ftp_client_active_control_list,
		       handle);
    globus_mutex_unlock(&globus_l_ftp_client_control_list_mutex);
}
/* globus_i_ftp_client_control_is_active() */

/**
 * Remove a reference to a control handle to the shutdown count.
 *
 * When deactivating, we wait for all callbacks associated with
 * the FTP client library to be completed. This function removes the
 * specified handle to the active handle list, so that deactivation
 * will not wait for it any more.
 *
 * This funciton also wakes up the deactivation function if it is
 * waiting for this handle's callbacks to terminate.
 *
 * @param handle
 *        The handle to remove to the list.
 */
void
globus_i_ftp_client_control_is_not_active(
    globus_ftp_control_handle_t *		handle)
{
    globus_list_t * node;

    globus_mutex_lock(&globus_l_ftp_client_control_list_mutex);
    node = globus_list_search(globus_l_ftp_client_active_control_list,
			      handle);
    globus_assert(node);
    globus_list_remove(&globus_l_ftp_client_active_control_list,
		       node);
    globus_cond_signal(&globus_l_ftp_client_control_list_cond);
    globus_mutex_unlock(&globus_l_ftp_client_control_list_mutex);
}
/* globus_i_ftp_client_control_is_not_active() */

/**
 * Convert and FTP operation into a string.
 *
 * This function is used in various error message generators in
 * the ftp client library.
 *
 * @param op
 *        The operation to stringify.
 *
 * @return This function returns a static string representation
 *         of the operation. The string MUST NOT be modified or
 *         freed by the caller.
 */
const char *
globus_i_ftp_op_to_string(
    globus_i_ftp_client_operation_t		op)
{
    static const char * get      = "GLOBUS_FTP_CLIENT_GET";
    static const char * list     = "GLOBUS_FTP_CLIENT_LIST";
    static const char * nlst     = "GLOBUS_FTP_CLIENT_NLST";
    static const char * mlsd     = "GLOBUS_FTP_CLIENT_MLSD";
    static const char * mlst     = "GLOBUS_FTP_CLIENT_MLST";    
    static const char * stat     = "GLOBUS_FTP_CLIENT_STAT";    
    static const char * chmod    = "GLOBUS_FTP_CLIENT_CHMOD";
    static const char * delete   = "GLOBUS_FTP_CLIENT_DELETE";
    static const char * mkdir    = "GLOBUS_FTP_CLIENT_MKDIR";
    static const char * rmdir    = "GLOBUS_FTP_CLIENT_RMDIR";
    static const char * move     = "GLOBUS_FTP_CLIENT_MOVE";
    static const char * feat     = "GLOBUS_FTP_CLIENT_FEAT";
    static const char * put      = "GLOBUS_FTP_CLIENT_PUT";
    static const char * transfer = "GLOBUS_FTP_CLIENT_TRANSFER";
    static const char * mdtm     = "GLOBUS_FTP_CLIENT_MDTM";
    static const char * size     = "GLOBUS_FTP_CLIENT_SIZE";
    static const char * cksm     = "GLOBUS_FTP_CLIENT_CKSM";
    static const char * idle     = "GLOBUS_FTP_CLIENT_IDLE";
    static const char * invalid  = "INVALID OPERATION";

    switch(op)
    {
    case GLOBUS_FTP_CLIENT_MKDIR:
	return mkdir;
    case GLOBUS_FTP_CLIENT_RMDIR:
	return rmdir;
    case GLOBUS_FTP_CLIENT_MOVE:
	return move;
    case GLOBUS_FTP_CLIENT_FEAT:
	return feat;
    case GLOBUS_FTP_CLIENT_MDTM:
	return mdtm;
    case GLOBUS_FTP_CLIENT_SIZE:
	return size;
    case GLOBUS_FTP_CLIENT_CKSM:
	return cksm;
    case GLOBUS_FTP_CLIENT_LIST:
	return list;
    case GLOBUS_FTP_CLIENT_NLST:
	return nlst;
    case GLOBUS_FTP_CLIENT_MLSD:
	return mlsd;
    case GLOBUS_FTP_CLIENT_MLST:
	return mlst;
    case GLOBUS_FTP_CLIENT_STAT:
	return stat;
    case GLOBUS_FTP_CLIENT_CHMOD:
	return chmod;
    case GLOBUS_FTP_CLIENT_DELETE:
	return delete;
    case GLOBUS_FTP_CLIENT_GET:
	return get;
    case GLOBUS_FTP_CLIENT_PUT:
	return put;
    case GLOBUS_FTP_CLIENT_TRANSFER:
	return transfer;
    case GLOBUS_FTP_CLIENT_IDLE:
	return idle;
    default:
	return invalid;
    }
}
/* globus_i_ftp_op_to_string() */

/**
 * Convert a target state into a string.
 *
 * This function is used in various error message generators in
 * the ftp client library.
 *
 * @param target_state
 *        The target state to stringify.
 *
 * @return This function returns a static string representation
 *         of the target state. The string MUST NOT be modified or
 *         freed by the caller.
 */
const char *
globus_i_ftp_target_state_to_string(
    globus_ftp_client_target_state_t		target_state)
{
    static const char * start                   = "START";
    static const char * connect                 = "CONNECT";
    static const char * authenticate            = "AUTHENTICATE";
    static const char * setup_site_fault        = "SETUP_SITE_FAULT";
    static const char * site_fault              = "SITE_FAULT";
    static const char * setup_site_help         = "SETUP_SITE_HELP";
    static const char * site_help               = "SITE_HELP";
    static const char * feat                    = "FEAT";
    static const char * setup_connection        = "SETUP_CONNECTION";
    static const char * setup_type              = "SETUP_TYPE";
    static const char * type                    = "TYPE";
    static const char * setup_mode              = "SETUP_MODE";
    static const char * mode                    = "MODE";
    static const char * setup_size              = "SETUP_SIZE";
    static const char * size                    = "SIZE";
    static const char * setup_cksm              = "SETUP_CKSM";
    static const char * cksm                    = "CKSM";
    static const char * setup_dcau              = "SETUP_DCAU";
    static const char * dcau                    = "DCAU";
    static const char * setup_pbsz              = "SETUP_PBSZ";
    static const char * pbsz                    = "PBSZ";
    static const char * setup_prot              = "SETUP_PROT";
    static const char * prot                    = "PROT";
    static const char * setup_bufsize           = "SETUP_BUFSIZE";
    static const char * bufsize                 = "BUFSIZE";
    static const char * setup_remote_retr_opt   = "SETUP_REMOTE_RETR_OPTS";
    static const char * remote_retr_opts        = "REMOTE_RETR_OPTS";
    static const char * setup_local_retr_opts   = "SETUP_LOCAL_RETR_OPTS";
    static const char * local_retr_opts         = "LOCAL_RETR_OPTS";
    static const char * setup_pasv              = "SETUP_PASV";
    static const char * pasv                    = "PASV";
    static const char * setup_port              = "SETUP_PORT";
    static const char * port                    = "PORT";
    static const char * setup_rest_stream       = "SETUP_REST_STREAM";
    static const char * setup_rest_eb           = "SETUP_REST_EB";
    static const char * rest                    = "REST";
    static const char * setup_operation         = "SETUP_OPERATION";
    static const char * setup_list              = "SETUP_LIST";
    static const char * setup_get               = "SETUP_GET";
    static const char * setup_put               = "SETUP_PUT";
    static const char * setup_transfer_source   = "SETUP_TRANSFER_SOURCE";
    static const char * setup_transfer_dest     = "SETUP_TRANSFER_DEST";
    static const char * setup_delete            = "SETUP_DELETE";
    static const char * setup_chmod             = "SETUP_CHMOD";
    static const char * setup_mkdir             = "SETUP_MKDIR";
    static const char * setup_rmdir             = "SETUP_RMDIR";
    static const char * setup_rnfr              = "SETUP_RNFR";
    static const char * setup_rnto              = "SETUP_RNTO";
    static const char * setup_mdtm              = "SETUP_MDTM";
    static const char * list                    = "LIST";
    static const char * setup_mlst              = "SETUP_MLST";
    static const char * mlst                    = "MLST";
    static const char * setup_stat              = "SETUP_STAT";
    static const char * setup_getput_get        = "SETUP_GETPUT_GET";
    static const char * setup_getput_put        = "SETUP_GETPUT_PUT";
    static const char * stat                    = "STAT";
    static const char * retr                    = "RETR";
    static const char * stor                    = "STOR";
    static const char * mdtm                    = "MDTM";
    static const char * getput_pasv_get         = "GETPUT_PASV_GET";
    static const char * getput_pasv_put         = "GETPUT_PASV_PUT";
    static const char * getput_pasv_transfer    = "GETPUT_PASV_TRANSFER";
    static const char * ready_for_data          = "READY_FOR_DATA";
    static const char * need_last_block         = "NEED_LAST_BLOCK";
    static const char * need_empty_queue        = "NEED_EMPTY_QUEUE";
    static const char * need_empty_and_complete = "NEED_EMPTY_AND_COMPLETE";
    static const char * need_complete           = "NEED_COMPLETE";
    static const char * completed_operation     = "COMPLETED_OPERATION";
    static const char * noop                    = "NOOP";
    static const char * fault                   = "FAULT";
    static const char * closed                  = "CLOSED";
    static const char * invalid                 = "INVALID_STATE";

    switch(target_state)
    {
        case GLOBUS_FTP_CLIENT_TARGET_START:
            return start;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_CONNECT:
            return connect;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_AUTHENTICATE:
            return authenticate;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_FAULT:
            return setup_site_fault;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SITE_FAULT:
            return site_fault;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_HELP:
            return setup_site_help;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SITE_HELP:
            return site_help;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_FEAT:
            return feat;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION:
            return setup_connection;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_TYPE:
            return setup_type;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_TYPE:
            return type;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_MODE:
            return setup_mode;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_MODE:
            return mode;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_SIZE:
            return setup_size;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SIZE:
            return size;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_CKSM:
            return setup_cksm;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_CKSM:
            return cksm;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_DCAU:
            return setup_dcau;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_DCAU:
            return dcau;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_PBSZ:
            return setup_pbsz;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_PBSZ:
            return pbsz;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_PROT:
            return setup_prot;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_PROT:
            return prot;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_BUFSIZE:
            return setup_bufsize;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_BUFSIZE:
            return bufsize;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_REMOTE_RETR_OPTS:
            return setup_remote_retr_opt;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_REMOTE_RETR_OPTS:
            return remote_retr_opts;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_LOCAL_RETR_OPTS:
            return setup_local_retr_opts;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_LOCAL_RETR_OPTS:
            return local_retr_opts;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_PASV:
            return setup_pasv;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_PASV:
            return pasv;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT:
            return setup_port;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_PORT:
            return port;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_STREAM:
            return setup_rest_stream;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_EB:
            return setup_rest_eb;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_REST:
            return rest;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION:
            return setup_operation;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_MLST:
            return setup_mlst;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_MLST:
            return mlst;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_STAT:
            return setup_stat;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_GET:
            return setup_getput_get;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_PUT:
            return setup_getput_put;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_STAT:
            return stat;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST:
            return setup_list;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_GET:
            return setup_get;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_PUT:
            return setup_put;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_SOURCE:
            return setup_transfer_source;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_DEST:
            return setup_transfer_dest;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_CHMOD:
            return setup_chmod;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_DELETE:
            return setup_delete;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_MKDIR:
            return setup_mkdir;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_RMDIR:
            return setup_rmdir;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_RNFR:
            return setup_rnfr;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_RNTO:
            return setup_rnto;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_SETUP_MDTM:
            return setup_mdtm;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_LIST:
            return list;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_RETR:
            return retr;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_STOR:
            return stor;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_MDTM:
            return mdtm;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_GET:
            return getput_pasv_get;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_PUT:
            return getput_pasv_put;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_TRANSFER:
            return getput_pasv_transfer;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA:
            return ready_for_data;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK:
            return need_last_block;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE:
            return need_empty_queue;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE:
            return need_empty_and_complete;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE:
            return need_complete;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION:
            return completed_operation;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_NOOP:
            return noop;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_FAULT:
            return fault;
            break;
        case GLOBUS_FTP_CLIENT_TARGET_CLOSED:
            return closed;
            break;
        default:
            return invalid;
            break;
    }
}

/**
 * Convert a handle state into a string.
 *
 * This function is used in various error message generators in
 * the ftp client library.
 *
 * @param handle_state
 *        The handle state to stringify.
 *
 * @return This function returns a static string representation
 *         of the handle state. The string MUST NOT be modified or
 *         freed by the caller.
 */
const char *
globus_i_ftp_handle_state_to_string(
    globus_ftp_client_handle_state_t		handle_state)
{
    static const char * start                               = "START";
    static const char * source_connect                      = "SOURCE_CONNECT";
    static const char * source_setup_connection             = "SOURCE_SETUP_CONNECTION";
    static const char * source_list                         = "SOURCE_LIST";
    static const char * source_retr_or_eret                 = "SOURCE_RETR_OR_ERET";
    static const char * dest_connect                        = "DEST_CONNECT";
    static const char * dest_setup_connection               = "DEST_SETUP_CONNECTION";
    static const char * dest_stor_or_esto                   = "DEST_STOR_OR_ESTO";
    static const char * abort                               = "ABORT";
    static const char * restart                             = "RESTART";
    static const char * failure                             = "FAILURE";
    static const char * third_party_transfer                = "THIRD_PARTY_TRANSFER";
    static const char * third_party_transfer_one_complete   = "THIRD_PARTY_TRANSFER_ONE_COMPLETE";
    static const char * finalize                            = "FINALIZE";
    static const char * invalid                             = "INVALID_STATE";

    switch(handle_state)
    {
        case GLOBUS_FTP_CLIENT_HANDLE_START:
            return start;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT:
            return source_connect;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION:
            return source_setup_connection;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST:
            return source_list;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET:
            return source_retr_or_eret;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT:
            return dest_connect;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION:
            return dest_setup_connection;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO:
            return dest_stor_or_esto;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_ABORT:
            return abort;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_RESTART:
            return restart;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_FAILURE:
            return failure;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER:
            return third_party_transfer;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE:
            return third_party_transfer_one_complete;
            break;
        case GLOBUS_FTP_CLIENT_HANDLE_FINALIZE:
            return finalize;
            break;
        default:
            return invalid;
            break;
    }
}

/**
 * Count the number of digits in an offset.
 *
 * This function is used by various string generators to figure
 * out how large a data buffer to allocate to hold the string
 * representation of a number.
 *
 * @param num
 *        The number to check.
 *
 * @return The numbe of digits (plus 1 for negative numbers) in
 *         an offset.
 */
int
globus_i_ftp_client_count_digits(globus_off_t num)
{
    int digits = 1;

    if(num < 0)
    {
	digits++;
	num = -num;
    }
    while(0 < (num = (num / 10))) digits++;

    return digits;
}
/* globus_i_ftp_client_count_digits() */


#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */

