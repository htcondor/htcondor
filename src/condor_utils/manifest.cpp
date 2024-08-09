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
#include "safe_fopen.h"
#include "condor_arglist.h"
#include "my_popen.h"

#include <string.h>
#include <string>
#include <map>
#include <filesystem>

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "ssl_version_hack.h"

#include "manifest.h"
#include "checksum.h"
#include "stl_string_utils.h"
#include "AWSv4-impl.h"

#include "shortfile.h"

#include "MapFile.h"
#include "condor_config.h"
#include "checkpoint_cleanup_utils.h"


namespace manifest {

int
getNumberFromFileName( const std::string & fn ) {
	const char * fileName = fn.c_str();
	if( fileName == strstr( fileName, "_condor_checkpoint_MANIFEST." ) ) {
		char * endptr;
		const char * suffix = fileName + strlen("_condor_checkpoint_MANIFEST.");
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

bool
deleteFilesStoredAt(
  const std::string & checkpointDestination,
  const std::string & manifestFileName,
  const std::filesystem::path & jobAdPath,
  std::string & error,
  bool wasFailedCheckpoint
) {
	FILE * fp = safe_fopen_no_create( manifestFileName.c_str(), "r" );
	if( fp == NULL ) {
		error = "Failed to open MANIFEST, aborting.";
		return false;
	}

	std::filesystem::path pathToManifestFile(manifestFileName);
	std::filesystem::path fileName = pathToManifestFile.filename();


	std::string argl;
	if(! fetchCheckpointDestinationCleanup( checkpointDestination, argl, error )) {
		// fetchCheckpointDestinationCleanup() set error for us already.
		return false;
	}

    StringTokenIterator sti( argl );
	sti.rewind();
	std::string pluginFileName = sti.next();
	std::filesystem::path pluginPath( pluginFileName );
	if( pluginPath.is_relative() ) {
		std::string libexec;
		param( libexec, "LIBEXEC" );
		pluginFileName = (libexec / pluginPath).string();
	}

	if(! std::filesystem::exists( pluginFileName )) {
		formatstr( error,
		    "Clean-up plug-in for '%s' (%s) does not exist, aborting",
		    checkpointDestination.c_str(), pluginFileName.c_str()
		);
		return false;
	}


	std::string manifestLine;
	for( bool rv = false; (rv = readLine( manifestLine, fp )); ) {
		trim( manifestLine );
		std::string file = manifest::FileFromLine( manifestLine );

		// Don't delete the MANIFEST file until we're sure we're done with it.
		if( fileName.string() == file ) {
			continue;
		}

		ArgList args;
		args.AppendArg(pluginFileName);
		sti.rewind(); sti.next();
		for( const char * entry = sti.next(); entry != NULL; entry = sti.next() ) {
			args.AppendArg(entry);
		}
		args.AppendArg("-from");
		args.AppendArg(checkpointDestination);
		args.AppendArg("-delete");
		args.AppendArg(file);
		args.AppendArg("-jobad");
		args.AppendArg(jobAdPath.string());

		if( wasFailedCheckpoint ) {
		    args.AppendArg("-ignore-missing-files");
		}

		std::string argStr;
		args.GetArgsStringForLogging( argStr );
		dprintf( D_FULLDEBUG, "About to run '%s'...\n", argStr.c_str() );

		MyPopenTimer subprocess;
		int rc = subprocess.start_program( args, subprocess.WITH_STDERR,
		  nullptr, subprocess.DROP_PRIVS
		);
		ASSERT( rc != subprocess.ALREADY_RUNNING );

		if( rc != 0 ) {
			formatstr( error, "Failed to run '%s': %d (%s), aborting.", argStr.c_str(), rc, subprocess.error_str() );
			return false;
		}

		int exit_status;
		// time_t timeout = 20;
		time_t timeout = param_integer( "CHECKPOINT_CLEANUP_TIMEOUT", 20 );
		if(! subprocess.wait_for_exit( timeout, & exit_status )) {
			const char * output = subprocess.output().data();
			subprocess.close_program(1);

			formatstr( error, "Timed out after %lu seconds waiting for '%s', aborting.\n", timeout, argStr.c_str() );
			if( output != NULL ) {
				formatstr_cat( error, "(Partial output: '%s')\n", output );
			}

			return false;
		}

		if( exit_status != 0 ) {
			const char * output = subprocess.output().data();

			formatstr( error, "Failure running '%s': exit code was %d, aborting.\n", argStr.c_str(), exit_status );
			if( output != NULL ) {
				formatstr_cat( error, "(Output: '%s')\n", output );
			}

			return false;
		}

		const char * output = subprocess.output().data();
		if( output != NULL ) {
			dprintf( D_FULLDEBUG, "Ran '%s', output on next line:\n%s\n", argStr.c_str(), output );
		}
	}

	fclose(fp);
	std::filesystem::remove(pathToManifestFile);

	return true;
}

bool
createManifestFor(
    const std::string & path,
    const std::string & manifestFileName,
    std::string & error
) {
    std::string manifestText;

    std::error_code errCode;
    for( const auto & dentry : std::filesystem::recursive_directory_iterator(path, errCode) ) {
        if( errCode ) {
            formatstr( error, "Unable to compute file checksums (%d: %s), aborting.\n", errCode.value(), errCode.message().c_str() );
            return false;
        }
        if( dentry.is_directory() || dentry.is_socket() ) { continue; }
        std::string fileName = dentry.path().string();

        std::string fileHash;
        if(! compute_file_sha256_checksum( fileName, fileHash )) {
            formatstr( error, "Failed to compute file (%s) checksum, aborting.\n", fileName.c_str() );
            return false;
        }

        formatstr_cat( manifestText, "%s *%s\n", fileHash.c_str(), fileName.c_str() );
    }

    if(! htcondor::writeShortFile( manifestFileName, manifestText )) {
        formatstr( error, "Failed write manifest file (%s), aborting.\n", manifestFileName.c_str() );
        return false;
    }

    std::string manifestHash;
    if(! compute_file_sha256_checksum( manifestFileName, manifestHash )) {
        formatstr( error, "Failed to compute manifest (%s) checksum, aborting.\n", manifestFileName.c_str() );
        return false;
    }

    std::string append;
    formatstr( append, "%s *%s\n", manifestHash.c_str(), manifestFileName.c_str() );
    if(! htcondor::appendShortFile( manifestFileName, append )) {
        formatstr( error, "Failed to write manifest checksum to manifest (%s), aborting.\n", manifestFileName.c_str() );
        return false;
    }

    return true;
}

} /* end namespace 'manifest' */
