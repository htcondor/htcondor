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

#include "manifest.h"
#include "checksum.h"
#include "stl_string_utils.h"
#include "AWSv4-impl.h"


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
validateFile( const std::string & fileName ) {
	EVP_MD_CTX * context = EVP_MD_CTX_new();
	if( context == NULL ) { return false; }
	if(! EVP_DigestInit_ex( context, EVP_sha256(), NULL )) {
		EVP_MD_CTX_free( context );
		return false;
	}

	std::ifstream ifs( fileName );
	std::string manifestLine;
	std::string nextManifestLine;
	std::getline( ifs, manifestLine );
	std::getline( ifs, nextManifestLine );
	for( ; ifs.good(); ) {
		manifestLine += "\n";
		EVP_DigestUpdate( context, manifestLine.c_str(), manifestLine.length() );

		manifestLine = nextManifestLine;
		std::getline( ifs, nextManifestLine );
	}

	unsigned char hash[SHA256_DIGEST_LENGTH];
	memset( hash, 0, sizeof(hash) );
	if(! EVP_DigestFinal_ex( context, hash, NULL )) {
		EVP_MD_CTX_destroy( context );
		return false;
	}
	EVP_MD_CTX_destroy( context );

	std::string file_hash;
	AWSv4Impl::convertMessageDigestToLowercaseHex(
		hash, SHA256_DIGEST_LENGTH, file_hash
	);

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
	std::ifstream ifs( manifestFileName.c_str() );
	if(! ifs.good() ) {
		error = "Failed to open MANIFEST, aborting.";
		return false;
	}

	// This doesn't check the checksum on the last line, because
	// that's what the validator does.
	bool readOneLine = false;
	std::string manifestLine;
	std::string nextManifestLine;
	std::getline( ifs, manifestLine );
	std::getline( ifs, nextManifestLine );
	for( ; ifs.good(); ) {
		std::string file = manifest::FileFromLine( manifestLine );
		std::string listedChecksum = manifest::ChecksumFromLine( manifestLine );

		std::string computedChecksum;
		if(! compute_file_checksum( file, computedChecksum )) {
			formatstr( error, "Failed to open checkpoint file ('%s') to compute checksum.", file.c_str() );
			return false;
		}

		if( listedChecksum != computedChecksum ) {
			formatstr( error, "Checkpoint file '%s' did not have expected checksum (%s vs %s).", file.c_str(), computedChecksum.c_str(), listedChecksum.c_str() );
			return false;
		}

		manifestLine = nextManifestLine;
		std::getline( ifs, nextManifestLine );
		readOneLine = true;
	}

	ifs.close();
	return readOneLine;
}

} /* end namespace 'manifest' */
