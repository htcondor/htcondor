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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_RESTART_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_RESTART_PLUGIN_H
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file
 */
#endif

/**
 * @defgroup globus_ftp_client_restart_plugin Restart Plugin
 * @ingroup globus_ftp_client_plugins
 *
 * The restart plugin implements one scheme for providing reliability
 * functionality for the FTP Client library. Other plugins may be developed
 * to provide other methods of reliability.
 *
 * The specific functionality of this plugin is
 * to restart any FTP operation when a fault occurs. The plugin's operation
 * is parameterized to control how often and when to attempt to restart
 * the operation.
 *
 * This restart plugin will restart an FTP operation if a noticable
 * fault has occurred---a connection timing out, a failure by the server
 * to process a command, a protocol error, an authentication error.
 *
 * This plugin has three user-configurable parameters; these are
 * the maximum number of retries to attempt, the interval to wait between
 * retries, and the deadline after which no further retries will be attempted.
 * These are set by initializing a restart plugin instance with the function
 * globus_ftp_client_restart_plugin_init().
 *
 * <h2>Example Usage</h2>
 *
 * The following example illustrates a typical use of the restart plugin.
 * In this case, we configure a plugin instance to restart the operation
 * for up to an hour, using an exponential back-off between retries.
 *
 * \include globus_ftp_client_restart_plugin.example
 */

#include "globus_ftp_client.h"

EXTERN_C_BEGIN

/** Module descriptor
 * @ingroup globus_ftp_client_restart_plugin
 */
#define GLOBUS_FTP_CLIENT_RESTART_PLUGIN_MODULE \
        (&globus_i_ftp_client_restart_plugin_module)
extern globus_module_descriptor_t globus_i_ftp_client_restart_plugin_module;

globus_result_t
globus_ftp_client_restart_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    int						max_retries,
    globus_reltime_t *				interval,
    globus_abstime_t *				deadline);

globus_result_t
globus_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin);

globus_result_t
globus_ftp_client_restart_plugin_set_stall_timeout(
    globus_ftp_client_plugin_t *        plugin,
    int                                 to_secs);

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_RESTART_PLUGIN_H */
