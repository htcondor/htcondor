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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_DEBUG_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_DEBUG_PLUGIN_H
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/**
 * @file
 */
#endif

/**
 * @defgroup globus_ftp_client_debug_plugin Debugging Plugin
 * @ingroup globus_ftp_client_plugins
 *
 * The FTP Debugging plugin provides a way for the user to trace FTP
 * protocol messages which occur while the GridFTP client library
 * processes an FTP operation. This may be useful for debugging FTP
 * configuration problems.
 *
 * When this plugin is used for a GridFTP Client operation, information
 * will be printed to the file stream associated with the plugin when a user
 * begins an operation, for all data buffers which pass through while
 * handling a data transfer, and for all protocol messages which are sent and
 * received.
 *
 * <b>Example Usage:</b>
 *
 * The following example illustrates a typical use of the debug plugin.
 * In this case, we configure a plugin instance to output log messages
 * preceded by the process name and pid to a file named gridftp.log.
 *
 * \include globus_ftp_client_debug_plugin.example

 *
 */

#include "globus_ftp_client_plugin.h"

EXTERN_C_BEGIN

/** Module descriptor
 * @ingroup globus_ftp_client_debug_plugin
 */
#define GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_MODULE \
        (&globus_i_ftp_client_debug_plugin_module)
extern globus_module_descriptor_t globus_i_ftp_client_debug_plugin_module;

globus_result_t
globus_ftp_client_debug_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    FILE *					stream,
    const char *				text);

globus_result_t
globus_ftp_client_debug_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin);

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_DEBUG_PLUGIN_H */
