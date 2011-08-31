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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_RESTART_MARKER_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_RESTART_MARKER_PLUGIN_H
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_ftp_client_restart_marker_plugin.h GridFTP Restart Marker Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_restart_marker_plugin.h,v $
 * $Revision: 1.4 $
 * $Date: 2006/01/19 05:54:53 $
 * $Author: mlink $
 */
#endif

/**
 * @defgroup globus_ftp_client_restart_marker_plugin Restart Marker Plugin
 * @ingroup globus_ftp_client_plugins
 *
 * This plugin is intended to allow users to make restart markers persistant.
 * During a transfer, every marker received will result in the user's 'marker'
 * callback being called with the new restart marker that can be stored. If
 * the application were to prematurely terminate (while transferring), the user
 * (after restarting the application) could pass this stored marker back to the
 * plugin via the 'begin' callback to force the transfer to be restarted from
 * the last marked point.
 */

#include "globus_ftp_client.h"
#include "globus_ftp_client_plugin.h"

#ifndef EXTERN_C_BEGIN
#ifdef __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif
#endif

EXTERN_C_BEGIN

/** Module descriptor
 * @ingroup globus_ftp_client_restart_marker_plugin
 */
#define GLOBUS_FTP_CLIENT_RESTART_MARKER_PLUGIN_MODULE (&globus_i_ftp_client_restart_marker_plugin_module)

extern
globus_module_descriptor_t globus_i_ftp_client_restart_marker_plugin_module;

/**
 * Transfer begin callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback is called when a get, put, or third party transfer is
 * started.
 *
 * The intended use for this callback is for the user to use the transfer
 * urls to locate a restart marker in some persistant storage. If one is
 * found, it should be copied into 'user_saved_marker' and the callback
 * should return GLOBUS_TRUE. This will cause the transfer to be restarted
 * using that restart marker.  If one is not found, return GLOBUS_FALSE to
 * indicate that the transfer should proceed from the beginning.
 *
 * In any case, this is also an opportunity for the user to set up any
 * storage in anticipation of restart markers for this transfer.
 *
 * @param handle
 *        this the client handle that this transfer will be occurring on
 *
 * @param user_arg
 *        this is the user_arg passed to the init func
 *
 * @param source_url
 *        source of the transfer (GLOBUS_NULL if 'put')
 *
 * @param dest_url
 *        dest of the transfer (GLOBUS_NULL if 'get')
 *
 * @param user_saved_marker
 *        pointer to an unitialized restart marker
 *
 * @return
 *        - GLOBUS_TRUE to indicate that the plugin should use
 *          'user_saved_marker' to restart the transfer (and subsequently,
 *          destroy the marker)
 *        - GLOBUS_FALSE to indicate that 'user_saved_marker' has not been
 *          modified, and that the transfer should proceed normally
 */

typedef globus_bool_t
(*globus_ftp_client_restart_marker_plugin_begin_cb_t)(
    void *                                                  user_arg,
    globus_ftp_client_handle_t *                            handle,
    const char *                                            source_url,
    const char *                                            dest_url,
    globus_ftp_client_restart_marker_t *                    user_saved_marker);

/**
 * Restart marker received callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback will be called every time a restart marker is available.
 *
 * To receive restart markers in a 'put' or 'third_party_transfer', the
 * transfer must be in Extended Block mode. 'get' transfers will have their
 * markers generated internally.  Markers generated internally will be 'sent'
 * at most, once per second.
 *
 * The intended use for this callback is to allow the user to store this marker
 * (most likely in place of any previous marker) in a format that the
 * 'begin_cb' can parse and pass back.
 *
 * @param handle
 *        this the client handle that this transfer is occurring on
 *
 * @param user_arg
 *        this is the user_arg passed to the init func
 *
 * @param marker
 *        the restart marker that has been received.  Thsi marker is owned
 *        by the caller.  The user must use the copy method to keep it.
 *        Note: this restart marker currently contains all ranges received
 *          as of yet.  Should I instead only pass a marker with the
 *          ranges just made available? If so, the user may need a way to
 *          combine restart markers (globus_ftp_client_restart_marker_combine)
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_restart_marker_plugin_marker_cb_t)(
    void *                                                  user_arg,
    globus_ftp_client_handle_t *                            handle,
    globus_ftp_client_restart_marker_t *                    marker);

/**
 * Transfer complete callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback will be called upon transfer completion (successful or
 * otherwise)
 *
 * @param handle
 *        this the client handle that this transfer was occurring on
 *
 * @param user_arg
 *        this is the user_arg passed to the init func
 *
 * @param error
 *        the error object indicating what went wrong (GLOBUS_NULL on success)
 *
 * @param error_url
 *        the url which is the source of the above error (GLOBUS_NULL on success)
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_restart_marker_plugin_complete_cb_t)(
    void *                                                  user_arg,
    globus_ftp_client_handle_t *                            handle,
    globus_object_t *                                       error,
    const char *                                            error_url);

globus_result_t
globus_ftp_client_restart_marker_plugin_init(
    globus_ftp_client_plugin_t *                            plugin,
    globus_ftp_client_restart_marker_plugin_begin_cb_t      begin_cb,
    globus_ftp_client_restart_marker_plugin_marker_cb_t     marker_cb,
    globus_ftp_client_restart_marker_plugin_complete_cb_t   complete_cb,
    void *                                                  user_arg);

globus_result_t
globus_ftp_client_restart_marker_plugin_destroy(
    globus_ftp_client_plugin_t *                    plugin);


EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_RESTART_MARKER_PLUGIN_H */
