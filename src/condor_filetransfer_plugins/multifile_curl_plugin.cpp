#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_utils/file_transfer_stats.h"
#include "../condor_utils/condor_url.h"
#include "multifile_curl_plugin.h"
#include "utc_time.h"
#include <exception>
#include <sstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <rapidjson/document.h>

#define MAX_RETRY_ATTEMPTS 20
int max_retry_attempts = MAX_RETRY_ATTEMPTS;

// Setup a transfer progress callback. We'll use this to manually timeout 
// any transfers that are not making forward progress.

struct xferProgress {
    double lastRunTime;
    CURL *curl;
};
struct xferProgress myProgress;

static int xferInfo(void *p, double /* dltotal */, double dlnow, double /* ultotal */, double ulnow)
{
	/* Note : we cannot rely on dltotal or ultotal being correct, since
	they will only be non-zero if the server responded with the optional
	Content-Length header.  */

    struct xferProgress *progress = (struct xferProgress *)p;
    CURL *curl = progress->curl;
    double curTime = 0;
    
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &curTime);

    // After 30 seconds, check if we're making forward progress (> 1 byte/s)
    if (curTime > 30) {

        // If this is a download and not making progress, abort
        if (dlnow > 0 && curTime > dlnow) return 1;

        // If this is an upload and not making progress, abort
        if (ulnow > 0 && curTime > ulnow) return 1;

        // If not a single byte has been transferred either direction after 30 seconds, abort
        if (dlnow <= 0 && ulnow <= 0) return 1;
    }

    // All good. Return success.
    return 0;
}


namespace {

bool
ShouldRetryTransfer(int rval) {
    switch (rval) {
        case CURLE_COULDNT_CONNECT:
        case CURLE_PARTIAL_FILE:
        case CURLE_READ_ERROR:
        case CURLE_WRITE_ERROR:
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_GOT_NOTHING:
            return true;
        default:
            return false;
    };
}

extern "C"
size_t
CurlReadCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
    if (userdata == nullptr) {
        return size*nitems;
    }
    return fread(buffer, size, nitems, static_cast<FILE*>(userdata));
}


extern "C"
size_t
CurlWriteCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
    if (userdata == nullptr) {
        return size*nitems;
    }
    return fwrite(buffer, size, nitems, static_cast<FILE*>(userdata));
}

void
GetToken(const std::string & cred_name, std::string & token) {
	if (cred_name.empty()) {
		return;
	}
	const char *creddir = getenv("_CONDOR_CREDS");
	if (!creddir) {
		std::stringstream ss; ss << "Credential for " << cred_name << " requested by $_CONDOR_CREDS not set";
		throw std::runtime_error(ss.str());
	}

	std::string cred_path = std::string(creddir) + DIR_DELIM_STRING + cred_name + ".use";
	int fd = open(cred_path.c_str(), O_RDONLY);
	if (-1 == fd) {
		fprintf( stderr, "Error: Unable to open credential file %s: %s (errno=%d)", cred_path.c_str(),
			strerror(errno), errno);
		std::stringstream ss; ss << "Unable to open credential file " << cred_path << ": " << strerror(errno) << " (errno=" << errno << ")";
		throw std::runtime_error(ss.str());
	}
	close(fd);
	std::ifstream istr(cred_path, std::ios::binary);
	if (!istr.is_open()) {
		throw std::runtime_error("Failed to reopen credential file");
	}
	for (std::string line; std::getline(istr, line); ) {
		auto iter = line.begin();
		while (isspace(*iter)) {iter++;}
		if (*iter == '#') continue;
		rapidjson::Document doc;
		if (doc.Parse(line.c_str()).HasParseError()) {
			// DO NOT include the error message as part of the exception; the error
			// message may include private information in the credential file itself,
			// which we don't want to go into the public hold message.
			throw std::runtime_error("Unable to parse token as JSON");
                }
		if (!doc.IsObject()) {
			throw std::runtime_error("Token is not a JSON object");
		}
		if (!doc.HasMember("access_token")) {
			throw std::runtime_error("No 'access_token' key in JSON object");
		}
		auto &access_obj = doc["access_token"];
		if (!access_obj.IsString()) {
			throw std::runtime_error("'access_token' value is not a string");
		}
		token = access_obj.GetString();
	}
}

}

MultiFileCurlPlugin::MultiFileCurlPlugin( bool diagnostic ) :
    _diagnostic ( diagnostic )
{
}

MultiFileCurlPlugin::~MultiFileCurlPlugin() {
    curl_easy_cleanup( _handle );
    curl_global_cleanup();
}

int
MultiFileCurlPlugin::InitializeCurl() {
    ParseAds();

    // Initialize win32 + SSL socket libraries.
    // Do not initialize these separately! Doing so causes https:// transfers
    // to segfault.
    int init = curl_global_init( CURL_GLOBAL_DEFAULT );
    if ( init != 0 ) {
        fprintf( stderr, "Error: curl_plugin initialization failed with error code %d\n", init );
    }
    if ( ( _handle = curl_easy_init() ) == NULL ) {
        fprintf( stderr, "Error: failed to initialize MultiFileCurlPlugin._handle\n" );
    }
    return init;
}

void
MultiFileCurlPlugin::InitializeCurlHandle(const std::string &url, const std::string &cred,
        struct curl_slist *& header_list)
{
	CURLcode r;
    r = curl_easy_setopt( _handle, CURLOPT_URL, url.c_str() );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CUROPT_URL\n");
	}
    r = curl_easy_setopt( _handle, CURLOPT_CONNECTTIMEOUT, 60 );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CONNECTIMEOUT\n");
	}

    // Provide default read / write callback functions; note these
    // don't segfault if a nullptr is given as the read/write data.
    r = curl_easy_setopt( _handle, CURLOPT_READFUNCTION, &CurlReadCallback );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt READFUNCTION\n");
	}
    r = curl_easy_setopt( _handle, CURLOPT_WRITEFUNCTION, &CurlWriteCallback );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt WRITEFUNCTION\n");
	}

    // Prevent curl from spewing to stdout / in by default.
    r = curl_easy_setopt( _handle, CURLOPT_READDATA, NULL );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt READDATA\n");
	}
    r = curl_easy_setopt( _handle, CURLOPT_WRITEDATA, NULL );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt WRITEDATA\n");
	}

    std::string token;

    // Libcurl options for HTTP, HTTPS and FILE
    if( !strncasecmp( url.c_str(), "http://", 7 ) ||
            !strncasecmp( url.c_str(), "https://", 8 ) ||
            !strncasecmp( url.c_str(), "file://", 7 ) ) {
        r = curl_easy_setopt( _handle, CURLOPT_FOLLOWLOCATION, 1 );
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt FOLLOWLOCATION\n");
		}
        r = curl_easy_setopt( _handle, CURLOPT_HEADERFUNCTION, &HeaderCallback );
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt HEADERFUNCTOIN\n");
		}

        GetToken(cred, token);
    }
    // Libcurl options for FTP
    else if( !strncasecmp( url.c_str(), "ftp://", 6 ) ) {
        r = curl_easy_setopt( _handle, CURLOPT_WRITEFUNCTION, &FtpWriteCallback );
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt WRITEFUNCTION\n");
		}
    }

    if (!token.empty()) {
        std::string authz_header = "Authorization: Bearer ";
        authz_header += token;
        header_list = curl_slist_append(header_list, authz_header.c_str());
    }

    // * If the following option is set to 0, then curl_easy_perform()
    // returns 0 even on errors (404, 500, etc.) So we can't identify
    // some failures. I don't think it's supposed to do that?
    // * If the following option is set to 1, then something else bad
    // happens? 500 errors fail before we see HTTP headers but I don't
    // think that's a big deal.
    // * Let's keep it set to 1 for now.
    r = curl_easy_setopt( _handle, CURLOPT_FAILONERROR, 1 );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt FAILONERROR\n");
	}

    if( _diagnostic ) {
        r = curl_easy_setopt( _handle, CURLOPT_VERBOSE, 1 );
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt VERBOSE\n");
		}
    }

    // Setup a buffer to store error messages. For debug use.
    _error_buffer[0] = '\0';
    r = curl_easy_setopt( _handle, CURLOPT_ERRORBUFFER, _error_buffer );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt ERRORBUFFER\n");
	}

    // Setup a transfer progress callback. We'll use this to determine if a 
    // transfer is not making progress, and if not then abort it.
    myProgress.curl = _handle;
    myProgress.lastRunTime = 0;
    r = curl_easy_setopt(_handle, CURLOPT_PROGRESSFUNCTION, xferInfo);
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt PROGRESSFUNCTION\n");
	}
    r = curl_easy_setopt(_handle, CURLOPT_PROGRESSDATA, &myProgress);
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt PROGRESSDATA\n");
	}
    r = curl_easy_setopt(_handle, CURLOPT_NOPROGRESS, 0L);
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt NOPROGRESS\n");
	}
}

FILE *
MultiFileCurlPlugin::OpenLocalFile(const std::string &local_file, const char *mode) const {
    FILE *file = nullptr;
    if ( !strcmp( local_file.c_str(), "-" ) ) {
        int fd = dup(1);
        if ( -1 != fd ) {
            if ( _diagnostic ) { fprintf( stderr, "Fetching %s to stdout\n", local_file.c_str() ); }
            file = fdopen(fd, mode);
        }
    }
    else {
        if ( _diagnostic ) { fprintf( stderr, "Fetching to %s\n", local_file.c_str() ); }
        file = safe_fopen_wrapper( local_file.c_str(), mode );
    }

    if( !file ) {
        fprintf( stderr, "ERROR: could not open local file %s, error %d (%s)\n", local_file.c_str(), errno, strerror(errno) );
    }

    return file;
}


void
MultiFileCurlPlugin::FinishCurlTransfer( int rval, FILE *file ) {

    // Gather more statistics
    double bytes_downloaded = 0;
    double bytes_uploaded = 0;
    double transfer_connection_time;
    double transfer_total_time;
    long return_code;
    curl_easy_getinfo( _handle, CURLINFO_SIZE_DOWNLOAD, &bytes_downloaded );
    curl_easy_getinfo( _handle, CURLINFO_SIZE_UPLOAD, &bytes_uploaded );
    curl_easy_getinfo( _handle, CURLINFO_CONNECT_TIME, &transfer_connection_time );
    curl_easy_getinfo( _handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
    curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &return_code );

    if(bytes_downloaded > 0) {
        _this_file_stats->TransferTotalBytes += ( long ) bytes_downloaded;
    }
    else {
        _this_file_stats->TransferTotalBytes += ( long ) bytes_uploaded;
    }

    _this_file_stats->ConnectionTimeSeconds +=  ( transfer_total_time - transfer_connection_time );
    _this_file_stats->TransferHTTPStatusCode = return_code;
    _this_file_stats->LibcurlReturnCode = rval;

    if( rval == CURLE_OK ) {
            // Transfer successful!
        _this_file_stats->TransferSuccess = true;
        _this_file_stats->TransferError = "";
        _this_file_stats->TransferFileBytes = ftell( file );
    }
    else if ( rval == CURLE_ABORTED_BY_CALLBACK ) {
            // Transfer failed because our xferInfo callback above returned abort.
            // The error string returned by libcurl just says "Callback aborted",
            // so lets give something more meaningful.
        _this_file_stats->TransferSuccess = false;
        _this_file_stats->TransferError = "Aborted due to lack of progress";
    }
    else {
        _this_file_stats->TransferSuccess = false;
        _this_file_stats->TransferError = _error_buffer;
    }
}

int
MultiFileCurlPlugin::UploadFile( const std::string &url, const std::string &local_file_name,
        const std::string &cred )
{
    FILE *file = nullptr;
    int rval = -1;

    if( !(file=OpenLocalFile(local_file_name, "r")) ) {
        return rval;
    }
    struct curl_slist *header_list = NULL;
    try {
        InitializeCurlHandle( url, cred, header_list );
    } catch (const std::exception &exc) {
        _this_file_stats->TransferSuccess = false;
        _this_file_stats->TransferError = exc.what();
        fprintf( stderr, "Error: %s.\n", exc.what() );
        return rval;
    }   

    int fd = fileno(file);
    struct stat stat_buf;
    if (-1 == fstat(fd, &stat_buf)) {
        if ( _diagnostic ) { fprintf(stderr, "Failed to stat the local file for upload: %s (errno=%d).\n", strerror(errno), errno); }
        fclose( file );
        return rval;
    }

	CURLcode r;
    r = curl_easy_setopt( _handle, CURLOPT_READDATA, file );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CUROPT_READDATA\n");
        fclose( file );
		return -1;
	}

    r = curl_easy_setopt( _handle, CURLOPT_UPLOAD, 1L);
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CUROPT_UPLOAD\n");
        fclose( file );
		return -1;
	}

    r = curl_easy_setopt( _handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)stat_buf.st_size );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CUROPT_INFILESIZE_LARGE\n");
        fclose( file );
		return -1;
	}

    if (header_list) {
		r = curl_easy_setopt(_handle, CURLOPT_HTTPHEADER, header_list);
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt CUROPT_HTTPHEADER\n");
        	fclose( file );
			return -1;
		}
	}

    // Update some statistics
    _this_file_stats->TransferType = "upload";
    _this_file_stats->TransferTries += 1;

    // Perform the curl request
    rval = curl_easy_perform( _handle );

    if (header_list) curl_slist_free_all(header_list);

    FinishCurlTransfer( rval, file );

        // Error handling and cleanup
    if( _diagnostic && rval ) {
        fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n",
                rval, curl_easy_strerror( ( CURLcode ) rval ) );
    }

    fclose( file );

    return rval;
}


int 
MultiFileCurlPlugin::DownloadFile( const std::string &url, const std::string &local_file_name, const std::string &cred, long &partial_bytes ) {

    char partial_range[20];
    FILE *file = NULL;
    int rval = -1;

    if ( !(file=OpenLocalFile(local_file_name, partial_bytes ? "a+" : "w")) ) {
        return rval;
    }
    struct curl_slist *header_list = NULL;
    try {
        InitializeCurlHandle( url, cred, header_list );
    } catch (const std::exception &exc) {
        _this_file_stats->TransferSuccess = false;
        _this_file_stats->TransferError = exc.what();
        fprintf( stderr, "Error: %s.\n", exc.what() );
        return rval;
    }   

    // Libcurl options that apply to all transfer protocols
	CURLcode r;
    r = curl_easy_setopt( _handle, CURLOPT_WRITEDATA, file );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CURLOPT_WRITEDATA\n");
	}
    r = curl_easy_setopt( _handle, CURLOPT_HEADERDATA, _this_file_stats.get() );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CURLOPT_HEADERDATA\n");
	}

    if (header_list) {
		r = curl_easy_setopt(_handle, CURLOPT_HTTPHEADER, header_list);
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt CURLOPT_HTTPHEADER\n");
		}
	}

    // If we are attempting to resume a download, set additional flags
    if( partial_bytes ) {
        sprintf( partial_range, "%lu-", partial_bytes );
        r = curl_easy_setopt( _handle, CURLOPT_RANGE, partial_range );
		if (r != CURLE_OK) {
			fprintf(stderr, "Can't setopt CURLOPT_RANGE\n");
		}
    }

    // Update some statistics
    _this_file_stats->TransferType = "download";
    _this_file_stats->TransferTries += 1;

    // Perform the curl request
    rval = curl_easy_perform( _handle );

    if (header_list) curl_slist_free_all(header_list);

    // Check if the request completed partially. If so, set some
    // variables so we can attempt a resume on the next try.
    if( ( rval == CURLE_PARTIAL_FILE ) && ServerSupportsResume( url ) && _this_file_stats->HttpCacheHitOrMiss != "HIT" ) {
        partial_bytes = ftell( file );
    }

    // Sometimes we get an HTTP redirection code (301 or 302) but without a
    // Location header. By default libcurl treats these as successful transfers.
    // We want to treat them as errors.
    // This needs to happen before FinishCurlTransfer() so that the transfer
    // is flagged correctly as failed.
    char* redirect_url;
    long return_code;
    curl_easy_getinfo( _handle, CURLINFO_REDIRECT_URL, &redirect_url );
    curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &return_code );
    if( ( return_code == 301 || return_code == 302 ) && !redirect_url ) {
        // Hack: set rval to a non-zero CURL error code
        rval = CURLE_REMOTE_FILE_NOT_FOUND;
        strcpy(_error_buffer, "The URL you requested could not be found.");
    }

    FinishCurlTransfer( rval, file );

        // Error handling and cleanup
    if( _diagnostic && rval ) {
        fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n", 
                rval, curl_easy_strerror( ( CURLcode ) rval ) ); 
    }

    fclose( file ); 

    return rval;
}


int
MultiFileCurlPlugin::BuildTransferRequests(const std::string &input_filename, std::vector<std::pair<std::string, transfer_request>> &requested_files) const {

    CondorClassAdFileIterator adFileIter;
    FILE* input_file;

    // Read input file containing data about files we want to transfer. Input
    // data is formatted as a series of classads, each with an arbitrary number
    // of inputs.
    input_file = safe_fopen_wrapper( input_filename.c_str(), "r" );
    if( input_file == NULL ) {
        fprintf( stderr, "Unable to open curl_plugin input file %s.\n", 
            input_filename.c_str() );
        return 1;
    }

    if( !adFileIter.begin( input_file, false, CondorClassAdFileParseHelper::Parse_new )) {
        fprintf( stderr, "Failed to start parsing classad input.\n" );
        return 1;
    }
    else {
        // Iterate over the classads in the file, and insert each one into our
        // requested_files map, with the key: url, value: additional details 
        // about the transfer.
        ClassAd transfer_file_ad;
        std::string local_file_name;
        std::string url;
        transfer_request request_details;
        std::pair< std::string, transfer_request > this_request;

        int count = 0;
        while ( adFileIter.next( transfer_file_ad ) > 0 ) {
            transfer_file_ad.EvaluateAttrString( "LocalFileName", local_file_name );
            transfer_file_ad.EvaluateAttrString( "Url", url );
            request_details.local_file_name = local_file_name;

            if (url.substr(0, 7) == "davs://") {
                url = std::string("https://") + url.substr(7);
            } else if (url.substr(0, 6) == "dav://") {
                url = std::string("http://") + url.substr(6);
            }

            this_request = std::make_pair( url, request_details );
            requested_files.push_back( this_request );
            count ++;
            if ( _diagnostic ) {
                fprintf( stderr, "Will transfer between URL %s and local file %s.\n", url.c_str(), local_file_name.c_str() );
            }
        }
        if ( _diagnostic ) {
            fprintf( stderr, "There are a total of %d files to transfer.\n", count );
        }
    }
    fclose(input_file);

    return 0;
}


TransferPluginResult
MultiFileCurlPlugin::UploadMultipleFiles( const std::string &input_filename ) {
    std::vector<std::pair<std::string, transfer_request>> requested_files;
    auto rval = BuildTransferRequests(input_filename, requested_files);
    if (rval != 0) {
        return TransferPluginResult::Error;
    }

    classad::ClassAdUnParser unparser;
    if ( _diagnostic ) { fprintf( stderr, "Uploading multiple files.\n" ); }

    for (const auto &file_pair : requested_files) {

        const auto &local_file_name = file_pair.second.local_file_name;
        const auto &url = file_pair.first;

        int retry_count = 0;
        int file_rval = -1;

        // Initialize the stats structure for this transfer.
        _this_file_stats.reset(new FileTransferStats());
        InitializeStats( url );
        _this_file_stats->TransferStartTime = time(NULL);
	_this_file_stats->TransferFileName = local_file_name;

        // Enter the loop that will attempt/retry the curl request
        for ( ;; ) {

            std::this_thread::sleep_for(std::chrono::seconds(retry_count++));

            // Everything prior to the first '+' is the credential name.
            std::string full_scheme = getURLType(url.c_str(), false);
            auto offset = full_scheme.find_last_of("+");
            auto cred = (offset == std::string::npos) ? "" : full_scheme.substr(0, offset);

            // The actual transfer should only be everything after the last '+'
            std::string full_url = url;
            if (offset != std::string::npos) {
                full_url = full_url.substr(offset + 1);
            }

            file_rval = UploadFile( full_url, local_file_name, cred );
            // If curl request is successful, break out of the loop
            if( file_rval == CURLE_OK ) {
                break;
            }
            // If we have not exceeded the maximum number of retries, and we encounter
            // a non-fatal error, stay in the loop and try again
            else if( retry_count <= max_retry_attempts &&
                     ShouldRetryTransfer(file_rval) ) {
                continue;
            }
            // On fatal errors, break out of the loop
            else {
                break;
            }
        }

        _this_file_stats->TransferEndTime = time(NULL);

        // Regardless of success/failure, update the stats
        classad::ClassAd stats_ad;
        _this_file_stats->Publish( stats_ad );
        std::string stats_string;
        unparser.Unparse( stats_string, &stats_ad );
        _all_files_stats += stats_string;
        stats_ad.Clear();

        // Note that we attempt to upload all files, even if one fails!
        // The upload protocol demands that all attempted files have a corresponding ad.
        if ( ( file_rval != CURLE_OK ) && ( rval != -1 ) ) {
            rval = file_rval;
        }
    }

    if ( rval != 0 ) return TransferPluginResult::Error;

    return TransferPluginResult::Success;
}


TransferPluginResult
MultiFileCurlPlugin::DownloadMultipleFiles( const std::string &input_filename ) {

    int rval = 0;

    std::vector<std::pair<std::string, transfer_request>> requested_files;
    rval = BuildTransferRequests(input_filename, requested_files);
    // If BuildTransferRequests failed, exit immediately
    if ( rval != 0 ) {
        return TransferPluginResult::Error;
    }
    classad::ClassAdUnParser unparser;

    // Iterate over the map of files to transfer.
    for ( const auto &file_pair : requested_files ) {

        const auto &local_file_name = file_pair.second.local_file_name;
        const auto &url = file_pair.first;
        if ( _diagnostic ) {
            fprintf( stderr, "Will download %s to %s.\n", url.c_str(), local_file_name.c_str() );
        }
        int retry_count = 0;

        // Initialize the stats structure for this transfer.
        _this_file_stats.reset( new FileTransferStats() );
        InitializeStats( url );
        _this_file_stats->TransferStartTime = time(NULL);
	_this_file_stats->TransferFileName = local_file_name;

        long partial_bytes = 0;
        // Enter the loop that will attempt/retry the curl request
        for ( ;; ) {
            if ( _diagnostic && retry_count ) { fprintf( stderr, "Retry count #%d\n", retry_count ); }

            std::this_thread::sleep_for(std::chrono::seconds(retry_count++));

            // Everything prior to the first '+' is the credential name.
            std::string full_scheme = getURLType(url.c_str(), false);
            auto offset = full_scheme.find_last_of("+");
            auto cred = (offset == std::string::npos) ? "" : full_scheme.substr(0, offset);

            // The actual transfer should only be everything after the last '+'
            std::string full_url = url;
            if (offset != std::string::npos) {
                full_url = full_url.substr(offset + 1);
            }

            // partial_bytes are updated if the file downloaded partially.
            rval = DownloadFile( full_url, local_file_name, cred, partial_bytes );

            // If curl request is successful, break out of the loop
            if( rval == CURLE_OK ) {
                break;
            }
            // If we have not exceeded the maximum number of retries, and we encounter
            // a non-fatal error, stay in the loop and try again
            else if( retry_count <= max_retry_attempts && 
                     ShouldRetryTransfer(rval) ) {
                continue;
            }
            // On fatal errors, break out of the loop
            else {
                break;
            }
        }

        _this_file_stats->TransferEndTime = time(NULL);

        // Regardless of success/failure, update the stats
        classad::ClassAd stats_ad;
        _this_file_stats->Publish( stats_ad );
	std::string stats_string;
        unparser.Unparse( stats_string, &stats_ad );
        _all_files_stats += stats_string;
        stats_ad.Clear();

        // If the transfer did fail, break out of the loop immediately
        if ( rval > 0 ) break;
    }

    if ( rval != 0 ) return TransferPluginResult::Error;

    return TransferPluginResult::Success;
}

/*
    Check if this server supports resume requests using the HTTP "Range" header
    by sending a Range request and checking the return code. Code 206 means
    resume is supported, code 200 means not supported. 
    Return: 1 if resume is supported, 0 if not.
*/
int 
MultiFileCurlPlugin::ServerSupportsResume( const std::string &url ) {

    int rval = -1;

    // Send a basic request, with Range set to a null range
	CURLcode r;
    r = curl_easy_setopt( _handle, CURLOPT_URL, url.c_str() );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CURLOPT_URL\n");
		return 0;
	}
    r = curl_easy_setopt( _handle, CURLOPT_CONNECTTIMEOUT, 60 );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CURLOPT_CONNECTTIMEOUT\n");
		return 0;
	}
    r = curl_easy_setopt( _handle, CURLOPT_RANGE, "0-0" );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CURLOPT_RANGE\n");
		return 0;
	}

    rval = curl_easy_perform(_handle);

    // Check the HTTP status code that was returned
    if( rval == 0 ) {
        char* finalURL = NULL;
        rval = curl_easy_getinfo( _handle, CURLINFO_EFFECTIVE_URL, &finalURL );

        if( rval == 0 ) {
            if( strstr( finalURL, "http" ) == finalURL ) {
                long httpCode = 0;
                rval = curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &httpCode );

                // A 206 status code indicates resume is supported. Return true!
                if( httpCode == 206 ) {
                    return 1;
                }
            }
        }
    }

    // If we've gotten this far the server does not support resume. Clear the
    // HTTP "Range" header and return false.
    r = curl_easy_setopt( _handle, CURLOPT_RANGE, NULL );
	if (r != CURLE_OK) {
		fprintf(stderr, "Can't setopt CURLOPT_RANGE\n");
		return 0;
	}
    return 0;
}

void
MultiFileCurlPlugin::InitializeStats( std::string request_url ) {

    char* url = strdup( request_url.c_str() );
    char* url_token;

    // Set the transfer protocol. If it's not http, ftp and file, then just
    // leave it blank because this transfer will fail quickly.
    if ( !strncasecmp( url, "http://", 7 ) ) {
        _this_file_stats->TransferProtocol = "http";
    }
    else if ( !strncasecmp( url, "https://", 8 ) ) {
        _this_file_stats->TransferProtocol = "https";
    }
    else if ( !strncasecmp( url, "ftp://", 6 ) ) {
        _this_file_stats->TransferProtocol = "ftp";
    }
    else if ( !strncasecmp( url, "file://", 7 ) ) {
        _this_file_stats->TransferProtocol = "file";
    }

    // Set the request host name by parsing it out of the URL
    _this_file_stats->TransferUrl = url;
    url_token = strtok( url, ":/" );
    url_token = strtok( NULL, "/" );
    _this_file_stats->TransferHostName = url_token;

    // Set the host name of the local machine using getaddrinfo().
    struct addrinfo hints, *info;
    int addrinfo_result;

    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_CANONNAME;

    // Look up the host name. If this fails for any reason, do not include
    // it with the stats.
    if ( ( addrinfo_result = getaddrinfo( hostname, "http", &hints, &info ) ) == 0 ) {
        _this_file_stats->TransferLocalMachineName = info->ai_canonname;
    }

    // Cleanup and exit
    free( url );
    freeaddrinfo( info );
}

size_t
MultiFileCurlPlugin::HeaderCallback( char* buffer, size_t size, size_t nitems, void *userdata ) {
    fprintf(stderr, "[MultiFileCurlPlugin::HeaderCallback] called\n");
    auto ft_stats = static_cast<FileTransferStats*>(userdata);

    const char* delimiters = " \r\n";
    size_t numBytes = nitems * size;

    // In some unique cases, ftstats get passed in as null. If this happens, abort.
    if( !ft_stats ) return numBytes;

    // Parse this HTTP header
    // We should probably add more error checking to this parse method...
    char* token = strtok( buffer, delimiters );
    while( token ) {
        // X-Cache header provides details about cache hits
        if( strcmp ( token, "X-Cache:" ) == 0 ) {
            token = strtok( NULL, delimiters );
            ft_stats->HttpCacheHitOrMiss = token;
        }
        // Via header provides details about cache host
        else if( strcmp ( token, "Via:" ) == 0 ) {
            // The next token is a version number. We can ignore it.
            token = strtok( NULL, delimiters );
            // Next comes the actual cache host
            if( token != NULL ) {
                token = strtok( NULL, delimiters );
                ft_stats->HttpCacheHost = token;
            }
        }
        token = strtok( NULL, delimiters );
    }
    return numBytes;
}

size_t
MultiFileCurlPlugin::FtpWriteCallback( void* buffer, size_t size, size_t nmemb, void* stream ) {
    FILE* outfile = ( FILE* ) stream;
    return fwrite( buffer, size, nmemb, outfile); 
}


void
MultiFileCurlPlugin::ParseAds() {

        // Look in the job ad for speed limits; job ad is mandatory
    const char *job_ad_env = getenv("_CONDOR_JOB_AD");
    if (!job_ad_env) {
        return;
    }
        // If not present in the job ad, the machine ad can also contain
        // default limits; machine ad is optional.
    const char *machine_ad_env = getenv("_CONDOR_MACHINE_AD");

    std::unique_ptr<FILE,decltype(&fclose)> fp(nullptr, fclose);
    fp.reset(safe_fopen_wrapper(job_ad_env, "r"));
    if (!fp) {
        return;
    }

    int error;
    bool is_eof;
    classad::ClassAd job_ad;
    if (InsertFromFile(fp.get(), job_ad, is_eof, error) < 0) {
        return;
    }

    classad::ClassAd machine_ad;
    if (machine_ad_env) {
        fp.reset(safe_fopen_wrapper(machine_ad_env, "r"));
        if (fp) {
                // Note we ignore errors; failure to parse machine ad
                // is not fatal.
            InsertFromFile(fp.get(), machine_ad, is_eof, error);
        }
    }

    job_ad.ChainToAd(&machine_ad);

    int speed_limit;
    if (job_ad.EvaluateAttrInt("LowSpeedLimit", speed_limit)) {
        m_speed_limit = speed_limit;
    }
    int speed_time;
    if (job_ad.EvaluateAttrInt("LowSpeedTime", speed_time)) {
        m_speed_time = speed_time;
    }
}


int
main( int argc, char **argv ) {

    bool valid_inputs = true;
    FILE* output_file;
    bool diagnostic = false;
    bool upload = false;
    TransferPluginResult result;
    std::string input_filename;
    std::string output_filename;
    std::string transfer_files;

    // Check if this is a -classad request
    if ( argc == 2 ) {
        if ( strcmp( argv[1], "-classad" ) == 0 ) {
            printf( "%s",
                "MultipleFileSupport = true\n"
                "PluginVersion = \"0.2\"\n"
                "PluginType = \"FileTransfer\"\n"
                "SupportedMethods = \"http,https,ftp,file,dav,davs\"\n"
            );
            return (int)TransferPluginResult::Success;
        }
    }
    // If not, iterate over command-line arguments and set variables appropriately
    else {
        for( int i = 1; i < argc; i ++ ) {
            if ( strcmp( argv[i], "-infile" ) == 0 ) {
                if ( i < ( argc - 1 ) ) {
                    input_filename = argv[i+1];
                }
                else {
                    valid_inputs = false;
                }
            }
            if ( strcmp( argv[i], "-outfile" ) == 0 ) {
                if ( i < ( argc - 1 ) ) {
                    output_filename = argv[i+1];
                }
                else {
                    valid_inputs = false;
                }
            }
            if ( strcmp( argv[i], "-diagnostic" ) == 0 ) {
                diagnostic = true;
            }
            if ( strcmp( argv[i], "-upload" ) == 0 ) {
                upload = true;
            }
        }
    }

    if ( !valid_inputs || input_filename.empty() ) {
        fprintf( stderr, "Error: invalid arguments\n" );
        fprintf( stderr, "Usage: %s -infile <input-filename> -outfile <output-filename> [general-opts]\n\n", argv[0] );
        fprintf( stderr, "[general-opts] are:\n" );
        fprintf( stderr, "\t-diagnostic\t\tRun the plugin in diagnostic (verbose) mode\n\n" );
        fprintf( stderr, "\t-upload\t\tRun the plugin in upload mode, copying files to a remote location\n\n" );
        return -1;
    }

	// Mainly for testing to not wait forever to see errors
	const char *attempts_str = getenv("CONDOR_CURL_MAX_RETRY_ATTEMPTS");
	if (attempts_str) {
		if (atoi(attempts_str) > 0) {
			max_retry_attempts = atoi(attempts_str);
		}
	}

    // Instantiate a MultiFileCurlPlugin object and handle the request
    MultiFileCurlPlugin curl_plugin( diagnostic );
    if( curl_plugin.InitializeCurl() != 0 ) {
        fprintf( stderr, "ERROR: curl_plugin failed to initialize. Aborting.\n" );
        return (int)TransferPluginResult::Error;
    }

    // Do the transfer(s)
    result = upload ?
             curl_plugin.UploadMultipleFiles( input_filename )
           : curl_plugin.DownloadMultipleFiles( input_filename );

    // Now that we've finished all transfers, write statistics to output file
    if( !output_filename.empty() ) {
        output_file = safe_fopen_wrapper( output_filename.c_str(), "w" );
        if( output_file == NULL ) {
            fprintf( stderr, "Unable to open curl_plugin output file: %s\n", output_filename.c_str() );
            return (int)TransferPluginResult::Error;
        }
        fprintf( output_file, "%s", curl_plugin.GetStats().c_str() );
        fclose( output_file );
    }
    else {
        printf( "%s\n", curl_plugin.GetStats().c_str() );
    }

    return (int)result;
}


