#include "condor_common.h"
#include "condor_classad.h"
#include "../condor_utils/file_transfer_stats.h"
#include "multifile_curl_plugin.h"
#include "utc_time.h"
#include <iostream>

#define MAX_RETRY_ATTEMPTS 20

using namespace std;

MultiFileCurlPlugin::MultiFileCurlPlugin( int diagnostic ) :
    _handle ( NULL ),
    _this_file_stats ( NULL ),
    _diagnostic ( diagnostic ),
    _all_files_stats ( "" )
{}

MultiFileCurlPlugin::~MultiFileCurlPlugin() {
    curl_easy_cleanup( _handle );
    curl_global_cleanup();
}

int
MultiFileCurlPlugin::InitializeCurl() {
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

int 
MultiFileCurlPlugin::DownloadFile( const char* url, const char* local_file_name ) {

    char error_buffer[CURL_ERROR_SIZE];
    char partial_range[20];
    double bytes_downloaded;
    double transfer_connection_time;
    double transfer_total_time;
    FILE *file = NULL;
    long return_code;
    int rval = -1;
    static int partial_file = 0;
    static long partial_bytes = 0;

    int close_output = 1;
    if ( !strcmp( local_file_name, "-" ) ) {
        file = stdout;
        close_output = 0;
        if ( _diagnostic ) { 
            fprintf( stderr, "Fetching %s to stdout\n", url ); 
        }
    }
    else {
        file = partial_file ? fopen( local_file_name, "a+" ) : fopen( local_file_name, "w" ); 
        close_output = 1;
        if ( _diagnostic ) { 
            fprintf( stderr, "Fetching %s to %s\n", url, local_file_name ); 
        }
    }

    if( file ) {
        // Libcurl options that apply to all transfer protocols
        curl_easy_setopt( _handle, CURLOPT_URL, url );
        curl_easy_setopt( _handle, CURLOPT_CONNECTTIMEOUT, 60 );
        curl_easy_setopt( _handle, CURLOPT_WRITEDATA, file );

        // Libcurl options for HTTP, HTTPS and FILE
        if( !strncasecmp( url, "http://", 7 ) || 
                            !strncasecmp( url, "https://", 8 ) || 
                            !strncasecmp( url, "file://", 7 ) ) {
            curl_easy_setopt( _handle, CURLOPT_FOLLOWLOCATION, 1 );
            curl_easy_setopt( _handle, CURLOPT_HEADERFUNCTION, this->HeaderCallback );
        }
        // Libcurl options for FTP
        else if( !strncasecmp( url, "ftp://", 6 ) ) {
            curl_easy_setopt( _handle, CURLOPT_WRITEFUNCTION, this->FtpWriteCallback );
        }

        // * If the following option is set to 0, then curl_easy_perform()
        // returns 0 even on errors (404, 500, etc.) So we can't identify
        // some failures. I don't think it's supposed to do that?
        // * If the following option is set to 1, then something else bad
        // happens? 500 errors fail before we see HTTP headers but I don't
        // think that's a big deal.
        // * Let's keep it set to 1 for now.
        curl_easy_setopt( _handle, CURLOPT_FAILONERROR, 1 );

        if( _diagnostic ) {
            curl_easy_setopt( _handle, CURLOPT_VERBOSE, 1 );
        }

        // If we are attempting to resume a download, set additional flags
        if( partial_file ) {
            sprintf( partial_range, "%lu-", partial_bytes );
            curl_easy_setopt( _handle, CURLOPT_RANGE, partial_range );
        }

        // Setup a buffer to store error messages. For debug use.
        error_buffer[0] = 0;
        curl_easy_setopt( _handle, CURLOPT_ERRORBUFFER, error_buffer ); 

        // Does curl protect against redirect loops otherwise?  It's
        // unclear how to tune this constant.
        // curl_easy_setopt(_handle, CURLOPT_MAXREDIRS, 1000);

        // Update some statistics
        _this_file_stats->TransferType = "download";
        _this_file_stats->TransferTries += 1;

        // Perform the curl request
        rval = curl_easy_perform( _handle );

        // Check if the request completed partially. If so, set some
        // variables so we can attempt a resume on the next try.
        if( rval == CURLE_PARTIAL_FILE ) {
            if( ServerSupportsResume( url ) ) {
                partial_file = 1;
                partial_bytes = ftell( file );
            }
        }

        // Gather more statistics
        curl_easy_getinfo( _handle, CURLINFO_SIZE_DOWNLOAD, &bytes_downloaded );
        curl_easy_getinfo( _handle, CURLINFO_CONNECT_TIME, &transfer_connection_time );
        curl_easy_getinfo( _handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
        curl_easy_getinfo( _handle, CURLINFO_RESPONSE_CODE, &return_code );

        _this_file_stats->TransferTotalBytes += ( long ) bytes_downloaded;
        _this_file_stats->ConnectionTimeSeconds +=  ( transfer_total_time - transfer_connection_time );
        _this_file_stats->TransferReturnCode = return_code;

        if( rval == CURLE_OK ) {
            _this_file_stats->TransferSuccess = true;
            _this_file_stats->TransferError = "";
            _this_file_stats->TransferFileBytes = ftell( file );
        }
        else {
            _this_file_stats->TransferSuccess = false;
            _this_file_stats->TransferError = error_buffer;
        }

        // Error handling and cleanup
        if( _diagnostic && rval ) {
            fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n", 
                    rval, curl_easy_strerror( ( CURLcode ) rval ) ); 
        }
        if( close_output ) {
            fclose( file ); 
            file = NULL;
        }
    }
    else {
        fprintf( stderr, "ERROR: could not open output file %s, error %d (%s)\n", local_file_name, errno, strerror(errno) ); 
    }

    return rval;
}

int
MultiFileCurlPlugin::DownloadMultipleFiles( string input_filename ) {

    classad::ClassAd stats_ad;
    classad::ClassAdUnParser unparser;
    CondorClassAdFileIterator adFileIter;
    FILE* input_file;
    map<string, transfer_request> requested_files;
    string stats_string;
    int retry_count;
    int rval = 0;

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
        string local_file_name;
        string url;
        transfer_request request_details;
        std::pair< string, transfer_request > this_request;

        while ( adFileIter.next( transfer_file_ad ) > 0 ) {
            transfer_file_ad.EvaluateAttrString( "DownloadFileName", local_file_name );
            transfer_file_ad.EvaluateAttrString( "Url", url );
            request_details.local_file_name = local_file_name;
            this_request = std::make_pair( url, request_details );
            requested_files.insert( this_request );
        }
    }
    fclose(input_file);

    // Iterate over the map of files to transfer.
    for ( std::map<string, transfer_request>::iterator it = requested_files.begin(); it != requested_files.end(); ++it ) {

        string local_file_name = it->second.local_file_name;
        string url = it->first;
        retry_count = 0;

        // Initialize the stats structure for this transfer.
        _this_file_stats = new FileTransferStats();
        InitializeStats( url );
        _this_file_stats->TransferStartTime = condor_gettimestamp_double();

        // Point the global static pointer to the _this_file_stats class member.
        // This allows us to access it from static callback functions.
        _global_ft_stats = _this_file_stats;

        // Enter the loop that will attempt/retry the curl request
        for ( ;; ) {

            // The sleep function is defined differently in Windows and Linux
            #ifdef WIN32
                Sleep( ( retry_count++ ) * 1000 );
            #else
                sleep( retry_count++ );
            #endif

            rval = DownloadFile( url.c_str(), local_file_name.c_str() );

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

        _this_file_stats->TransferEndTime = condor_gettimestamp_double();

        // Regardless of success/failure, update the stats
        _this_file_stats->Publish( stats_ad );
        unparser.Unparse( stats_string, &stats_ad );
        _all_files_stats += stats_string;
        delete _this_file_stats;
        stats_ad.Clear();
        stats_string = "";

        // If the transfer did fail, break out of the loop immediately
        if ( rval > 0 ) break;
    }

    return rval;
}

/*
    Check if this server supports resume requests using the HTTP "Range" header
    by sending a Range request and checking the return code. Code 206 means
    resume is supported, code 200 means not supported. 
    Return: 1 if resume is supported, 0 if not.
*/
int 
MultiFileCurlPlugin::ServerSupportsResume( const char* url ) {

    int rval = -1;

    // Send a basic request, with Range set to a null range
    curl_easy_setopt( _handle, CURLOPT_URL, url );
    curl_easy_setopt( _handle, CURLOPT_CONNECTTIMEOUT, 60 );
    curl_easy_setopt( _handle, CURLOPT_RANGE, "0-0" );

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
    curl_easy_setopt( _handle, CURLOPT_RANGE, NULL );
    return 0;
}

void
MultiFileCurlPlugin::InitializeStats( string request_url ) {

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
MultiFileCurlPlugin::HeaderCallback( char* buffer, size_t size, size_t nitems ) {
    const char* delimiters = " \r\n";
    size_t numBytes = nitems * size;

    // Parse this HTTP header
    // We should probably add more error checking to this parse method...
    char* token = strtok( buffer, delimiters );
    while( token ) {
        // X-Cache header provides details about cache hits
        if( strcmp ( token, "X-Cache:" ) == 0 ) {
            token = strtok( NULL, delimiters );
            _global_ft_stats->HttpCacheHitOrMiss = token;
        }
        // Via header provides details about cache host
        else if( strcmp ( token, "Via:" ) == 0 ) {
            // The next token is a version number. We can ignore it.
            token = strtok( NULL, delimiters );
            // Next comes the actual cache host
            if( token != NULL ) {
                token = strtok( NULL, delimiters );
                _global_ft_stats->HttpCacheHost = token;
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


int
main( int argc, char **argv ) {

    bool valid_inputs = true;
    FILE* output_file;
    int diagnostic = 0;
    int rval = 0;
    string input_filename;
    string output_filename;
    string transfer_files;

    // Check if this is a -classad request
    if ( argc == 2 ) {
        if ( strcmp( argv[1], "-classad" ) == 0 ) {
            printf( "%s",
                "MultipleFileSupport = true\n"
                "PluginVersion = \"0.2\"\n"
                "PluginType = \"FileTransfer\"\n"
                "SupportedMethods = \"http,https,ftp,file\"\n"
            );
            return 0;
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
                diagnostic = 1;
            }
        }
    }

    if ( !valid_inputs || input_filename.empty() ) {
        fprintf( stderr, "Error: invalid arguments\n" );
        fprintf( stderr, "Usage: curl_plugin -infile <input-filename> -outfile <output-filename> [general-opts]\n\n" );
        fprintf( stderr, "[general-opts] are:\n" );
        fprintf( stderr, "\t-diagnostic\t\tRun the plugin in diagnostic (verbose) mode\n\n" );
        return 1;
    }

    // Instantiate a MultiFileCurlPlugin object and handle the request
    MultiFileCurlPlugin curl_plugin( diagnostic );
    if( curl_plugin.InitializeCurl() != 0 ) {
        fprintf( stderr, "ERROR: curl_plugin failed to initialize. Aborting.\n" );
        return 1;
    }

    // Do the transfer(s)
    rval = curl_plugin.DownloadMultipleFiles( input_filename );

    // Now that we've finished all transfers, write statistics to output file
    if( !output_filename.empty() ) {
        output_file = safe_fopen_wrapper( output_filename.c_str(), "w" );
        if( output_file == NULL ) {
            fprintf( stderr, "Unable to open curl_plugin output file: %s\n", output_filename.c_str() );
            return 1;
        }
        fprintf( output_file, curl_plugin.GetStats().c_str() );
        fclose( output_file );
    }
    else {
        printf( "%s\n", curl_plugin.GetStats().c_str() );
    }

    // 0 on success, error code >= 1 on failure
    return rval;
}


