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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_H
#define GLOBUS_INCLUDE_FTP_CLIENT_H

/**
 * @anchor globus_ftp_client_api
 * @mainpage Globus FTP Client API
 *
 * The Globus FTP Client library provides a convenient way of accessing
 * files on remote FTP servers. In addition to supporting the basic FTP
 * protocol, the FTP Client library supports several security and
 * performance extensions to make FTP more suitable for Grid applications.
 * These extensions are described in the Grid FTP Protocol document.
 *
 * In addition to protocol support for grid applications, the FTP Client
 * library provides a @link globus_ftp_client_plugins plugin architecture
 * @endlink for installing application or grid-specific fault recovery
 * and performance tuning algorithms within the library. Application
 * writers may then target their code toward the FTP Client library, and by
 * simply enabling the appropriate plugins, easily tune their application
 * to run it on a different grid.
 *
 * All applications which use the Globus FTP Client API must include the
 * header file "globus_ftp_client.h" and activate the
 * @link globus_ftp_client_activation GLOBUS_FTP_CLIENT_MODULE @endlink.
 *
 * To use the Globus FTP Client API, one must create an
 * @link globus_ftp_client_handle FTP Client handle @endlink. This
 * structure contains context information about FTP operations which are
 * being executed, a cache of FTP control and data connections, and
 * information about plugins which are being used. The specifics of the
 * connection caching and plugins are found in the "@ref
 * globus_ftp_client_handleattr" section of this manual.
 *
 * Once the handle is created, one may begin transferring files or doing
 * other FTP operations by calling the functions in the "@ref
 * globus_ftp_client_operations" section of this manual. In addition to
 * whole-file transfers, the API supports partial file transfers, restarting
 * transfers from a known point, and various FTP directory management
 * commands.  All FTP operations may have a set of attributes, defined
 * in the "@ref globus_ftp_client_operationattr" section, associated with
 * them to tune various FTP parameters. The data structures and functions
 * needed to restart a file transfer are described in the "@ref
 * globus_ftp_client_restart_marker" section of this manual. For operations
 * which require the user to send to or receive data from an FTP server
 * the must call the functions in the "@ref globus_ftp_client_data" section
 * of the manual.
 *
 * @htmlonly
 * <a href="main.html" target="_top">View documentation without frames</a><br>
 * <a href="index.html" target="_top">View documentation with frames</a><br>
 * @endhtmlonly
 */
#include "globus_ftp_control.h"
#include "globus_priority_q.h"

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
/**
 * @defgroup globus_ftp_client_activation Activation
 *
 * The Globus FTP Client library uses the standard module activation
 * and deactivation API to initialize its state. Before any FTP
 * functions are called, the module must be activated
 *
 * @code
 *    globus_module_activate(GLOBUS_FTP_CLIENT_MODULE);
 * @endcode
 *
 * This function returns GLOBUS_SUCCESS if the FTP library was
 * successfully initialized. This may be called multiple times.
 *
 * To deactivate the FTP library, the following must be called
 *
 * @code
 *    globus_module_deactivate(GLOBUS_FTP_CLIENT_MODULE);
 * @endcode
 */

/** Module descriptor
 * @ingroup globus_ftp_client_activation
 * @hideinitializer
 */
#define GLOBUS_FTP_CLIENT_MODULE (&globus_i_ftp_client_module)

extern globus_module_descriptor_t globus_i_ftp_client_module;

/**
 * Error types
 */
typedef enum
{
    GLOBUS_FTP_CLIENT_ERROR_PARAMETER,
    GLOBUS_FTP_CLIENT_ERROR_MEMORY,
    GLOBUS_FTP_CLIENT_ERROR_ABORTED,
    GLOBUS_FTP_CLIENT_ERROR_INTERNAL,
    GLOBUS_FTP_CLIENT_ERROR_IN_USE,
    GLOBUS_FTP_CLIENT_ERROR_NOT_IN_USE,
    GLOBUS_FTP_CLIENT_ERROR_COMPLETED,
    GLOBUS_FTP_CLIENT_ERROR_INVALID_OPERATION,
    GLOBUS_FTP_CLIENT_ERROR_UNEXPECTED_EOF,
    GLOBUS_FTP_CLIENT_ERROR_NO_SUCH_FILE,
    GLOBUS_FTP_CLIENT_ERROR_PROTOCOL,
    GLOBUS_FTP_CLIENT_ERROR_RESPONSE,
    GLOBUS_FTP_CLIENT_ERROR_FEATURE,
    GLOBUS_FTP_CLIENT_ERROR_NO_RESTART_MARKER
} globus_ftp_client_error_t;

/**
 * Restart marker types.
 * @ingroup globus_ftp_client_restart_markers
 * @internal
 *
 * This enum defines the different types of restart markers which may be used
 * with the client library.
 */
typedef enum
{
    GLOBUS_FTP_CLIENT_RESTART_NONE,
    GLOBUS_FTP_CLIENT_RESTART_STREAM,
    GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK
}
globus_ftp_client_restart_type_t;

/**
 * Stream mode restart marker
 * @ingroup globus_ftp_client_restart_markers
 * @internal
 *
 * This structure contains all restart marker information from a
 * stream mode transfer. Markers should be accessed and modified
 * using the functions described in the
 * @ref globus_ftp_client_restart_marker "restart marker"
 * section of this manual.
 */
typedef struct
{
    /**
     * @internal
     * Must be GLOBUS_FTP_CLIENT_STREAM
     */ 
    globus_ftp_client_restart_type_t		type;
    /**
     * @internal
     * The offset of a binary file transfer.
     */
    globus_off_t				offset;
    /**
     * @internal
     * The offset of an ASCII file transfer.
     */
    globus_off_t				ascii_offset;
}
globus_ftp_client_restart_stream_t;

/**
 * Extended block mode restart marker
 * @ingroup globus_ftp_client_restart_markers
 * @internal
 *
 * This structure contains all restart marker information from an
 * extended block mode transfer. Markers should be accessed and modified
 * using the functions described in the
 * @ref globus_ftp_client_restart_marker "restart marker"
 * section of this manual.
 */
typedef struct
{
    /**
     * @internal
     * Must be GLOBUS_FTP_CLIENT_RESTART_EXTENDED_BLOCK
     */ 
    globus_ftp_client_restart_type_t		type;
    /**
     * @internal
     * A fifo of globus_i_ftp_client_range_t byte range entries
     */
    globus_fifo_t				ranges;
}
globus_ftp_client_restart_extended_block_t;

/**
 * Restart marker.
 * @ingroup globus_ftp_client_restart_marker
 *
 * This structure is may be either a stream mode transfer offset,
 * or an extended block mode byte range.
 *
 * @see globus_ftp_client_restart_marker_init(),
 * globus_ftp_client_restart_marker_destroy(),
 * globus_ftp_client_restart_marker_copy(),
 * globus_ftp_client_restart_marker_insert_range(),
 * globus_ftp_client_restart_marker_set_offset()
 */
typedef union
{
    globus_ftp_client_restart_type_t		type;
    globus_ftp_client_restart_stream_t		stream;
    globus_ftp_client_restart_extended_block_t	extended_block;
}
globus_ftp_client_restart_marker_t;

/**
 * FTP Client Handle.
 * @ingroup globus_ftp_client_handle
 *
 * An FTP client handle is used to associate state with a group of
 * operations. Handles can have
 * @ref globus_ftp_client_handleattr_t "attributes"
 * associated with them. All FTP @link
 * globus_ftp_client_operations operations @endlink take a handle pointer
 * as a parameter.
 *
 * @see globus_ftp_client_handle_init(),
 * globus_ftp_client_handle_destroy(), globus_ftp_client_handleattr_t
 */
typedef struct globus_i_ftp_client_handle_t * globus_ftp_client_handle_t;

/**
 * FTP Client plugin
 * @ingroup globus_ftp_client_plugins
 *
 * An FTP Client plugin is used to add restart, monitoring,
 * and performance tuning operations to the FTP Client library, without
 * modifying the base API. Multiple plugins may be associated with a
 * globus_ftp_client_handle_t.
 *
 * @see globus_ftp_client_handle_init(),
 * globus_ftp_client_handle_destroy(), globus_ftp_client_handleattr_t,
 * @link globus_ftp_client_debug_plugin Debugging Plugin @endlink
 */
typedef struct globus_i_ftp_client_plugin_t * globus_ftp_client_plugin_t;

/**
 * Operation complete callback.
 * @ingroup globus_ftp_client_operations
 *
 * Every FTP Client operation (get, put, transfer, mkdir, etc) is
 * asynchronous. A callback of this type is passed to each of the
 * operation function calls to let the user know when the operation is
 * complete.  The completion callback is called only once per
 * operation, after all other callbacks for the operation have
 * returned.
 *
 * @param user_arg
 *        The user_arg parameter passed to the operation.
 * @param handle
 *        The handle on which the operation was done.
 * @param error
 *        A Globus error object indicating any problem which occurred,
 *        or GLOBUS_SUCCESS, if the operation completed successfully.
 */
typedef void (*globus_ftp_client_complete_callback_t) (
    void *					user_arg,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error);

/**
 * Data Callback.
 * @ingroup globus_ftp_client_data
 * Each read or write operation in the FTP Client library is
 * asynchronous. A callback of this type is passed to each of the data
 * operation function calls to let the user know when the data block
 * has been processed.
 *
 * @param user_arg
 *        The user_arg parameter passed to the read or write function.
 * @param handle
 *        The handle on which the data block operation was done.
 * @param error
 *        A Globus error object indicating any problem which occurred
 *        processing this data block, or or GLOBUS_SUCCESS if the operation
 *        completed successfully.
 * @param buffer
 *        The data buffer passed to the original read or write call.
 * @param length
 *        The amount of data in the data buffer. When reading data, this
 *        may be smaller than original buffer's length.
 * @param offset
 *        The offset into the file which this data block contains.
 * @param eof
 *        GLOBUS_TRUE if EOF has been reached on this data
 *        transfer, GLOBUS_FALSE otherwise. This may be set to
 *        GLOBUS_TRUE for multiple callbacks.
 */
typedef void (*globus_ftp_client_data_callback_t) (
    void *					user_arg,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    globus_byte_t *				buffer,
    globus_size_t				length,
    globus_off_t				offset,
    globus_bool_t				eof);

/**
 * Operation Attributes.
 * @ingroup globus_ftp_client_operationattr
 *
 * FTP Client attributes are used to control the parameters needed to
 * access an URL using the FTP protocol. Attributes are created and
 * manipulated using the functions in the
 * @link globus_ftp_client_operationattr attributes @endlink section
 * of the library.
 *
 * @see globus_ftp_client_operationattr_init(),
 * globus_ftp_client_operationattr_destroy()
 */
typedef struct globus_i_ftp_client_operationattr_t *
globus_ftp_client_operationattr_t;

/**
 * Handle Attributes.
 * @ingroup globus_ftp_client_handleattr
 *
 * Handle attributes are used to control the caching behavior of the
 * ftp client handle, and to implement the plugin features for
 * reliability and performance tuning.
 *
 * @see globus_ftp_client_handle_t, @ref globus_ftp_client_handleattr
 */
typedef struct
    globus_i_ftp_client_handleattr_t * globus_ftp_client_handleattr_t;
/**
 * @defgroup globus_ftp_client_restart_marker Restart Markers
 *
 * FTP Restart Markers
 *
 * The Globus FTP Client library provides the ability to start a
 * file transfer from a known location into the file. This is
 * accomplished by passing a restart marker to the
 * globus_ftp_client_get(), globus_ftp_client_put(), or
 * globus_ftp_client_third_party_transfer() functions.
 *
 */
#ifndef DOXYGEN
globus_result_t
globus_ftp_client_restart_marker_init(
    globus_ftp_client_restart_marker_t *	marker);

globus_result_t
globus_ftp_client_restart_marker_destroy(
    globus_ftp_client_restart_marker_t *	marker);

globus_result_t
globus_ftp_client_restart_marker_copy(
    globus_ftp_client_restart_marker_t *	new_marker,
    globus_ftp_client_restart_marker_t *	marker);

globus_result_t
globus_ftp_client_restart_marker_insert_range(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t				offset,
    globus_off_t				end_offset);

globus_result_t
globus_ftp_client_restart_marker_set_offset(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t				offset);

globus_result_t
globus_ftp_client_restart_marker_set_ascii_offset(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t				offset,
    globus_off_t				ascii_offset);

globus_result_t
globus_ftp_client_restart_marker_get_total(
    globus_ftp_client_restart_marker_t *	marker,
    globus_off_t *				total_bytes);

globus_result_t
globus_ftp_client_restart_marker_get_first_block(
    globus_ftp_client_restart_marker_t *        marker,
    globus_off_t *                              offset,
    globus_off_t *                              end_offset);

globus_result_t
globus_ftp_client_restart_marker_to_string(
    globus_ftp_client_restart_marker_t *	marker,
    char **					marker_string);

globus_result_t
globus_ftp_client_restart_marker_from_string(
    globus_ftp_client_restart_marker_t *	marker,
    const char *				marker_string);
#endif

/**
 * @defgroup globus_ftp_client_handle Handle Management
 *
 * Create/Destroy/Modify an FTP Client Handle.
 *
 * Within the Globus FTP Client Libary, all FTP operations require a
 * handle parameter. Currently, only one FTP operation may be in
 * progress at once per FTP handle. FTP connections may be cached
 * between FTP operations, for improved performance.
 *
 * This section defines operations to create and destroy FTP Client
 * handles, as well as to modify handles' connection caches.
 */
#ifndef DOXYGEN
globus_result_t
globus_ftp_client_handle_init(
    globus_ftp_client_handle_t *		handle,
    globus_ftp_client_handleattr_t*		attr);

globus_result_t
globus_ftp_client_handle_destroy(
    globus_ftp_client_handle_t *		handle);

globus_result_t
globus_ftp_client_handle_cache_url_state(
    globus_ftp_client_handle_t *		handle,
    const char *				url);

globus_result_t
globus_ftp_client_handle_flush_url_state(
    globus_ftp_client_handle_t *		handle,
    const char *				url);

globus_result_t
globus_ftp_client_handle_set_user_pointer(
    globus_ftp_client_handle_t *		handle,
    void *					user_pointer);

globus_result_t
globus_ftp_client_handle_get_user_pointer(
    const globus_ftp_client_handle_t *		handle,
    void **					user_pointer);

globus_result_t
globus_ftp_client_handle_get_restart_marker(
    const globus_ftp_client_handle_t *          handle,
    globus_ftp_client_restart_marker_t *        marker);

globus_result_t
globus_ftp_client_handle_add_plugin(
    globus_ftp_client_handle_t *		handle,
    globus_ftp_client_plugin_t *		plugin);

globus_result_t
globus_ftp_client_handle_remove_plugin(
    globus_ftp_client_handle_t *		handle,
    globus_ftp_client_plugin_t *		plugin);

#endif

/**
 * @defgroup globus_ftp_client_handleattr Handle Attributes
 *
 * Handle attributes are used to control additional features of the
 * FTP Client handle. These features are operation independent.
 *
 * The attribute which can currently set on a handle concern the
 * connection caching behavior of the handle, and the associations of
 * plugins with a handle.
 *
 * @see #globus_ftp_client_handle_t
 */
#ifndef DOXYGEN
globus_result_t
globus_ftp_client_handleattr_init(
    globus_ftp_client_handleattr_t *		attr);

globus_result_t
globus_ftp_client_handleattr_destroy(
    globus_ftp_client_handleattr_t *		attr);

globus_result_t
globus_ftp_client_handleattr_copy(
    globus_ftp_client_handleattr_t *		dest,
    globus_ftp_client_handleattr_t *		src);

globus_result_t
globus_ftp_client_handleattr_add_cached_url(
    globus_ftp_client_handleattr_t *		attr,
    const char *				url);

globus_result_t
globus_ftp_client_handleattr_remove_cached_url(
    globus_ftp_client_handleattr_t *		attr,
    const char *				url);

globus_result_t
globus_ftp_client_handleattr_set_cache_all(
    globus_ftp_client_handleattr_t *		attr,
    globus_bool_t				cache_all);

globus_result_t
globus_ftp_client_handleattr_get_cache_all(
    const globus_ftp_client_handleattr_t *	attr,
    globus_bool_t *				cache_all);

globus_result_t
globus_ftp_client_handleattr_set_rfc1738_url(
    globus_ftp_client_handleattr_t *		attr,
    globus_bool_t				rfc1738_url);

globus_result_t
globus_ftp_client_handleattr_get_rfc1738_url(
    const globus_ftp_client_handleattr_t *	attr,
    globus_bool_t *				rfc1738_url);

typedef void (*globus_ftp_client_pipeline_callback_t)(
    globus_ftp_client_handle_t *                handle,
    char **                                     source_url,
    char **                                     dest_url,
    void *                                      user_arg);

globus_result_t
globus_ftp_client_handleattr_set_pipeline(
    globus_ftp_client_handleattr_t *            attr,
    globus_size_t                               outstanding_commands,
    globus_ftp_client_pipeline_callback_t       pipeline_callback,
    void *                                      pipeline_arg);

globus_result_t
globus_ftp_client_handleattr_get_pipeline(
    const globus_ftp_client_handleattr_t *      attr,
    globus_size_t *                             outstanding_commands,
    globus_ftp_client_pipeline_callback_t *     pipeline_callback,
    void **                                     pipeline_arg);

globus_result_t
globus_ftp_client_handleattr_set_gridftp2(
    globus_ftp_client_handleattr_t *		attr,
    globus_bool_t				gridftp2);

globus_result_t
globus_ftp_client_handleattr_get_gridftp2(
    const globus_ftp_client_handleattr_t *	attr,
    globus_bool_t *				gridftp2);

globus_result_t
globus_ftp_client_handleattr_set_netlogger(
    globus_ftp_client_handleattr_t *		attr,
    globus_netlogger_handle_t *	                nl_handle);

globus_result_t
globus_ftp_client_handleattr_set_clientinfo(
    globus_ftp_client_handleattr_t *    attr,
    const char *                        app_name,
    const char *                        app_version,
    const char *                        other);

globus_result_t
globus_ftp_client_handleattr_get_clientinfo(
    globus_ftp_client_handleattr_t *    attr,
    char **                             app_name,
    char **                             app_version,
    char **                             other);

globus_result_t
globus_ftp_client_handleattr_set_netlogger_ftp_io(
    globus_ftp_client_handleattr_t *            attr,
    globus_netlogger_handle_t *                 nl_handle,
    globus_bool_t                               ftp,
    globus_bool_t                               io);

globus_result_t
globus_ftp_client_handleattr_remove_plugin(
    globus_ftp_client_handleattr_t *		attr,
    globus_ftp_client_plugin_t *		plugin);

globus_result_t
globus_ftp_client_handleattr_add_plugin(
    globus_ftp_client_handleattr_t *		attr,
    globus_ftp_client_plugin_t *		plugin);
#endif

/**
 * @defgroup globus_ftp_client_operations FTP Operations
 *
 * Initiate an FTP operation.
 *
 * This module contains the API functions for a user to request a
 * get, put, third-party transfer, or other FTP file operation.
 */
#ifndef DOXYGEN

globus_result_t
globus_ftp_client_chmod(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    int         				mode,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_cksm(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    char *					cksm,
    globus_off_t                                offset,
    globus_off_t                                length,
    const char *				algorithm,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_delete(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_mkdir(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_rmdir(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_verbose_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_machine_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_mlst(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_byte_t **    			mlst_buffer,
    globus_size_t *                             mlst_buffer_length,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_stat(
    globus_ftp_client_handle_t *		u_handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_byte_t **    			stat_buffer,
    globus_size_t *                             stat_buffer_length,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_move(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_get(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_put(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_third_party_transfer(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    globus_ftp_client_operationattr_t *		source_attr,
    const char *				dest_url,
    globus_ftp_client_operationattr_t *		dest_attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_partial_get(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_partial_put(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_partial_third_party_transfer(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    globus_ftp_client_operationattr_t *		source_attr,
    const char *				dest_url,
    globus_ftp_client_operationattr_t *		dest_attr,
    globus_ftp_client_restart_marker_t *	restart,
    globus_off_t				partial_offset,
    globus_off_t				partial_end_offset,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_extended_get(
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                eret_alg_str,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);

globus_result_t
globus_ftp_client_extended_put(
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_ftp_client_operationattr_t *         attr,
    globus_ftp_client_restart_marker_t *        restart,
    const char *                                esto_alg_str,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);

globus_result_t
globus_ftp_client_extended_third_party_transfer(
    globus_ftp_client_handle_t *                handle,
    const char *                                source_url,
    globus_ftp_client_operationattr_t *         source_attr,
    const char *                                eret_alg_str,
    const char *                                dest_url,
    globus_ftp_client_operationattr_t *         dest_attr,
    const char *                                esto_alg_str,
    globus_ftp_client_restart_marker_t *        restart,
    globus_ftp_client_complete_callback_t       complete_callback,
    void *                                      callback_arg);

globus_result_t
globus_ftp_client_abort(
    globus_ftp_client_handle_t *		handle);

globus_result_t
globus_ftp_client_modification_time(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_abstime_t *				modification_time,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_size(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_off_t *				size,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_exists(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_client_complete_callback_t	complete_callback,
    void *					callback_arg);
#endif

/**
 * @defgroup globus_ftp_client_operationattr FTP Operation Attributes
 *
 * Operation attributes are used to control the security and
 * performance of an FTP operation. These features are often dependent
 * on the capabilities of the FTP server which you are going to access.
 */
#ifndef DOXYGEN
globus_result_t
globus_ftp_client_operationattr_init(
    globus_ftp_client_operationattr_t *		attr);

globus_result_t
globus_ftp_client_operationattr_destroy(
    globus_ftp_client_operationattr_t *		attr);

globus_result_t
globus_ftp_client_operationattr_set_parallelism(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_parallelism_t *	parallelism);

globus_result_t
globus_ftp_client_operationattr_get_parallelism(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_parallelism_t *		parallelism);

globus_result_t
globus_ftp_client_operationattr_set_storage_module(
    globus_ftp_client_operationattr_t *     attr,
    const char *                            module_name,
    const char *                            module_args);

globus_result_t
globus_ftp_client_operationattr_get_storage_module(
    const globus_ftp_client_operationattr_t *   attr,
    char **                                 module_name,
    char **                                 module_args);

globus_result_t
globus_ftp_client_operationattr_set_net_stack(
    globus_ftp_client_operationattr_t *         attr,
    const char *                                driver_list);

globus_result_t
globus_ftp_client_operationattr_get_net_stack(
    const globus_ftp_client_operationattr_t *   attr,
    char **                                     driver_list);

globus_result_t
globus_ftp_client_operationattr_set_disk_stack(
    globus_ftp_client_operationattr_t *         attr,
    const char *                                driver_list);

globus_result_t
globus_ftp_client_operationattr_get_disk_stack(
    const globus_ftp_client_operationattr_t *   attr,
    char **                                     driver_list);

globus_result_t
globus_ftp_client_operationattr_set_authz_assert(
    globus_ftp_client_operationattr_t *         attr,
    const char *                                authz_assert,
    globus_bool_t                               cache_authz_assert);

globus_result_t
globus_ftp_client_operationattr_get_authz_assert(
    const globus_ftp_client_operationattr_t *   attr,
    char **                                     authz_assert,
    globus_bool_t *                             cache_authz_assert);

globus_result_t
globus_ftp_client_operationattr_set_striped(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t 				striped);

globus_result_t
globus_ftp_client_operationattr_get_striped(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				striped);

globus_result_t
globus_ftp_client_operationattr_set_layout(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_layout_t *		layout);

globus_result_t
globus_ftp_client_operationattr_get_layout(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_layout_t *		layout);

globus_result_t
globus_ftp_client_operationattr_set_tcp_buffer(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_tcpbuffer_t *	tcp_buffer);

globus_result_t
globus_ftp_client_operationattr_get_tcp_buffer(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_tcpbuffer_t *		tcp_buffer);

globus_result_t
globus_ftp_client_operationattr_set_type(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_type_t			type);

globus_result_t
globus_ftp_client_operationattr_get_type(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_type_t *			type);

globus_result_t
globus_ftp_client_operationattr_set_mode(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_mode_t			mode);

globus_result_t
globus_ftp_client_operationattr_get_mode(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_mode_t *			mode);

globus_result_t
globus_ftp_client_operationattr_set_list_uses_data_mode(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t        			list_uses_data_mode);

globus_result_t
globus_ftp_client_operationattr_get_list_uses_data_mode(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *     			list_uses_data_mode);

globus_result_t
globus_ftp_client_operationattr_set_delayed_pasv(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t        			delayed_pasv);

globus_result_t
globus_ftp_client_operationattr_get_delayed_pasv(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *     			delayed_pasv);

globus_result_t
globus_ftp_client_operationattr_set_dcau(
    globus_ftp_client_operationattr_t *		attr,
    const globus_ftp_control_dcau_t *		dcau);

globus_result_t
globus_ftp_client_operationattr_get_dcau(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_dcau_t *			dcau);

globus_result_t
globus_ftp_client_operationattr_set_data_protection(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_protection_t		protection);

globus_result_t
globus_ftp_client_operationattr_get_data_protection(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_protection_t *		protection);

globus_result_t
globus_ftp_client_operationattr_set_control_protection(
    globus_ftp_client_operationattr_t *		attr,
    globus_ftp_control_protection_t		protection);

globus_result_t
globus_ftp_client_operationattr_get_control_protection(
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_control_protection_t *		protection);

globus_result_t
globus_ftp_client_operationattr_set_resume_third_party_transfer(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				resume);

globus_result_t
globus_ftp_client_operationattr_get_resume_third_party_transfer(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				resume);

globus_result_t
globus_ftp_client_operationattr_set_authorization(
    globus_ftp_client_operationattr_t *		attr,
    gss_cred_id_t				credential,
    const char *				user,
    const char *				password,
    const char *				account,
    const char *				subject);

globus_result_t
globus_ftp_client_operationattr_get_authorization(
    const globus_ftp_client_operationattr_t *	attr,
    gss_cred_id_t *				credential,
    char **					user,
    char **					password,
    char **					account,
    char **					subject);

globus_result_t
globus_ftp_client_operationattr_set_append(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				append);

globus_result_t
globus_ftp_client_operationattr_get_append(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				append);

globus_result_t
globus_ftp_client_operationattr_set_read_all(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				read_all,
    globus_ftp_client_data_callback_t		intermediate_callbacks,
    void *					intermediate_callback_arg);

globus_result_t
globus_ftp_client_operationattr_get_read_all(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				read_all,
    globus_ftp_client_data_callback_t *		intermediate_callbacks,
    void **					intermediate_callback_arg);

globus_result_t
globus_ftp_client_operationattr_set_allow_ipv6(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				allow);

globus_result_t
globus_ftp_client_operationattr_get_allow_ipv6(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				allow);

globus_result_t
globus_ftp_client_operationattr_set_allocate(
    globus_ftp_client_operationattr_t *		attr,
    const globus_off_t  	                allocated_size);
    
globus_result_t
globus_ftp_client_operationattr_get_allocate(
    const globus_ftp_client_operationattr_t *	attr,
    globus_off_t *		                allocated_size);

globus_result_t
globus_ftp_client_operationattr_copy(
    globus_ftp_client_operationattr_t *		dst,
    const globus_ftp_client_operationattr_t *	src);

globus_result_t
globus_ftp_client_operationattr_set_rfc1738_url(
    globus_ftp_client_operationattr_t *		attr,
    globus_bool_t				rfc1738_url);

globus_result_t
globus_ftp_client_operationattr_get_rfc1738_url(
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t *				rfc1738_url);
#endif

/**
 * @defgroup globus_ftp_client_data Reading and Writing Data
 *
 * Certain FTP client operations require the user to supply buffers
 * for reading or writing data to an FTP server. These operations are
 * globus_ftp_client_get(), globus_ftp_client_partial_get(),
 * globus_ftp_client_put(), globus_ftp_client_partial_put(),
 * globus_ftp_client_list(), globus_ftp_client_machine_list(), 
 * and globus_ftp_client_verbose_list().
 *
 * When doing these operations, the user must pass data buffers
 * to the FTP Client library. Data is read or written directly from
 * the data buffers, without any internal copies being done.
 *
 * The functions in this section of the manual may be called as soon
 * as the operation function has returned. Multiple data blocks may be
 * registered with the FTP Client Library at once, and may be sent
 * in parallel to or from the FTP server if the GridFTP protocol extensions
 * are being used.
 */
#ifndef DOXYGEN
globus_result_t
globus_ftp_client_register_read(
    globus_ftp_client_handle_t *		handle,
    globus_byte_t *				buffer,
    globus_size_t				buffer_length,
    globus_ftp_client_data_callback_t		callback,
    void *					callback_arg);

globus_result_t
globus_ftp_client_register_write(
    globus_ftp_client_handle_t *		handle,
    globus_byte_t *				buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof,
    globus_ftp_client_data_callback_t		callback,
    void *					callback_arg);
#endif

/**
 * Types for feature existence
 * @ingroup globus_ftp_client_operations
 * FALSE and TRUE are known to be fact that a feature does or does not exist
 * MAYBE means that the feature may exist
 */ 
typedef enum
{
    GLOBUS_FTP_CLIENT_FALSE = GLOBUS_FALSE,
    GLOBUS_FTP_CLIENT_TRUE  = GLOBUS_TRUE,
    GLOBUS_FTP_CLIENT_MAYBE
}
globus_ftp_client_tristate_t;

/**
 * Types of features
 * @ingroup globus_ftp_client_operations
 */ 
typedef enum
{
    /* Buffer-size setting commands; keep these at the beginning of
     * the enum
     */
    GLOBUS_FTP_CLIENT_FEATURE_RETRBUFSIZE = 0,
    GLOBUS_FTP_CLIENT_FEATURE_RBUFSZ,
    GLOBUS_FTP_CLIENT_FEATURE_RBUFSIZ,
    GLOBUS_FTP_CLIENT_FEATURE_STORBUFSIZE,
    GLOBUS_FTP_CLIENT_FEATURE_SBUSSZ,
    GLOBUS_FTP_CLIENT_FEATURE_SBUFSIZ,
    GLOBUS_FTP_CLIENT_FEATURE_BUFSIZE,
    GLOBUS_FTP_CLIENT_FEATURE_SBUF,
    GLOBUS_FTP_CLIENT_FEATURE_ABUF,
    
    GLOBUS_FTP_CLIENT_FEATURE_REST_STREAM,
    GLOBUS_FTP_CLIENT_FEATURE_PARALLELISM,
    GLOBUS_FTP_CLIENT_FEATURE_DCAU,
    GLOBUS_FTP_CLIENT_FEATURE_ESTO,
    GLOBUS_FTP_CLIENT_FEATURE_ERET,
    GLOBUS_FTP_CLIENT_FEATURE_SIZE,
    GLOBUS_FTP_CLIENT_FEATURE_CKSM,
    GLOBUS_FTP_CLIENT_FEATURE_CHMOD,
    GLOBUS_FTP_CLIENT_FEATURE_AUTHZ_ASSERT,
    GLOBUS_FTP_CLIENT_FEATURE_CLIENTINFO,
    GLOBUS_FTP_CLIENT_FEATURE_SITE_SETNETSTACK,
    GLOBUS_FTP_CLIENT_FEATURE_DELAYED_PASV,
    GLOBUS_FTP_CLIENT_FEATURE_GETPUT,
    GLOBUS_FTP_CLIENT_FEATURE_MLST,
    GLOBUS_FTP_CLIENT_FEATURE_MAX,
    GLOBUS_FTP_CLIENT_LAST_BUFFER_COMMAND = GLOBUS_FTP_CLIENT_FEATURE_ABUF,
    GLOBUS_FTP_CLIENT_FIRST_FEAT_FEATURE = GLOBUS_FTP_CLIENT_FEATURE_SBUF,
    GLOBUS_FTP_CLIENT_LAST_FEAT_FEATURE = GLOBUS_FTP_CLIENT_FEATURE_MLST
}
globus_ftp_client_probed_feature_t;
/**
 * Feature Handle
 * @ingroup globus_ftp_client_operations
 * Handle used to associate state with feature operations.
 * @see globus_ftp_client_feat,
 *      globus_ftp_client_features_init,
 *      globus_ftp_client_features_destroy
 */
typedef struct globus_i_ftp_client_features_s * globus_ftp_client_features_t;
#ifndef DOXYGEN

globus_result_t
globus_ftp_client_feat(
    globus_ftp_client_handle_t *                 u_handle, 
    char *                                       url,
    globus_ftp_client_operationattr_t *          attr,
    globus_ftp_client_features_t *               u_features,
    globus_ftp_client_complete_callback_t        complete_callback,
    void *                                       callback_arg);

globus_result_t
globus_ftp_client_is_feature_supported(
    const globus_ftp_client_features_t *           u_features,
    globus_ftp_client_tristate_t *                 answer,
    const globus_ftp_client_probed_feature_t       feature);

globus_result_t globus_ftp_client_features_init(
    globus_ftp_client_features_t *);
globus_result_t globus_ftp_client_features_destroy(
    globus_ftp_client_features_t *);

/**
 * returns the FTP response code for GLOBUS_ERROR_TYPE_FTP errors.
 * this function will search the entire chain for the correct type error and
 * return the code stored in that error.
 * 
 * @param error
 *      an error object or chain with type GLOBUS_ERROR_TYPE_FTP
 * 
 * @return
 *      the associated FTP response code if found, 0 otherwise.
 */
int
globus_error_ftp_error_get_code(
    globus_object_t *                   error);

#define GLOBUS_ERROR_TYPE_FTP (&GLOBUS_ERROR_TYPE_FTP_DEFINITION)
extern const globus_object_type_t GLOBUS_ERROR_TYPE_FTP_DEFINITION;
   
#endif
EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_H */
