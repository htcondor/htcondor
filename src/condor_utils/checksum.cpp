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

#include <string>

#include "safe_open.h"

#include <openssl/sha.h>

//
// The functions hex_digit() and encode_hex() were copied from
// condor_had/Utils.cpp.  compute[_file]_checksum() were adapted from the
// same source, but don't include the deliberate text-mode bug.
//

// encode an unsigned character array as a null terminated hex string
// the caller must supply an input buffer that is at least 2*datalen+1 in size
static char hex_digit(unsigned char n) { return n + ((n < 10) ? '0' : ('a' - 10)); }
static const char * encode_hex(char * buf, int bufsiz, const unsigned char * data, int datalen)
{
    if (!buf || bufsiz < 3) return "";
    const unsigned char * d = data;
    char * p = buf;
    char * endp = buf + bufsiz - 2; // we advance by 2 each loop iteration
    while (datalen-- > 0) {
        unsigned char ch = *d++;
        *p++ = hex_digit((ch >> 4) & 0xF);
        *p++ = hex_digit(ch & 0xF);
        if (p >= endp) break;
    }
    *p = 0;
    return buf;
}

bool
compute_checksum( int fd, std::string & checksum ) {
    const size_t BUF_SIZ = 1024 * 1024;
    unsigned char * buffer = (unsigned char *)calloc(BUF_SIZ, 1);
    ASSERT( buffer != NULL );

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX context;
    SHA256_Init( &context );

    ssize_t bytesTotal = 0;
    ssize_t bytesRead = read( fd, buffer, BUF_SIZ );
    while( bytesRead > 0 ) {
        SHA256_Update( &context, buffer, bytesRead );
        memset( buffer, 0, BUF_SIZ );
        bytesTotal += bytesRead;
        bytesRead = read( fd, buffer, BUF_SIZ );
    }
    free(buffer); buffer = NULL;

    memset( hash, 0, sizeof(hash) );
    SHA256_Final( hash, &context );

    char file_hash[SHA256_DIGEST_LENGTH * 2 + 4];
    encode_hex( file_hash, sizeof(file_hash), hash, sizeof(hash) );
    checksum.assign( file_hash, sizeof(file_hash) );

    if( bytesRead == -1 ) { return false; }
    return true;
}

bool
compute_file_checksum( const std::string & file_name, std::string & checksum ) {
    // Do we need to do anything on Windows to make sure this is binary mode?
    int fd = safe_open_wrapper_follow( file_name.c_str(), O_RDONLY | O_LARGEFILE, 0 );
    if( fd < 0 ) { return false; }
    bool rv = compute_checksum( fd, checksum );
    close(fd); fd = -1;
    return rv;
}
