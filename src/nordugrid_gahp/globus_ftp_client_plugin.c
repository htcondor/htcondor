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
 * @file globus_ftp_client_plugin.c Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_plugin.c,v $
 * $Revision: 1.13 $
 * $Date: 2006/10/14 07:21:56 $
 */
#endif

#include "globus_i_ftp_client.h"

#include <string.h>

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
#define PLUGIN_SUPPORTS_OP(op,plugin) \
   (((op) == GLOBUS_FTP_CLIENT_GET && (plugin)->get_func) \
    || ((op) == GLOBUS_FTP_CLIENT_PUT && (plugin)->put_func) \
    || ((op) == GLOBUS_FTP_CLIENT_TRANSFER && \
	(plugin)->third_party_transfer_func) \
    || ((op) == GLOBUS_FTP_CLIENT_NLST && (plugin)->list_func) \
    || ((op) == GLOBUS_FTP_CLIENT_LIST && (plugin)->verbose_list_func) \
    || ((op) == GLOBUS_FTP_CLIENT_MLSD && (plugin)->machine_list_func) \
    || ((op) == GLOBUS_FTP_CLIENT_MLST && (plugin)->mlst_func) \
    || ((op) == GLOBUS_FTP_CLIENT_STAT && (plugin)->stat_func) \
    || ((op) == GLOBUS_FTP_CLIENT_CHMOD && (plugin)->chmod_func) \
    || ((op) == GLOBUS_FTP_CLIENT_DELETE && (plugin)->delete_func) \
    || ((op) == GLOBUS_FTP_CLIENT_MKDIR && (plugin)->mkdir_func) \
    || ((op) == GLOBUS_FTP_CLIENT_RMDIR && (plugin)->rmdir_func) \
    || ((op) == GLOBUS_FTP_CLIENT_MOVE && (plugin)->move_func) \
    || ((op) == GLOBUS_FTP_CLIENT_MDTM && (plugin)->modification_time_func) \
    || ((op) == GLOBUS_FTP_CLIENT_SIZE && (plugin)->size_func) \
    || ((op) == GLOBUS_FTP_CLIENT_CKSM && (plugin)->cksm_func) \
    || ((op) == GLOBUS_FTP_CLIENT_FEAT && (plugin)->feat_func))
#endif


static
globus_result_t
globus_l_ftp_client_plugin_restart_operation(
    globus_i_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_ftp_client_restart_marker_t *	restart,
    const globus_abstime_t *			when);



/**
 * Restart an existing list.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in list events will
 * receive a list callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the list.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original list's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original list's attribute set.
 * @param when
 *        Absolute time for when to restart the list. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_list);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_list() */

/**
 * Restart an existing verbose list.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in list events will
 * receive a list callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the list.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original list's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original list's attribute set.
 * @param when
 *        Absolute time for when to restart the list. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_verbose_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_verbose_list);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_verbose_list() */


/**
 * Restart an existing machine list.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in list events will
 * receive a list callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the list.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original list's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original list's attribute set.
 * @param when
 *        Absolute time for when to restart the list. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_machine_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_machine_list);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_machine_list() */

/**
 * Restart an existing MLST.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in list events will
 * receive a list callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the list.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original list's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original list's attribute set.
 * @param when
 *        Absolute time for when to restart the list. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_mlst(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_mlst);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_mlst() */

/**
 * Restart an existing STAT.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in list events will
 * receive a list callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the list.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original list's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original list's attribute set.
 * @param when
 *        Absolute time for when to restart the list. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_stat(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_stat);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_stat() */

/**
 * Restart an existing chmod.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing chmod operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in chmod events will
 * receive a chmod callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the chmod.
 * @param url
 *        The destination URL of the transfer. This may be different than
 *        the original chmod's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param mode
 *        The file mode that will be applied. Must be an octal number repre-
 *        senting the bit pattern for the new permissions.
 * @param attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original chmod's attribute set.
 * @param when
 *        Absolute time for when to restart the chmod. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_chmod(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_chmod);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }
    if(mode == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("mode");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_chmod() */

/**
 * Restart an existing cksm.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing cksm operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in cksm events will
 * receive a cksm callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the cksm.
 * @param url
 *        The destination URL of the transfer. This may be different than
 *        the original cksm's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param offset
 *        File offset to start calculating checksum.    
 * @param length
 *        Length of data to read from the starting offset.  Use -1 to read the
 *        entire file.
 * @param algorithm
 *        A pointer to a string to be filled with the checksum of the
 *        file. On error the value pointed to by it is undefined.          
 * @param attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original cksm's attribute set.
 * @param when
 *        Absolute time for when to restart the cksm. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_cksm(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_cksm);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_cksm() */

/**
 * Restart an existing delete.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing delete operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in delete events will
 * receive a delete callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the delete.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original delete's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original delete's attribute set.
 * @param when
 *        Absolute time for when to restart the delete. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_delete(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_delete);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_delete() */

/**
 * Restart an existing feat.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing feat operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in feat events will
 * receive a feat callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the feat.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original feat's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original feat's attribute set.
 * @param when
 *        Absolute time for when to restart the feat. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_feat(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_feat);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_feat() */

/**
 * Restart an existing mkdir.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in mkdir events will
 * receive a mkdir callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the mkdir.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original mkdir's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original mkdir's attribute set.
 * @param when
 *        Absolute time for when to restart the mkdir. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_mkdir(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_mkdir);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_mkdir() */

/**
 * Restart an existing rmdir.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in rmdir events will
 * receive a rmdir callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the rmdir.
 * @param source_url
 *        The destination URL of the transfer. This may be different than
 *        the original rmdir's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original rmdir's attribute set.
 * @param when
 *        Absolute time for when to restart the rmdir. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_rmdir(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_rmdir);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }
    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_rmdir() */



/**
 * Restart an existing move.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing move operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially new URLs and
 * attributes.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in get events will
 * receive a move callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the move.
 * @param source_url
 *        The source URL of the move. This may be different than
 *        the original get's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param dest_url
 *        The destination URL of the move. This may be different than
 *        the original get's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL. Note that only the path
 *        component of this URL is used.
 * @param attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original move's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param when
 *        Absolute time for when to restart the move. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_move(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_move);

    if(source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("source_url");

	return globus_error_put(err);
    }

    if(dest_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("dest_url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							source_url,
							attr,
							dest_url,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_move() */

/**
 * Restart an existing get.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued will be cleared and reused once the connection is
 * re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in get events will
 * receive a get callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the get.
 * @param url
 *        The source URL of the transfer. This may be different than
 *        the original get's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original get's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param restart_marker
 *        Plugin-provided restart marker for resuming at a non-default
 *        restart point. This may be used to implement a persistent
 *        restart across process invocations. The default behavior if
 *        this is NULL is to use any restart information which has
 *        been received by the ftp client library while processing this
 *        operation when restarted.
 * @param when
 *        Absolute time for when to restart the get. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_get(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_get);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							restart_marker,
							when);
}
/* globus_ftp_client_plugin_restart_get() */

/**
 * Restart an existing put.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes. Any data buffers which are
 * currently queued but not called back will be resent once the
 * connection is re-established.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in get events will
 * receive a put callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the put.
 * @param url
 *        The URL of the transfer. This may be different than
 *        the original put's URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL. If the put is restarted with a
 *        different URL, the plugin must re-send any data which has
 *        already been acknowledged by it's callback.
 * @param attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original put's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param restart_marker
 *        Plugin-provided restart marker for resuming at a non-default
 *        restart point. This may be used to implement a persistent
 *        restart across process invocations. The default behavior if
 *        this is NULL is to use any restart information which has
 *        been received by the ftp client library while processing this
 *        operation when restarted.

 * @param when
 *        Absolute time for when to restart the put. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_put(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_put);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }
    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							GLOBUS_NULL,
							GLOBUS_NULL,
							url,
							attr,
							restart_marker,
							when);
}
/* globus_ftp_client_plugin_restart_put() */

/**
 * Restart an existing third-party transfer.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URLs and
 * attributes.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in third-party transfer
 * events will receive a transfer callback with the restart boolean set to
 * GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the transfer.
 * @param source_url
 *        The source URL of the transfer. This may be different than
 *        the original URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL
 * @param source_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original transfer's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param dest_url
 *        The destination URL of the transfer. This may be different than
 *        the original destination URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param dest_attr
 *        The attributes to use for the new transfer. This may be a
 *        modified version of the original transfer's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param restart_marker
 *        Plugin-provided restart marker for resuming at a non-default
 *        restart point. This may be used to implement a persistent
 *        restart across process invocations. The default behavior if
 *        this is NULL is to use any restart information which has
 *        been received by the ftp client library while processing this
 *        operation when restarted.

 * @param when
 *        Absolute time for when to restart the transfer. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_third_party_transfer(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_transfer);

    if(source_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("source_url");

	return globus_error_put(err);
    }
    else if(dest_url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("dest_url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							source_url,
							source_attr,
							dest_url,
							dest_attr,
							restart_marker,
							when);
}
/* globus_ftp_client_plugin_restart_third_party_transfer() */

/**
 * Restart a size check operation.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing size check operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in size operations will
 * receive a size callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the operation.
 * @param source_url
 *        The source URL of the size check. This may be different than
 *        the original operations URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param source_attr
 *        The attributes to use for the new operation. This may be a
 *        modified version of the original operations's attribute set.
 * @param when
 *        Absolute time for when to restart the size check. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_size(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_size);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_size() */

/**
 * Restart a modification time check operation.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing modification time check
 * operation to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URL and
 * attributes.
 *
 * The user will not receive any notification that a restart has happened. Each
 * plugin which is interested in modification time operations will receive a
 * modification time callback with the restart boolean set to GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the operation.
 * @param source_url
 *        The source URL of the modification time check. This may be different
 *        than the original operations URL, if the plugin decides to redirect
 *        to another FTP server due to performance or reliability problems with
 *        the original URL.
 * @param source_attr
 *        The attributes to use for the new operation. This may be a
 *        modified version of the original operations's attribute set.
 * @param when
 *        Absolute time for when to restart the modification time check. The
 *        current control and data connections will be stopped immediately. If
 *        this completes before <b>when</b>, then the restart will be delayed
 *        until that time. Otherwise, it will be immediately restarted.
 */
globus_result_t
globus_ftp_client_plugin_restart_modification_time(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when)
{
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_modification_time);

    if(url == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");

	return globus_error_put(err);
    }

    i_handle = *handle;

    return globus_l_ftp_client_plugin_restart_operation(i_handle,
							url,
							attr,
							GLOBUS_NULL,
							GLOBUS_NULL,
							GLOBUS_NULL,
							when);
}
/* globus_ftp_client_plugin_restart_modification_time() */


/**
 * Get restart marker
 * @ingroup globus_ftp_client_plugins
 *
 * This function will allow this user to get the restart marker
 * associated with a restarted file transfer.  This function may only be
 * called within the get, put, or third party transfer callback in which
 * the 'restart' argument is GLOBUS_TRUE
 *
 * @param handle
 *        The handle which is associated with the transfer.
 *
 * @param marker
 *        Pointer to an uninitialized restart marker type
 *
 * @return
 *        - Error on NULL handle or marker
 *        - Error on invalid use of function
 *        - GLOBUS_SUCCESS (marker will be populated)
 */

globus_result_t
globus_ftp_client_plugin_restart_get_marker(
    globus_ftp_client_handle_t *		handle,
    globus_ftp_client_restart_marker_t *	marker)
{
    globus_result_t				result;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_restart_get_marker);

    if(handle == GLOBUS_NULL)
    {
        return globus_error_put(
	    GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle"));
    }

    if(marker == GLOBUS_NULL)
    {
        return globus_error_put(
	    GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("marker"));
    }

    i_handle = *handle;

    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle))
    {
	return globus_error_put(
	    GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("marker"));
    }

    globus_i_ftp_client_handle_lock(i_handle);

    if(i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
        if(i_handle->op == GLOBUS_FTP_CLIENT_GET ||
            i_handle->op == GLOBUS_FTP_CLIENT_PUT ||
            i_handle->op == GLOBUS_FTP_CLIENT_TRANSFER)
        {
            if(i_handle->restart_info)
            {
                result = globus_ftp_client_restart_marker_copy(
                    marker,
                    &i_handle->restart_info->marker);
            }
            else
            {
                result = globus_error_put(
                    GLOBUS_I_FTP_CLIENT_NO_RESTART_MARKER());
            }
        }
        else
        {
            result = globus_error_put(
                GLOBUS_I_FTP_CLIENT_NO_RESTART_MARKER());
        }
    }
    else
    {
        result = globus_error_put(
            GLOBUS_I_FTP_CLIENT_NO_RESTART_MARKER());
    }

    globus_i_ftp_client_handle_unlock(i_handle);


    return result;
}

/**
 * Abort a transfer operation.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to be aborted. When this happens, all plugins will be notified by
 * their abort callbacks. Once those are processed, the complete
 * callback will be called for all plugins, and then for the user's
 * callback.
 *
 * The complete callback will indicate that the transfer did not
 * complete successfully.
 *
 * @param handle
 *        The handle which is associated with the transfer.
 */
globus_result_t
globus_ftp_client_plugin_abort(
    globus_ftp_client_handle_t *		handle)
{
    return globus_ftp_client_abort(handle);
}
/* globus_ftp_client_plugin_abort() */

/**
 * Add data channels to an existing put transfer.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to have additional data channels acquired if the attribute set
 * allows it.
 *
 * @param handle
 *        The handle which is associated with the transfer.
 * @param num_channels
 *        The number of channels to add to the transfer.
 * @param stripe
 *        The stripe number to have the channels added to.
 *
 * @note Do the plugins need to be notified when this happens?
 */
globus_result_t
globus_ftp_client_plugin_add_data_channels(
    globus_ftp_client_handle_t *		handle,
    unsigned int				num_channels,
    unsigned int				stripe)
{
    globus_result_t				result;
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_add_data_channels);

    i_handle = *handle;
    globus_i_ftp_client_handle_lock(i_handle);

    if(i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
       i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
       i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_NOT_IN_USE("handle");

	goto err_exit;
    }

    if(i_handle->op != GLOBUS_FTP_CLIENT_PUT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_OPERATION(i_handle->op);

	goto err_exit;
    }
    else
    {
	result = globus_ftp_control_data_add_channels(
	    i_handle->dest->control_handle,
	    num_channels,
	    stripe);

	goto result_exit;
    }

 err_exit:
    result = globus_error_put(err);
    globus_i_ftp_client_handle_unlock(i_handle);

 result_exit:
    return result;
}
/* globus_ftp_client_plugin_add_data_channels() */

/**
 * Remove data channels from an existing put transfer.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing transfer operation
 * to have data channels removed,  if the attribute set
 * allows it.
 *
 * @param handle
 *        The handle which is associated with the transfer.
 * @param num_channels
 *        The number of channels to remove from the transfer.
 * @param stripe
 *        The stripe number to have the channels removed from.
 *
 * @note Do the plugins need to be notified when this happens?
 */
globus_result_t
globus_ftp_client_plugin_remove_data_channels(
    globus_ftp_client_handle_t *		handle,
    unsigned int				num_channels,
    unsigned int				stripe)
{
    globus_result_t				result;
    globus_object_t *				err;
    globus_i_ftp_client_handle_t *		i_handle;
    GlobusFuncName(globus_ftp_client_plugin_remove_data_channels);

    i_handle = *handle;
    globus_i_ftp_client_handle_lock(i_handle);

    if(i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
       i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
       i_handle->state == GLOBUS_FTP_CLIENT_HANDLE_FAILURE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_NOT_IN_USE("handle");

	goto err_exit;
    }

    if(i_handle->op != GLOBUS_FTP_CLIENT_PUT)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_OPERATION(i_handle->op);

	goto err_exit;
    }
    else
    {
	result = globus_ftp_control_data_remove_channels(
	    i_handle->source->control_handle,
	    num_channels,
	    stripe);

	goto result_exit;
    }

 err_exit:
    result = globus_error_put(err);

result_exit:
    globus_i_ftp_client_handle_unlock(i_handle);

    return result;
}
/* globus_ftp_client_plugin_remove_data_channels() */

globus_result_t
globus_ftp_client_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    const char *				plugin_name,
    globus_ftp_client_plugin_command_mask_t	command_mask,
    void *					plugin_specific)
{
    globus_i_ftp_client_plugin_t *		i_plugin;
    globus_object_t *				err;
    GlobusFuncName(globus_ftp_client_plugin_init);

    if(plugin == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("plugin");

	goto error_exit;
    }
    if(plugin_name == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("plugin_name");

	goto nullify_exit;
    }

    i_plugin = globus_libc_calloc(1, sizeof(globus_i_ftp_client_plugin_t));

    if(i_plugin == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("plugin");

	goto free_exit;
    }
    i_plugin->plugin_name = globus_libc_strdup(plugin_name);
    if(i_plugin->plugin_name == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("plugin");

	goto free_exit;
    }
    i_plugin->command_mask = command_mask;
    i_plugin->plugin_specific = plugin_specific;
    i_plugin->plugin = plugin;

    *plugin = i_plugin;
    return GLOBUS_SUCCESS;

free_exit:
    globus_libc_free(i_plugin);

nullify_exit:
    *plugin = GLOBUS_NULL;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_plugin_init() */

globus_result_t
globus_ftp_client_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin)
{
    globus_object_t *				err;
    GlobusFuncName(globus_ftp_client_plugin_destroy);

    if(plugin == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("plugin");

	goto error_exit;
    }
    if(*plugin == GLOBUS_NULL)
    {
	return GLOBUS_SUCCESS;
    }
    if((*plugin)->plugin_name)
    {
	globus_libc_free((*plugin)->plugin_name);
    }
    globus_libc_free((*plugin));
    *plugin = GLOBUS_NULL;

    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_plugin_destroy() */

globus_result_t
globus_ftp_client_plugin_get_plugin_specific(
    globus_ftp_client_plugin_t *		plugin,
    void **					plugin_specific)
{
    globus_i_ftp_client_plugin_t *		i_plugin;
    globus_object_t *				err;
    GlobusFuncName(globus_ftp_client_plugin_get_plugin_specific);

    if(plugin == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("plugin");

	goto error_exit;
    }
    i_plugin = *plugin;
    if(i_plugin == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("plugin");

	goto error_exit;
    }
    *plugin_specific = i_plugin->plugin_specific;
    return GLOBUS_SUCCESS;

error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_plugin_get_plugin_specific() */

/* @{ */
/** @name Plugin Accessor Functions */
#define GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(type) \
globus_result_t \
globus_ftp_client_plugin_set_##type##_func(\
    globus_ftp_client_plugin_t *		plugin, \
    globus_ftp_client_plugin_##type##_t		type) \
{ \
    globus_i_ftp_client_plugin_t *		i_plugin; \
    globus_object_t *				err; \
    GlobusFuncName(globus_ftp_client_plugin_set_" #type "_func); \
    if(plugin == GLOBUS_NULL) \
    { \
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("plugin"); \
 \
	goto error_exit; \
    } \
    i_plugin = *plugin; \
    if(i_plugin == GLOBUS_NULL) \
    { \
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("plugin"); \
 \
	goto error_exit; \
    } \
    i_plugin->type##_func = type; \
 \
    return GLOBUS_SUCCESS; \
 \
error_exit: \
    return globus_error_put(err); \
}

GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(copy)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(destroy)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(chmod)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(cksm)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(delete)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(feat)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(mkdir)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(rmdir)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(move)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(verbose_list)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(machine_list)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(mlst)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(stat)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(list)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(get)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(put)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(third_party_transfer)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(modification_time)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(size)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(abort)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(connect)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(authenticate)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(read)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(write)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(data)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(command)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(response)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(fault)
GLOBUS_FTP_CLIENT_PLUGIN_SET_FUNC(complete)
/* @} */

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/*--------------------------------------------------------------------------
 * Local/Internal Functions.
 *--------------------------------------------------------------------------
 */
/**
 * Restart an existing operation.
 * @ingroup globus_ftp_client_plugins
 *
 * This function will cause the currently executing operation
 * to be restarted. When a restart happens, the operation will be
 * silently aborted, and then restarted with potentially a new URLs and
 * attributes.
 *
 * The user will not receive any notification that a restart has
 * happened. Each plugin which is interested in events caused by the
 * operation will receive a callback with the restart boolean set to
 * GLOBUS_TRUE.
 *
 * @param handle
 *        The handle which is associated with the operation.
 * @param source_url
 *        The source URL of the operation. This may be different than
 *        the original URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL
 * @param source_attr
 *        The attributes to use for the new operation. This may be a
 *        modified version of the original operations's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param dest_url
 *        The destination URL of the operation. This may be different than
 *        the original destination URL, if the plugin decides to redirect to
 *        another FTP server due to performance or reliability
 *        problems with the original URL.
 * @param dest_attr
 *        The attributes to use for the new operation. This may be a
 *        modified version of the original operations's attribute set. This
 *        may be useful when the plugin wishes to send restart markers
 *        to the FTP server to prevent re-sending the data which has
 *        already been sent.
 * @param restart_marker
 *        Plugin-provided restart marker for resuming at a non-default
 *        restart point. This may be used to implement a persistent
 *        restart across process invocations. The default behavior if
 *        this is NULL is to use any restart information which has
 *        been received by the ftp client library while processing this
 *        operation when restarted.
 * @param when
 *        Absolute time for when to restart the operation. The current
 *        control and data connections will be stopped
 *        immediately. If this completes before <b>when</b>, then the
 *	  restart will be delayed until that time. Otherwise, it will
 *        be immediately restarted.
 */
static
globus_result_t
globus_l_ftp_client_plugin_restart_operation(
    globus_i_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *			when)
{
    globus_object_t *				err;
    globus_result_t				result;
    globus_i_ftp_client_restart_t *		restart_info;
    GlobusFuncName(globus_i_ftp_client_plugin_restart_operation);

    if(handle == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("handle");

	goto error_exit;
    }
    /* Check handle state */
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(handle->handle))
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("handle");

	goto error_exit;
    }
    restart_info = (globus_i_ftp_client_restart_t *)
	globus_libc_malloc(sizeof(globus_i_ftp_client_restart_t));

    if(restart_info == GLOBUS_NULL)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	goto error_exit;
    }

    if(dest_url != GLOBUS_NULL)
    {
	restart_info->dest_url = globus_libc_strdup(dest_url);
	if(restart_info->dest_url == GLOBUS_NULL)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	    goto free_restart_exit;
	}
	if(dest_attr)
	{
	    result = globus_ftp_client_operationattr_copy(
		&restart_info->dest_attr,
		dest_attr);
	    if(result)
	    {
		err = globus_error_get(result);

		goto free_dest_attr_exit;
	    }
	}
	else
	{
	    restart_info->dest_attr = GLOBUS_NULL;
	}
    }
    else
    {
	restart_info->dest_url = GLOBUS_NULL;
	restart_info->dest_attr = GLOBUS_NULL;
    }

    if(source_url != GLOBUS_NULL)
    {
	restart_info->source_url = globus_libc_strdup(source_url);
	if(restart_info->source_url == GLOBUS_NULL)
	{
	    err = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();

	    goto destroy_dest_attr_exit;
	}

	if(source_attr)
	{
	    result = globus_ftp_client_operationattr_copy(
		&restart_info->source_attr,
		source_attr);
	    if(result)
	    {
		err = globus_error_get(result);

		goto free_source_attr_exit;
	    }
	}
	else
	{
	    restart_info->source_attr = GLOBUS_NULL;
	}
    }
    else
    {
	restart_info->source_url = GLOBUS_NULL;
	restart_info->source_attr = GLOBUS_NULL;
    }

    if(restart_marker)
    {
	globus_ftp_client_restart_marker_copy(&restart_info->marker,
					      restart_marker);
    }
    else
    {
	globus_ftp_client_restart_marker_copy(&restart_info->marker,
					      &handle->restart_marker);
    }

    if(when)
    {
	GlobusTimeAbstimeCopy(restart_info->when, *when);
    }
    else
    {
	GlobusTimeAbstimeSet(restart_info->when, 0L, 0L);
    }


    globus_i_ftp_client_handle_lock(handle);

    if(handle->op == GLOBUS_FTP_CLIENT_IDLE)
    {
	err = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_OPERATION(handle->op);

	goto unlock_error;
    }

    err = globus_i_ftp_client_restart(handle,
				      restart_info);

    if(err != GLOBUS_SUCCESS)
    {
	goto unlock_error;
    }

    globus_i_ftp_client_handle_unlock(handle);
    return GLOBUS_SUCCESS;

unlock_error:
    globus_i_ftp_client_handle_unlock(handle);

    if(restart_info->source_attr)
    {
	globus_ftp_client_operationattr_destroy(&restart_info->source_attr);
    }
free_source_attr_exit:
    if(restart_info->source_attr)
    {
	globus_libc_free(restart_info->source_attr);
    }
    globus_libc_free(restart_info->source_url);
destroy_dest_attr_exit:
    if(restart_info->dest_attr)
    {
	globus_ftp_client_operationattr_destroy(&restart_info->dest_attr);
    }
free_dest_attr_exit:
    if(restart_info->dest_attr)
    {
	globus_libc_free(restart_info->dest_attr);
    }
    globus_libc_free(restart_info->dest_url);
free_restart_exit:
    globus_libc_free(restart_info);
error_exit:
    return globus_error_put(err);
}
/* globus_ftp_client_plugin_restart_operation() */

/*@{*/
/**
 * Plugin notification functions
 * @ingroup globus_ftp_client_plugins
 *
 * These function notify all interested plugins that an event related
 * to the current transfer for the handle is happening. Event
 * notification is delivered to a plugin only if the command_mask
 * associated with the plugin indicates that the plugin is interested
 * in the event, and the plugin supports the operation.
 */
void
globus_i_ftp_client_plugin_notify_delete(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->delete_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->delete_func)(plugin->plugin,
				  plugin->plugin_specific,
				  handle->handle,
				  url,
				  &attr,
				  GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

/*@{*/
/**
 * Plugin notification functions
 * @ingroup globus_ftp_client_plugins
 *
 * These function notify all interested plugins that an event related
 * to the current transfer for the handle is happening. Event
 * notification is delivered to a plugin only if the command_mask
 * associated with the plugin indicates that the plugin is interested
 * in the event, and the plugin supports the operation.
 */
void
globus_i_ftp_client_plugin_notify_chmod(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->chmod_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->chmod_func)(plugin->plugin,
				  plugin->plugin_specific,
				  handle->handle,
				  url,
				  mode,
				  &attr,
				  GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void

globus_i_ftp_client_plugin_notify_cksm(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->cksm_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->cksm_func)(plugin->plugin,
				  plugin->plugin_specific,
				  handle->handle,
				  url,
				  offset,
				  length,
				  algorithm,
				  &attr,
				  GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_feat(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->feat_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->feat_func)(plugin->plugin,
				  plugin->plugin_specific,
				  handle->handle,
				  url,
				  &attr,
				  GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_mkdir(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->mkdir_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->mkdir_func)(plugin->plugin,
				 plugin->plugin_specific,
				 handle->handle,
				 url,
				 &attr,
				 GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_rmdir(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->rmdir_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->rmdir_func)(plugin->plugin,
				 plugin->plugin_specific,
				 handle->handle,
				 url,
				 &attr,
				 GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_move(
    globus_i_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);
	if(plugin->move_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->move_func)(plugin->plugin,
				plugin->plugin_specific,
				handle->handle,
				source_url,
				dest_url,
				&attr,
				GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_verbose_list(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);
	if(plugin->verbose_list_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->verbose_list_func)(plugin->plugin,
					plugin->plugin_specific,
					handle->handle,
					url,
					&attr,
					GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_machine_list(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);
	if(plugin->machine_list_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->machine_list_func)(plugin->plugin,
					plugin->plugin_specific,
					handle->handle,
					url,
					&attr,
					GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_list(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->list_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->list_func)(plugin->plugin,
				plugin->plugin_specific,
				handle->handle,
				url,
				&attr,
				GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_mlst(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);
	if(plugin->mlst_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->mlst_func)(plugin->plugin,
					plugin->plugin_specific,
					handle->handle,
					url,
					&attr,
					GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_stat(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);
	if(plugin->stat_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->stat_func)(plugin->plugin,
					plugin->plugin_specific,
					handle->handle,
					url,
					&attr,
					GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_get(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr,
    const globus_ftp_client_restart_marker_t *	restart)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->get_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->get_func)(plugin->plugin,
			       plugin->plugin_specific,
			       handle->handle,
			       url,
			       &attr,
			       GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}


void
globus_i_ftp_client_plugin_notify_put(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr,
    const globus_ftp_client_restart_marker_t *	restart)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;


    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->put_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->put_func)(plugin->plugin,
			       plugin->plugin_specific,
			       handle->handle,
			       url,
			       &attr,
			       GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_transfer(
    globus_i_ftp_client_handle_t *		handle,
    const char *				source_url,
    globus_i_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    globus_i_ftp_client_operationattr_t *	dest_attr,
    const globus_ftp_client_restart_marker_t *	restart)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;


    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->third_party_transfer_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->third_party_transfer_func)(plugin->plugin,
				                plugin->plugin_specific,
				                handle->handle,
				                source_url,
				                &source_attr,
				                dest_url,
				                &dest_attr,
				                GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_modification_time(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->modification_time_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->modification_time_func)(plugin->plugin,
				             plugin->plugin_specific,
				             handle->handle,
				             url,
				             &attr,
				             GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}

void
globus_i_ftp_client_plugin_notify_size(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->size_func)
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->size_func)(plugin->plugin,
				plugin->plugin_specific,
				handle->handle,
				url,
				&attr,
				GLOBUS_FALSE);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}



void
globus_i_ftp_client_plugin_notify_connect(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

        if(plugin->connect_func &&
           PLUGIN_SUPPORTS_OP(handle->op, plugin) &&
           (plugin->command_mask &
            GLOBUS_FTP_CLIENT_CMD_MASK_CONTROL_ESTABLISHMENT))
        {
            if(!unlocked)
            {
                globus_i_ftp_client_handle_unlock(handle);
                unlocked = GLOBUS_TRUE;
            }
            (plugin->connect_func)(plugin->plugin,
        		       plugin->plugin_specific,
        		       handle->handle,
        		       url);
        }
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_connect() */

void
globus_i_ftp_client_plugin_notify_authenticate(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_control_auth_info_t *	auth_info)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->authenticate_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin) &&
	   (plugin->command_mask &
	    GLOBUS_FTP_CLIENT_CMD_MASK_CONTROL_ESTABLISHMENT))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->authenticate_func)(plugin->plugin,
					plugin->plugin_specific,
				        handle->handle,
				        url,
				        auth_info);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }
    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_authenticate() */

void
globus_i_ftp_client_plugin_notify_read(
    globus_i_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;


    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->read_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->read_func)(plugin->plugin,
				plugin->plugin_specific,
				handle->handle,
				buffer,
				buffer_length);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_read() */

void
globus_i_ftp_client_plugin_notify_write(
    globus_i_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->write_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->write_func)(plugin->plugin,
				 plugin->plugin_specific,
				 handle->handle,
				 buffer,
				 buffer_length,
				 offset,
				 eof);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_write() */

void
globus_i_ftp_client_plugin_notify_command(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_plugin_command_mask_t	command_mask,
    const char *				command_spec,
    ...)
{
    va_list					ap;
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    int						len;
    char *					tmpstr;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    va_start(ap, command_spec);

    len = globus_libc_vprintf_length(command_spec, ap);

    va_end(ap);
    
    tmpstr = globus_libc_malloc(len + 1);

    va_start(ap, command_spec);
    
    globus_libc_vsprintf(tmpstr, command_spec, ap);

    va_end(ap);

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->command_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin) &&
	   (plugin->command_mask & command_mask))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->command_func)(plugin->plugin,
				   plugin->plugin_specific,
				   handle->handle,
				   url,
				   tmpstr);
	}
    }
    globus_libc_free(tmpstr);
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_command() */

void
globus_i_ftp_client_plugin_notify_response(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_plugin_command_mask_t	command_mask,
    globus_object_t *				error,
    const globus_ftp_control_response_t *	ftp_response)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;


    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->response_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin) &&
	   (plugin->command_mask & command_mask))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->response_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle,
				    url,
				    error,
				    ftp_response);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_response() */

void
globus_i_ftp_client_plugin_notify_fault(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->fault_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->fault_func)(plugin->plugin,
				 plugin->plugin_specific,
				 handle->handle,
				 url,
				 error);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_fault() */

void
globus_i_ftp_client_plugin_notify_complete(
    globus_i_ftp_client_handle_t *		handle)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->complete_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->complete_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_complete() */

void
globus_i_ftp_client_plugin_notify_data(
    globus_i_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    handle->notify_in_progress++;

    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->data_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    (plugin->data_func)(plugin->plugin,
				plugin->plugin_specific,
				handle->handle,
				error,
				buffer,
				buffer_length,
				offset,
				eof);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_data() */

void
globus_i_ftp_client_plugin_notify_abort(
    globus_i_ftp_client_handle_t *		handle)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    if(handle->notify_in_progress)
    {
	handle->notify_abort = GLOBUS_TRUE;

	return;
    }
    handle->notify_in_progress++;


    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(plugin->abort_func &&
	   PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }

	    (plugin->abort_func)(plugin->plugin,
				 plugin->plugin_specific,
				 handle->handle);
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_restart)
    {
	handle->notify_restart = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_restart(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_abort() */

void
globus_i_ftp_client_plugin_notify_restart(
    globus_i_ftp_client_handle_t *		handle)
{
    globus_i_ftp_client_plugin_t *		plugin;
    globus_list_t *				tmp;
    globus_bool_t				unlocked = GLOBUS_FALSE;

    if(handle->notify_in_progress)
    {
	handle->notify_restart = GLOBUS_TRUE;
	return;
    }
    handle->notify_in_progress++;


    tmp = handle->attr.plugins;
    while(!globus_list_empty(tmp))
    {
	plugin = (globus_i_ftp_client_plugin_t *) globus_list_first(tmp);
	tmp = globus_list_rest(tmp);

	if(PLUGIN_SUPPORTS_OP(handle->op, plugin))
	{
	    if(!unlocked)
	    {
		globus_i_ftp_client_handle_unlock(handle);
		unlocked = GLOBUS_TRUE;
	    }
	    if(handle->op == GLOBUS_FTP_CLIENT_GET)
	    {
		(plugin->get_func)(plugin->plugin,
				   plugin->plugin_specific,
				   handle->handle,
				   handle->restart_info->source_url,
				   &handle->restart_info->source_attr,
				   GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_LIST)
	    {
		(plugin->verbose_list_func)(plugin->plugin,
					    plugin->plugin_specific,
				            handle->handle,
				            handle->restart_info->source_url,
				            &handle->restart_info->source_attr,
				            GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_NLST)
	    {
		(plugin->list_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle,
				    handle->restart_info->source_url,
				    &handle->restart_info->source_attr,
				    GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_MLSD)
	    {
		(plugin->machine_list_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle,
				    handle->restart_info->source_url,
				    &handle->restart_info->source_attr,
				    GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_MLST)
	    {
		(plugin->mlst_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle,
				    handle->restart_info->source_url,
				    &handle->restart_info->source_attr,
				    GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_STAT)
	    {
		(plugin->stat_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle,
				    handle->restart_info->source_url,
				    &handle->restart_info->source_attr,
				    GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_CHMOD)
	    {
		(plugin->chmod_func)(plugin->plugin,
				      plugin->plugin_specific,
				      handle->handle,
				      handle->restart_info->source_url,
				      handle->chmod_file_mode,
				      &handle->restart_info->source_attr,
				      GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_DELETE)
	    {
		(plugin->delete_func)(plugin->plugin,
				      plugin->plugin_specific,
				      handle->handle,
				      handle->restart_info->source_url,
				      &handle->restart_info->source_attr,
				      GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_FEAT)
	    {
		(plugin->feat_func)(plugin->plugin,
				      plugin->plugin_specific,
				      handle->handle,
				      handle->restart_info->source_url,
				      &handle->restart_info->source_attr,
				      GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_SIZE)
	    {
		(plugin->size_func)(plugin->plugin,
				      plugin->plugin_specific,
				      handle->handle,
				      handle->restart_info->source_url,
				      &handle->restart_info->source_attr,
				      GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_CKSM)
	    {
		(plugin->cksm_func)(plugin->plugin,
				      plugin->plugin_specific,
				      handle->handle,
				      handle->restart_info->source_url,
				      handle->checksum_offset,
				      handle->checksum_length,
				      handle->checksum_alg,
				      &handle->restart_info->source_attr,
				      GLOBUS_TRUE);
            }
	    else if(handle->op == GLOBUS_FTP_CLIENT_MDTM)
	    {
		(plugin->modification_time_func)(plugin->plugin,
				      plugin->plugin_specific,
				      handle->handle,
				      handle->restart_info->source_url,
				      &handle->restart_info->source_attr,
				      GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_MKDIR)
	    {
		(plugin->mkdir_func)(plugin->plugin,
				     plugin->plugin_specific,
				     handle->handle,
				     handle->restart_info->source_url,
				     &handle->restart_info->source_attr,
				     GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_RMDIR)
	    {
		(plugin->rmdir_func)(plugin->plugin,
				     plugin->plugin_specific,
				     handle->handle,
				     handle->restart_info->source_url,
				     &handle->restart_info->source_attr,
				     GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_MOVE)
	    {
		(plugin->move_func)(plugin->plugin,
				    plugin->plugin_specific,
				    handle->handle,
				    handle->restart_info->source_url,
				    handle->restart_info->dest_url,
				    &handle->restart_info->source_attr,
				    GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_PUT)
	    {
		(plugin->put_func)(plugin->plugin,
				   plugin->plugin_specific,
				   handle->handle,
				   handle->restart_info->dest_url,
				   &handle->restart_info->dest_attr,
				   GLOBUS_TRUE);
	    }
	    else if(handle->op == GLOBUS_FTP_CLIENT_TRANSFER)
	    {
		(plugin->third_party_transfer_func)(
				   plugin->plugin,
				   plugin->plugin_specific,
				   handle->handle,
				   handle->restart_info->source_url,
				   &handle->restart_info->source_attr,
				   handle->restart_info->dest_url,
				   &handle->restart_info->dest_attr,
				   GLOBUS_TRUE);
	    }
	    else
	    {
	        globus_assert(0 && "Unexpected operation");
	    }
	}
    }
    if(unlocked)
    {
	globus_i_ftp_client_handle_lock(handle);
    }

    handle->notify_in_progress--;
    if(handle->notify_abort)
    {
	handle->notify_abort = GLOBUS_FALSE;

	globus_i_ftp_client_plugin_notify_abort(handle);
    }
}
/* globus_i_ftp_client_plugin_notify_restart() */
/*@}*/
#endif
