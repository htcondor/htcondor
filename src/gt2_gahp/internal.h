/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/* These are internal Globus structures included here for hack */

// This comes from xio/src/builtins/tcp/globus_xio_tcp_driver.c
typedef struct
{
    globus_xio_system_socket_handle_t   listener_system;
    globus_xio_system_socket_t          listener_fd;
    globus_bool_t                       converted;
} my_globus_l_server_t;

// This comes from xio/src/globus_xio_system_select.c
typedef struct globus_l_xio_system_s
{
    globus_xio_system_type_t            type;
    int                                 fd;
    globus_mutex_t                      lock; /* only used to protect below */
    globus_off_t                        file_position;
} globus_l_xio_system_t;

// This comes from xio/src/globus_i_xio.h
typedef struct globus_i_xio_monitor_s
{
    int                                 count;
} globus_i_xio_monitor_t;

// This comes from xio/src/globus_i_xio.h
typedef struct globus_i_xio_server_entry_s
{
    globus_xio_driver_t                 driver;
    void *                              server_handle;
} globus_i_xio_server_entry_t;

// This comes from xio/src/globus_i_xio.h
typedef enum globus_xio_server_state_e
{
    GLOBUS_XIO_SERVER_STATE_NONE,
    GLOBUS_XIO_SERVER_STATE_OPEN,
    GLOBUS_XIO_SERVER_STATE_ACCEPTING,
    GLOBUS_XIO_SERVER_STATE_COMPLETING,
    GLOBUS_XIO_SERVER_STATE_CLOSE_PENDING,
    GLOBUS_XIO_SERVER_STATE_CLOSING,
    GLOBUS_XIO_SERVER_STATE_CLOSED
} globus_xio_server_state_t;

// This comes from xio/src/globus_i_xio.h
typedef struct globus_i_xio_server_s
{
    globus_i_xio_monitor_t *            sd_monitor;
    
    globus_xio_server_state_t           state;

    globus_xio_timeout_server_callback_t accept_timeout;
    globus_reltime_t                    accept_timeout_period;
    struct globus_i_xio_op_s *          op;

    globus_xio_server_callback_t        cb;
    void *                              user_arg;

    int                                 outstanding_operations;

    int                                 ref;
    globus_mutex_t                      mutex;
    globus_callback_space_t             space;

    globus_bool_t                       blocking;
    char *                              contact_string;
    
    int                                 stack_size;
    globus_i_xio_server_entry_t         entry[1];
} globus_i_xio_server_t;

// This comes from io/compat/globus_io_xio_compat.c
typedef enum
{
    GLOBUS_I_IO_FILE_HANDLE = 1,
    GLOBUS_I_IO_TCP_HANDLE  = 2
} globus_l_io_handle_type_t;

// This comes from io/compat/globus_io_xio_compat.c
typedef struct globus_l_io_handle_s
{
    globus_l_io_handle_type_t                   type;
    int                                         refs;
    globus_io_handle_t *                        io_handle;
    globus_xio_handle_t                         xio_handle;
    globus_callback_space_t                     space;
    
    globus_list_t *                             pending_ops;
    globus_mutex_t                              pending_lock;
    void *                                      user_pointer;
    globus_io_attr_t                            attr;
    
    /* used only for listener */
    globus_xio_server_t                         xio_server;
    globus_xio_handle_t                         accepted_handle;
} globus_l_io_handle_t;

//typedef struct globus_l_io_handle_s *   globus_io_handle_t;

// This comes from gass/transfer/source/library/globus_gass_transfer_proto.h
typedef struct globus_gass_transfer_listener_proto_s
globus_gass_transfer_listener_proto_t;

typedef void
(* globus_gass_transfer_proto_listener_t)(
    globus_gass_transfer_listener_proto_t *     proto,
    globus_gass_transfer_listener_t             listener);

typedef void
(* globus_gass_transfer_proto_accept_t)(
    globus_gass_transfer_listener_proto_t *     proto,
    globus_gass_transfer_listener_t             listener,
    globus_gass_transfer_request_t              request,
    globus_gass_transfer_requestattr_t *        attr);

// This comes from gass/transfer/source/library/globus_l_gass_transfer_http.h
typedef enum
{
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_STARTING,
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_LISTENING,
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_READY,
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_ACCEPTING,
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_CLOSING1,
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_CLOSING2,
    GLOBUS_GASS_TRANSFER_HTTP_LISTENER_CLOSED
} globus_gass_transfer_listener_state_t;

typedef struct
{
    /* Standard "proto" elements */
    globus_gass_transfer_proto_listener_t       close_listener;
    globus_gass_transfer_proto_listener_t       listen;
    globus_gass_transfer_proto_accept_t         accept;
    globus_gass_transfer_proto_listener_t       destroy;

    /* Begin internal http-specific proto state */
    globus_gass_transfer_listener_t             listener;
    globus_io_handle_t                          handle;
    globus_url_scheme_t                         url_scheme;

    globus_gass_transfer_listener_state_t       state;
    globus_bool_t                               destroy_called;

    struct globus_gass_transfer_http_request_proto_s *  request;
} globus_gass_transfer_http_listener_proto_t;

// This comes from gass/transfer/source/library/globus_i_gass_transfer.h
typedef struct
{ 
    char *                                      base_url;
    globus_gass_transfer_listener_status_t      status;

    /* struct globus_gass_transfer_listener_proto_s *proto; */
    globus_gass_transfer_http_listener_proto_t  *proto;

    globus_gass_transfer_listen_callback_t      listen_callback;
    void *                                      listen_callback_arg;

    globus_gass_transfer_close_callback_t       close_callback;
    void *                                      close_callback_arg;
    void *                                      user_pointer;
} globus_gass_transfer_listener_struct_t;

extern globus_handle_table_t globus_i_gass_transfer_listener_handles;
/* */

// Declarations of symbols in my_ez.c
extern globus_hashtable_t globus_l_gass_server_ez_listeners;

extern int
my_globus_gass_server_ez_init(globus_gass_transfer_listener_t * listener,
                           globus_gass_transfer_listenerattr_t * attr,
                           char * scheme,
                           globus_gass_transfer_requestattr_t * reqattr,
                           unsigned long options,
                           globus_gass_server_ez_client_shutdown_t callback);
