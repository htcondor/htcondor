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

#ifndef GLOBUS_INCLUDE_FTP_CLIENT_PLUGIN_H
#define GLOBUS_INCLUDE_FTP_CLIENT_PLUGIN_H
#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/** @file
 */
#endif
/**
 * @defgroup globus_ftp_client_plugins Plugins
 *
 * Plugin API
 *
 * A plugin is a way to implement application-independent reliability
 * and performance tuning behavior. Plugins are written using the API
 * described in this document.
 *
 * A plugin is created by defining a globus_ftp_client_plugin_t which
 * contains the function pointers and plugin-specific data needed for
 * the plugin's operation. It is recommended that a plugin define a
 * a globus_module_descriptor_t and plugin initialization functions,
 * to ensure that the plugin is properly initialized.
 *
 * The functions pointed to in a plugin are called when significant
 * events in the life of an FTP Client operation occur. Note that
 * plugins will only be called when the plugin has the function
 * pointer for both the operation (get, put, list, etc), and the event
 * (connect, authenticate, command, etc), are defined. The command and
 * response functions are filtered based on the command_mask defined in
 * the plugin structure.
 *
 * Every plugin must define @link
 * #globus_ftp_client_plugin_copy_t copy @endlink and
 * @link #globus_ftp_client_plugin_destroy_t destroy @endlink functions. The
 * copy function is called when the plugin is added to an attribute set
 * or a handle is initialized with an attribute set containing the plugin.
 * The destroy function is called when the handle or attribute set is
 * destroyed.
 */

#include "globus_ftp_client.h"

EXTERN_C_BEGIN

/**
 * Command Mask.
 * @ingroup globus_ftp_client_plugins
 *
 * This enumeration includes the types of commands which the plugin
 * is interested in.
 */
typedef enum
{
    GLOBUS_FTP_CLIENT_CMD_MASK_NONE =    0,

    /** connect, authenticate */
    GLOBUS_FTP_CLIENT_CMD_MASK_CONTROL_ESTABLISHMENT	= 1<<0,

    /** PASV, PORT, SPOR, SPAS */
    GLOBUS_FTP_CLIENT_CMD_MASK_DATA_ESTABLISHMENT	= 1<<1,

    /** MODE, TYPE, STRU, OPTS RETR, DCAU */
    GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_PARAMETERS	= 1<<2,

    /** ALLO, REST */
    GLOBUS_FTP_CLIENT_CMD_MASK_TRANSFER_MODIFIERS	= 1<<3,

    /** STOR, RETR, ESTO, ERET, APPE, LIST, NLST, MLSD, GET, PUT */
    GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS		= 1<<4,

    /** HELP, SITE HELP, FEAT, STAT, SYST, SIZE */
    GLOBUS_FTP_CLIENT_CMD_MASK_INFORMATION		= 1<<5,

    /** SITE, NOOP */
    GLOBUS_FTP_CLIENT_CMD_MASK_MISC			= 1<<6,

    /** SBUF, ABUF */
    GLOBUS_FTP_CLIENT_CMD_MASK_BUFFER			= 1<<7,

    /** All possible commands */
    GLOBUS_FTP_CLIENT_CMD_MASK_ALL			= 0x7fffffff
}
globus_ftp_client_plugin_command_mask_t;


/**
 * Plugin copy function.
 * @ingroup globus_ftp_client_plugins
 *
 * This function is used to create a new copy or reference count a
 * plugin. This function is called by the FTP Client library when
 * a plugin is added to a handle attribute set, or when a handle
 * is initialized with an attribute which contains the plugin.
 *
 * A plugin may not call any of the plugin API functions from it's
 * instantiate method.
 *
 * @param plugin_template
 *        A plugin previously initialized by a call to the plugin-specific
 *        initialization function.
 *        by the user.
 * @param plugin_specific
 *        Plugin-specific data.
 *
 * @return A pointer to a plugin. This plugin copy must remain valid
 *         until the copy's
 *         @ref globus_ftp_client_plugin_destroy_t "destroy"
 *         function is called on the copy.
 *
 * @see #globus_ftp_client_plugin_destroy_t
 */
typedef globus_ftp_client_plugin_t * (*globus_ftp_client_plugin_copy_t)(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific);

/**
 * Plugin destroy function.
 * @ingroup globus_ftp_client_plugins
 *
 * This function is used to free or unreference a copy of a plugin which
 * was allocated by calling the instantiate function from the plugin.
 *
 * @param plugin
 *        The plugin, created by the create function, which is to be
 *        destroyed.
 * @param plugin_specific
 *        Plugin-specific data.
 */
typedef void (*globus_ftp_client_plugin_destroy_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific);

/**
 * Plugin connection begin function.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that connection
 * establishment is being done for this client handle.  This
 * notification can occur when a new request is made or when a restart
 * is done by a plugin.
 *
 * If a response_callback is defined by a plugin, then that will be
 * once the connection establishment has completed (successfully or
 * unsuccessfully).
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the connection.
 *
 * @note This function will not be called for a get, put, or
 * third-party transfer operation when a cached connection is used.
 */
typedef void (*globus_ftp_client_plugin_connect_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url);

/**
 * Plugin authentication notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that an authentication
 * handshake is being done for this client handle.  This
 * notification can occur when a new request is made or when a hard restart
 * is done by a plugin.
 *
 * If a response_callback is defined by a plugin, then that will be
 * once the authentication has completed (successfully or
 * unsuccessfully).
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the connection.
 * @param url
 *        The URL of the server to connect to.
 * @param auth_info
 *        Authentication and authorization info being used to
 *        authenticate with the FTP or GridFTP server.
 */
typedef void (*globus_ftp_client_plugin_authenticate_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_control_auth_info_t *	auth_info);

/**
 * Plugin chmod notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a chmod is being
 * requested  on a client handle. This notification happens both when
 * the user requests a chmod, and when a plugin restarts the currently
 * active chmod request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the chmod will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the delete operation.
 * @param url
 *        The url to chmod.
 * @param mode
 *        The file mode to be applied.
 * @param attr
 *        The attributes to be used during this operation.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current delete operation;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_chmod_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int         				mode,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin chmod notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a chmod is being
 * requested  on a client handle. This notification happens both when
 * the user requests a chmod, and when a plugin restarts the currently
 * active chmod request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the chmod will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the delete operation.
 * @param url
 *        The url to chmod.
 * @param offset
 *        File offset to start calculating checksum.    
 * @param length
 *        Length of data to read from the starting offset.  Use -1 to read the
 *        entire file.
 * @param algorithm
 *        A pointer to a string to be filled with the checksum of the
 *        file. On error the value pointed to by it is undefined.          
 * @param attr
 *        The attributes to be used during this operation.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current delete operation;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_cksm_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin delete notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a delete is being
 * requested  on a client handle. This notification happens both when
 * the user requests a delete, and when a plugin restarts the currently
 * active delete request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the delete will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the delete operation.
 * @param url
 *        The url to be deleted.
 * @param attr
 *        The attributes to be used during this operation.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current delete operation;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_delete_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin feat notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a feat is being
 * requested  on a client handle. This notification happens both when
 * the user requests a feat, and when a plugin restarts the currently
 * active feat request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the feat will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the feat operation.
 * @param url
 *        The url to be feat'd.
 * @param attr
 *        The attributes to be used during this operation.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current feat operation;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_feat_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin mkdir notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a mkdir is being
 * requested  on a client handle. This notification happens both when
 * the user requests a mkdir, and when a plugin restarts the currently
 * active mkdir request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the mkdir will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the mkdir operation.
 * @param url
 *        The url of the directory to create.
 * @param attr
 *        The attributes to be used during this operation.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current mkdir operation;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */

typedef void (*globus_ftp_client_plugin_mkdir_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin rmdir notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a rmdir is being
 * requested  on a client handle. This notification happens both when
 * the user requests a rmdir, and when a plugin restarts the currently
 * active rmdir request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the rmdir will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the rmdir operation.
 * @param url
 *        The url of the rmdir operation.
 * @param attr
 *        The attributes to be used during this operation.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current rmdir operation;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_rmdir_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);


/**
 * Plugin list notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a list is being
 * requested  on a client handle. This notification happens both when
 * the user requests a list, and when a plugin restarts the currently
 * active list request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the list will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_list_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin verbose list notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a list is being
 * requested  on a client handle. This notification happens both when
 * the user requests a list, and when a plugin restarts the currently
 * active list request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the list will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_verbose_list_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin machine list notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a list is being
 * requested  on a client handle. This notification happens both when
 * the user requests a list, and when a plugin restarts the currently
 * active list request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the list will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_machine_list_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin mlst notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a mlst is being
 * requested  on a client handle. This notification happens both when
 * the user requests a list, and when a plugin restarts the currently
 * active list request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the list will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_mlst_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);


/**
 * Plugin stat notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a stat is being
 * requested  on a client handle. This notification happens both when
 * the user requests a list, and when a plugin restarts the currently
 * active list request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the list will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_stat_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);


/**
 * Plugin move notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a move is being
 * requested  on a client handle. This notification happens both when
 * the user requests a move, and when a plugin restarts the currently
 * active move request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the move will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the move operation.
 * @param source_url
 *        The source url of the move operation.
 * @param dest_url
 *        The destination url of the move operation.
 * @param attr
 *        The attributes to be used during this move.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current move transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_move_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin get notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a get is being
 * requested  on a client handle. This notification happens both when
 * the user requests a get, and when a plugin restarts the currently
 * active get request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the get will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the get operation.
 * @param url
 *        The url of the get operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current get transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_get_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin put notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a put is being
 * requested  on a client handle. This notification happens both when
 * the user requests a put, and when a plugin restarts the currently
 * active put request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the put will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the put operation.
 * @param url
 *        The url of the put operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current put transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_put_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin third-party transfer notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a transfer is being
 * requested  on a client handle. This notification happens both when
 * the user requests a transfer, and when a plugin restarts the currently
 * active transfer request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the third-party transfer will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the transfer operation.
 * @param source_url
 *        The source url of the transfer operation.
 * @param source_attr
 *        The attributes to be used during this transfer on the source.
 * @param dest_url
 *        The destination url of the third-party transfer operation.
 * @param dest_attr
 *        The attributes to be used during this transfer on the destination.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current transfer transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_third_party_transfer_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t				restart);

/**
 * Plugin modification time notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a modification time check
 * is being requested  on a client handle. This notification happens both when
 * the user requests the modification time of a file,
 * and when a plugin restarts the currently active request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the modification time request will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_modification_time_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin size notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a size check
 * is being requested  on a client handle. This notification happens both when
 * the user requests the size of a file,
 * and when a plugin restarts the currently active request.
 *
 * If this function is not defined by the plugin, then no plugin
 * callbacks associated with the size request will be called.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the list operation.
 * @param url
 *        The url of the list operation.
 * @param attr
 *        The attributes to be used during this transfer.
 * @param restart
 *        This value is set to GLOBUS_TRUE when this callback is
 *        caused by a plugin restarting the current list transfer;
 *	  otherwise, this is set to GLOBUS_FALSE.
 */
typedef void (*globus_ftp_client_plugin_size_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t				restart);

/**
 * Plugin abort notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that an abort is being
 * requested on a client handle. This notification happens both when
 * the user aborts a request and when a plugin aborts the currently
 * active request.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 */
typedef void (*globus_ftp_client_plugin_abort_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle);

/**
 * Plugin read registration callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that the client API has
 * registered a buffer with the FTP control API for reading when
 * processing a get.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 * @param buffer
 *        The data buffer to read into.
 * @param buffer_length
 *        The maximum amount of data to read into the buffer.
 */
typedef void (*globus_ftp_client_plugin_read_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length);

/**
 * Plugin write registration callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that the client API has
 * registered a buffer with the FTP control API for writing when
 * processing a put.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 * @param buffer
 *        The buffer which is being written.
 * @param buffer_length
 *        The amount of data in the buffer.
 * @param offset
 *        The offset within the file where the buffer is to be written.
 * @param eof
 *        This value is set to GLOBUS_TRUE if this is the last data
 *        buffer to be sent for this put request.
 */
typedef void (*globus_ftp_client_plugin_write_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof);

/**
 * Plugin data callback handler.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a read or write
 * operation previously registered has completed. The buffer pointer
 * will match that of a previous plugin read or write registration
 * callback.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 * @param buffer
 *        The buffer which was successfully transferred over the network.
 * @param length
 *        The amount of data to read or written.
 * @param offset
 *        The offset into the file where this data buffer belongs.
 * @param eof
 *        This value is set to GLOBUS_TRUE if end-of-file is being processed
 *        for this transfer.
 */
typedef void (*globus_ftp_client_plugin_data_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    const globus_byte_t *			buffer,
    globus_size_t				length,
    globus_off_t				offset,
    globus_bool_t				eof);

/**
 * Command callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a FTP control
 * command is being sent.The client library will only call this
 * function for response callbacks associated with a command which is
 * in the plugin's command mask, and associated with one of the other
 * ftp operations with a defined callback in the plugin.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 * @param url
 *        The URL which this command is being sent to.
 * @param command
 *        A string containing the command which is being sent
 *        to the server (TYPE I, for example).
 */
typedef void (*globus_ftp_client_plugin_command_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const char *				command);

/**
 * Response callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a FTP control
 * response has occurred on a control connection. FTP response
 * callbacks will come back to the user in the order which the
 * commands were executed. The client library will only call this
 * function for response callbacks associated with a command which is
 * in the plugin's command mask, or associated with one of the other
 * ftp operations with a defined callback in the plugin.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 * @param url
 *        The URL which this response came from.
 * @param error
 *        An error which occurred while processing this
 *	  command/response pair.
 * @param ftp_response
 *        The response structure from the ftp control library.
 */
typedef void (*globus_ftp_client_plugin_response_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error,
    const globus_ftp_control_response_t *	ftp_response);

/**
 * Fault notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that a fault occurred
 * while processing the request. The fault may be internally
 * generated, or come from a call to another library.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the request.
 * @param url
 *        The url being processed when the fault ocurred.
 * @param error
 *        An error object describing the fault.
 */
typedef void (*globus_ftp_client_plugin_fault_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error);

/**
 * Completion notification callback.
 * @ingroup globus_ftp_client_plugins
 *
 * This callback is used to notify a plugin that an operation previously
 * begun has completed. The plugin may not call any other plugin operation
 * on this handle after this has occurred. This is the final callback for
 * the plugin while processing the operation.  The plugin may free any
 * internal state associated with the operation at this point.
 *
 * @param plugin
 *        The plugin which is being notified.
 * @param plugin_specific
 *        Plugin-specific data.
 * @param handle
 *        The handle associated with the operation.
 */
typedef void (*globus_ftp_client_plugin_complete_t)(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle);


/* Plugin Implementation API */
globus_result_t
globus_ftp_client_plugin_restart_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_verbose_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_machine_list(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_mlst(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_stat(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_delete(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_chmod(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_cksm(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_feat(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_mkdir(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_rmdir(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_move(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_get(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_put(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_third_party_transfer(
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_ftp_client_restart_marker_t *	restart_marker,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_size(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_modification_time(
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    const globus_abstime_t *            	when);

globus_result_t
globus_ftp_client_plugin_restart_get_marker(
    globus_ftp_client_handle_t *		handle,
    globus_ftp_client_restart_marker_t *	marker);

globus_result_t
globus_ftp_client_plugin_abort(
    globus_ftp_client_handle_t *		handle);

globus_result_t
globus_ftp_client_plugin_add_data_channels(
    globus_ftp_client_handle_t *		handle,
    unsigned int				num_channels,
    unsigned int				stripe);

globus_result_t
globus_ftp_client_plugin_remove_data_channels(
    globus_ftp_client_handle_t *		handle,
    unsigned int				num_channels,
    unsigned int				stripe);

globus_result_t
globus_ftp_client_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    const char *				plugin_name,
    globus_ftp_client_plugin_command_mask_t	command_mask,
    void *					plugin_specific);

globus_result_t
globus_ftp_client_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin);

globus_result_t
globus_ftp_client_plugin_get_plugin_specific(
    globus_ftp_client_plugin_t *		plugin,
    void **					plugin_specific);

globus_result_t
globus_ftp_client_plugin_set_copy_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_copy_t		copy);

globus_result_t
globus_ftp_client_plugin_set_destroy_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_destroy_t		destroy);

globus_result_t
globus_ftp_client_plugin_set_chmod_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_chmod_t		chmod_func);

globus_result_t
globus_ftp_client_plugin_set_cksm_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_cksm_t		cksm_func);

globus_result_t
globus_ftp_client_plugin_set_delete_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_delete_t		delete_func);

globus_result_t
globus_ftp_client_plugin_set_feat_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_feat_t		feat_func);

globus_result_t
globus_ftp_client_plugin_set_mkdir_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_mkdir_t		mkdir_func);

globus_result_t
globus_ftp_client_plugin_set_rmdir_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_rmdir_t		rmdir_func);

globus_result_t
globus_ftp_client_plugin_set_move_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_move_t		move_func);

globus_result_t
globus_ftp_client_plugin_set_verbose_list_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_verbose_list_t	verbose_list_func);

globus_result_t
globus_ftp_client_plugin_set_machine_list_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_machine_list_t	machine_list_func);

globus_result_t
globus_ftp_client_plugin_set_list_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_list_t		list_func);

globus_result_t
globus_ftp_client_plugin_set_mlst_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_mlst_t		mlst_func);

globus_result_t
globus_ftp_client_plugin_set_stat_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_stat_t		stat_func);

globus_result_t
globus_ftp_client_plugin_set_get_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_get_t		get_func);

globus_result_t
globus_ftp_client_plugin_set_put_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_put_t		put_func);

globus_result_t
globus_ftp_client_plugin_set_third_party_transfer_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_third_party_transfer_t
						third_party_transfer_func);

globus_result_t
globus_ftp_client_plugin_set_modification_time_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_modification_time_t
						modification_time_func);
globus_result_t
globus_ftp_client_plugin_set_size_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_size_t		size_func);

globus_result_t
globus_ftp_client_plugin_set_abort_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_abort_t		abort_func);

globus_result_t
globus_ftp_client_plugin_set_connect_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_connect_t		connect_func);

globus_result_t
globus_ftp_client_plugin_set_authenticate_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_authenticate_t	auth_func);

globus_result_t
globus_ftp_client_plugin_set_read_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_read_t		read_func);

globus_result_t
globus_ftp_client_plugin_set_write_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_write_t		write_func);

globus_result_t
globus_ftp_client_plugin_set_data_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_data_t		data_func);

globus_result_t
globus_ftp_client_plugin_set_command_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_command_t		command_func);

globus_result_t
globus_ftp_client_plugin_set_response_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_response_t		response_func);

globus_result_t
globus_ftp_client_plugin_set_fault_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_fault_t		fault_func);

globus_result_t
globus_ftp_client_plugin_set_complete_func(
    globus_ftp_client_plugin_t *		plugin,
    globus_ftp_client_plugin_complete_t		complete_func);

EXTERN_C_END

#endif /* GLOBUS_INCLUDE_FTP_CLIENT_PLUGIN_H */
