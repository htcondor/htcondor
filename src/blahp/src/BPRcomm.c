/*
#  File:     BPRcomm.c
#
#
#  Copyright (c) Members of the EGEE Collaboration. 2007-2010. 
#
#    See http://www.eu-egee.org/partners/ for details on the copyright
#    holders.  
#  
#    Licensed under the Apache License, Version 2.0 (the "License"); 
#    you may not use this file except in compliance with the License. 
#    You may obtain a copy of the License at 
#  
#        http://www.apache.org/licenses/LICENSE-2.0 
#  
#    Unless required by applicable law or agreed to in writing, software 
#    distributed under the License is distributed on an "AS IS" BASIS, 
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
#    See the License for the specific language governing permissions and 
#    limitations under the License.
#
#
*/
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <globus_gss_assist.h>
#include "tokens.h"
#include "BPRcomm.h"

#define CHECK_GLOBUS_CALL(error_str, error_code, token_status) \
	if (major_status != GSS_S_COMPLETE) \
	{ \
		globus_gss_assist_display_status( \
				stderr, \
				error_str, \
				major_status, \
				minor_status, \
				token_status); \
		return (error_code); \
	}

int
delegate_proxy(const char *proxy_file, gss_cred_id_t cred_handle, gss_ctx_id_t gss_context, int sck)
{
	int return_status = BPR_DELEGATE_PROXY_ERROR;
	int send_status;
	gss_buffer_desc  input_token, output_token;
	gss_buffer_desc  rcv_token, snd_token;
	OM_uint32        del_maj_stat, maj_stat, min_stat;
	int conf_req_flag = 1; /* Non zero value to request confidentiality */

	if (gss_context != GSS_C_NO_CONTEXT)
	{
		/* Bootstrap call */
		del_maj_stat = gss_init_delegation(&min_stat,
					gss_context,
				        cred_handle,
					GSS_C_NO_OID,
					GSS_C_NO_OID_SET,
					GSS_C_NO_BUFFER_SET,
					GSS_C_NO_BUFFER,
					GSS_C_GLOBUS_SSL_COMPATIBLE,
					0,
					&snd_token);

		while (1) /* Will break after the last token has been sent */
		{

			maj_stat = gss_wrap(
					&min_stat,
					gss_context,
					conf_req_flag,
					GSS_C_QOP_DEFAULT,
					&snd_token,
					NULL,
					&output_token);
			gss_release_buffer(&min_stat, &snd_token);

			if (!GSS_ERROR(maj_stat))
			{
				send_status = send_token((void*)&sck, output_token.value, output_token.length);
				gss_release_buffer(&min_stat, &output_token);
				if (send_status < 0)
				{
					break;
				}
			}
			else 
			{
				break;
			}
			if (del_maj_stat != GSS_S_CONTINUE_NEEDED) break;

			if (get_token(&sck, &input_token.value, &input_token.length) < 0) break;
			maj_stat = gss_unwrap(
					&min_stat,
					gss_context,
					&input_token,
					&rcv_token,
					NULL,
					NULL);
	
			gss_release_buffer(&min_stat, &input_token);

			del_maj_stat = gss_init_delegation(&min_stat,
						gss_context,
					        cred_handle,
						GSS_C_NO_OID,
						GSS_C_NO_OID_SET,
						GSS_C_NO_BUFFER_SET,
						&rcv_token,
						GSS_C_GLOBUS_SSL_COMPATIBLE,
						0,
						&snd_token);
			gss_release_buffer(&min_stat, &rcv_token);
		}
	}

	if (del_maj_stat == GSS_S_COMPLETE) return_status = BPR_DELEGATE_PROXY_OK;

	return return_status;

}

int 
receive_delegated_proxy(char **s, gss_ctx_id_t gss_context, int sck)
{
	char             *buf;
	int              return_status = BPR_RECEIVE_DELEGATED_PROXY_ERROR;
	int              send_status;
	gss_buffer_desc  input_token,output_token;
	gss_buffer_desc  snd_token, rcv_token;
	gss_buffer_desc  cred_token;
	gss_cred_id_t    delegated_cred = GSS_C_NO_CREDENTIAL;
	OM_uint32        del_maj_stat, maj_stat, min_stat, time_rec;
	int conf_req_flag = 1; /* Non zero value to request confidentiality */

	del_maj_stat = GSS_S_CONTINUE_NEEDED;

	if (gss_context != GSS_C_NO_CONTEXT) 
	{
		while (del_maj_stat == GSS_S_CONTINUE_NEEDED)
		{
                        /* get_token can return -1 on error or GLOBUS_GSS_ASSIST_TOKEN_EOF==3 */
			if (get_token(&sck, &input_token.value, &input_token.length) != 0) break;
			maj_stat = gss_unwrap(
					&min_stat,
					gss_context,
					&input_token,
					&rcv_token,
					NULL,
					NULL);
	
			gss_release_buffer(&min_stat, &input_token);
	
			if (!GSS_ERROR(maj_stat))
			{
				del_maj_stat=gss_accept_delegation(&min_stat,
					gss_context,
					GSS_C_NO_OID_SET,
					GSS_C_NO_BUFFER_SET,
					&rcv_token,
					GSS_C_GLOBUS_SSL_COMPATIBLE,
					0,
					&time_rec,
					&delegated_cred,
					NULL,
					&snd_token);
			}
			else break;
	
			gss_release_buffer(&min_stat, &rcv_token);

			if (del_maj_stat != GSS_S_CONTINUE_NEEDED) break;

			maj_stat = gss_wrap(
					&min_stat,
					gss_context,
					conf_req_flag,
					GSS_C_QOP_DEFAULT,
					&snd_token,
					NULL,
					&output_token);
			gss_release_buffer(&min_stat, &snd_token);

			if (!GSS_ERROR(maj_stat))
			{
				send_status = send_token((void*)&sck, output_token.value, output_token.length);
				gss_release_buffer(&min_stat, &output_token);
				if (send_status < 0)
				{
					break;
				}
			}
			else 
			{
				break;
			}
		}
	}
	if (del_maj_stat == GSS_S_COMPLETE) 
	{
		/* Export credential to a string */
		maj_stat = gss_export_cred(&min_stat, 
					delegated_cred, 
					GSS_C_NO_OID,
					0,
					&cred_token);
		if (maj_stat == GSS_S_COMPLETE) {
			
			if (s != NULL) 
			{
				*s = (char *)malloc(cred_token.length + 1);
				if (*s != NULL)
				{
					memcpy(*s, cred_token.value, cred_token.length);
					(*s)[cred_token.length]='\000';
					return_status = BPR_RECEIVE_DELEGATED_PROXY_OK;
				}
			}
		}
		gss_release_buffer(&min_stat, &cred_token);
	}
	return return_status;
}

int
send_proxy(const char *s, gss_ctx_id_t gss_context, int sck)
{
	int return_status = BPR_SEND_PROXY_ERROR;
	gss_buffer_desc  input_token;
	gss_buffer_desc  output_token;
	OM_uint32        maj_stat, min_stat;
	int conf_req_flag = 1; /* Non zero value to request confidentiality */

	if (gss_context != GSS_C_NO_CONTEXT)
	{
		input_token.value = (void*)s;
		input_token.length = strlen(s) + 1; 
					        
		maj_stat = gss_wrap(
				&min_stat,
				gss_context,
				conf_req_flag,
				GSS_C_QOP_DEFAULT,
				&input_token,
				NULL,
				&output_token);

		if (!GSS_ERROR(maj_stat))
		{
			if (send_token((void*)&sck, output_token.value, output_token.length) >= 0)
				return_status = BPR_SEND_PROXY_OK;
		}
						        
		gss_release_buffer(&min_stat, &output_token);
	}
	return return_status;
}

int
receive_proxy(char **s, gss_ctx_id_t gss_context, int sck)
{
	char             *buf;
	int              return_status = BPR_RECEIVE_PROXY_ERROR;
	gss_buffer_desc  input_token;
	gss_buffer_desc  output_token;
	OM_uint32        maj_stat, min_stat;

	if (!(gss_context == GSS_C_NO_CONTEXT || get_token(&sck, &input_token.value, &input_token.length) != 0)) 
	{
		maj_stat = gss_unwrap(
				&min_stat,
				gss_context,
				&input_token,
				&output_token,
				NULL,
				NULL);

		if (!GSS_ERROR(maj_stat))
		{
			if ((buf = (char *)malloc(output_token.length + 1)) == NULL)
			{
				fprintf(stderr, "Error allocating buffer...\n");
				return(return_status);
			}
			memcpy(buf, output_token.value, output_token.length);
			buf[output_token.length] = 0;
			*s = buf;
			return_status = BPR_RECEIVE_PROXY_OK;
		}
		gss_release_buffer(&min_stat, &output_token);
		gss_release_buffer(&min_stat, &input_token);
	}
	return return_status;
}


OM_uint32
get_cred_lifetime(const gss_cred_id_t credential_handle)
{
	OM_uint32        major_status = 0;
	OM_uint32        minor_status = 0;
	gss_name_t       name = NULL;
	OM_uint32        lifetime;
	gss_OID_set      mechanisms;
	gss_cred_usage_t cred_usage;

	major_status = gss_inquire_cred(
			&minor_status,
			credential_handle,
			&name,
			&lifetime,
			&cred_usage,
			&mechanisms);

	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(
				stderr,
				"Error acquiring credentials",
				major_status,
				minor_status,
				0);
		return(-1);
	}
	return(lifetime);
			
}

gss_cred_id_t
acquire_cred(const gss_cred_usage_t cred_usage)
{
	OM_uint32       major_status = 0;
	OM_uint32       minor_status = 0;
	gss_cred_id_t   credential_handle = GSS_C_NO_CREDENTIAL;

	/* Acquire GSS credential */
	major_status = globus_gss_assist_acquire_cred(
			&minor_status,
			cred_usage,
			&credential_handle);

	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(
				stderr,
				"Error acquiring credentials",
				major_status,
				minor_status,
				0);
		return(GSS_C_NO_CREDENTIAL);
	}
	return(credential_handle);
}


gss_ctx_id_t initiate_context(gss_cred_id_t credential_handle, const char *server_name, int sck)
{
	OM_uint32	major_status = 0;
	OM_uint32	minor_status = 0;
	int		token_status = 0;
	OM_uint32	ret_flags = 0;
	gss_ctx_id_t	context_handle = GSS_C_NO_CONTEXT;

	major_status = globus_gss_assist_init_sec_context(
			&minor_status,
      			credential_handle,
			&context_handle,
			(char *) server_name,
			GSS_C_MUTUAL_FLAG | GSS_C_CONF_FLAG |
			GSS_C_GLOBUS_ACCEPT_PROXY_SIGNED_BY_LIMITED_PROXY_FLAG,
			&ret_flags,
			&token_status,
			get_token,
			(void *) &sck,
			send_token,
			(void *) &sck);

	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(stderr,
				"GSS Authentication failure: client\n ",
				major_status,
				minor_status,
				token_status);
		return(GSS_C_NO_CONTEXT); /* fail somehow */
	}
	return(context_handle);
}

gss_ctx_id_t accept_context(gss_cred_id_t credential_handle, char **client_name, int sck)
{
	OM_uint32       major_status = 0;
	OM_uint32       minor_status = 0;
	int             token_status = 0;
	OM_uint32       ret_flags = 0;
	gss_ctx_id_t    context_handle = GSS_C_NO_CONTEXT;
	gss_cred_id_t   delegated_cred = GSS_C_NO_CREDENTIAL;
		                        
	major_status = globus_gss_assist_accept_sec_context(
		&minor_status, /* minor_status */
		&context_handle, /* context_handle */
		credential_handle, /* acceptor_cred_handle */
		client_name, /* src_name as char ** */
		&ret_flags, /* ret_flags */
		NULL, /* don't need user_to_user */
		&token_status, /* token_status */
		&delegated_cred, /* no delegated cred */
		get_token,
		(void *) &sck,
		send_token,
		(void *) &sck);
                                                                                                                                                                                    
	if (major_status != GSS_S_COMPLETE)
	{
		globus_gss_assist_display_status(
				stderr,
				"GSS authentication failure ",
				major_status,
				minor_status,
				token_status);
		return (GSS_C_NO_CONTEXT);
	}
	return (context_handle);
}


int verify_context(gss_ctx_id_t context_handle)
{
	OM_uint32       major_status = 0;
	OM_uint32       minor_status = 0;
	OM_uint32       ret_flags = 0;
	gss_name_t	target_name = GSS_C_NO_NAME;
	gss_name_t	src_name = GSS_C_NO_NAME;
	gss_buffer_desc	name_buffer  = GSS_C_EMPTY_BUFFER;

	char	*target_name_str = NULL;
	char	*src_name_str = NULL;
        int	DN_cleaned = 0;
	int	CNoffset, i;

        const char *ignored_CNs[] = {", CN=proxy", ", CN=limited proxy"};

	major_status = gss_inquire_context(
		&minor_status, /* minor_status */
		context_handle, /* context_handle */
		&src_name,  /* The client principal name */
		&target_name, /* The server principal name */
		NULL, /* don't need user_to_user */
		NULL, /* don't need user_to_user */
		NULL, /* don't need user_to_user */
		NULL, /* don't need user_to_user */
		NULL  /* don't need user_to_user */
		);
	CHECK_GLOBUS_CALL("GSS context inquire failure ", 1, major_status);

	/* Get the server principal name */
	major_status = gss_display_name(
		&minor_status,
		target_name, 
		&name_buffer,
		NULL);
	CHECK_GLOBUS_CALL("GSS display_name failure ", 1, major_status);
	if ((target_name_str = (char *)malloc((name_buffer.length + 1) * sizeof(char))) == NULL)
	{
		fprintf(stderr, "verify_context(): Out of memory\n");
		return(1);
	}
	memcpy(target_name_str, name_buffer.value, name_buffer.length);
	target_name_str[name_buffer.length] = '\0';
	major_status = gss_release_name(&minor_status, &target_name);
	major_status = gss_release_buffer(&minor_status, &name_buffer);

	/* Get the client principal name */
	major_status = gss_display_name(
		&minor_status,
		src_name, 
		&name_buffer,
		NULL);
	CHECK_GLOBUS_CALL("GSS display_name failure ", 1, major_status);
	if ((src_name_str = (char *)malloc((name_buffer.length + 1) * sizeof(char))) == NULL)
	{
		fprintf(stderr, "verify_context(): Out of memory\n");
		return(1);
	}
	memcpy(src_name_str, name_buffer.value, name_buffer.length);
	src_name_str[name_buffer.length] = '\0';
	major_status = gss_release_name(&minor_status, &target_name);
	major_status = gss_release_buffer(&minor_status, &name_buffer);

	/* Strip trailing (limited) proxy CNs */
	/* This is only needed (and effective) with very old */
	/* versions of Globus. More recent versions return the */
        /* pruned 'globusid' base name, making this stripping */
        /* both useless and harmless. */
        while (!DN_cleaned)
	{
		DN_cleaned = 1;
		for (i = 0; i < sizeof(ignored_CNs)/sizeof(char *); i++)
		{
			CNoffset = strlen(src_name_str) - strlen(ignored_CNs[i]);
			if (strcmp(src_name_str + CNoffset, ignored_CNs[i]) == 0)
			{
				src_name_str[CNoffset] = '\0';
				DN_cleaned = 0;
			}
		}
	}
        DN_cleaned = 0;
        while (!DN_cleaned)
	{
		DN_cleaned = 1;
		for (i = 0; i < sizeof(ignored_CNs)/sizeof(char *); i++)
		{
			CNoffset = strlen(target_name_str) - strlen(ignored_CNs[i]);
			if (strcmp(target_name_str + CNoffset, ignored_CNs[i]) == 0)
			{
				target_name_str[CNoffset] = '\0';
				DN_cleaned = 0;
			}
		}
	}


	return (strcmp(src_name_str, target_name_str));
}
