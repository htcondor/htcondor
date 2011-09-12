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

#include "globus_i_ftp_client.h"

/*
 *
 * internal interface to 
 * globus_i_ftp_client_features_t 
 *
 */

globus_ftp_client_tristate_t 
globus_i_ftp_client_feature_get(
    globus_i_ftp_client_features_t *    features,
    globus_ftp_client_probed_feature_t  feature) 
{
    if(features)
    {
        return features->list[feature];
    }
    
    return GLOBUS_FTP_CLIENT_MAYBE;
}


void 
globus_i_ftp_client_feature_set(
    globus_i_ftp_client_features_t *    features,
    globus_ftp_client_probed_feature_t  feature,
    globus_ftp_client_tristate_t        value)
{
    features->list[feature]=value;
}

globus_i_ftp_client_features_t *
globus_i_ftp_client_features_init()
{
    int                                 i;
    globus_i_ftp_client_features_t *    features;
    
    features = (globus_i_ftp_client_features_t *)
        globus_malloc(sizeof(globus_i_ftp_client_features_t));
      
    for (i = 0; i < GLOBUS_FTP_CLIENT_FEATURE_MAX; i++)
    {
        globus_i_ftp_client_feature_set(
            features, i, GLOBUS_FTP_CLIENT_MAYBE);
    }
    
    return features;
}

globus_result_t 
globus_i_ftp_client_features_destroy(
    globus_i_ftp_client_features_t *    features)
{
  globus_libc_free(features);
  return GLOBUS_SUCCESS;
}    


/**
 * @name Features
 */
/*@{*/
/**
 * @ingroup globus_ftp_client_operations
 * Initialize the feature set, to be later used by 
 * globus_ftp_client_feat(). Each feature gets initial
 * value GLOBUS_FTP_CLIENT_MAYBE.
 * @note Structure initialized by this function must be
 * destroyed using globus_ftp_client_features_destroy()
 * @return GLOBUS_SUCCESS on success, otherwise error.
 */
globus_result_t 
globus_ftp_client_features_init(
    globus_ftp_client_features_t *      u_features)
{
    globus_i_ftp_client_features_t *    features;
    
    features  = globus_i_ftp_client_features_init();
    *u_features = features;
    if(!features)
    {
        return globus_error_put(GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY());
    }
    
    return GLOBUS_SUCCESS;  
}

/**
 * @ingroup globus_ftp_client_operations
 * Destroy the feature set.
 * @note Structure passed to this function must have been previously
 * initialized by globus_ftp_client_features_init().
 * @return GLOBUS_SUCCESS on success, otherwise error. 
 */
globus_result_t 
globus_ftp_client_features_destroy(
    globus_ftp_client_features_t *      u_features)
{
    return globus_i_ftp_client_features_destroy(*u_features);
}

/**
 * @ingroup globus_ftp_client_operations
 * Check the features supported by  the server (FTP FEAT command).  After
 * this  procedure  completes, the  features  set (parameter  u_features)
 * represents the features supported by the server. Prior to calling this
 * procedure,   the   structure   should   have   been   initialized   by
 * globus_ftp_client_features_init(); afterwards, it should be destroyed 
 * by globus_ftp_client_features_destroy(). After globus_ftp_client_feat()
 * returns,  each   feature  in   the  list  has   one  of   the  values:
 * GLOBUS_FTP_CLIENT_TRUE,           GLOBUS_FTP_CLIENT_FALSE,          or
 * GLOBUS_FTP_CLIENT_MAYBE. The  first two denote  the server supporting,
 * or not supporting, the given feature. The last one means that the test
 * has not  been performed. This is not necessarily caused by error; 
 * there might have been no reason to check for this particular feature.
 *
 * @param u_handle
 *        An FTP Client handle to use for the list operation.
 * @param url
 *        The URL to list. The URL may be an ftp or gsiftp URL.
 * @param attr
 *        Attributes for this file transfer.
 * @param u_features
 *        A pointer to a globus_ftp_client_features_t to be filled
 *        with the feature set supported by the server. 
 * @param complete_callback
 *        Callback to be invoked once the size check is completed.
 * @param callback_arg
 *        Argument to be passed to the complete_callback.
 *
 * @return
 *        This function returns an error when any of these conditions are
 *        true:
 *        - u_handle is GLOBUS_NULL
 *        - source_url is GLOBUS_NULL
 *        - source_url cannot be parsed
 *        - source_url is not a ftp or gsiftp url
 *        - u_features is GLOBUS_NULL or badly initialized
 *        - complete_callback is GLOBUS_NULL
 *        - handle already has an operation in progress
 */
globus_result_t
globus_ftp_client_feat(
    globus_ftp_client_handle_t *                 u_handle,
    char *                                       url,
    globus_ftp_client_operationattr_t *          attr,
    globus_ftp_client_features_t *               u_features,
    globus_ftp_client_complete_callback_t        complete_callback,
    void *                                       callback_arg)
{
    globus_i_ftp_client_handle_t *      handle;
    globus_object_t *                   error;
    globus_result_t                     result;
    globus_bool_t                       registered;
    
    GlobusFuncName(globus_ftp_client_feat);
    
    if(u_handle == GLOBUS_NULL) 
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("u_handle");
        goto error_param;
    }
    if(u_features == GLOBUS_NULL || *u_features == GLOBUS_NULL) 
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("u_features");
        goto error_param;
    }
    if(url == GLOBUS_NULL) 
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("url");
        goto error_param;
    }
    if(complete_callback == GLOBUS_NULL)
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("complete_callback");

        goto error_param;
    }
    if(GLOBUS_I_FTP_CLIENT_BAD_MAGIC(u_handle))
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("u_handle");
        goto error_param;
    }
    
    handle = *u_handle;
    u_handle = handle->handle;
    
    /* add reference to the handle to the shutdown count */
    globus_i_ftp_client_handle_is_active(u_handle);
    
    globus_i_ftp_client_handle_lock(handle);
    if(handle->op != GLOBUS_FTP_CLIENT_IDLE)
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_OBJECT_IN_USE("handle");
        goto unlock_exit;
    }
    
    handle->source_url = globus_libc_strdup(url);
    if(handle->source_url == GLOBUS_NULL)
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_OUT_OF_MEMORY();
        goto unlock_exit;
    }
    
    /* Setup handle for the FEAT*/
    handle->op = GLOBUS_FTP_CLIENT_FEAT;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = complete_callback;
    handle->callback_arg = callback_arg;
    handle->features_pointer = *u_features;
    
    error = globus_i_ftp_client_target_find(
        handle, url, attr ? *attr : GLOBUS_NULL, &handle->source);
    if(error != GLOBUS_SUCCESS)
    {
        goto free_url;
    }
    
    globus_i_ftp_client_plugin_notify_feat(
        handle, handle->source_url, handle->source->attr);
    if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
    {
	error = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
	goto abort;
    }
    else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
    {
	goto restart;
    }
    			   
    if(handle->source->features)
    {
        int                             i;
        
        /* already have all the features, do a oneshot */
        for(i = 0; i < GLOBUS_FTP_CLIENT_FEATURE_MAX; i++)
        {
            globus_i_ftp_client_feature_set(
                handle->features_pointer,
                i,
                globus_i_ftp_client_feature_get(handle->source->features, i));
        }
        
        result = globus_callback_register_oneshot(
            GLOBUS_NULL,
            GLOBUS_NULL,
            globus_l_ftp_client_complete_kickout,
            handle);
        if(result != GLOBUS_SUCCESS)
        {
            error = globus_error_get(result);
            goto source_problem_exit;
        }
    }
    else
    {
        error = globus_i_ftp_client_target_activate(
            handle, handle->source, &registered);
        if(registered == GLOBUS_FALSE)
        {
            /* 
             * A restart or abort happened during activation, before any
             * callbacks were registered. We must deal with them here.
             */
            globus_assert(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT ||
                          handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART ||
                          error != GLOBUS_SUCCESS);
            
            if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_ABORT)
            {
                error = GLOBUS_I_FTP_CLIENT_ERROR_OPERATION_ABORTED();
                goto abort;
            }
            else if(handle->state == GLOBUS_FTP_CLIENT_HANDLE_RESTART)
            {
                goto restart;
            }
            else if(error != GLOBUS_SUCCESS)
            {
                goto source_problem_exit;
            }
        }
    }
    
    globus_i_ftp_client_handle_unlock(handle);
    
    return GLOBUS_SUCCESS;
    
restart:
    globus_i_ftp_client_target_release(handle, handle->source);
    error = globus_i_ftp_client_restart_register_oneshot(handle);
    if(!error)
    {
        globus_i_ftp_client_handle_unlock(handle);
        return GLOBUS_SUCCESS;
    }
    /* else fallthrough */
abort:
source_problem_exit:
    if(handle->source)
    {
        globus_i_ftp_client_target_release(handle, handle->source);
    }

free_url:    
    /* Reset the state of the handle. */
    globus_libc_free(handle->source_url);
    handle->source_url = GLOBUS_NULL;
    handle->op = GLOBUS_FTP_CLIENT_IDLE;
    handle->state = GLOBUS_FTP_CLIENT_HANDLE_START;
    handle->callback = GLOBUS_NULL;
    handle->callback_arg = GLOBUS_NULL;

unlock_exit:    
    globus_i_ftp_client_handle_unlock(handle);
    globus_i_ftp_client_handle_is_not_active(u_handle);

error_param:    
    return globus_error_put(error);
}
/* globus_ftp_client_feat() */

/**
 * @ingroup globus_ftp_client_operations
 * Check if the feature is supported by the server.
 * After the function completes, parameter answer contains
 * the state of the server support of the given function.
 * It can have one of the values: GLOBUS_FTP_CLIENT_TRUE,  
 * GLOBUS_FTP_CLIENT_FALSE, or GLOBUS_FTP_CLIENT_MAYBE. 
 *
 * @param u_features 
 *        list of features, as returned by globus_ftp_client_feat()
 * @param answer
 *        this variable will contain the answer
 * @param feature
 *        feature number, 0 <= feature < GLOBUS_FTP_CLIENT_FEATURE_MAX 
 * @return
 *      error when any of the parameters is null or badly initialized
 */
globus_result_t
globus_ftp_client_is_feature_supported(
    const globus_ftp_client_features_t *        u_features,
    globus_ftp_client_tristate_t *              answer,
    globus_ftp_client_probed_feature_t          feature) 
{
    globus_object_t *                     error;
    
    if(answer == GLOBUS_NULL) 
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("answer");
        goto error_param;
    }
    if (u_features == GLOBUS_NULL || *u_features == GLOBUS_NULL) 
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_NULL_PARAMETER("u_features");
        goto error_param;
    }
    if(feature < 0 || feature >= GLOBUS_FTP_CLIENT_FEATURE_MAX)
    {
        error = GLOBUS_I_FTP_CLIENT_ERROR_INVALID_PARAMETER("feature");
        goto error_param;
    }
    
    *answer = globus_i_ftp_client_feature_get(*u_features, feature);
    
    return GLOBUS_SUCCESS;
    
error_param:
    return globus_error_put(error);
}
/*globus_ftp_client_is_feature_supported()*/
/*@}*/
