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

#include "condor_common.h"
#include "Utils.h"
//#include "HadCommands.h"
//#include "ReplicationCommands.h"
#include "FilesOperations.h"
// for 'CHILD_ON' and 'CHILD_OFF_FAST'
#include "condor_commands.h"
// for 'getHostFromAddr' and 'getPortFromAddr'
#include "internet.h"
// for MD5
#ifdef HAVE_EXT_OPENSSL
#include <openssl/md5.h>
#endif
// for SHA-2 (SHA256)
#include <openssl/sha.h>

#include "condor_netdb.h"
#include "ipv6_hostname.h"

#include <fstream>

using namespace std;

// for MD5 blocks computation, and for backward compat leading 0's
#define MD5_MAC_SIZE   16

extern void main_shutdown_graceful();

std::string
utilNoParameterError( const char* parameter, const char* daemonName )
{
	std::string error;

	if( ! strcasecmp( daemonName, "HAD" ) ) {
		formatstr( error, "HAD configuration error: no %s in config file",
                        parameter );
	} else if( ! strcasecmp( daemonName, "REPLICATION" ) ) {
		formatstr( error, "Replication configuration error: no %s in config file",
                       parameter );
	} else {
		dprintf( D_ALWAYS, "utilNoParameterError no such daemon name %s\n", 
				 daemonName );
	}

	return error;
}

std::string
utilConfigurationError( const char* parameter, const char* daemonName )
{
	std::string error;

    if( ! strcasecmp( daemonName, "HAD" ) ) {
		formatstr(error, "HAD configuration error: %s is not valid in config file",
					   parameter );
	} else if( ! strcasecmp( daemonName, "REPLICATION" ) ) {
		formatstr( error, "Replication configuration error: %s is not valid "
		           "in config file", parameter );
	} else {
        dprintf( D_ALWAYS, "utilConfigurationError no such daemon name %s\n", 
                 daemonName );
    }

    return error;
}

void
utilCrucialError( const char* message )
{
    dprintf( D_ALWAYS, "%s\n", message );
    main_shutdown_graceful( );
}

void
utilCancelTimer(int& timerId)
{
     if (timerId >= 0) {                    
        daemonCore->Cancel_Timer( timerId );
        timerId = -1;
     }
}

void
utilCancelReaper(int& reaperId)
{
    if(reaperId >= 0) {
        daemonCore->Cancel_Reaper( reaperId );
        reaperId = -1;
    }
}

// returns allocated by 'malloc' string upon success or NULL upon failure
// to be deallocated by 'free' function only
char*
utilToSinful( char* address )
{
    int port = getPortFromAddr( address );
    
    if( port == 0 ) {
        return 0;
    }

    // getHostFromAddr returns dynamically allocated buffer
    char* hostName = getHostFromAddr( address );
    char* ipAddress = hostName;
    
    if( hostName == 0) {
      return 0;
    }

	condor_sockaddr addr;
	if (!addr.from_ip_string(ipAddress)) {
		std::vector<condor_sockaddr> addrs = resolve_hostname(hostName);
		if (addrs.empty()) {
			free(hostName);
			return 0;
		}
		free(ipAddress);
		std::string ipaddr_str = addrs.front().to_ip_string();
		ipAddress = strdup(ipaddr_str.c_str());
	}
	std::string sinfulString;

	sinfulString = generate_sinful(ipAddress, port);
    free( ipAddress );
    
    return strdup( sinfulString.c_str( ) );
}

void
utilClearList( StringList& list )
{
    char* element = NULL;
    list.rewind ();

    while ( (element = list.next( )) ) {
        list.deleteCurrent( );
    }
}

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
utilSafePutFile( ReliSock& socket, const std::string& filePath, int fips_mode )
{
	REPLICATION_ASSERT( filePath != "" );
	// bool       successValue = true;
	filesize_t bytes        = 0; // set by socket.put_file
	char       md[MD5_MAC_SIZE]; // holds MD5 digest in non-fips mode, and all zeros in fips mode
	char       file_hash[SHA256_DIGEST_LENGTH * 2 + 4]; // the SHA256 checksum as a hex string, used only in FIPS mode
	dprintf(D_ALWAYS, "utilSafePutFile %s started\n", filePath.c_str());

	// The original MD5 sum was done using a ifstream opened in text mode.  This is wrong
	// but we have to preserve the wrongness for backward compatibility
	const int bin_flag = fips_mode ? _O_BINARY : 0;
	int fd = safe_open_wrapper_follow(filePath.c_str(), O_RDONLY | O_LARGEFILE | bin_flag, 0);
	if (fd < 0) {
		dprintf( D_ALWAYS, "utilSafePutFile failed to open file %s\n", filePath.c_str() ); 
		return false;
	}
	ssize_t bytesTotal = 0;
	ssize_t bytesRead  = 0;

	// MD5 sum. in FIPS mode we just send this as 0's
	memset(md, 0, sizeof(md));
	// SHA256 file hash, as null terminated hex string, used only in FIPS mode
	memset(file_hash, 0, sizeof(file_hash));

	const size_t BUF_SIZ = 1024 * 1024; // read file in 1Mb chunks
	unsigned char *buffer = (unsigned char *)calloc(BUF_SIZ, 1);
	ASSERT(buffer != NULL);

	if (fips_mode) {
		unsigned char hash[SHA256_DIGEST_LENGTH];  // TODO: support other sizes of SHA-2 hash here?

		SHA256_CTX  context;
		SHA256_Init(&context);

		bytesRead = read(fd, buffer, BUF_SIZ);
		while (bytesRead > 0) {
			SHA256_Update(&context, buffer, bytesRead);
			memset(buffer, 0, BUF_SIZ);
			bytesTotal += bytesRead;
			bytesRead = read(fd, buffer, BUF_SIZ);
		}

		memset(hash, 0, sizeof(hash));
		SHA256_Final(hash, &context);
		encode_hex(file_hash, sizeof(file_hash), hash, sizeof(hash));
	} else {

		// initializing MD5 structures
		MD5_CTX  md5Context;
		MD5_Init(&md5Context);

		bytesRead = read(fd, buffer, BUF_SIZ);
		while (bytesRead > 0) {
			// generating MAC gradually, chunk by chunk
			MD5_Update(& md5Context, buffer, bytesRead);
			bytesTotal += bytesRead;
			bytesRead = read(fd, buffer, BUF_SIZ);
		}

		MD5_Final((unsigned char*)md, & md5Context);
	}

	close(fd); fd = -1;
	free(buffer); buffer = NULL;

	if (bytesRead == -1) {
		dprintf(D_ALWAYS, "utilSafePutFile error reading from %s: %s\n",
			filePath.c_str(), strerror(errno));
		return false;
	}

	dprintf( D_FULLDEBUG, "utilSafePutFile %s created, total bytes read %lld\n",
			fips_mode ? "SHA-2 hash" : "MAC", (long long)bytesTotal );
	socket.encode( );

	if ( ! socket.code_bytes(md, sizeof(md))) {
		dprintf(D_ALWAYS, "utilSafePutFile unable to send MAC\n");
		return false;
	}
	if (fips_mode) {
		dprintf(D_FULLDEBUG, "Sending %d char file SHA-2 hash %s\n", (int)strlen(file_hash), file_hash);
		if ( ! socket.put(file_hash)) {
			dprintf(D_ALWAYS, "utilSafePutFile unable to send file SHA-2 hash\n");
			return false;
		}
	}
	if (socket.put_file(&bytes, filePath.c_str()) < 0 || ! socket.end_of_message()) {
		dprintf(D_ALWAYS, "utilSafePutFile unable to code end of message\n");
	}
	dprintf(D_ALWAYS, "utilSafePutFile finished successfully\n");

	return true;
}

bool
utilSafeGetFile( ReliSock& socket, const std::string& filePath, int fips_mode )
{
	REPLICATION_ASSERT( filePath != "" );
	filesize_t  bytes      = 0;
	char        wireMd[MD5_MAC_SIZE]; // only use when talking to pre-FIPS daemons
	char        localMd[MD5_MAC_SIZE];
	std::string file_hash; // used only in FIPS mode

	memset(wireMd, 0, sizeof(wireMd));
	memset(localMd, 0, sizeof(localMd));

	dprintf( D_ALWAYS, "utilSafeGetFile %s started\n", filePath.c_str( ) );	
	socket.decode( );

	if ( ! socket.code_bytes(wireMd, sizeof(wireMd))) {
		dprintf(D_ALWAYS, "utilSafeGetFile unable to get leading MAC\n");
		return false;
	}
	// if we got a leading 16 byte checksum of all zeros,
	// then expect the next thing to be a SHA-2 checksum sent as a hex string
	if (0 == memcmp(wireMd, localMd, sizeof(wireMd))) {
		dprintf( D_FULLDEBUG, "Got a MAC of all zeros, expecting a SHA-2 hash for file %s\n", filePath.c_str());
		if ( ! socket.get(file_hash)) {
			dprintf( D_ALWAYS, "Can't read file SHA-2 hash for file %s\n", filePath.c_str());
			return false;
		} else {
			dprintf(D_ALWAYS, "got %d char SHA-2 hash %s\n", (int)file_hash.size(), file_hash.c_str());
		}
	}
	if (socket.get_file(&bytes, filePath.c_str(), true) < 0) {
		dprintf(D_ALWAYS, "utilSafeGetFile unable to get file %s\n", filePath.c_str());
		return false;
	}
	// we should definitely be at end-of-message now
	if ( ! socket.end_of_message()) {
		dprintf( D_ALWAYS, "Error - Expected EOM for file %s\n", filePath.c_str());
		return false;
	}

	// check to see if we have a file hash to verify, or if
	// we are not in FIPS mode and have a non-zero MD5 sum to verify
	// since we know that localMd is all zeros at this point, we can check
	// the wireMd for zeros by comparing the two.
	bool verify_file = false;
	if ( ! file_hash.empty()) {
		fips_mode = true;
		verify_file = true;
	} else if (memcmp(wireMd, localMd, sizeof(wireMd)) && ! fips_mode) {
		verify_file = true;
	}

	if ( ! verify_file) {
		dprintf( D_ALWAYS, "utilSafeGetFile (no checksum) got %s\n", filePath.c_str() );
		return true;
	}

	dprintf(D_ALWAYS, "utilSafeGetFile verifying file %s\n", filePath.c_str());

	// The original MD5 sum was done using a ifstream opened in text mode.  This is wrong
	// but we have to preserve the wrongness for backward compatibility
	const int bin_flag = fips_mode ? _O_BINARY : 0;
	int fd = safe_open_wrapper_follow(filePath.c_str(), O_RDONLY | O_LARGEFILE | bin_flag, 0);
	if (fd < 0) {
		dprintf(D_ALWAYS, "utilSafeGetFile failed to open file %s\n", filePath.c_str());
		return false;
	}

	const size_t BUF_SIZ = 1024 * 1024; // read file in 1Mb chunks
	unsigned char *buffer = (unsigned char *)calloc(BUF_SIZ, 1);
	ASSERT(buffer != NULL);

	ssize_t bytesTotal = 0;
	ssize_t bytesRead  = 0;

	int hash_diff = 0;
	if (fips_mode) {
		unsigned char hash[SHA256_DIGEST_LENGTH];	 // TODO: support other sizes of SHA-2 hash here?
		char hex_hash[SHA256_DIGEST_LENGTH * 2 + 4]; // the SHA256 checksum as a hex string

		SHA256_CTX  context;
		SHA256_Init(&context);

		bytesRead = read(fd, buffer, BUF_SIZ);
		while (bytesRead > 0) {
			SHA256_Update(&context, buffer, bytesRead);
			memset(buffer, 0, BUF_SIZ);
			bytesTotal += bytesRead;
			bytesRead = read(fd, buffer, BUF_SIZ);
		}

		memset(hash, 0, sizeof(hash));
		SHA256_Final(hash, &context);
		encode_hex(hex_hash, sizeof(hex_hash), hash, sizeof(hash));

		hash_diff = strcasecmp(file_hash.c_str(), hex_hash);
		if (hash_diff) {
			dprintf(D_ALWAYS, "utilSafeGetFile %s received with errors: local SHA-2 hash does not match remote SHA-2 hash\n", filePath.c_str());
		}
	} else {
		MD5_CTX  md5Context;
		// initializing MD5 structures
		MD5_Init(& md5Context);

		bytesRead = read(fd, buffer, BUF_SIZ);
		while (bytesRead > 0) {
			// generating MAC gradually, chunk by chunk
			MD5_Update(& md5Context, buffer, bytesRead);
			bytesTotal += bytesRead;
			bytesRead = read(fd, buffer, BUF_SIZ);
		}

		MD5_Final((unsigned char*)localMd, & md5Context);

		hash_diff = memcmp(wireMd, localMd, sizeof(wireMd));
		if (hash_diff) {
			dprintf(D_ALWAYS, "utilSafeGetFile %s received with errors: local MAC does not match remote MAC\n", filePath.c_str());
		}
	}

	free(buffer);
	close(fd);

	if (hash_diff) {
		return false;
	}

	dprintf( D_ALWAYS, "utilSafeGetFile received and verified %s\n", filePath.c_str() );

	return true;
}

