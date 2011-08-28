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
 * @file globus_ftp_client_debug_plugin.c GridFTP Debugging Plugin Implementation
 *
 * $RCSfile: globus_ftp_client_debug_plugin.c,v $
 * $Revision: 1.19 $
 * $Date: 2007/06/01 17:23:06 $
 */
#endif

#include "globus_ftp_client_debug_plugin.h"

#include <stdio.h>
#include <string.h>
#include "version.h"

#ifndef GLOBUS_DONT_DOCUMENT_INTERNAL
/* Module specific macros */
#define GLOBUS_L_FTP_CLIENT_DEBUG_PLUGIN_NAME "globus_ftp_client_debug_plugin"

#define GLOBUS_L_FTP_CLIENT_DEBUG_PLUGIN_RETURN(plugin) \
    if(plugin == GLOBUS_NULL) \
    {\
	return globus_error_put(globus_error_construct_string(\
		GLOBUS_FTP_CLIENT_MODULE,\
		GLOBUS_NULL,\
		"[%s] NULL plugin at %s\n",\
		GLOBUS_FTP_CLIENT_MODULE->module_name,\
		_globus_func_name));\
    }
#define GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(d, func) \
    result = globus_ftp_client_plugin_set_##func##_func(d, globus_l_ftp_client_debug_plugin_##func); \
    if(result != GLOBUS_SUCCESS) goto result_exit;

/* Module specific data structures */

/**
 * Plugin specific data for the debugging plugin.
 * @internal
 */
typedef struct
{
    /** file stream to log information to */
    FILE *					stream;
    /** user-supplied text prefix to logging information. */
    char *					text;
}
globus_l_ftp_client_debug_plugin_t;


/* Module specific prototypes */
static
globus_ftp_client_plugin_t *
globus_l_ftp_client_debug_plugin_copy(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific);

static
void
globus_l_ftp_client_debug_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific);

static
void
globus_l_ftp_client_debug_plugin_connect(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url);

static
void
globus_l_ftp_client_debug_plugin_authenticate(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_control_auth_info_t *	auth_info);

static
void
globus_l_ftp_client_debug_plugin_chmod(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_cksm(
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
globus_l_ftp_client_debug_plugin_delete(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_feat(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_modification_time(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_mkdir(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_rmdir(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_size(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_verbose_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_machine_list(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_mlst(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_stat(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_move(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_get(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_put(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_third_party_transfer(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t 				restart);

static
void
globus_l_ftp_client_debug_plugin_abort(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle);

static
void
globus_l_ftp_client_debug_plugin_read(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t 				buffer_length);

static
void
globus_l_ftp_client_debug_plugin_write(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t 				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof);

static
void
globus_l_ftp_client_debug_plugin_data(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    const globus_byte_t *			buffer,
    globus_size_t 				length,
    globus_off_t				offset,
    globus_bool_t				eof);

static
void
globus_l_ftp_client_debug_plugin_command(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const char *				command_name);

static
void
globus_l_ftp_client_debug_plugin_response(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error,
    const globus_ftp_control_response_t *	ftp_response);

static
void
globus_l_ftp_client_debug_plugin_fault(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error);

static
void
globus_l_ftp_client_debug_plugin_complete(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle);

static int globus_l_ftp_client_debug_plugin_activate(void);
static int globus_l_ftp_client_debug_plugin_deactivate(void);

/**
 * Module descriptor static initializer.
 */
globus_module_descriptor_t globus_i_ftp_client_debug_plugin_module =
{
    "globus_ftp_client_debug_plugin",
    globus_l_ftp_client_debug_plugin_activate,
    globus_l_ftp_client_debug_plugin_deactivate,
    GLOBUS_NULL,
    GLOBUS_NULL,
    &local_version
};


static
int
globus_l_ftp_client_debug_plugin_activate(void)
{
    return 0;
}

static
int
globus_l_ftp_client_debug_plugin_deactivate(void)
{
    return 0;
}


static
globus_ftp_client_plugin_t *
globus_l_ftp_client_debug_plugin_copy(
    globus_ftp_client_plugin_t *		plugin_template,
    void *					plugin_specific)
{
    globus_ftp_client_plugin_t *		newguy;
    globus_l_ftp_client_debug_plugin_t *	d;
    globus_result_t				result;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    newguy = globus_libc_malloc(sizeof(globus_ftp_client_plugin_t));
    if(newguy == GLOBUS_NULL)
    {
	goto error_exit;
    }
    result = globus_ftp_client_debug_plugin_init(newguy,
						 d->stream,
						 d->text);
    if(result != GLOBUS_SUCCESS)
    {
	goto free_exit;
    }
    return newguy;

free_exit:
    globus_libc_free(newguy);
error_exit:
    return GLOBUS_NULL;
}
/* globus_l_ftp_client_debug_plugin_copy() */

static
void
globus_l_ftp_client_debug_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific)
{
    globus_ftp_client_debug_plugin_destroy(plugin);
    globus_libc_free(plugin);
}
/* globus_l_ftp_client_debug_plugin_destroy() */

static
void
globus_l_ftp_client_debug_plugin_connect(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sconnecting to %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_connect() */

static
void
globus_l_ftp_client_debug_plugin_authenticate(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_control_auth_info_t *	auth_info)
{
    globus_l_ftp_client_debug_plugin_t *	d;
    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sauthenticating with %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_authenticate() */

static
void
globus_l_ftp_client_debug_plugin_chmod(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    int                                         mode,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting chmod %04o %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    mode,
	    url);
}
/* globus_l_ftp_client_debug_plugin_chmod() */

static
void
globus_l_ftp_client_debug_plugin_cksm(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_off_t				offset,
    globus_off_t				length,
    const char *				algorithm,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting %s cksm %s, "
            "offset: %"GLOBUS_OFF_T_FORMAT" length: %"GLOBUS_OFF_T_FORMAT"\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    algorithm,
	    url,
	    offset,
	    length);
}
/* globus_l_ftp_client_debug_plugin_cksm() */


static
void
globus_l_ftp_client_debug_plugin_delete(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to delete %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_delete() */


static
void
globus_l_ftp_client_debug_plugin_feat(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to feat %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_feat() */

static
void
globus_l_ftp_client_debug_plugin_modification_time(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to modification_time %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_modification_time() */

static
void
globus_l_ftp_client_debug_plugin_mkdir(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to mkdir %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_mkdir() */

static
void
globus_l_ftp_client_debug_plugin_rmdir(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to rmdir %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_rmdir() */

static
void
globus_l_ftp_client_debug_plugin_size(
    globus_ftp_client_plugin_t *		plugin,
    void * 					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *        d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
        return;
    }

    fprintf(d->stream, "%s%sstarting to size %s\n",
            d->text ? d->text : "",
            d->text ? ": " : "",
            url);
}

static
void
globus_l_ftp_client_debug_plugin_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to list %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_list() */

static
void
globus_l_ftp_client_debug_plugin_verbose_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to verbose list %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_verbose_list() */

static
void
globus_l_ftp_client_debug_plugin_machine_list(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to machine list %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_machine_list() */

static
void
globus_l_ftp_client_debug_plugin_mlst(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to MLST %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_mlst() */

static
void
globus_l_ftp_client_debug_plugin_stat(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to STAT %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_stat() */

static
void
globus_l_ftp_client_debug_plugin_move(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to move %s to %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    source_url,
	    dest_url);
}
/* globus_l_ftp_client_debug_plugin_move() */

static
void
globus_l_ftp_client_debug_plugin_get(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to get %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_get() */

static
void
globus_l_ftp_client_debug_plugin_put(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const globus_ftp_client_operationattr_t *	attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to put %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url);
}
/* globus_l_ftp_client_debug_plugin_put() */

static
void
globus_l_ftp_client_debug_plugin_third_party_transfer(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				source_url,
    const globus_ftp_client_operationattr_t *	source_attr,
    const char *				dest_url,
    const globus_ftp_client_operationattr_t *	dest_attr,
    globus_bool_t 				restart)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sstarting to transfer %s to %s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    source_url,
	    dest_url);
}
/* globus_l_ftp_client_debug_plugin_third_party_transfer() */

static
void
globus_l_ftp_client_debug_plugin_abort(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%saborting current operation\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "");
}
/* globus_l_ftp_client_debug_plugin_abort() */

static
void
globus_l_ftp_client_debug_plugin_read(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t 				buffer_length)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sreading into data buffer %p, maximum length %ld\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    buffer,
	    (long) buffer_length);
}
/* globus_l_ftp_client_debug_plugin_read() */

static
void
globus_l_ftp_client_debug_plugin_write(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const globus_byte_t *			buffer,
    globus_size_t 				buffer_length,
    globus_off_t				offset,
    globus_bool_t				eof)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%swriting buffer %p, length %ld, "
	           "offset=%"GLOBUS_OFF_T_FORMAT", eof=%s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    buffer,
	    (long) buffer_length,
	    offset,
	    eof ? "true" : "false");
}
/* globus_l_ftp_client_debug_plugin_write() */

static
void
globus_l_ftp_client_debug_plugin_data(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    globus_object_t *				error,
    const globus_byte_t *			buffer,
    globus_size_t 				length,
    globus_off_t				offset,
    globus_bool_t				eof)
{
    globus_l_ftp_client_debug_plugin_t *	d;
    char * error_str;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(error)
    {
        error_str = globus_object_printable_to_string(error);
    }
    else
    {
	error_str = GLOBUS_NULL;
    }

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%sdata callback, %serror%s%s, buffer %p, length %ld, "
	           "offset=%"GLOBUS_OFF_T_FORMAT", eof=%s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    error_str ? "" : "no ",
	    error_str ? " " : "",
	    error_str ? error_str : "",
	    buffer,
	    (long) length,
	    offset,
	    eof ? "true" : "false");
    if(error_str)
    {
	globus_libc_free(error_str);
    }
}
/* globus_l_ftp_client_debug_plugin_data() */

static
void
globus_l_ftp_client_debug_plugin_command(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    const char *				command_name)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%ssending command to %s:\n%s\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "",
	    url,
	    command_name);
}
/* globus_l_ftp_client_debug_plugin_command() */

static
void
globus_l_ftp_client_debug_plugin_response(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error,
    const globus_ftp_control_response_t *	ftp_response)
{
    globus_l_ftp_client_debug_plugin_t *	d;
    char * error_str;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    if(!error)
    {
	fprintf(d->stream, "%s%sresponse from %s:\n%s\n",
		d->text ? d->text : "",
		d->text ? ": " : "",
		url,
		ftp_response->response_buffer);
    }
    else
    {
	error_str = globus_object_printable_to_string(error);

	fprintf(d->stream, "%s%serror reading response from %s: %s\n",
		d->text ? d->text : "",
		d->text ? ": " : "",
		url,
		error_str);

	globus_libc_free(error_str);
    }
}
/* globus_l_ftp_client_debug_plugin_response() */

static
void
globus_l_ftp_client_debug_plugin_fault(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle,
    const char *				url,
    globus_object_t *				error)
{
    globus_l_ftp_client_debug_plugin_t *	d;
    char * error_str;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    if(!error)
    {
	fprintf(d->stream, "%s%sfault on connection to %s\n",
		d->text ? d->text : "",
		d->text ? ": " : "",
		url);
    }
    else
    {
	error_str = globus_object_printable_to_string(error);

	fprintf(d->stream, "%s%sfault on connection to %s: %s\n",
		d->text ? d->text : "",
		d->text ? ": " : "",
		url,
		error_str);

	globus_libc_free(error_str);
    }
}
/* globus_l_ftp_client_debug_plugin_fault() */

static
void
globus_l_ftp_client_debug_plugin_complete(
    globus_ftp_client_plugin_t *		plugin,
    void *					plugin_specific,
    globus_ftp_client_handle_t *		handle)
{
    globus_l_ftp_client_debug_plugin_t *	d;

    d = (globus_l_ftp_client_debug_plugin_t *) plugin_specific;

    if(!d->stream)
    {
	return;
    }

    fprintf(d->stream, "%s%soperation complete\n",
	    d->text ? d->text : "",
	    d->text ? ": " : "");
}
/* globus_l_ftp_client_debug_plugin_complete() */
#endif

/**
 * Initialize an instance of the GridFTP debugging plugin
 * @ingroup globus_ftp_client_debug_plugin
 *
 * This function will initialize the debugging plugin-specific instance data
 * for this plugin, and will make the plugin usable for ftp
 * client handle attribute and handle creation.
 *
 * @param plugin
 *        A pointer to an uninitialized plugin. The plugin will be
 *        configured as a debugging plugin, with the default of sending
 *        debugging messages to stderr.
 *
 * @return This function returns an error if
 * - plugin is null
 *
 * @see globus_ftp_client_debug_plugin_destroy(),
 *      globus_ftp_client_handleattr_add_plugin(),
 *      globus_ftp_client_handleattr_remove_plugin(),
 *      globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_debug_plugin_init(
    globus_ftp_client_plugin_t *		plugin,
    FILE *					stream,
    const char *				text)
{
    globus_l_ftp_client_debug_plugin_t *	d;
    globus_result_t				result;
    GlobusFuncName(globus_ftp_client_debug_plugin_init);

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
	globus_libc_malloc(sizeof(globus_l_ftp_client_debug_plugin_t));

    if(! d)
    {
	return globus_error_put(globus_error_construct_string(
		                GLOBUS_FTP_CLIENT_MODULE,
				GLOBUS_NULL,
				"[%s] Out of memory at %s\n",
				 GLOBUS_FTP_CLIENT_MODULE->module_name,
				 _globus_func_name));
    }

    d->stream = stream;
    d->text = globus_libc_strdup(text);

    result = globus_ftp_client_plugin_init(plugin,
				  GLOBUS_L_FTP_CLIENT_DEBUG_PLUGIN_NAME,
				  GLOBUS_FTP_CLIENT_CMD_MASK_ALL,
				  d);
    if(result != GLOBUS_SUCCESS)
    {
	globus_libc_free(d);

	return result;
    }

    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, copy);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, destroy);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, chmod);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, cksm);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, delete);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, feat);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, modification_time);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, mkdir);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, rmdir);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, size);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, move);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, verbose_list);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, machine_list);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, mlst);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, stat);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, list);    
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, get);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, put);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, third_party_transfer);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, abort);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, connect);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, authenticate);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, read);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, write);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, data);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, command);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, response);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, fault);
    GLOBUS_FTP_CLIENT_DEBUG_PLUGIN_SET_FUNC(plugin, complete);

    return GLOBUS_SUCCESS;

result_exit:
    globus_ftp_client_plugin_destroy(plugin);
    return result;
}
/* globus_ftp_client_debug_plugin_init() */

/**
 * Destroy an instance of the GridFTP debugging plugin
 * @ingroup globus_ftp_client_debug_plugin
 *
 * This function will free all debugging plugin-specific instance data
 * from this plugin, and will make the plugin unusable for further ftp
 * handle creation.
 *
 * Existing FTP client handles and handle attributes will not be affected by
 * destroying a plugin associated with them, as a local copy of the plugin
 * is made upon handle initialization.
 *
 * @param plugin
 *        A pointer to a GridFTP debugging plugin, previously initialized by
 *        calling globus_ftp_client_debug_plugin_init()
 *
 * @return This function returns an error if
 * - plugin is null
 * - plugin is not a debugging plugin
 *
 * @see globus_ftp_client_debug_plugin_init(),
 *      globus_ftp_client_handleattr_add_plugin(),
 *      globus_ftp_client_handleattr_remove_plugin(),
 *      globus_ftp_client_handle_init()
 */
globus_result_t
globus_ftp_client_debug_plugin_destroy(
    globus_ftp_client_plugin_t *		plugin)
{
    globus_l_ftp_client_debug_plugin_t * d;
    globus_result_t result;
    GlobusFuncName(globus_ftp_client_debug_plugin_destroy);

    GLOBUS_L_FTP_CLIENT_DEBUG_PLUGIN_RETURN(plugin);

    result = globus_ftp_client_plugin_get_plugin_specific(plugin,
	                                                  (void **) &d);
    if(result != GLOBUS_SUCCESS)
    {
	return result;
    }

    if(d->text)
    {
	globus_libc_free(d->text);
    }
    globus_libc_free(d);

    return globus_ftp_client_plugin_destroy(plugin);
}
/* globus_ftp_client_debug_plugin_destroy() */
