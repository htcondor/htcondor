#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_utils/file_transfer_stats.h"
#include "../condor_utils/condor_url.h"
#include "utc_time.h"
#include "file_transfer.h"

#ifdef WIN32
#define CURL_STATICLIB // this has to match the way the curl library was built.
#endif

#include <curl/curl.h>

#define MAX_RETRY_ATTEMPTS 20

int send_curl_request( char** argv, int diagnostic, CURL* handle, FileTransferStats* stats );
int server_supports_resume( CURL* handle, const char* url );
void init_stats( char **argv );
static size_t header_callback( char* buffer, size_t size, size_t nitems );
static size_t ftp_write_callback( void* buffer, size_t size, size_t nmemb, void* stream );

static FileTransferStats* ft_stats;

#define curl_easy_setopt_wrapper(handle, option, value) \
{ \
	CURLcode r = curl_easy_setopt(handle, option, value); \
	if (r != CURLE_OK) { \
		fprintf(stderr, "Can't setopt %d\n", option); \
	} \
}

int 
main( int argc, char **argv ) {
    CURL* handle = NULL;
    ClassAd stats_ad;
    FileTransferStats stats;
    int retry_count = 0;
    int rval = -1;
    int diagnostic = 0;
    std::string stats_string;

    // Point the global curl_stats pointer to our local object
    ft_stats = &stats;

    if(argc == 2 && strcmp(argv[1], "-classad") == 0) {
        printf("%s",
            "PluginVersion = \"0.1\"\n"
            "PluginType = \"FileTransfer\"\n"
            "SupportedMethods = \"http,https,ftp,file\"\n"
            );

        return 0;
    }

    if ((argc > 3) && ! strcmp(argv[3],"-diagnostic")) {
        diagnostic = 1;
    } 
    else if(argc != 3) {
        printf("Usage: %s SOURCE DEST\n", argv[0]);
        return 1;
    }

    // Initialize win32 + SSL socket libraries.
    // Do not initialize these separately! Doing so causes https:// transfers
    // to segfault.
    int init = curl_global_init( CURL_GLOBAL_DEFAULT );
    if( init != 0 ) {
        fprintf( stderr, "Error: curl_plugin initialization failed with error"
                                                " code %d\n", init ); 
        return -1;
    }

    if ( ( handle = curl_easy_init() ) == NULL ) {
        return -1;
    }

    // Initialize the stats structure
    init_stats( argv );
    stats.TransferStartTime = condor_gettimestamp_double();

    // Enter the loop that will attempt/retry the curl request
    for(;;) {
    
        // The sleep function is defined differently in Windows and Linux
        #ifdef WIN32
            Sleep( ( retry_count++ ) * 1000 );
        #else
            sleep( retry_count++ );
        #endif
        
        rval = send_curl_request( argv, diagnostic, handle, ft_stats );

        // If curl request is successful, break out of the loop
        if( rval == CURLE_OK ) {    
            break;
        }
        // If we have not exceeded the maximum number of retries, and we encounter
        // a non-fatal error, stay in the loop and try again
        else if( retry_count <= MAX_RETRY_ATTEMPTS && 
                                  ( rval == CURLE_COULDNT_CONNECT ||
                                    rval == CURLE_PARTIAL_FILE || 
                                    rval == CURLE_READ_ERROR || 
                                    rval == CURLE_OPERATION_TIMEDOUT || 
                                    rval == CURLE_SEND_ERROR || 
                                    rval == CURLE_RECV_ERROR ) ) {
            continue;
        }
        // On fatal errors, break out of the loop
        else {
            break;
        }
    }

    stats.TransferEndTime = condor_gettimestamp_double();

    // If the transfer was successful, output the statistics to stdout
    if( rval != -1 ) {
        stats.Publish( stats_ad );
        sPrintAd( stats_string, stats_ad );
        fprintf( stdout, "%s", stats_string.c_str() );
    }

    // Cleanup 
    curl_easy_cleanup(handle);
    curl_global_cleanup();

    if ( rval != 0 ) {
        return (int)TransferPluginResult::Error;
    }

    return (int)TransferPluginResult::Success;
}

/*
    Perform the curl request, and output the results either to file to stdout.
*/
int 
send_curl_request( char** argv, int diagnostic, CURL* handle, FileTransferStats* stats ) {

    char error_buffer[CURL_ERROR_SIZE];
    char partial_range[20];
    double bytes_downloaded;    
    double bytes_uploaded; 
    double transfer_connection_time;
    double transfer_total_time;
    FILE *file = NULL;
    long return_code;
    int rval = -1;
    static int partial_file = 0;
    static long partial_bytes = 0;

    std::string scheme_suffix = IsUrl(argv[1]) ?
        getURLType(argv[1], true) : "";

    // Input transfer: URL -> file
    if ( scheme_suffix == "http" || scheme_suffix == "https" ||
         scheme_suffix == "ftp" || scheme_suffix == "file" ) {

        // Everything prior to the first '+' is the credential name.
        std::string full_scheme = getURLType(argv[1], false);
        auto offset = full_scheme.find_last_of("+");
        auto cred = (offset == std::string::npos) ? "" : full_scheme.substr(0, offset);

        // The actual transfer should only be everything after the last '+'
        std::string full_url(argv[1]);
        if (offset != std::string::npos) {
            full_url = full_url.substr(offset+1);
        }

        int close_output = 1;
        if ( ! strcmp(argv[2],"-") ) {
            file = stdout;
            close_output = 0;
            if ( diagnostic ) {
                fprintf( stderr, "fetching %s to stdout\n", full_url.c_str() );
            }
        } 
        else {
            file = partial_file ? fopen( argv[2], "a+" ) : fopen(argv[2], "w" ); 
            close_output = 1;
            if ( diagnostic ) { 
                fprintf( stderr, "fetching %s to %s\n", full_url.c_str(), argv[2] );
            }
        }

        if( file ) {
            // Libcurl options that apply to all transfer protocols
            curl_easy_setopt_wrapper( handle, CURLOPT_URL, full_url.c_str() );
            curl_easy_setopt_wrapper( handle, CURLOPT_CONNECTTIMEOUT, 60 );
            curl_easy_setopt_wrapper( handle, CURLOPT_WRITEDATA, file );

            // Libcurl options for HTTP, HTTPS and FILE
            if( scheme_suffix == "http" ||
                    scheme_suffix == "https" ||
                    scheme_suffix == "file" ) {
                curl_easy_setopt_wrapper( handle, CURLOPT_FOLLOWLOCATION, 1 );
                curl_easy_setopt_wrapper( handle, CURLOPT_HEADERFUNCTION, header_callback );
            }
            // Libcurl options for FTP
            else if( scheme_suffix == "ftp" ) {
                curl_easy_setopt_wrapper( handle, CURLOPT_WRITEFUNCTION, ftp_write_callback );
            }

            // We need to set CURLOPT_FAILONERROR to 0 so the client doesn't
            // die immediately a 401 or 500 error (which sometimes causes a
            // broken socket on the server).
            // However with this setting, HTTP errors (401, 500, etc.) are
            // considered successful, so we need to manually check for these.
            curl_easy_setopt_wrapper( handle, CURLOPT_FAILONERROR, 0 );
            
            if( diagnostic ) {
                curl_easy_setopt_wrapper( handle, CURLOPT_VERBOSE, 1 );
            }

            // If we are attempting to resume a download, set additional flags
            if( partial_file ) {
                sprintf( partial_range, "%lu-", partial_bytes );
                curl_easy_setopt_wrapper( handle, CURLOPT_RANGE, partial_range );
            }

            // Setup a buffer to store error messages. For debug use.
            error_buffer[0] = 0;
            curl_easy_setopt_wrapper( handle, CURLOPT_ERRORBUFFER, error_buffer ); 

            // Does curl protect against redirect loops otherwise?  It's
            // unclear how to tune this constant.
            // curl_easy_setopt_wrapper(handle, CURLOPT_MAXREDIRS, 1000);
            
            // Update some statistics
            stats->TransferType = "download";
            stats->TransferTries += 1;

            // Perform the curl request
            rval = curl_easy_perform( handle );

            // Check if the request completed partially. If so, set some
            // variables so we can attempt a resume on the next try.
            if( rval == CURLE_PARTIAL_FILE ) {
                if( server_supports_resume( handle, full_url.c_str() ) ) {
                    partial_file = 1;
                    partial_bytes = ftell( file );
                }
            }

            // Gather more statistics
            curl_easy_getinfo( handle, CURLINFO_SIZE_DOWNLOAD, &bytes_downloaded );
            curl_easy_getinfo( handle, CURLINFO_CONNECT_TIME, &transfer_connection_time );
            curl_easy_getinfo( handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
            curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &return_code );
            
            stats->TransferTotalBytes += ( long ) bytes_downloaded;
            stats->ConnectionTimeSeconds +=  ( transfer_total_time - transfer_connection_time );
            stats->TransferHTTPStatusCode = return_code;

            // Make sure to check the return code here as well as rval!
            // HTTP error codes like 401, 500 are considered successful by 
            // libcurl, but we want to treat them as errors.
            if( rval == CURLE_OK && return_code <= 400 ) {
                stats->TransferSuccess = true;
                stats->TransferError = "";
                stats->TransferFileBytes = ftell( file );
            }
            else {
                stats->TransferSuccess = false;
                stats->TransferError = error_buffer;
                // If we got an HTTP error code, need to change a couple more things
                if( rval != CURLE_OK ) {
                    const char * curl_err = curl_easy_strerror( ( CURLcode ) rval );
                    if (curl_err) {
                        stats->TransferError = "Client library encountered an error: " + std::string(curl_err);
                    } else {
                        stats->TransferError = "Client library encountered an unknown error.";
                    }
                } else if( return_code > 400 ) {
                    rval = CURLE_HTTP_RETURNED_ERROR;
                    stats->TransferError = "Server returned HTTP error code " + std::to_string((long long int)return_code);
                }
            }

            // Error handling and cleanup
            if( diagnostic && rval ) {
                fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n", 
                        rval, curl_easy_strerror( ( CURLcode ) rval ) ); 
            }
            if( close_output ) {
                fclose( file ); 
                file = NULL;
            }
        }
        else {
            fprintf( stderr, "ERROR: could not open output file %s\n", argv[2] ); 
        }
    } 
    
    // Output transfer: file -> URL
    else {
        int close_input = 1;
        int content_length = 0;
        struct stat file_info;

        if ( !strcmp(argv[1], "-") ) {
            fprintf( stderr, "ERROR: must provide a filename for curl_plugin uploads\n" ); 
            exit(1);
        } 

        // Verify that the specified file exists, and check its content length 
        file = fopen( argv[1], "r" );
        if( !file ) {
            fprintf( stderr, "ERROR: File %s could not be opened\n", argv[1] );
            exit(1);
        }
        if( fstat( fileno( file ), &file_info ) == -1 ) {
            fprintf( stderr, "ERROR: fstat failed to read file %s\n", argv[1] );
            exit(1);
        }
        content_length = file_info.st_size;
        close_input = 1;
        if ( diagnostic ) { 
            fprintf( stderr, "sending %s to %s\n", argv[1], argv[2] ); 
        }

        // Set curl upload options
        curl_easy_setopt_wrapper( handle, CURLOPT_URL, argv[2] );
        curl_easy_setopt_wrapper( handle, CURLOPT_UPLOAD, 1 );
        curl_easy_setopt_wrapper( handle, CURLOPT_READDATA, file );
        curl_easy_setopt_wrapper( handle, CURLOPT_FOLLOWLOCATION, -1 );
        curl_easy_setopt_wrapper( handle, CURLOPT_INFILESIZE_LARGE, 
                                        (curl_off_t) content_length );
        curl_easy_setopt_wrapper( handle, CURLOPT_FAILONERROR, 1 );
        if( diagnostic ) {
            curl_easy_setopt_wrapper( handle, CURLOPT_VERBOSE, 1 );
        }
    
        // Does curl protect against redirect loops otherwise?  It's
        // unclear how to tune this constant.
        // curl_easy_setopt_wrapper(handle, CURLOPT_MAXREDIRS, 1000);

        // Gather some statistics
        stats->TransferType = "upload";
        stats->TransferTries += 1;

        // Perform the curl request
        rval = curl_easy_perform( handle );

        // Gather more statistics
        curl_easy_getinfo( handle, CURLINFO_SIZE_UPLOAD, &bytes_uploaded );
        curl_easy_getinfo( handle, CURLINFO_CONNECT_TIME, &transfer_connection_time );
        curl_easy_getinfo( handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
        curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &return_code );
        
        stats->TransferTotalBytes += ( long ) bytes_uploaded;
        stats->ConnectionTimeSeconds += transfer_total_time - transfer_connection_time;
        stats->TransferHTTPStatusCode = return_code;

        if( rval == CURLE_OK ) {
            stats->TransferSuccess = true;    
            stats->TransferError = "";
            stats->TransferFileBytes = ftell( file );
        }
        else {
            stats->TransferSuccess = false;
            stats->TransferError = error_buffer;
        }

        // Error handling and cleanup
        if ( diagnostic && rval ) {
            fprintf( stderr, "curl_easy_perform returned CURLcode %d: %s\n",
                    rval, curl_easy_strerror( ( CURLcode )rval ) );
        }
        if ( close_input ) {
            fclose( file ); 
            file = NULL;
        }

    }
    return rval;    // 0 on success
}

/*
    Check if this server supports resume requests using the HTTP "Range" header
    by sending a Range request and checking the return code. Code 206 means
    resume is supported, code 200 means not supported. 
    Return 1 if resume is supported, 0 if not.
*/
int 
server_supports_resume( CURL* handle, const char* url ) {

    int rval = -1;

    // Send a basic request, with Range set to a null range
    curl_easy_setopt_wrapper( handle, CURLOPT_URL, url );
    curl_easy_setopt_wrapper( handle, CURLOPT_CONNECTTIMEOUT, 60 );
    curl_easy_setopt_wrapper( handle, CURLOPT_RANGE, "0-0" );
    
    rval = curl_easy_perform(handle);

    // Check the HTTP status code that was returned
    if( rval == 0 ) {
        char* finalURL = NULL;
        rval = curl_easy_getinfo( handle, CURLINFO_EFFECTIVE_URL, &finalURL );

        if( rval == 0 ) {
            if( strstr( finalURL, "http" ) == finalURL ) {
                long httpCode = 0;
                rval = curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &httpCode );

                // A 206 status code indicates resume is supported. Return true!
                if( httpCode == 206 ) {
                    return 1;
                }
            }
        }
    }

    // If we've gotten this far the server does not support resume. Clear the 
    // HTTP "Range" header and return false.
	
    curl_easy_setopt_wrapper( handle, CURLOPT_RANGE, NULL );
    return 0;    
}

/*
    Initialize the stats ClassAd
*/
void 
init_stats( char **argv ) {
    
    char* request_url = strdup( argv[1] );
    char* url_token;
    
     // Set the transfer protocol. If it's not http, ftp and file, then just
    // leave it blank because this transfer will fail quickly.
    if ( !strncasecmp( request_url, "http://", 7 ) ) {
        ft_stats->TransferProtocol = "http";
    }
    else if ( !strncasecmp( request_url, "https://", 8 ) ) {
        ft_stats->TransferProtocol = "https";
    }
    else if ( !strncasecmp( request_url, "ftp://", 6 ) ) {
        ft_stats->TransferProtocol = "ftp";
    }
    else if ( !strncasecmp( request_url, "file://", 7 ) ) {
        ft_stats->TransferProtocol = "file";
    }
    
    // Set the request host name by parsing it out of the URL
    ft_stats->TransferUrl = request_url;
    url_token = strtok( request_url, ":/" );
    url_token = strtok( NULL, "/" );
    ft_stats->TransferHostName = url_token;

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
        ft_stats->TransferLocalMachineName = info->ai_canonname;
    }

    // Cleanup and exit
    free( request_url );
    freeaddrinfo( info );
}

/*
    Callback function provided by libcurl, which is called upon receiving
    HTTP headers. We use this to gather statistics.
*/
static size_t 
header_callback( char* buffer, size_t size, size_t nitems ) {
    
    const char* delimiters = " \r\n";
    size_t numBytes = nitems * size;

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

/*
    Write callback function for FTP file transfers.
*/
static size_t 
ftp_write_callback( void* buffer, size_t size, size_t nmemb, void* stream ) {
    FILE* outfile = ( FILE* ) stream;
    return fwrite( buffer, size, nmemb, outfile); 
 
}
