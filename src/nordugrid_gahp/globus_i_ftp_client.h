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
 * @file globus_i_ftp_client.h
 * Globus FTP Client Library
 *
 * $RCSfile: globus_i_ftp_client.h,v $
 * $Revision: 1.45 $
 * $Date: 2008/04/04 01:51:47 $
 */

#include "globus_common.h"
#include "globus_ftp_client.h"
#include "globus_ftp_client_plugin.h"
#include "globus_error_string.h"
#include "globus_xio.h"

#define SSH_EXEC_SCRIPT "gridftp-ssh"

#ifndef GLOBUS_L_INCLUDE_FTP_CLIENT_H
#define GLOBUS_L_INCLUDE_FTP_CLIENT_H

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


#ifdef DEBUG_LOCKS
#define globus_i_ftp_client_handle_lock(handle) \
	printf("locking handle %p at %s:%d\n", (handle), __FILE__, __LINE__), \
	globus_mutex_lock(&(handle)->mutex)
#define globus_i_ftp_client_handle_unlock(handle) \
	printf("unlocking handle %p at %s:%d\n", (handle), __FILE__, __LINE__), \
	globus_mutex_unlock(&(handle)->mutex)
#define globus_i_ftp_client_data_target_lock(data_target) \
	printf("locking data_target %p at %s:%d\n", (data_target), __FILE__, __LINE__), \
	globus_mutex_lock(&(data_target)->mutex)
#define globus_i_ftp_client_data_target_unlock(data_target) \
	printf("unlocking data_target %p at %s:%d\n", (data_target), __FILE__, __LINE__), \
	globus_mutex_unlock(&(data_target)->mutex)
#else
#define globus_i_ftp_client_handle_lock(handle) \
	globus_mutex_lock(&handle->mutex)
#define globus_i_ftp_client_handle_unlock(handle) \
	globus_mutex_unlock(&handle->mutex)
#define globus_i_ftp_client_data_target_lock(data_target) \
	globus_mutex_lock(&(data_target)->mutex)
#define globus_i_ftp_client_data_target_unlock(data_target) \
	globus_mutex_unlock(&(data_target)->mutex)
#endif

/**
 * Client handle magic number.
 *
 * Modify this if the handle data structure is changed.
 */
#define GLOBUS_FTP_CLIENT_MAGIC_STRING "FTPClient-1.0"

#ifdef BUILD_DEBUG
#define GLOBUS_I_FTP_CLIENT_BAD_MAGIC(h) \
    (!(h && (*h) && \
       !strcmp(((*h))->magic, \
	   GLOBUS_FTP_CLIENT_MAGIC_STRING)))
#else
#define GLOBUS_I_FTP_CLIENT_BAD_MAGIC(h) 0
#endif

extern int globus_i_ftp_client_debug_level;

#ifdef BUILD_DEBUG
#define globus_i_ftp_client_debug(Level) \
    (globus_i_ftp_client_debug_level >= (Level))

#define globus_i_ftp_client_debug_printf(level, message) \
do { \
    if (globus_i_ftp_client_debug(level)) \
    { \
	globus_libc_fprintf message; \
    } \
} while (0)

#define globus_i_ftp_client_debug_states(level, handle)                     \
    globus_i_ftp_client_debug_printf(level, (stderr,                        \
        "   handle state = %s\n"                                            \
        "   source state = %s\n"                                            \
        "   dest   state = %s\n",                                           \
        globus_i_ftp_handle_state_to_string(handle->state),                 \
        handle->source                                                      \
            ? globus_i_ftp_target_state_to_string(handle->source->state)    \
            : "NULL",                                                       \
        handle->dest                                                        \
            ? globus_i_ftp_target_state_to_string(handle->dest->state)      \
            : "NULL"))
            
#else
#define globus_i_ftp_client_debug_printf(level, message)
#define globus_i_ftp_client_debug_states(level, handle)
#endif

/* send NOOP if idle for more than 15 seconds */
#define GLOBUS_I_FTP_CLIENT_NOOP_IDLE 15 
/*
 * Attributes
 */

/**
 * The globus_i_ftp_client_operationattr_t is a pointer to this structure type.
 */
typedef struct globus_i_ftp_client_operationattr_t
{
    globus_ftp_control_parallelism_t            parallelism;
    globus_bool_t				force_striped;
    globus_ftp_control_layout_t                 layout;
    globus_ftp_control_tcpbuffer_t              buffer;
    globus_bool_t                               using_default_auth;
    globus_ftp_control_auth_info_t              auth_info;
    globus_ftp_control_type_t                   type;
    globus_ftp_control_mode_t                   mode;
    globus_bool_t                               list_uses_data_mode;
    globus_bool_t                               append;
    globus_ftp_control_dcau_t                   dcau;
    globus_ftp_control_protection_t             data_prot;
    globus_bool_t                               resume_third_party;
    globus_bool_t                               read_all;
    globus_ftp_client_data_callback_t           read_all_intermediate_callback;
    void *                                      read_all_intermediate_callback_arg;
    globus_bool_t                               allow_ipv6;
    globus_off_t                                allocated_size;
    globus_bool_t                               cwd_first;
    char *                                      authz_assert;
    globus_bool_t                               cache_authz_assert;
    globus_bool_t                               delayed_pasv;
    char *                                      net_stack_str;
    char *                                      disk_stack_str;
    char *                                      module_alg_str;
}
globus_i_ftp_client_operationattr_t;

/**
 * Byte range report.
 * @ingroup globus_ftp_client_operationattr
 *
 * This structure contains information about a single extent of data
 * stored on an FTP server. A report structure is generated from each
 * part of an extended-block mode restart marker message from an FTP
 * server.
 */
typedef struct
{
    /**
     *  Minimum value of this range.
     */
    globus_off_t				offset;

    /**
     * Maximum value of this range.
     */
    globus_off_t				end_offset;
}
globus_i_ftp_client_range_t;

/**
 * Handle attributes.
 * @ingroup globus_ftp_client_handleattr
 */
typedef struct globus_i_ftp_client_handleattr_t
{
    /**
     * Cache all connections.
     *
     * This attribute is used to cause the ftp client library to keep
     * all control (and where possible) data connections open between
     * ftp operations.
     */
    globus_bool_t                               cache_all;

    /**
     * parse all URLs for caching with RFC1738 compliant parser
     */

    globus_bool_t				rfc1738_url;

    /**
     * Use GRIDFTP2 if supported by the server
     */

    globus_bool_t				gridftp2;

    /** 
     * List of cached URLs.
     *
     * This list is used to manage the URL cache which is manipulated
     * by the user calling globus_ftp_client_handle_cache_url_state()
     * and globus_ftp_client_handle_flush_url_state().
     */
    globus_list_t *                             url_cache;
    /**
     * List of plugin structures.
     *
     * This list contains all plugins which can be associated
     * with an ftp client handle. These plugins will be notified when
     * operations are done using a handle associated with them.
     */
    globus_list_t *                             plugins;

    globus_size_t                               outstanding_commands;
    globus_ftp_client_pipeline_callback_t       pipeline_callback;
    void *                                      pipeline_arg;
    globus_bool_t                               pipeline_done;
    
    /*
     *  NETLOGGER
     */
    globus_bool_t                               nl_ftp;
    globus_bool_t                               nl_io;
    globus_netlogger_handle_t *                 nl_handle;
}
globus_i_ftp_client_handleattr_t;

/* Handles */
/**
 * Handle state machine.
 */
typedef enum
{
    GLOBUS_FTP_CLIENT_HANDLE_START,
    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_CONNECT,
    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_SETUP_CONNECTION,
    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_LIST,
    GLOBUS_FTP_CLIENT_HANDLE_SOURCE_RETR_OR_ERET,
    GLOBUS_FTP_CLIENT_HANDLE_DEST_CONNECT,
    GLOBUS_FTP_CLIENT_HANDLE_DEST_SETUP_CONNECTION,
    GLOBUS_FTP_CLIENT_HANDLE_DEST_STOR_OR_ESTO,
    GLOBUS_FTP_CLIENT_HANDLE_ABORT,
    GLOBUS_FTP_CLIENT_HANDLE_RESTART,
    GLOBUS_FTP_CLIENT_HANDLE_FAILURE,
    /* This is called when one side of a third-party transfer has sent
     * it's positive or negative response to the transfer, but the
     * other hasn't yet done the same.
     */
    GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER,
    GLOBUS_FTP_CLIENT_HANDLE_THIRD_PARTY_TRANSFER_ONE_COMPLETE,
    GLOBUS_FTP_CLIENT_HANDLE_FINALIZE
}
globus_ftp_client_handle_state_t;

/**
 * Supported operation types.
 */
typedef enum
{
    GLOBUS_FTP_CLIENT_IDLE,
    GLOBUS_FTP_CLIENT_CHMOD,
    GLOBUS_FTP_CLIENT_DELETE,
    GLOBUS_FTP_CLIENT_MKDIR,
    GLOBUS_FTP_CLIENT_RMDIR,
    GLOBUS_FTP_CLIENT_MOVE,
    GLOBUS_FTP_CLIENT_LIST,
    GLOBUS_FTP_CLIENT_NLST,
    GLOBUS_FTP_CLIENT_MLSD,
    GLOBUS_FTP_CLIENT_MLST,
    GLOBUS_FTP_CLIENT_STAT,
    GLOBUS_FTP_CLIENT_GET,
    GLOBUS_FTP_CLIENT_PUT,
    GLOBUS_FTP_CLIENT_TRANSFER,
    GLOBUS_FTP_CLIENT_MDTM,
    GLOBUS_FTP_CLIENT_SIZE,
    GLOBUS_FTP_CLIENT_CKSM,
    GLOBUS_FTP_CLIENT_FEAT,
    GLOBUS_FTP_CLIENT_CWD
}
globus_i_ftp_client_operation_t;

typedef enum
{
    GLOBUS_FTP_CLIENT_TARGET_START, 
    GLOBUS_FTP_CLIENT_TARGET_CONNECT,
    GLOBUS_FTP_CLIENT_TARGET_AUTHENTICATE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_FAULT,
    GLOBUS_FTP_CLIENT_TARGET_SITE_FAULT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_SITE_HELP,
    GLOBUS_FTP_CLIENT_TARGET_SITE_HELP,
    GLOBUS_FTP_CLIENT_TARGET_FEAT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_CONNECTION,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_TYPE,
    GLOBUS_FTP_CLIENT_TARGET_TYPE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_MODE,
    GLOBUS_FTP_CLIENT_TARGET_MODE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_SIZE,
    GLOBUS_FTP_CLIENT_TARGET_SIZE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_CKSM,
    GLOBUS_FTP_CLIENT_TARGET_CKSM,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_DCAU,
    GLOBUS_FTP_CLIENT_TARGET_DCAU,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_PBSZ,
    GLOBUS_FTP_CLIENT_TARGET_PBSZ,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_PROT,
    GLOBUS_FTP_CLIENT_TARGET_PROT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_BUFSIZE,
    GLOBUS_FTP_CLIENT_TARGET_BUFSIZE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_REMOTE_RETR_OPTS,
    GLOBUS_FTP_CLIENT_TARGET_REMOTE_RETR_OPTS,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_LOCAL_RETR_OPTS,
    GLOBUS_FTP_CLIENT_TARGET_LOCAL_RETR_OPTS,
    GLOBUS_FTP_CLIENT_TARGET_OPTS_PASV_DELAYED,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_PASV,
    GLOBUS_FTP_CLIENT_TARGET_PASV,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_PORT,
    GLOBUS_FTP_CLIENT_TARGET_PORT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_AUTHZ_ASSERT,
    GLOBUS_FTP_CLIENT_TARGET_AUTHZ_ASSERT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_SETNETSTACK,
    GLOBUS_FTP_CLIENT_TARGET_SETNETSTACK,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_SETDISKSTACK,
    GLOBUS_FTP_CLIENT_TARGET_SETDISKSTACK,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_ALLO,
    GLOBUS_FTP_CLIENT_TARGET_ALLO,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_STREAM,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_REST_EB,
    GLOBUS_FTP_CLIENT_TARGET_REST,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_OPERATION,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_LIST_CWD,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_GET,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_PUT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_SOURCE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_TRANSFER_DEST,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_CHMOD,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_DELETE,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_MKDIR,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_RMDIR,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_RNFR,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_RNTO,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_MDTM,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_MLST,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_STAT,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_GET,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_GETPUT_PUT,
    GLOBUS_FTP_CLIENT_TARGET_MLST,
    GLOBUS_FTP_CLIENT_TARGET_STAT,
    GLOBUS_FTP_CLIENT_TARGET_LIST,
    GLOBUS_FTP_CLIENT_TARGET_RETR,
    GLOBUS_FTP_CLIENT_TARGET_STOR,
    GLOBUS_FTP_CLIENT_TARGET_MDTM,
    GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_GET,
    GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_PUT,
    GLOBUS_FTP_CLIENT_TARGET_GETPUT_PASV_TRANSFER,
    GLOBUS_FTP_CLIENT_TARGET_READY_FOR_DATA,
    GLOBUS_FTP_CLIENT_TARGET_NEED_LAST_BLOCK,
    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_QUEUE,
    GLOBUS_FTP_CLIENT_TARGET_NEED_EMPTY_AND_COMPLETE,
    GLOBUS_FTP_CLIENT_TARGET_NEED_COMPLETE,
    GLOBUS_FTP_CLIENT_TARGET_COMPLETED_OPERATION,
    GLOBUS_FTP_CLIENT_TARGET_NOOP,
    GLOBUS_FTP_CLIENT_TARGET_FAULT,
    GLOBUS_FTP_CLIENT_TARGET_CLOSED,
    GLOBUS_FTP_CLIENT_TARGET_SETUP_CWD
}
globus_ftp_client_target_state_t;


typedef struct globus_i_ftp_client_url_ent_s
{
    globus_i_ftp_client_operation_t             op;
    char *                                      source_url;
    globus_url_t                                src_url;
    char *                                      dest_url;
    globus_url_t                                dst_url;
    
} globus_i_ftp_client_url_ent_t;


/**
 * FTP server features we are interested in. 
 *
 * Upon a new connection, we will attempt to probe via the SITE HELP
 * and FEAT commands which the server supports. If we can't determine
 * from that, we'll try using the command and find out if the server
 * supports it.
 *
 */



/* Do not access members of the feature 
   structures below. Instead, use interface functions:
   globus_i_ftp_client_feature_get()
   globus_i_ftp_client_feature_set()
   globus_i_ftp_client_features_init()
   globus_i_ftp_client_features_destroy()
   This will facilitate future changes in the structures.
*/
typedef struct globus_i_ftp_client_features_s
{
  globus_ftp_client_tristate_t list[GLOBUS_FTP_CLIENT_FEATURE_MAX];
} globus_i_ftp_client_features_t;


/* Should it be needed to store feature parameters 
   beside feature names, these are the proposed structures:

typedef struct globus_i_ftp_client_feature_s{
  globus_ftp_client_tristate_t                   supported;
  char **                                        attrs;
} globus_i_ftp_client_feature_t;

typedef struct globus_i_ftp_client_features_s
{
  globus_ftp_client_feature_t list[GLOBUS_FTP_CLIENT_FEATURE_MAX];
} globus_i_ftp_client_features_t;

*/

globus_i_ftp_client_features_t *
globus_i_ftp_client_features_init();

globus_result_t 
globus_i_ftp_client_features_destroy(
    globus_i_ftp_client_features_t *             features);
    
globus_ftp_client_tristate_t 
globus_i_ftp_client_feature_get(
    globus_i_ftp_client_features_t *             features,
    globus_ftp_client_probed_feature_t           feature);

void 
globus_i_ftp_client_feature_set(
    globus_i_ftp_client_features_t *             features,
    globus_ftp_client_probed_feature_t           feature,
    globus_ftp_client_tristate_t                 value);

void
globus_i_ftp_client_find_ssh_client_program();

/**
 * Data connection caching information.
 * @ingroup globus_ftp_client_handle
 * @internal
 *
 * This structure contains the information about which FTP client targets
 * and control handles (or the ftp client library itself) are connected
 * together in extended block mode. The
 * client library uses this information to decide whether to re-use data
 * channels associated for subsequent operations.
 *
 * @see globus_i_ftp_client_can_reuse_data_conn()
 */
typedef struct
{
    /**
     * A pointer to the source of whatever FTP operation this
     * data channel was last used on.
     */
    struct globus_i_ftp_client_target_s *	source;
    /**
     * A pointer to the source of whatever FTP operation this
     * data channel was last used on.
     */
    struct globus_i_ftp_client_target_s *	dest;
    /**
     * The type of operation which this data channel was used for.
     */
    globus_i_ftp_client_operation_t		operation;
}
globus_i_ftp_client_data_target_t;

/**
 * FTP Client handle implementation
 * @ingroup globus_ftp_client_handle
 * @internal
 *
 * This structure contains all of the state associated with an FTP
 * handle. This state includes operations in progress, cached connections,
 * plugins, user callbacks, and other state needed to implement the GridFTP
 * protocol.
 *
 * @see globus_ftp_client_handle_init(), globus_ftp_client_handle_destroy()
 */
typedef struct globus_i_ftp_client_handle_t
{
    /** client handle magic number */
    char                                        magic[24];

    /** The user's handle pointer used to initialize this structure */
    globus_ftp_client_handle_t *		handle;

    /**
     * Information about the connection to the source URL for a get
     * or third-party transfer.
     */ 
    struct globus_i_ftp_client_target_s *       source;

    /** source URL */
    char *                                      source_url;

    /**
     * Information about the connection to the destination URL for a put
     * or third-party transfer.
     */ 
    struct globus_i_ftp_client_target_s *       dest;
    /** destination URL */
    char *                                      dest_url;
    globus_i_ftp_client_handleattr_t            attr;

    /** Current operation on this handle */
    globus_i_ftp_client_operation_t             op;

    /** Callback to be called once this operation is completed. */
    globus_ftp_client_complete_callback_t       callback;
    /** User-supplied parameter to this callback */
    void *                                      callback_arg;

    /** Current state of the operation we are processing */
    globus_ftp_client_handle_state_t            state;

    /** 
     * Priority queue of data blocks which haven't yet been sent 
     * to the FTP control library
     */
    globus_priority_q_t                         stalled_blocks;
    
    /**
     * Hash of data blocks which are currently being processed by the
     * control handle.
     */
    globus_hashtable_t                          active_blocks;

    /**
     * Number of blocks in the active_blocks hash.
     */
    int                                         num_active_blocks;

    /**
     * Address of PASV side of a transfer.
     */
    globus_ftp_control_host_port_t *            pasv_address;

    /**
     * Number of passive addresses we know about.
     */
    int                                         num_pasv_addresses;

    /** Error object to pass to the completion callback */
    globus_object_t *                           err;

    /**
     * Restart information.
     */
    struct globus_i_ftp_client_restart_s *      restart_info;

    /**
     * Delayed notification information.
     */
    int                                         notify_in_progress;
    globus_bool_t                               notify_abort;
    globus_bool_t                               notify_restart;

    /** Size of the file to be downloaded, if known. */
    globus_off_t                                source_size;

    /** Current information about what has been transferred so far. */
    globus_ftp_client_restart_marker_t          restart_marker;

    /** Partial file transfer starting offset. */
    globus_off_t                                partial_offset;

    /** Partial file transfer ending offset. */
    globus_off_t                                partial_end_offset;

    /**
     * Base offset for a transfer, to be added to all offsets in
     * stream mode
     */
    globus_off_t                                base_offset;
    /** Offset used to determine what length to return in a read callback
     * in when the read_all attribute is set.
     */
    globus_off_t                                read_all_biggest_offset;

    /** Pointer to user's modification time buffer */
    globus_abstime_t *				modification_time_pointer;

    /** Pointer to user's size buffer */
    globus_off_t *				size_pointer;
    
    /** Pointer to user's features buffer */
    globus_i_ftp_client_features_t *		features_pointer;

    /** Pointer to user's MLST/STAT string buffer */
    globus_byte_t **		                mlst_buffer_pointer;
    globus_size_t *                             mlst_buffer_length_pointer;

    /** file mode for CHMOD **/
    int                                         chmod_file_mode;
    
    /** Thread safety */
    globus_mutex_t                              mutex;

    /** cksm pointer**/
    char * 					checksum;
    
    /** checksum parameters **/
    globus_off_t                                checksum_offset;
    globus_off_t                                checksum_length;
    char *                                      checksum_alg;
    
    /** piplining operation queue */
    globus_fifo_t                               src_op_queue;
    globus_fifo_t                               dst_op_queue;
    globus_fifo_t                               src_response_pending_queue;
    globus_fifo_t                               dst_response_pending_queue;

    int                                         no_callback_count;
    /** User pointer
     * @see globus_ftp_client_handle_set_user_pointer(),
     *      globus_ftp_client_handle_get_user_pointer()
     */
    void *                                      user_pointer;
}
globus_i_ftp_client_handle_t;

/** 
 * FTP Connection State.
 *
 * This type is used to store information about an active FTP control
 * connection. This information includes the FTP control handle, the
 * extensions which the server supports, and the current session
 * settings which have been set on the control handle.
 */
typedef struct globus_i_ftp_client_target_s
{
    /** Current connection/activity state of this target */
    globus_ftp_client_target_state_t		state;

    /** Handle to an FTP control connection. */
    globus_ftp_control_handle_t *		control_handle;
    /** URL we are currently processing. */
    char *					url_string;
    /** Host/port we are connected to. */
    globus_url_t				url;
    /** Information about server authentication. */
    globus_ftp_control_auth_info_t		auth_info;

    /** Features we've discovered about this target so far. */
    globus_i_ftp_client_features_t *       	features;
    /** Current settings */
    globus_ftp_control_dcau_t			dcau;
    globus_ftp_control_protection_t		data_prot;
    unsigned long                               pbsz;
    globus_ftp_control_type_t			type;
    globus_ftp_control_tcpbuffer_t		tcp_buffer;
    globus_ftp_control_mode_t			mode;
    globus_ftp_control_structure_t		structure;
    globus_ftp_control_layout_t			layout;
    globus_ftp_control_parallelism_t		parallelism;
    char *                                      authz_assert;
    char *                                      net_stack_str;
    char *                                      disk_stack_str;
    globus_bool_t                               delayed_pasv;
    
    /** Requested settings */
    globus_i_ftp_client_operationattr_t *	attr;

    /** The client that this target is associated with */
    globus_i_ftp_client_handle_t *		owner;

    /** Data connection caching information */
    globus_i_ftp_client_data_target_t		cached_data_conn;
    
    /** Plugin mask associated with the currently pending command. */
    globus_ftp_client_plugin_command_mask_t	mask;
    
    /* used for determining need for NOOP */
    globus_abstime_t                            last_access;
    
    globus_bool_t                               src_command_sent;
    globus_bool_t                               dst_command_sent;
    
    globus_list_t *                             net_stack_list;
} globus_i_ftp_client_target_t;

/**
 * URL caching support structure.
 *
 * This structure is used to implement the cache of URLs. When a
 * target is needed, the client library first checks the handle's
 * cache. If the target associated with the url is available, and it
 * matches the security attributes of the operation being performed,
 * it will be used for the operation.
 *
 * The current implementation only allows for a URL to be cached only
 * once per handle.
 *
 * The cache manipulations are done by the API functions
 * globus_ftp_client_cache_url_state() and
 * globus_ftp_client_flush_url_state(), and the internal functions
 * globus_i_ftp_client_target_find() and
 * globus_i_ftp_client_target_release().
 */
typedef struct
{ 
    /** URL which the user has requested to be cached. */
    globus_url_t				url;
    /** 
     * Target which matches that URL. If this is NULL, then the cache
     * entry is empty.
     */
    globus_i_ftp_client_target_t *		target;
}
globus_i_ftp_client_cache_entry_t;

/**
 * Restart information management.
 */
typedef struct globus_i_ftp_client_restart_s
{
    char *					source_url;
    globus_i_ftp_client_operationattr_t *	source_attr;
    char *					dest_url;
    globus_i_ftp_client_operationattr_t *	dest_attr;
    globus_ftp_client_restart_marker_t		marker;
    globus_abstime_t				when;
    globus_callback_handle_t                    callback_handle;
}
globus_i_ftp_client_restart_t;

/**
 * FTP Client Plugin.
 * @ingroup globus_ftp_client_plugins
 *
 * Each plugin implementation should define a method for initializing
 * one of these structures. Plugins may be implemented as either a
 * static function table, or a specialized plugin with plugin-specific 
 * attributes.
 *
 * Each plugin function may be either GLOBUS_NULL, or a valid function 
 * pointer. If the function is GLOBUS_NULL, then the plugin will not
 * be notified when the corresponding event happens.
 */
typedef struct globus_i_ftp_client_plugin_t
{
    /** 
     * Plugin name.
     *
     * The plugin name is used by the FTP Client library to detect
     * multiple instances of the same plugin being associated with
     * a #globus_ftp_client_handleattr_t or #globus_ftp_client_handle_t.
     * 
     * Each plugin type should have a unique plugin name, which must
     * be a NULL-terminated string of arbitrary length.
     */
    char *					plugin_name;

    /**
     * The value the user/plugin implementation passed into the plugin
     * handling parts of the API.
     */
    globus_ftp_client_plugin_t *		plugin;

    /**
     * Plugin function pointers.
     */
    globus_ftp_client_plugin_copy_t		copy_func;
    /**
     * Plugin function pointers.
     */
    globus_ftp_client_plugin_destroy_t		destroy_func; 

    globus_ftp_client_plugin_chmod_t	        chmod_func;
    globus_ftp_client_plugin_delete_t		delete_func;
    globus_ftp_client_plugin_mkdir_t		mkdir_func;
    globus_ftp_client_plugin_rmdir_t		rmdir_func;
    globus_ftp_client_plugin_move_t		move_func;
    globus_ftp_client_plugin_feat_t		feat_func;
    globus_ftp_client_plugin_verbose_list_t     verbose_list_func;
    globus_ftp_client_plugin_machine_list_t     machine_list_func;
    globus_ftp_client_plugin_list_t		list_func;
    globus_ftp_client_plugin_mlst_t		mlst_func;
    globus_ftp_client_plugin_stat_t		stat_func;
    globus_ftp_client_plugin_get_t		get_func;
    globus_ftp_client_plugin_put_t		put_func;
    globus_ftp_client_plugin_third_party_transfer_t
						third_party_transfer_func;

    globus_ftp_client_plugin_modification_time_t
						modification_time_func;
    globus_ftp_client_plugin_size_t		size_func;
    globus_ftp_client_plugin_cksm_t		cksm_func;
    globus_ftp_client_plugin_abort_t		abort_func;
    globus_ftp_client_plugin_connect_t		connect_func;
    globus_ftp_client_plugin_authenticate_t	authenticate_func;
    globus_ftp_client_plugin_read_t		read_func;
    globus_ftp_client_plugin_write_t		write_func;
    globus_ftp_client_plugin_data_t		data_func;

    globus_ftp_client_plugin_command_t		command_func;
    globus_ftp_client_plugin_response_t		response_func;
    globus_ftp_client_plugin_fault_t		fault_func;
    globus_ftp_client_plugin_complete_t		complete_func;

    /** 
     * Command Mask
     *
     * The bits set in this mask determine which command responses the plugin
     * is interested in. The command_mask should be a bitwise-or of
     * the values in the globus_ftp_client_plugin_command_mask_t enumeration.
     */
    globus_ftp_client_plugin_command_mask_t	command_mask;

    /** This pointer is reserved for plugin-specific data */
    void *					plugin_specific;
} globus_i_ftp_client_plugin_t;


#ifndef DOXYGEN
/* globus_ftp_client_attr.c */
globus_result_t
globus_i_ftp_client_handleattr_copy(
    globus_i_ftp_client_handleattr_t *		dest,
    globus_i_ftp_client_handleattr_t *		src);

void
globus_i_ftp_client_handleattr_destroy(
    globus_i_ftp_client_handleattr_t *		i_attr);

int
globus_i_ftp_client_plugin_list_search(
    void *					datum,
    void *					arg);
/* globus_ftp_client.c */
void
globus_i_ftp_client_handle_is_active(globus_ftp_client_handle_t *handle);

void
globus_i_ftp_client_handle_is_not_active(globus_ftp_client_handle_t *handle);

void
globus_i_ftp_client_control_is_active(globus_ftp_control_handle_t * handle);

void
globus_i_ftp_client_control_is_not_active(globus_ftp_control_handle_t * handle);

const char *
globus_i_ftp_op_to_string(
    globus_i_ftp_client_operation_t		op);

const char *
globus_i_ftp_target_state_to_string(
    globus_ftp_client_target_state_t		target_state);

const char *
globus_i_ftp_handle_state_to_string(
    globus_ftp_client_handle_state_t		handle_state);

int
globus_i_ftp_client_count_digits(
    globus_off_t				num);

extern globus_ftp_control_auth_info_t globus_i_ftp_client_default_auth_info;
extern globus_reltime_t                 globus_i_ftp_client_noop_idle;

extern globus_xio_stack_t               ftp_client_i_popen_stack;
extern globus_xio_driver_t              ftp_client_i_popen_driver;
extern globus_bool_t                    ftp_client_i_popen_ready;


/* globus_ftp_client_handle.c */
globus_object_t *
globus_i_ftp_client_target_find(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr,
    globus_i_ftp_client_target_t **		target);

void
globus_i_ftp_client_target_release(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target);

void
globus_i_ftp_client_restart_info_delete(
    globus_i_ftp_client_restart_t *		restart_info);

globus_bool_t
globus_i_ftp_client_can_reuse_data_conn(
    globus_i_ftp_client_handle_t *		client_handle);

globus_result_t
globus_i_ftp_client_cache_add(
    globus_list_t **				cache,
    const char *				url,
    globus_bool_t				rfc1738_url);

globus_result_t
globus_i_ftp_client_cache_remove(
    globus_list_t **				cache,
    const char *				url,
    globus_bool_t                               rfc1738_url);

globus_result_t
globus_i_ftp_client_cache_destroy(
    globus_list_t **				cache);

/* globus_ftp_client_data.c  */
int
globus_i_ftp_client_data_cmp(
    void *					priority_1,
    void *					priority_2);

globus_object_t *
globus_i_ftp_client_data_dispatch_queue(
    globus_i_ftp_client_handle_t *		handle);

void
globus_i_ftp_client_data_flush(
    globus_i_ftp_client_handle_t *		handle);

/* globus_ftp_client_transfer.c */
void
globus_i_ftp_client_transfer_complete(
    globus_i_ftp_client_handle_t *		client_handle);

globus_object_t *
globus_i_ftp_client_restart(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_restart_t *		restart_info);

/* globus_ftp_client_plugin.c */

void
globus_i_ftp_client_plugin_notify_list(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_verbose_list(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_machine_list(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);
    
void
globus_i_ftp_client_plugin_notify_mlst(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_stat(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_chmod(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_cksm(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t                                offset,
    globus_off_t                                length,
    const char *                                algorithm,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_delete(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_feat(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_mkdir(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_rmdir(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_move(
    globus_i_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_get(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr,
    const globus_ftp_client_restart_marker_t *	restart);

void
globus_i_ftp_client_plugin_notify_put(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr,
    const globus_ftp_client_restart_marker_t *	restart);

void
globus_i_ftp_client_plugin_notify_transfer(
    globus_i_ftp_client_handle_t *		handle,
    const char *				source_url,
    globus_i_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    globus_i_ftp_client_operationattr_t *	dest_attr,
    const globus_ftp_client_restart_marker_t *	restart);

void
globus_i_ftp_client_plugin_notify_modification_time(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_size(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_i_ftp_client_operationattr_t *	attr);

void
globus_i_ftp_client_plugin_notify_restart(
    globus_i_ftp_client_handle_t *		handle);

void
globus_i_ftp_client_plugin_notify_connect(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url);

void
globus_i_ftp_client_plugin_notify_authenticate(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_control_auth_info_t *	auth_info);

void
globus_i_ftp_client_plugin_notify_abort(
    globus_i_ftp_client_handle_t *		handle);

void
globus_i_ftp_client_plugin_notify_read(
    globus_i_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length);

void
globus_i_ftp_client_plugin_notify_write(
    globus_i_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof);

void
globus_i_ftp_client_plugin_notify_data(
    globus_i_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    const globus_byte_t *			buffer,
    globus_size_t				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof);

void
globus_i_ftp_client_plugin_notify_command(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_plugin_command_mask_t	command_mask,
    const char *				command_spec,
    ...);

void
globus_i_ftp_client_plugin_notify_response(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_ftp_client_plugin_command_mask_t	command_mask,
    globus_object_t *				error,
    const globus_ftp_control_response_t *	ftp_response);

void
globus_i_ftp_client_plugin_notify_fault(
    globus_i_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error);

void
globus_i_ftp_client_plugin_notify_complete(
    globus_i_ftp_client_handle_t *		handle);

/* globus_ftp_client_restart.c */
globus_object_t *
globus_i_ftp_client_restart_register_oneshot(
    globus_i_ftp_client_handle_t *		handle);

/* globus_ftp_client_transfer.c */

globus_object_t *
globus_l_ftp_client_url_parse(
    const char *				url_string,
    globus_url_t *				url,
    globus_bool_t 				rfc1738_url);

void
globus_i_ftp_client_force_close_callback(
    void *					user_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_ftp_control_response_t *		response);

globus_object_t *
globus_i_ftp_client_target_activate(
    globus_i_ftp_client_handle_t *		handle,
    globus_i_ftp_client_target_t *		target,
    globus_bool_t *				registered);

/* globus_ftp_client_state.c */
void
globus_i_ftp_client_response_callback(
    void *					user_arg,
    globus_ftp_control_handle_t *		handle,
    globus_object_t *				error,
    globus_ftp_control_response_t *		response);

/* globus_ftp_client_data.c */
void
globus_l_ftp_client_complete_kickout(
    void *					user_arg);

globus_object_t *
globus_i_ftp_client_wrap_ftp_error(
    globus_module_descriptor_t *        base_source,
    int                                 code,
    const char *                        message,
    int                                 error_type,
    const char *                        source_file,
    const char *                        source_func,
    int                                 source_line,
    const char *                        format,
    ...);
/* Errors */

#define GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER(param) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_PARAMETER, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"a NULL value for %s was used", param)
#define GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER(param) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_PARAMETER, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"an invalid value for %s was used", param)
#define GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY() \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_MEMORY, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"a memory allocation failed")
#define GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED() \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_ABORTED, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"the operation was aborted")
#define GLOBUS_I_FTP_CLIENT_ERROR_INTERNAL_ERROR(err) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		err,\
		GLOBUS_FTP_CLIENT_ERROR_INTERNAL, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"an internal error occurred in globus_ftp_client")
#define GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE(obj) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_IN_USE, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"%s was already in use", obj)
#define GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_NOT_IN_USE(obj) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_NOT_IN_USE, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"%s was not in use", obj)
#define GLOBUS_I_FTP_CLIENT_ERROR_ALREADY_DONE() \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_COMPLETED, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"already done")
#define GLOBUS_I_FTP_CLIENT_ERROR_INVALID_OPERATION(op) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_INVALID_OPERATION, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"%s not allowed here",\
		globus_i_ftp_op_to_string(op))
#define GLOBUS_I_FTP_CLIENT_ERROR_EOF() \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_UNEXPECTED_EOF, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"EOF has been reached")
#define GLOBUS_I_FTP_CLIENT_ERROR_NO_SUCH_FILE(file) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_NO_SUCH_FILE, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"%s does not exist", file)
#define GLOBUS_I_FTP_CLIENT_ERROR_PROTOCOL_ERROR() \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_PROTOCOL, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"a protocol error occurred")
#define GLOBUS_I_FTP_CLIENT_ERROR_RESPONSE(response) \
        globus_i_ftp_client_wrap_ftp_error( \
            GLOBUS_FTP_CLIENT_MODULE, \
            (response)->code, \
            (char *) (response)->response_buffer, \
            GLOBUS_FTP_CLIENT_ERROR_RESPONSE, \
            __FILE__, \
            _globus_func_name, \
            __LINE__, \
            "the server responded with an error")
#define GLOBUS_I_FTP_CLIENT_ERROR_UNSUPPORTED_FEATURE(feature) \
	globus_error_construct_error(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		GLOBUS_FTP_CLIENT_ERROR_FEATURE, \
		__FILE__, \
		_globus_func_name, \
		__LINE__, \
		"the server doesn't support the %s feature",\
		feature)
#define GLOBUS_I_FTP_CLIENT_NO_RESTART_MARKER() \
        globus_error_construct_error( \
            GLOBUS_FTP_CLIENT_MODULE, \
            GLOBUS_NULL, \
            GLOBUS_FTP_CLIENT_ERROR_NO_RESTART_MARKER, \
            __FILE__, \
            _globus_func_name, \
            __LINE__, \
            "Could not find restart info\n")

#endif

EXTERN_C_END

#endif /* GLOBUS_L_INCLUDE_FTP_CLIENT_H */

#endif /* GLOBUS_DONT_DOCUMENT_INTERNAL */
