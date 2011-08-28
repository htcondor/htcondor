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


#include "globus_common.h"
#include "globus_ftp_client.h"

typedef struct
{
    int                                 code;
    char *                              message;
} globus_l_error_ftp_data_t;


static
void
globus_l_error_ftp_copy(
    void *                              src,
    void **                             dst)
{
    globus_l_error_ftp_data_t *         copy;
    globus_l_error_ftp_data_t *         source;
    
    if(src && dst)
    {
        copy = (globus_l_error_ftp_data_t *)
            globus_malloc(sizeof(globus_l_error_ftp_data_t));
        if(copy)
        {
            source = (globus_l_error_ftp_data_t *) src;
            
            copy->code = source->code;
            copy->message = source->message 
                ? globus_libc_strdup(source->message) : NULL;
        }
        
        *dst = copy;
    }
}

static
void
globus_l_error_ftp_free(
    void *                              data)
{
    globus_l_error_ftp_data_t *         d;
    
    d = (globus_l_error_ftp_data_t *) data;
    
    if(d->message)
    {
        globus_free(d->message);
    }
    globus_free(d);
}

static
char *
globus_l_error_ftp_printable(
    globus_object_t *                   error)
{
    globus_l_error_ftp_data_t *         data;
    char *                              error_string;
    char                                buf[4];
    char *                              message;
    int                                 message_len;
    
    if(!error)
    {
        return NULL;
    }

    data = (globus_l_error_ftp_data_t *)
        globus_object_get_local_instance_data(error);
    message = data->message;
    
    if(message)
    {
        message_len = strlen(message);
        if(message_len > 3 && message[3] == ' ')
        {
            buf[0] = message[0];
            buf[1] = message[1];
            buf[2] = message[2];
            buf[3] = '\0';
            
            if(data->code == atoi(buf))
            {
                message += 4;
                message_len -= 4;
            }
        }
    }
    else
    {
        message_len = 0;
    }
    
    error_string = globus_malloc(sizeof(char) * (15 + message_len));
    if(error_string)
    {
        sprintf(
            error_string,
            "%d %s",
            data->code,
            message ? message : "");
    }
    
    return error_string;
}

int
globus_error_ftp_error_get_code(
    globus_object_t *                   error)
{
    int                                 code = 0;
    globus_l_error_ftp_data_t *         data;
    
    do
    {
        if(globus_object_get_type(error) == GLOBUS_ERROR_TYPE_FTP)
        {
            data = (globus_l_error_ftp_data_t *)
                globus_object_get_local_instance_data(error);
            code = data->code;
        }
    } while(code == 0 && (error = globus_error_get_cause(error)));
    
    return code;
}
    
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
    ...)
{
    va_list                             ap;
    globus_object_t *                   error;
    globus_object_t *                   causal_error;
    globus_l_error_ftp_data_t *         data;

    causal_error = globus_object_construct(GLOBUS_ERROR_TYPE_FTP);
    if(!causal_error)
    {
        goto error_object;
    }
    
    data = (globus_l_error_ftp_data_t *)
        globus_malloc(sizeof(globus_l_error_ftp_data_t));
    if(!data)
    {
        goto error_data;
    }
    
    data->code = code;
    data->message = globus_libc_strdup(message);
    globus_object_set_local_instance_data(causal_error, data);
    globus_error_initialize_base(causal_error, base_source, NULL);

    va_start(ap, format);

    error = globus_error_v_construct_error(
        base_source,
        causal_error,
        error_type,
        source_file,
        source_func,
        source_line,
        format,
        ap);

    va_end(ap);

    if(!error)
    {
        goto error_construct;
    }

    return error;

error_construct:
error_data:
    globus_object_free(causal_error);

error_object:
    return NULL;
}

const globus_object_type_t GLOBUS_ERROR_TYPE_FTP_DEFINITION = 
globus_error_type_static_initializer(
    GLOBUS_ERROR_TYPE_BASE,
    globus_l_error_ftp_copy,
    globus_l_error_ftp_free,
    globus_l_error_ftp_printable);
