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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_PERF_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_PERF_PLUGIN_H
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_ftp_client_perf_plugin.h GridFTP Performance Marker Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_perf_plugin.h,v $
 * $Revision: 1.7 $
 * $Date: 2006/01/19 05:54:53 $
 * $Author: mlink $
 */
#endif

/**
 * @defgroup globus_ftp_client_perf_plugin Performance Marker Plugin
 * @ingroup globus_ftp_client_plugins
 *
 * The FTP Performance Marker plugin allows the user to obtain
 * performance markers for all types of transfers except a
 * third party transfer in which Extended Block mode is not enabled.
 *
 * These markers may be generated internally, or they may be received
 * from a server ('put' or third_party_transfer' only).
 *
 * Copy constructor and destructor callbacks are also provided to allow
 * one to more easily layer other plugins on top of this one.
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
 * @ingroup globus_ftp_client_perf_plugin
 */
#define GLOBUS_FTP_CLIENT_PERF_PLUGIN_MODULE (&globus_i_ftp_client_perf_plugin_module)

extern
globus_module_descriptor_t globus_i_ftp_client_perf_plugin_module;

/**
 * Transfer begin callback
 * @ingroup globus_ftp_client_perf_plugin
 *
 * This callback is called when a get, put, or third party transfer is
 * started. Note that it is possible for this callback to be made multiple
 * times before ever receiving the complete callback... this would be the case
 * if a transfer was restarted.  The 'restart' will indicate whether or not we
 * have been restarted.
 *
 * @param handle
 *        this the client handle that this transfer will be occurring on
 *
 * @param user_specific
 *        this is user specific data either created by the copy method,
 *        or, if a copy method was not specified, the value passed to
 *        init
 *
 * @param source_url
 *        source of the transfer (GLOBUS_NULL if 'put')
 *
 * @param dest_url
 *        dest of the transfer (GLOBUS_NULL if 'get')
 *
 * @param restart
 *        boolean indicating whether this callback is result of a restart
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_begin_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    const char *                                    source_url,
    const char *                                    dest_url,
    globus_bool_t                                   restart);

/**
 * Performance marker received callback
 * @ingroup globus_ftp_client_perf_plugin
 *
 * This callback is called for all types of transfers except a third
 * party in which extended block mode is not used (because 112 perf markers
 * wont be sent in that case). For extended mode 'put' and '3pt', actual 112
 * perf markers will be used and the frequency of this callback is dependent
 * upon the frequency those messages are received. For 'put' in which
 * extended block mode is not enabled and 'get' transfers, the information in
 * this callback will be determined locally and the frequency of this callback
 * will be at a maximum of one per second.
 *
 * @param handle
 *        this the client handle that this transfer is occurring on
 *
 * @param user_specific
 *        this is user specific data either created by the copy method,
 *        or, if a copy method was not specified, the value passed to
 *        init
 *
 * @param time_stamp
 *        the timestamp at which the number of bytes is valid
 *
 * @param stripe_ndx
 *        the stripe index this data refers to
 *
 * @param num_stripes total number of stripes involved in this transfer
 *
 * @param nbytes
 *        the total bytes transfered on this stripe
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_marker_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    long                                            time_stamp_int,
    char                                            time_stamp_tength,
    int                                             stripe_ndx,
    int                                             num_stripes,
    globus_off_t                                    nbytes);

/**
 * Transfer complete callback
 * @ingroup globus_ftp_client_perf_plugin
 *
 * This callback will be called upon transfer completion (successful or
 * otherwise)
 *
 * @param handle
 *        this the client handle that this transfer was occurring on
 *
 * @param user_specific
 *        this is user specific data either created by the copy method,
 *        or, if a copy method was not specified, the value passed to
 *        init
 *
 * @param success
 *        indicates whether this transfer completed successfully or was
 *        interrupted (by error or abort)
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_complete_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    globus_bool_t                                   success);

/**
 * Copy constructor
 * @ingroup globus_ftp_client_perf_plugin
 *
 * This callback will be called when a copy of this plugin is made,
 * it is intended to allow initialization of a new user_specific data
 *
 * @param user_specific
 *        this is user specific data either created by this copy
 *        method, or the value passed to init
 *
 * @return
 *        - a pointer to a user specific piece of data
 *        - GLOBUS_NULL (does not indicate error)
 */

typedef void * (*globus_ftp_client_perf_plugin_user_copy_cb_t)(
    void *                                          user_specific);

/**
 * Destructor
 * @ingroup globus_ftp_client_perf_plugin
 *
 * This callback will be called when a copy of this plugin is destroyed,
 * it is intended to allow the user to free up any memory associated with
 * the user specific data
 *
 * @param user_specific
 *        this is user specific data created by the copy method
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_perf_plugin_user_destroy_cb_t)(
    void *                                          user_specific);

globus_result_t
globus_ftp_client_perf_plugin_init(
    globus_ftp_client_plugin_t *                    plugin,
    globus_ftp_client_perf_plugin_begin_cb_t        begin_cb,
    globus_ftp_client_perf_plugin_marker_cb_t       marker_cb,
    globus_ftp_client_perf_plugin_complete_cb_t     complete_cb,
    void *                                          user_specific);

globus_result_t
globus_ftp_client_perf_plugin_set_copy_destroy(
    globus_ftp_client_plugin_t *                    plugin,
    globus_ftp_client_perf_plugin_user_copy_cb_t    copy_cb,
    globus_ftp_client_perf_plugin_user_destroy_cb_t destroy_cb);

globus_result_t
globus_ftp_client_perf_plugin_destroy(
    globus_ftp_client_plugin_t *                    plugin);

globus_result_t
globus_ftp_client_perf_plugin_get_user_specific(
    globus_ftp_client_plugin_t *                    plugin,
    void **                                         user_specific);

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_PERF_PLUGIN_H */
