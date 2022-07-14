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

#include <string.h>
#include <string>
#include <map>
#include <fstream>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "ssl_version_hack.h"

#include "manifest.h"
#include "checksum.h"
#include "stl_string_utils.h"
#include "AWSv4-impl.h"
#include "safe_fopen.h"


namespace manifest {

int
getNumberFromFileName( const std::string & fn ) {
	const char * fileName = fn.c_str();
	if( fileName == strstr( fileName, "MANIFEST." ) ) {
		char * endptr;
		const char * suffix = fileName + 9;
		if( *suffix != '\0' && isdigit(*suffix) ) {
			int manifestNumber = strtol( suffix, & endptr, 10 );
			if( *endptr == '\0' ) {
				return manifestNumber;
			}
		}
	}
	return -1;
}

bool
validateManifestFile( const std::string & fileName ) {
	EVP_MD_CTX * context = condor_EVP_MD_CTX_new();
	if( context == NULL ) { return false; }
	if(! EVP_DigestInit_ex( context, EVP_sha256(), NULL )) {
		condor_EVP_MD_CTX_free( context );
		return false;
	}

	FILE * fp = safe_fopen_no_create( fileName.c_str(), "r" );
	if( fp == NULL ) {
		condor_EVP_MD_CTX_free( context );
		return false;
	}

	std::string manifestLine;
	if(! readLine( manifestLine, fp )) {
		condor_EVP_MD_CTX_free( context );
		fclose(fp);
		return false;
	}

	std::string nextManifestLine;
	bool rv = readLine( nextManifestLine, fp );

	while( rv ) {
		EVP_DigestUpdate( context, manifestLine.c_str(), manifestLine.length() );

		manifestLine = nextManifestLine;
		rv = readLine( nextManifestLine, fp );
	}
	fclose(fp);

	unsigned char hash[SHA256_DIGEST_LENGTH];
	memset( hash, 0, sizeof(hash) );
	if(! EVP_DigestFinal_ex( context, hash, NULL )) {
		condor_EVP_MD_CTX_free( context );
		return false;
	}
	condor_EVP_MD_CTX_free( context );

	std::string file_hash;
	AWSv4Impl::convertMessageDigestToLowercaseHex(
		hash, SHA256_DIGEST_LENGTH, file_hash
	);

	trim( manifestLine );
	std::string manifestFileName = FileFromLine( manifestLine );
	std::string manifestChecksum = ChecksumFromLine( manifestLine );

	// These don't have to match exactly because the shadow calls this
	// function with the full path to the file name.
	if(! ends_with( fileName, manifestFileName )) { return false; }
	if( manifestChecksum != file_hash ) { return false; }
	return true;
}

std::string
FileFromLine( const std::string & manifestLine ) {
	auto pos = manifestLine.find(' ');
	if( pos == std::string::npos ) { return std::string(); }
	if( manifestLine[++pos] == '*' ) { ++pos; }
	return manifestLine.substr(pos);
}

std::string
ChecksumFromLine( const std::string & manifestLine ) {
	auto pos = manifestLine.find(' ');
	return manifestLine.substr(0, pos);
}

bool
validateFilesListedIn(
  const std::string & manifestFileName,
  std::string & error
) {

	FILE * fp = safe_fopen_no_create( manifestFileName.c_str(), "r" );
	if( fp == NULL ) {
		error = "Failed to open MANIFEST, aborting.";
		return false;
	}

	// This doesn't check the checksum on the last line, because
	// that's what the validator does.
	bool readOneLine = false;
	std::string manifestLine;
	if(! readLine( manifestLine, fp )) {
		error = "Failed to read first line of MANIFEST, aborting.";
		fclose(fp);
		return false;
	}

	std::string nextManifestLine;
	bool rv = readLine( nextManifestLine, fp );

	while( rv ) {
		trim( manifestLine );
		std::string file = manifest::FileFromLine( manifestLine );
		std::string listedChecksum = manifest::ChecksumFromLine( manifestLine );

		std::string computedChecksum;
		if(! compute_file_sha256_checksum( file, computedChecksum )) {
			formatstr( error, "Failed to open checkpoint file ('%s') to compute checksum.", file.c_str() );
			fclose(fp);
			return false;
		}

		if( listedChecksum != computedChecksum ) {
			formatstr( error, "Checkpoint file '%s' did not have expected checksum (%s vs %s).", file.c_str(), computedChecksum.c_str(), listedChecksum.c_str() );
			fclose(fp);
			return false;
		}

		manifestLine = nextManifestLine;
		rv = readLine( nextManifestLine, fp );
		readOneLine = true;
	}
	fclose(fp);

	return readOneLine;
}

} /* end namespace 'manifest' */
