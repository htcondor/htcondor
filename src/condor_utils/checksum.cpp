/***************************************************************
 *
 * Copyright (C) 2022, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_debug.h"
#include "condor_system.h"

#include <string>
#include <map>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <ssl_version_hack.h>

#include "safe_open.h"
#include "AWSv4-impl.h"


bool
compute_sha256_checksum( int fd, std::string & checksum ) {
    const size_t BUF_SIZ = 1024 * 1024;
    unsigned char * buffer = (unsigned char *)calloc(BUF_SIZ, 1);
    ASSERT( buffer != NULL );

    EVP_MD_CTX * context = condor_EVP_MD_CTX_new();
    if( context == NULL ) { free(buffer) ; return false; }
    if(! EVP_DigestInit_ex( context, EVP_sha256(), NULL )) {
        condor_EVP_MD_CTX_free( context );
        free(buffer);
        return false;
    }

    ssize_t bytesTotal = 0;
    // FIXME: This doesn't handle EAGAIN, but really should.  Look at
    // full_read(), instead.
    ssize_t bytesRead = read( fd, buffer, BUF_SIZ );
    while( bytesRead > 0 ) {
        EVP_DigestUpdate( context, buffer, bytesRead );
        memset( buffer, 0, BUF_SIZ );
        bytesTotal += bytesRead;
        bytesRead = read( fd, buffer, BUF_SIZ );
    }
    free(buffer); buffer = NULL;

    unsigned char hash[SHA256_DIGEST_LENGTH];
    memset( hash, 0, sizeof(hash) );
    if(! EVP_DigestFinal_ex( context, hash, NULL )) {
        condor_EVP_MD_CTX_free( context );
        return false;
    }
    condor_EVP_MD_CTX_free( context );

    if( bytesRead == -1 ) { return false; }

    AWSv4Impl::convertMessageDigestToLowercaseHex(
        hash, SHA256_DIGEST_LENGTH, checksum
    );
    return true;
}

bool
compute_file_sha256_checksum( const std::string & file_name, std::string & checksum ) {
    int fd = safe_open_wrapper_follow( file_name.c_str(), O_RDONLY | O_LARGEFILE | _O_BINARY, 0 );
    if( fd < 0 ) { return false; }
    bool rv = compute_sha256_checksum( fd, checksum );
    close(fd); fd = -1;
    return rv;
}
