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


#ifndef _GSSAPI_OPENSSL_H
#define _GSSAPI_OPENSSL_H

#include "gssapi.h"

#include "globus_common.h"
#include "globus_gsi_callback.h"
#include "globus_gsi_proxy.h"
#include "globus_gsi_credential.h"

#include <stdio.h>
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"

/* After a successful GSI authentication, we need to extract our peer's
 * full certificate, but the public GSI interface doesn't provide a way
 * to do this. So we have to duplicate the following internal struct
 * definitions from the GSI source so that we can grab the data ourselves.
 * Fortunately, the layout of these structs has been very stable.
 */

typedef struct gss_name_desc_struct {
    /* gss_buffer_desc  name_buffer ; */
    gss_OID                             name_oid;

    X509_NAME *                         x509n;
    char *                              x509n_oneline;
    GENERAL_NAMES *                     subjectAltNames;
    char *                              user_name;
    char *                              service_name;
    char *                              host_name;
    char *                              ip_address;
    char *                              ip_name;
} gss_name_desc;

typedef struct gss_cred_id_desc_struct {
    globus_gsi_cred_handle_t            cred_handle;
    gss_name_desc *                     globusid;
    gss_cred_usage_t                    cred_usage;
    SSL_CTX *                           ssl_context;
    gss_OID                             mech;
} gss_cred_id_desc;

typedef struct gss_ctx_id_desc_struct{
    globus_mutex_t                      mutex;
    globus_gsi_callback_data_t          callback_data;
    gss_cred_id_desc *                  peer_cred_handle;
    gss_cred_id_desc *                  cred_handle;
    gss_cred_id_desc *                  deleg_cred_handle;
    globus_gsi_proxy_handle_t           proxy_handle;
    OM_uint32                           ret_flags;
    OM_uint32                           req_flags;
    OM_uint32                           ctx_flags;
    int                                 cred_obtained;
#if OPENSSL_VERSION_NUMBER >= 0x10000100L
    /** For GCM ciphers, sequence number of next read MAC token */
    uint64_t                            mac_read_sequence;
    /** For GCM ciphers, sequence number of next write MAC token */
    uint64_t                            mac_write_sequence;
    /** For GCM ciphers, key for MAC token generation/validation */
    unsigned char *                     mac_key;
    /**
     * For GCM ciphers, fixed part of the IV for MAC token
     * generation/validation
     */
    unsigned char *                     mac_iv_fixed;
#endif
    SSL *                               gss_ssl; 
    BIO *                               gss_rbio;
    BIO *                               gss_wbio;
    BIO *                               gss_sslbio;
    int			                        gss_state;
    int                                 locally_initiated;
    int					                delegation_state;
    gss_OID_set                         extension_oids;
    gss_cred_id_t                      *sni_credentials;
    size_t                              sni_credentials_count;
    char                               *sni_servername;
    unsigned char                      *alpn;
    size_t                              alpn_length;
} gss_ctx_id_desc;

#endif /* _GSSAPI_OPENSSL_H */
