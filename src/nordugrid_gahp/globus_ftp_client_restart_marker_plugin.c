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
 * @file globus_ftp_client_restart_marker_plugin.c GridFTP Restart Marker Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_restart_marker_plugin.c,v $
 * $Revision: 1.5 $
 * $Date: 2006/01/19 05:54:53 $
 * $Author: mlink $
 */

#include "globus_ftp_client_restart_marker_plugin.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#define GLOBUS_L_FTP_CLIENT_RESTART_MARKER_PLUGIN_NAME "globus_ftp_client_restart_marker_plugin"

static globus_bool_t globus_l_ftp_client_restart_marker_plugin_activate(void);
static globus_bool_t globus_l_ftp_client_restart_marker_plugin_deactivate(void);

globus_module_descriptor_t globus_i_ftp_client_restart_marker_plugin_module =
{
    GLOBUS_L_FTP_CLIENT_RESTART_MARKER_PLUGIN_NAME,
    globus_l_ftp_client_restart_marker_plugin_activate,
    globus_l_ftp_client_restart_marker_plugin_deactivate,
    GLOBUS_NULL
};

static
int
globus_l_ftp_client_restart_marker_plugin_activate(void)
{
    int rc;

    rc = globus_module_activate(GLOBUS_FTP_CLIENT_MODULE);
    return rc;
}

static
int
globus_l_ftp_client_restart_marker_plugin_deactivate(void)
{
    return globus_module_deactivate(GLOBUS_FTP_CLIENT_MODULE);
}

typedef struct restart_marker_plugin_info_s
{
    void *                                                  user_arg;
    globus_ftp_client_restart_marker_plugin_begin_cb_t      begin_cb;
    globus_ftp_client_restart_marker_plugin_marker_cb_t     marker_cb;
    globus_ftp_client_restart_marker_plugin_complete_cb_t   complete_cb;

    char *                                          error_url;
    globus_object_t *                               error_obj;

    globus_ftp_client_restart_marker_t              restart_marker;

    /* used for get command only */
    globus_bool_t                                   use_data;
    time_t                                          last_time;
    globus_mutex_t                                  lock;

} restart_marker_plugin_info_t;


/**
 * Plugin complete callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback will be called when one of the transfer commands
 * is completed. Will also call user's 'complete' callback
 */

static
void
restart_marker_plugin_complete_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle)
{
    restart_marker_plugin_info_t *              ps;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->complete_cb)
    {
        ps->complete_cb(
            ps->user_arg,
            handle,
            ps->error_obj,
            ps->error_url);
    }

    globus_ftp_client_restart_marker_destroy(&ps->restart_marker);
}

/**
 * Plugin abort callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback will be called when an abort has been requested
 */

static
void
restart_marker_plugin_abort_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle)
{
    restart_marker_plugin_info_t  *             ps;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    ps->error_obj = globus_error_construct_string(
        GLOBUS_FTP_CLIENT_MODULE,
        GLOBUS_NULL,
        "[restart_marker_plugin_abort_cb] Transfer aborted by user\n");
}

/**
 * Plugin fault callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback will be called when one of the transfer commands
 * has failed.
 */

static
void
restart_marker_plugin_fault_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_object_t *                           error)
{
    restart_marker_plugin_info_t *              ps;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->error_url || ps->error_obj)
    {
        /* already received error (before restart attempt) */
        return;
    }

    ps->error_url = globus_libc_strdup(url);
    ps->error_obj = globus_object_copy(error);
}

/**
 * Plugin response callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * Parses out restart markers and calls user's 'marker' callback.
 */

static
void
restart_marker_plugin_response_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_object_t *                           error,
    const globus_ftp_control_response_t *       ftp_response)
{
    restart_marker_plugin_info_t *              ps;
    char *                                      p;
    globus_off_t                                offset;
    globus_off_t                                end;
    int                                         consumed;
    globus_result_t                             result;
    int                                         count;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->marker_cb &&
        !error &&
        ftp_response &&
        ftp_response->response_buffer &&
        ftp_response->code == 111)
    {
        /* skip '111 Range Marker' */
        p = strstr((char *) ftp_response->response_buffer, "Range Marker");
        if(!p)
        {
            return;
        }

        p += sizeof("Range Marker");

        count = 0;
        while(sscanf(p, " %" GLOBUS_OFF_T_FORMAT " - %" GLOBUS_OFF_T_FORMAT " %n",
                &offset, &end, &consumed) >= 2)
        {
            result = globus_ftp_client_restart_marker_insert_range(
                &ps->restart_marker,
                offset,
                end);

            if(result == GLOBUS_SUCCESS)
            {
                count++;
            }
            else
            {
                break;
            }

            p += consumed;

            if(*p == ',')
            {
                p++;
            }
            else
            {
                break;
            }
        }

        if(count)
        {
            ps->marker_cb(
                ps->user_arg,
                handle,
                &ps->restart_marker);
        }
    }
}

/**
 * Plugin data callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This plugin is used to create restart marker callbacks in the case
 * of a 'get' command.  (no restart markers are received in the 'get'
 * case)
 */

static
void
restart_marker_plugin_data_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    globus_object_t *                           error,
    const globus_byte_t *                       buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof)
{
    restart_marker_plugin_info_t *              ps;
    time_t                                      time_now;
    globus_result_t                             result;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->marker_cb && ps->use_data && !error)
    {
        globus_mutex_lock(&ps->lock);

        time_now = time(NULL);

        result = globus_ftp_client_restart_marker_insert_range(
                &ps->restart_marker,
                offset,
                offset + length);

        if(result == GLOBUS_SUCCESS &&
            time_now > ps->last_time)
        {
            ps->last_time = time_now;

            ps->marker_cb(
                ps->user_arg,
                handle,
                &ps->restart_marker);
        }

        globus_mutex_unlock(&ps->lock);
    }
}

/**
 * Plugin get command callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback signifies the start of a 'get' command. It will
 * call the user's 'begin' callback
 */

static
void
restart_marker_plugin_get_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    const globus_ftp_client_operationattr_t *   attr,
    globus_bool_t                               restart)
{
    restart_marker_plugin_info_t *              ps;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->error_obj)
    {
        globus_object_free(ps->error_obj);
        ps->error_obj = GLOBUS_NULL;
    }

    if(ps->error_url)
    {
        globus_libc_free(ps->error_url);
        ps->error_url = GLOBUS_NULL;
    }

    if(restart)
    {
        /* we have been restarted.. previous fault disregarded */
        return;
    }

    ps->use_data = GLOBUS_TRUE;
    ps->last_time = 0;

    if(ps->begin_cb)
    {
        restart = ps->begin_cb(
            ps->user_arg,
            handle,
            url,
            GLOBUS_NULL,
            &ps->restart_marker);
    }

    if(restart)
    {
        globus_ftp_client_plugin_restart_get(
            handle,
            url,
            attr,
            &ps->restart_marker,
            GLOBUS_NULL);
    }
    else
    {
        globus_ftp_client_restart_marker_init(&ps->restart_marker);
    }
}


/**
 * Plugin put command callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback signifies the start of a 'put' command. It will
 * call the user's 'begin' callback
 */

static
void
restart_marker_plugin_put_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    const globus_ftp_client_operationattr_t *   attr,
    globus_bool_t                               restart)
{
    restart_marker_plugin_info_t *              ps;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->error_obj)
    {
        globus_object_free(ps->error_obj);
        ps->error_obj = GLOBUS_NULL;
    }

    if(ps->error_url)
    {
        globus_libc_free(ps->error_url);
        ps->error_url = GLOBUS_NULL;
    }

    if(restart)
    {
        /* we have been restarted.. previous fault disregarded */
        return;
    }

    ps->use_data = GLOBUS_FALSE;

    if(ps->begin_cb)
    {
        restart = ps->begin_cb(
            ps->user_arg,
            handle,
            GLOBUS_NULL,
            url,
            &ps->restart_marker);
    }

    if(restart)
    {
        globus_ftp_client_plugin_restart_put(
            handle,
            url,
            attr,
            &ps->restart_marker,
            GLOBUS_NULL);
    }
    else
    {
        globus_ftp_client_restart_marker_init(&ps->restart_marker);
    }
}


/**
 * Plugin thrid party transfer command callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback signifies the start of a 'transfer' command. It will
 * call the user's 'begin' callback
 */

static
void
restart_marker_plugin_transfer_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    const char *                                source_url,
    const globus_ftp_client_operationattr_t *   source_attr,
    const char *                                dest_url,
    const globus_ftp_client_operationattr_t *   dest_attr,
    globus_bool_t                               restart)
{
    restart_marker_plugin_info_t *              ps;

    ps = (restart_marker_plugin_info_t *) plugin_specific;

    if(ps->error_obj)
    {
        globus_object_free(ps->error_obj);
        ps->error_obj = GLOBUS_NULL;
    }

    if(ps->error_url)
    {
        globus_libc_free(ps->error_url);
        ps->error_url = GLOBUS_NULL;
    }

    if(restart)
    {
        /* we have been restarted.. previous fault disregarded */
        return;
    }

    ps->use_data = GLOBUS_FALSE;

    if(ps->begin_cb)
    {
        restart = ps->begin_cb(
            ps->user_arg,
            handle,
            source_url,
            dest_url,
            &ps->restart_marker);
    }

    if(restart)
    {
        globus_ftp_client_plugin_restart_third_party_transfer(
            handle,
            source_url,
            source_attr,
            dest_url,
            dest_attr,
            &ps->restart_marker,
            GLOBUS_NULL);
    }
    else
    {
        globus_ftp_client_restart_marker_init(&ps->restart_marker);
    }
}

/**
 * Plugin copy callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback is called to create a new copy of this plugin.
 */

static
globus_ftp_client_plugin_t *
restart_marker_plugin_copy_cb(
    globus_ftp_client_plugin_t *                plugin_template,
    void *                                      plugin_specific)
{
    restart_marker_plugin_info_t *              old_ps;
    globus_ftp_client_plugin_t *                new_plugin;
    globus_result_t                             result;

    old_ps = (restart_marker_plugin_info_t *) plugin_specific;

    new_plugin = (globus_ftp_client_plugin_t *)
        globus_malloc(sizeof(globus_ftp_client_plugin_t));

    if(new_plugin == GLOBUS_NULL)
    {
        return GLOBUS_NULL;
    }

    result = globus_ftp_client_restart_marker_plugin_init(
        new_plugin,
        old_ps->begin_cb,
        old_ps->marker_cb,
        old_ps->complete_cb,
        old_ps->user_arg);

    if(result != GLOBUS_SUCCESS)
    {
        globus_free(new_plugin);
        new_plugin = GLOBUS_NULL;
    }

    return new_plugin;
}

/**
 * Plugin destroy callback
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This callback is called to destroy a copy of a plugin made with the
 * copy callback above.
 */

static
void
restart_marker_plugin_destroy_cb(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific)
{
    globus_ftp_client_restart_marker_plugin_destroy(plugin);
    globus_free(plugin);
}

#endif  /* GLOBUS_DONT_DOCUMENT_INTERNAL */

/**
 * Initialize a restart marker plugin
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * This function initializes a restart marker plugin. Any params
 * except for the plugin may be GLOBUS_NULL
 *
 * @param plugin
 *        a pointer to a plugin type to be initialized
 *
 * @param user_arg
 *        a pointer to some user specific data that will be provided to
 *        all callbacks
 *
 * @param begin_cb
 *        the callback to be called upon the start of a transfer
 *
 * @param marker_cb
 *        the callback to be called with every restart marker received
 *
 * @param complete_cb
 *        the callback to be called to indicate transfer completion
 *
 * @return
 *        - GLOBUS_SUCCESS
 *        - Error on NULL plugin
 *        - Error on init internal plugin
 */


globus_result_t
globus_ftp_client_restart_marker_plugin_init(
    globus_ftp_client_plugin_t *                            plugin,
    globus_ftp_client_restart_marker_plugin_begin_cb_t      begin_cb,
    globus_ftp_client_restart_marker_plugin_marker_cb_t     marker_cb,
    globus_ftp_client_restart_marker_plugin_complete_cb_t   complete_cb,
    void *                                                  user_arg)
{
    restart_marker_plugin_info_t *                  ps;
    globus_result_t                                 result;
    GlobusFuncName(globus_ftp_client_restart_marker_plugin_init);

    if(plugin == GLOBUS_NULL)
    {
        return globus_error_put(globus_error_construct_string(
            GLOBUS_FTP_CLIENT_MODULE,
            GLOBUS_NULL,
            "[%s] NULL plugin at %s\n",
            GLOBUS_FTP_CLIENT_MODULE->module_name,
            _globus_func_name));
    }

    ps = (restart_marker_plugin_info_t *)
        globus_malloc(sizeof(restart_marker_plugin_info_t));

    if(ps == GLOBUS_NULL)
    {
        return globus_error_put(globus_error_construct_string(
            GLOBUS_FTP_CLIENT_MODULE,
            GLOBUS_NULL,
            "[%s] Out of memory at %s\n",
             GLOBUS_FTP_CLIENT_MODULE->module_name,
             _globus_func_name));
    }

    /*
     *  initialize plugin specific structure.
     */
    ps->user_arg       = user_arg;
    ps->begin_cb       = begin_cb;
    ps->marker_cb      = marker_cb;
    ps->complete_cb    = complete_cb;

    ps->error_url      = GLOBUS_NULL;
    ps->error_obj      = GLOBUS_NULL;

    globus_mutex_init(&ps->lock, GLOBUS_NULL);

    result = globus_ftp_client_plugin_init(
              plugin,
              GLOBUS_L_FTP_CLIENT_RESTART_MARKER_PLUGIN_NAME,
              GLOBUS_FTP_CLIENT_CMD_MASK_FILE_ACTIONS,
              ps);

    if(result != GLOBUS_SUCCESS)
    {
        globus_mutex_destroy(&ps->lock);
        globus_free(ps);

        return result;
    }

    globus_ftp_client_plugin_set_destroy_func(plugin,
        restart_marker_plugin_destroy_cb);
    globus_ftp_client_plugin_set_copy_func(plugin,
        restart_marker_plugin_copy_cb);
    globus_ftp_client_plugin_set_get_func(plugin,
        restart_marker_plugin_get_cb);
    globus_ftp_client_plugin_set_data_func(plugin,
        restart_marker_plugin_data_cb);
    globus_ftp_client_plugin_set_put_func(plugin,
        restart_marker_plugin_put_cb);
    globus_ftp_client_plugin_set_third_party_transfer_func(plugin,
        restart_marker_plugin_transfer_cb);
    globus_ftp_client_plugin_set_response_func(plugin,
        restart_marker_plugin_response_cb);
    globus_ftp_client_plugin_set_complete_func(plugin,
        restart_marker_plugin_complete_cb);
    globus_ftp_client_plugin_set_fault_func(plugin,
        restart_marker_plugin_fault_cb);
    globus_ftp_client_plugin_set_abort_func(plugin,
        restart_marker_plugin_abort_cb);

    return GLOBUS_SUCCESS;
}

/**
 * Destroy restart marker plugin
 * @ingroup globus_ftp_client_restart_marker_plugin
 *
 * Frees up memory associated with plugin
 *
 * @param plugin
 *        plugin previously initialized with init (above)
 *
 * @return
 *        - GLOBUS_SUCCESS
 *        - Error on NULL plugin
 */

globus_result_t
globus_ftp_client_restart_marker_plugin_destroy(
    globus_ftp_client_plugin_t *                    plugin)
{
    globus_result_t                                 result;
    restart_marker_plugin_info_t *                  ps;
    GlobusFuncName(globus_ftp_client_restart_marker_plugin_destroy);

    if(plugin == GLOBUS_NULL)
    {
        return globus_error_put(globus_error_construct_string(
            GLOBUS_FTP_CLIENT_MODULE,
            GLOBUS_NULL,
            "[%s] NULL plugin at %s\n",
            GLOBUS_FTP_CLIENT_MODULE->module_name,
            _globus_func_name));
    }

    result = globus_ftp_client_plugin_get_plugin_specific(
        plugin,
        (void **) &ps);

    if(result != GLOBUS_SUCCESS)
    {
        return result;
    }

    if(ps->error_obj)
    {
        globus_object_free(ps->error_obj);
        ps->error_obj = GLOBUS_NULL;
    }

    if(ps->error_url)
    {
        globus_libc_free(ps->error_url);
        ps->error_url = GLOBUS_NULL;
    }

    globus_mutex_destroy(&ps->lock);
    globus_free(ps);

    return globus_ftp_client_plugin_destroy(plugin);
}
