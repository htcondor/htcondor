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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_THROUGHPUT_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_THROUGHPUT_PLUGIN_H
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file globus_ftp_client_throughput_plugin.h GridFTP Throughput Performance Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_throughput_plugin.h,v $
 * $Revision: 1.7 $
 * $Date: 2006/01/19 05:54:53 $
 * $Author: mlink $
 */
#endif

/**
 * @defgroup globus_ftp_client_throughput_plugin Throughput Performance Plugin
 * @ingroup globus_ftp_client_plugins
 *
 * The FTP Throughput Performance plugin allows the user to obtain
 * calculated performance information for all types of transfers except a
 * third party transfer in which Extended Block mode is not enabled.
 *
 * Note: Since this plugin is built on top of the Performance Marker Plugin,
 * it is not possible to associate both plugins with a handle
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
 * @ingroup globus_ftp_client_throughput_plugin
 */
#define GLOBUS_FTP_CLIENT_THROUGHPUT_PLUGIN_MODULE (&globus_i_ftp_client_throughput_plugin_module)

extern
globus_module_descriptor_t globus_i_ftp_client_throughput_plugin_module;

/**
 * Transfer begin callback
 * @ingroup globus_ftp_client_throughput_plugin
 *
 * This callback will be called when a transfer begins
 *
 * @param handle
 *        The client handle associated with this transfer
 *
 * @param user_specific
 *        User argument passed to globus_ftp_client_throughput_plugin_init
 *
 * @param source_url
 *        source of the transfer (GLOBUS_NULL if 'put')
 *
 * @param dest_url
 *        dest of the transfer (GLOBUS_NULL if 'get')
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_throughput_plugin_begin_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    const char *                                    source_url,
    const char *                                    dest_url);

/**
 * Stripe performace throughput callback
 * @ingroup globus_ftp_client_throughput_plugin
 *
 * This callback will be called with every performance callback that is
 * received by the perf plugin. The first
 * callback for each stripe_ndx will have an instantaneous_throughput
 * based from the time the command was sent.
 *
 * @param handle
 *        The client handle associated with this transfer
 *
 * @param user_specific
 *        User argument passed to globus_ftp_client_throughput_plugin_init
 *
 * @param bytes
 *        The total number of bytes received on this stripe
 *
 * @param instantaneous_throughput
 *        Instanteous throughput on this stripe (bytes / sec)
 *
 * @param avg_throughput
 *        Average throughput on this stripe (bytes / sec)
 *
 * @param stripe_ndx
 *        This stripe's index
 *
 */

typedef void (*globus_ftp_client_throughput_plugin_stripe_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    int                                             stripe_ndx,
    globus_off_t                                    bytes,
    float                                           instantaneous_throughput,
    float                                           avg_throughput);

/**
 * Total performace throughput callback
 * @ingroup globus_ftp_client_throughput_plugin
 *
 * This callback will be called with every performance callback that is
 * received by the perf plugin. The first
 * callback for will have an instantaneous_throughput based from the time
 * the command was sent.  This callback will be called after the per_stripe_cb
 *
 * @param handle
 *        The client handle associated with this transfer
 *
 * @param user_specific
 *        User argument passed to globus_ftp_client_throughput_plugin_init
 *
 * @param bytes
 *        The total number of bytes received on all stripes
 *
 * @param instantaneous_throughput
 *        Total instanteous throughput on all stripes (bytes / sec)
 *
 * @param avg_throughput
 *        Average total throughput on all stripes (bytes / sec)
 *
 */

typedef void (*globus_ftp_client_throughput_plugin_total_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    globus_off_t                                    bytes,
    float                                           instantaneous_throughput,
    float                                           avg_throughput);

/**
 * Transfer complete callback
 * @ingroup globus_ftp_client_throughput_plugin
 *
 * This callback will be called upon transfer completion (successful or
 * otherwise)
 *
 * @param handle
 *        The client handle associated with this transfer
 *
 * @param user_specific
 *        User argument passed to globus_ftp_client_throughput_plugin_init
 *
 * @param success
 *        indicates whether this transfer completed successfully or was
 *        interrupted (by error or abort)
 *
 * @return
 *        - n/a
 */

typedef void (*globus_ftp_client_throughput_plugin_complete_cb_t)(
    void *                                          user_specific,
    globus_ftp_client_handle_t *                    handle,
    globus_bool_t                                   success);


/**
 * Copy constructor
 * @ingroup globus_ftp_client_throughput_plugin
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

typedef void * (*globus_ftp_client_throughput_plugin_user_copy_cb_t)(
    void *                                          user_specific);

/**
 * Destructor
 * @ingroup globus_ftp_client_throughput_plugin
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

typedef void (*globus_ftp_client_throughput_plugin_user_destroy_cb_t)(
    void *                                          user_specific);

globus_result_t
globus_ftp_client_throughput_plugin_init(
    globus_ftp_client_plugin_t *                        plugin,
    globus_ftp_client_throughput_plugin_begin_cb_t      begin_cb,
    globus_ftp_client_throughput_plugin_stripe_cb_t     per_stripe_cb,
    globus_ftp_client_throughput_plugin_total_cb_t      total_cb,
    globus_ftp_client_throughput_plugin_complete_cb_t   complete_cb,
    void *                                              user_specific);

globus_result_t
globus_ftp_client_throughput_plugin_set_copy_destroy(
    globus_ftp_client_plugin_t *                          plugin,
    globus_ftp_client_throughput_plugin_user_copy_cb_t    copy_cb,
    globus_ftp_client_throughput_plugin_user_destroy_cb_t destroy_cb);

globus_result_t
globus_ftp_client_throughput_plugin_destroy(
    globus_ftp_client_plugin_t *                        plugin);

globus_result_t
globus_ftp_client_throughput_plugin_get_user_specific(
    globus_ftp_client_plugin_t *                        plugin,
    void **                                             user_specific);

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_THROUGHPUT_PLUGIN_H */
