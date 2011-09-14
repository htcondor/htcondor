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
 * @file globus_ftp_client_restart_plugin.c GridFTP Restart Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_restart_plugin.c,v $
 * $Revision: 1.24 $
 * $Date: 2009/12/17 20:27:20 $
 */
#endif

#include "globus_i_ftp_client.h"
#include "globus_ftp_client_restart_plugin.h"

#include <stdio.h>
#include <string.h>
#include "version.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
#define GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_NAME "globus_ftp_client_restart_plugin"

#define GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_RETURN(plugin) \
    if(plugin == GLOBUS_NULL) \
    {\
	return globus_error_put(globus_error_construct_string(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		"[%s] NULL plugin at %s\n",\
		GLOBUS_FTP_CLIENT_MODULE->module_name,\
		_globus_func_name));\
    }
#define GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(d, func) \
    result = globus_ftp_client_plugin_set_##func##_func(d, globus_l_ftp_client_restart_plugin_##func); \
    if(result != GLOBUS_SUCCESS) goto result_exit;

/**
 * Plugin specific data for the restart plugin
 */
typedef struct
{
    /** Maximum num of faults to handle. If -1, then we won't be
     * limiting ourselves.
     */
    int						max_retries;

    /**
     * If true, then we will do an exponential backoff of th
     * interval between retries.
     */
    globus_bool_t				backoff;
    /**
     * Delay time between fault detection and next restart.
     */
    globus_reltime_t				interval;
    
    /**
     * Deadline, after which no further restart attempts will
     * be tried. If zero, then we won't be limiting ourselves
     */
    globus_abstime_t				deadline;

    /**
     * Source used in our operation (if applicable).
     */
    char *					source_url;

    /**
     * Destination used in our operation (if applicable).
     */
    char *					dest_url;

    /**
     * Source attributes
     */
    globus_ftp_client_operationattr_t 		source_attr;

    /**
     * Destination attributes.
     */
    globus_ftp_client_operationattr_t 		dest_attr;

    /**
     * Operation we are processing.
     */
    globus_i_ftp_client_operation_t		operation;
    
    /** file mode for chmod calls **/
    int                                         chmod_file_mode;

    /** parameters for checksum calls **/
    globus_off_t                                checksum_offset;
    globus_off_t                                checksum_length;
    const char *                                checksum_alg;

    globus_bool_t                               abort_pending;

    int                                 ticker;
    int                                 stall_timeout;
    globus_callback_handle_t            ticker_handle;
    globus_bool_t                       ticker_set;
    globus_bool_t                       xfer_running;
    globus_ftp_client_handle_t *        ticker_ftp_handle;

    globus_off_t *                      ticker_nbyte_a;
}
globus_l_ftp_client_restart_plugin_t;

static
void
l_begin_xfer(
    globus_ftp_client_handle_t *        ftp_handle,
    globus_l_ftp_client_restart_plugin_t *  d);

static
globus_ftp_client_plugin_t *
globus_l_ftp_client_restart_plugin_copy(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific);

static
void
globus_l_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific);

static
void
globus_l_ftp_client_restart_plugin_chmod(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_cksm(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_delete(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_modification_time(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_size(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);


static
void
globus_l_ftp_client_restart_plugin_feat(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_mkdir(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_rmdir(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_verbose_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_machine_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_mlst(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_stat(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_move(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_get(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_put(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_restart_plugin_third_party_transfer(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t 				restart);

static
void globus_l_ftp_client_restart_plugin_abort(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle);

static
void
globus_l_ftp_client_restart_plugin_fault(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error);

static
void
globus_l_ftp_client_restart_plugin_genericify(
    globus_l_ftp_client_restart_plugin_t *	d);

static int globus_l_ftp_client_restart_plugin_activate(void);
static int globus_l_ftp_client_restart_plugin_deactivate(void);

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t globus_i_ftp_client_restart_plugin_module =
{
    "globus_ftp_client_restart_plugin",
    globus_l_ftp_client_restart_plugin_activate,
    globus_l_ftp_client_restart_plugin_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};


static
int
globus_l_ftp_client_restart_plugin_activate(void)
{
    return globus_module_activate(GLOBUS_FTP_CLIENT_MODULE);
}

static
int
globus_l_ftp_client_restart_plugin_deactivate(void)
{
    return globus_module_deactivate(GLOBUS_FTP_CLIENT_MODULE);
}


static
globus_ftp_client_plugin_t *
globus_l_ftp_client_restart_plugin_copy(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific)
{
    globus_ftp_client_plugin_t *		newguy;
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_l_ftp_client_restart_plugin_t *	newd;
    globus_result_t				result;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    newguy = globus_libc_malloc(sizeof(globus_ftp_client_plugin_t));
    if(newguy == GLOBUS_NULL)
    {
	goto error_exit;
    }
    result = globus_ftp_client_restart_plugin_init(newguy,
	    d->max_retries,
	    &d->interval,
	    &d->deadline);

    if(result != GLOBUS_SUCCESS)
    {
	goto free_exit;
    }
    result = globus_ftp_client_plugin_get_plugin_specific(newguy,
	                                                  (void **) &newd);
    if(result != GLOBUS_SUCCESS)
    {
	goto destroy_exit;
    }
    newd->backoff = d->backoff;
    newd->stall_timeout = d->stall_timeout;

    return newguy;

destroy_exit:
    globus_ftp_client_restart_plugin_destroy(newguy);
free_exit:
    globus_libc_free(newguy);
error_exit:
    return GLOBUS_NULL;
}
/* globus_l_ftp_client_restart_plugin_copy() */

static
void
globus_l_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific)
{
    globus_ftp_client_restart_plugin_destroy(plugin);
    globus_libc_free(plugin);
}
/* globus_l_ftp_client_restart_plugin_destroy() */

static
void
globus_l_ftp_client_restart_plugin_delete(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_DELETE;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_delete() */

static
void
globus_l_ftp_client_restart_plugin_chmod(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_CHMOD;
    d->source_url = globus_libc_strdup(url);
    d->chmod_file_mode = mode;
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_chmod() */

static
void
globus_l_ftp_client_restart_plugin_cksm(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_CKSM;
    d->source_url = globus_libc_strdup(url);
    d->checksum_offset=offset;
    d->checksum_length=length;
    d->checksum_alg=algorithm;
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_cksm() */

static
void
globus_l_ftp_client_restart_plugin_modification_time(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MDTM;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_modification_time() */

static
void
globus_l_ftp_client_restart_plugin_size(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_SIZE;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_size() */


static
void
globus_l_ftp_client_restart_plugin_feat(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_FEAT;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_feat() */

static
void
globus_l_ftp_client_restart_plugin_mkdir(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MKDIR;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_mkdir() */

static
void
globus_l_ftp_client_restart_plugin_rmdir(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_RMDIR;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_rmdir() */

static
void
globus_l_ftp_client_restart_plugin_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_NLST;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_list() */

static
void
globus_l_ftp_client_restart_plugin_verbose_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_LIST;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_verbose_list() */

static
void
globus_l_ftp_client_restart_plugin_machine_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MLSD;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);
}
/* globus_l_ftp_client_restart_plugin_machine_list() */

static
void
globus_l_ftp_client_restart_plugin_mlst(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MLST;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_mlst() */

static
void
globus_l_ftp_client_restart_plugin_stat(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_STAT;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr,attr);

}
/* globus_l_ftp_client_restart_plugin_stat() */


static
void
globus_l_ftp_client_restart_plugin_move(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_MOVE;
    d->source_url = globus_libc_strdup(source_url);
    globus_ftp_client_operationattr_copy(&d->source_attr, attr);
    d->dest_url = globus_libc_strdup(dest_url);
    globus_ftp_client_operationattr_copy(&d->dest_attr, attr);
}
/* globus_l_ftp_client_restart_plugin_move() */

static
void
globus_l_ftp_client_restart_plugin_get(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_GET;
    d->source_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->source_attr, attr);

    l_begin_xfer(handle, d);
}
/* globus_l_ftp_client_restart_plugin_get() */

static
void
globus_l_ftp_client_restart_plugin_put(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_PUT;
    d->dest_url = globus_libc_strdup(url);
    globus_ftp_client_operationattr_copy(&d->dest_attr, attr);

    l_begin_xfer(handle, d);
}
/* globus_l_ftp_client_restart_plugin_put() */

static
void
globus_l_ftp_client_restart_plugin_third_party_transfer(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    globus_l_ftp_client_restart_plugin_genericify(d);
    d->operation = GLOBUS_FTP_CLIENT_TRANSFER;
    d->source_url = globus_libc_strdup(source_url);
    globus_ftp_client_operationattr_copy(&d->source_attr, source_attr);
    d->dest_url = globus_libc_strdup(dest_url);
    globus_ftp_client_operationattr_copy(&d->dest_attr, dest_attr);

    if(d->source_attr->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK &&
        d->dest_attr->mode == GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK)
    {
        l_begin_xfer(handle, d);
    }
}
/* globus_l_ftp_client_restart_plugin_third_party_transfer() */

static
void globus_l_ftp_client_restart_plugin_abort(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    d->abort_pending = GLOBUS_TRUE;
}



/* 
 *  when a transfer ends
 */
static
void
globus_l_ftp_client_restart_plugin_complete(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle)
{
    globus_l_ftp_client_restart_plugin_t *  d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    /* XXX examine for race */
    d->xfer_running = GLOBUS_FALSE;
    if(d->ticker_nbyte_a != NULL)
    {
        free(d->ticker_nbyte_a);
        d->ticker_nbyte_a = NULL;
    }
    if(d->ticker_set)
    {
        d->ticker_set = GLOBUS_FALSE;
        globus_callback_unregister(
            d->ticker_handle, NULL, NULL, NULL);
    }

}

static
void
l_ticker_cb(
    void *                              user_arg)
{
    globus_result_t                         result;
    globus_l_ftp_client_restart_plugin_t *  d;
    globus_abstime_t	                    when;
    globus_bool_t                           retry = GLOBUS_TRUE;

    d = (globus_l_ftp_client_restart_plugin_t *) user_arg;

    /* no reason to do anything here if the transfer isnt running */
    if(!d->xfer_running || d->abort_pending)
    {
        return;
    }

    /* allow 1 tic to insure resolution */
    if(d->ticker > 1)
    {
        if(d->abort_pending)
        {
            return;
        }
        
        if(d->max_retries == 0)
        {
            retry = GLOBUS_FALSE;
        }
        else if(d->max_retries > 0)
        {
            d->max_retries--;
        }
        
        GlobusTimeAbstimeGetCurrent(when);
        if((d->deadline.tv_sec != 0 || d->deadline.tv_nsec != 0) &&
            globus_abstime_cmp(&when, &d->deadline) > 0)
        {
            retry = GLOBUS_FALSE;
        }

        GlobusTimeAbstimeSet(when, d->interval.tv_sec, d->interval.tv_usec);
        
        if(retry)
        {
            if(d->backoff)
            {
                GlobusTimeReltimeMultiply(d->interval, 2);
            }
            
            switch(d->operation)
            {
                case GLOBUS_FTP_CLIENT_GET:
                    result = globus_ftp_client_plugin_restart_get(
                        d->ticker_ftp_handle,
                        d->source_url,
                        &d->source_attr,
                        NULL,
                        &when);
                    break;
                case GLOBUS_FTP_CLIENT_PUT:
                    result = globus_ftp_client_plugin_restart_put(
                        d->ticker_ftp_handle,
                        d->dest_url,
                        &d->dest_attr,
                        NULL,
                        &when);
                    break;
                case GLOBUS_FTP_CLIENT_TRANSFER:
                    result = globus_ftp_client_plugin_restart_third_party_transfer(
                        d->ticker_ftp_handle,
                        d->source_url,
                        &d->source_attr,
                        d->dest_url,
                        &d->dest_attr,
                        NULL,
                        &when);
                    break;
    
                default:
                    globus_assert(0 && "should never happen--memory corruption");
            }
        }
    }
    d->ticker++;

    if(!retry)
    {
        globus_ftp_client_plugin_abort(d->ticker_ftp_handle);
    }
}

static 
void
l_begin_xfer(
    globus_ftp_client_handle_t *        ftp_handle,
    globus_l_ftp_client_restart_plugin_t *  d)
{
    globus_reltime_t                    period;

    if(!d->stall_timeout)
    {
        return;
    }
    d->ticker = 0;
    d->ticker_ftp_handle = ftp_handle;
    d->xfer_running = GLOBUS_TRUE;
    d->ticker_set = GLOBUS_TRUE;
    
    /* start a timer, half of stall timeout */
    /* we won't restart/abort until the 2nd callback with no data */
    GlobusTimeReltimeSet(period, d->stall_timeout/2, 0);
    globus_callback_register_periodic(
        &d->ticker_handle,
        &period,
        &period,
        l_ticker_cb,
        d);
}

static
void
globus_l_ftp_client_restart_plugin_data(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    globus_object_t *                           error,
    const globus_byte_t *                       buffer,
    globus_size_t                               length,
    globus_off_t                                offset,
    globus_bool_t                               eof)
{
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    if(!d->xfer_running)
    {
        return;
    }

    d->ticker = 0;
}


static
void globus_l_ftp_client_restart_plugin_response(
    globus_ftp_client_plugin_t *                plugin,
    void *                                      plugin_specific,
    globus_ftp_client_handle_t *                handle,
    const char *                                url,
    globus_object_t *                           error,
    const globus_ftp_control_response_t *       ftp_response)
{
    globus_off_t                        nbytes;
    char *                              tmp_ptr;
    char *                              buffer;
    int                                 sc;
    int                                 stripe_ndx;
    globus_l_ftp_client_restart_plugin_t *	d;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    if(!d->xfer_running)
    {
        return;
    }

    if(!error &&
        ftp_response &&
        ftp_response->response_buffer &&
        ftp_response->code == 112)
    {
        buffer = (char *) ftp_response->response_buffer;
        if(d->ticker_nbyte_a == NULL)
        {
            int                         num_stripes;
            /* parse out total stripe count */
            tmp_ptr = strstr(buffer, "Total Stripe Count:");
            if(tmp_ptr == NULL)
            {
                return;
            }
            sc = sscanf(tmp_ptr + sizeof("Total Stripe Count:"),
                " %d", &num_stripes);
            if(sc != 1)
            {
                return;
            }

            d->ticker_nbyte_a = (globus_off_t *)
                globus_calloc(num_stripes, sizeof(globus_off_t));
        }

        /* parse out Stripe Index */
        tmp_ptr = strstr(buffer, "Stripe Index:");
        if(tmp_ptr == NULL)
        {
            return;
        }
        sc = sscanf(tmp_ptr + sizeof("Stripe Index:"),
            " %d", &stripe_ndx);
        if(sc != 1)
        {
            return;
        }

        /* parse out bytes transfered */
        tmp_ptr = strstr(buffer, "Stripe Bytes Transferred:");
        if(tmp_ptr == NULL)
        {
            return;
        }
        sc = sscanf(tmp_ptr + sizeof("Stripe Bytes Transferred:"),
            " %" GLOBUS_OFF_T_FORMAT, &nbytes);
        if(sc != 1)
        {
            return;
        }
        if(nbytes == d->ticker_nbyte_a[stripe_ndx])
        {
            return;
        }

        d->ticker = 0;
        d->ticker_nbyte_a[stripe_ndx] = nbytes;
    }

}


static
void
globus_l_ftp_client_restart_plugin_fault(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error)
{
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_abstime_t				when;

    d = (globus_l_ftp_client_restart_plugin_t *) plugin_specific;

    if(d->abort_pending)
    {
        return;
    }

    if(d->max_retries == 0)
    {
	return;
    }
    else if(d->max_retries > 0)
    {
	d->max_retries--;
    }

    GlobusTimeAbstimeGetCurrent(when);
    if((d->deadline.tv_sec != 0 || d->deadline.tv_nsec != 0) &&
	globus_abstime_cmp(&when, &d->deadline) > 0)
    {
	return;
    }
    GlobusTimeAbstimeSet(when, d->interval.tv_sec, d->interval.tv_usec);

    switch(d->operation)
    {
	case GLOBUS_FTP_CLIENT_CHMOD:
	    globus_ftp_client_plugin_restart_chmod(
		    handle,
		    d->source_url,
		    d->chmod_file_mode,
		    &d->source_attr,
		    &when);
	    break;

	case GLOBUS_FTP_CLIENT_CKSM:
	    globus_ftp_client_plugin_restart_cksm(
		    handle,
		    d->source_url,
		    d->checksum_offset,
		    d->checksum_length,
		    d->checksum_alg,
		    &d->source_attr,
		    &when);
	    break;

	case GLOBUS_FTP_CLIENT_DELETE:
	    globus_ftp_client_plugin_restart_delete(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;

	case GLOBUS_FTP_CLIENT_FEAT:
	    globus_ftp_client_plugin_restart_feat(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;

        case GLOBUS_FTP_CLIENT_MKDIR:
	    globus_ftp_client_plugin_restart_mkdir(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_RMDIR:
	    globus_ftp_client_plugin_restart_rmdir(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_MOVE:
	    globus_ftp_client_plugin_restart_move(
		    handle,
		    d->source_url,
		    d->dest_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_LIST:
	    globus_ftp_client_plugin_restart_verbose_list(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_NLST:
	    globus_ftp_client_plugin_restart_list(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_MLSD:
	    globus_ftp_client_plugin_restart_machine_list(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_MLST:
	    globus_ftp_client_plugin_restart_mlst(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_STAT:
	    globus_ftp_client_plugin_restart_stat(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_GET:
	    globus_ftp_client_plugin_restart_get(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    GLOBUS_NULL,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_PUT:
	    globus_ftp_client_plugin_restart_put(
		    handle,
		    d->dest_url,
		    &d->dest_attr,
		    GLOBUS_NULL,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_TRANSFER:
	    globus_ftp_client_plugin_restart_third_party_transfer(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    d->dest_url,
		    &d->dest_attr,
		    GLOBUS_NULL,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_MDTM:
	    globus_ftp_client_plugin_restart_modification_time(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
	case GLOBUS_FTP_CLIENT_SIZE:
	    globus_ftp_client_plugin_restart_size(
		    handle,
		    d->source_url,
		    &d->source_attr,
		    &when);
	    break;
    default: /* Only state left is FTP_CLIENT_IDLE */
	  globus_assert(0 && "Unexpected state");
    }

    if(d->backoff)
    {
	GlobusTimeReltimeMultiply(d->interval, 2);
    }
}
/* globus_l_ftp_client_restart_plugin_fault() */
#endif

globus_result_t
globus_ftp_client_restart_plugin_set_stall_timeout(
    globus_ftp_client_plugin_t *        plugin,
    int                                 to_secs)
{
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_result_t                     result;

    result = globus_ftp_client_plugin_get_plugin_specific(
        plugin, (void **) &d);
    if(result != GLOBUS_SUCCESS)
    {
        return result;
    }
    
    d->stall_timeout = to_secs;
    return GLOBUS_SUCCESS;
}

/**
 * Initialize an instance of the GridFTP restart plugin
 * @ingroup globus_ftp_client_restart_plugin
 *
 * This function will initialize the plugin-specific instance data
 * for this plugin, and will make the plugin usable for ftp
 * client handle attribute and handle creation.
 *
 * @param plugin
 *        A pointer to an uninitialized plugin. The plugin will be
 *        configured as a restart plugin.
 * @param max_retries
 *        The maximum number of times to retry the operation before giving
 *        up on the transfer. If this value is less than or equal to 0,
 *        then the restart plugin will keep trying to restart the operation
 *        until it completes or the deadline is reached with an unsuccessful
 *        operation.
 * @param interval
 *        The interval to wait after a failures before retrying the transfer.
 *        If the interval is 0 seconds or GLOBUS_NULL, then an exponential 
 *        backoff will be used.
 * @param deadline
 *        An absolute timeout.  If the deadline is GLOBUS_NULL then the retry
 *        will never timeout.
 *
 * @return This function returns an error if
 * - plugin is null
 *
 * @see globus_ftp_client_restart_plugin_destroy(),
 *      globus_ftp_client_handleattr_add_plugin(),
 *      globus_ftp_client_handleattr_remove_plugin(),
 *      globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_restart_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    int						max_retries,
    globus_reltime_t *				interval,
    globus_abstime_t *				deadline)
{
    char *                              env_str;
    globus_l_ftp_client_restart_plugin_t *	d;
    globus_result_t				result;
    GlobusFuncName(globus_ftp_client_restart_plugin_init);

    if(plugin == GLOBUS_NULL)
    {
	return globus_error_put(globus_error_construct_string(
		GLOBUS_FTP_CLIENT_MODULE,
		GLOBUS_NULL,
		"[%s] NULL plugin at %s\n",
		GLOBUS_FTP_CLIENT_MODULE->module_name,
		_globus_func_name));
    }
        
    d =
	globus_libc_calloc(1, sizeof(globus_l_ftp_client_restart_plugin_t));

    if(! d)
    {
	return globus_error_put(globus_error_construct_string(
		                GLOBUS_FTP_CLIENT_MODULE,
				GLOBUS_NULL,
				"[%s] Out of memory at %s\n",
				 GLOBUS_FTP_CLIENT_MODULE->module_name,
				 _globus_func_name));
    }

    result = globus_ftp_client_plugin_init(plugin,
				  GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_NAME,
				  GLOBUS_FTP_CLIENT_CMD_MASK_ALL,
				  d);
    if(result != GLOBUS_SUCCESS)
    {
	globus_libc_free(d);

	return result;
    }

    d->max_retries = max_retries > 0 ? max_retries : -1;

    if(interval)
    {
	GlobusTimeReltimeCopy(d->interval, *interval);
    }
    if((!interval) || (interval->tv_sec == 0 && interval->tv_usec == 0))
    {
	d->backoff = GLOBUS_TRUE;
	d->interval.tv_sec = 1;
	d->interval.tv_usec = 0;
    }
    else
    {
        d->backoff = GLOBUS_FALSE;
    }

    if(deadline)
    {
	GlobusTimeAbstimeCopy(d->deadline, *deadline);
    }
    else
    {
	GlobusTimeAbstimeCopy(d->deadline, globus_i_abstime_infinity);
    }

    d->dest_url = GLOBUS_NULL;
    d->source_url = GLOBUS_NULL;

    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, copy);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, destroy);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, chmod);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, cksm);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, delete);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, modification_time);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, size);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, feat);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, mkdir);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, rmdir);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, move);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, verbose_list);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, machine_list);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, mlst);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, stat);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, list);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, get);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, put);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, third_party_transfer);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, fault);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, abort);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, complete);
    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin, data);

    GLOBUS_FTP_CLIENT_RESTART_PLUGIN_SET_FUNC(plugin,
        response);

    env_str = globus_libc_getenv("GUC_STALL_TIMEOUT");
    if(env_str != NULL)
    {
        int                             sc;
        int                             to_secs;

        sc = sscanf(env_str, "%d", &to_secs);
        if(sc == 1)
        {
            globus_ftp_client_restart_plugin_set_stall_timeout(
                plugin, to_secs);
        }
    }

    return GLOBUS_SUCCESS;

result_exit:
    globus_ftp_client_plugin_destroy(plugin);
    return result;
}
/* globus_ftp_client_restart_plugin_init() */

/* stalled transfer timer has ended */
static
void
l_ticker_done(
    void *                              user_arg)
{
    globus_free(user_arg);
}

/**
 * Destroy an instance of the GridFTP restart plugin
 * @ingroup globus_ftp_client_restart_plugin
 *
 * This function will free all restart plugin-specific instance data
 * from this plugin, and will make the plugin unusable for further ftp
 * handle creation.
 *
 * Existing FTP client handles and handle attributes will not be affected by
 * destroying a plugin associated with them, as a local copy of the plugin
 * is made upon handle initialization.
 *
 * @param plugin
 *        A pointer to a GridFTP restart plugin, previously initialized by
 *        calling globus_ftp_client_restart_plugin_init()
 *
 * @return This function returns an error if
 * - plugin is null
 * - plugin is not a restart plugin
 *
 * @see globus_ftp_client_restart_plugin_init(),
 *      globus_ftp_client_handleattr_add_plugin(),
 *      globus_ftp_client_handleattr_remove_plugin(),
 *      globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_restart_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin)
{
    globus_l_ftp_client_restart_plugin_t * d;
    globus_result_t result;
    GlobusFuncName(globus_ftp_client_restart_plugin_destroy);

    GLOBUS_L_FTP_CLIENT_RESTART_PLUGIN_RETURN(plugin);

    result = globus_ftp_client_plugin_get_plugin_specific(plugin,
	                                                  (void **) &d);
    if(result != GLOBUS_SUCCESS)
    {
        return result;
    }

    globus_l_ftp_client_restart_plugin_genericify(d);

    if(d->ticker_set)
    {
        d->ticker_set = GLOBUS_FALSE;
        /* XXX where is d freed? if was a previous leak free in l_ticker_done */
        globus_callback_unregister(
            d->ticker_handle, l_ticker_done, d, NULL);
    }
    else
    {
        /* XXX free d here ? */
        globus_free(d);
    }
    return globus_ftp_client_plugin_destroy(plugin);
}
/* globus_ftp_client_restart_plugin_destroy() */

static
void
globus_l_ftp_client_restart_plugin_genericify(
    globus_l_ftp_client_restart_plugin_t *	d)
{
    if(d->source_url)
    {
        globus_libc_free(d->source_url);
	    d->source_url = NULL;
        globus_ftp_client_operationattr_destroy(&d->source_attr);
    }
    if(d->dest_url)
    {
        globus_libc_free(d->dest_url);
        d->dest_url = NULL;
        globus_ftp_client_operationattr_destroy(&d->dest_attr);
    }

    d->operation = GLOBUS_FTP_CLIENT_IDLE;
    d->abort_pending = GLOBUS_FALSE;
}
/* globus_l_ftp_client_restart_plugin_genericify() */
