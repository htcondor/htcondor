#ifdef WIN32
#include <windows.h>
#define CURL_STATICLIB // this has to match the way the curl library was built.
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#include "condor_common.h"
#include "condor_config.h"
#include "condor_debug.h"
#include "condor_classad.h"
#include "MyString.h"

#include <curl/curl.h>
#include <sys/stat.h>
#include <string.h>

#define MAX_RETRY_ATTEMPTS 20

int send_curl_request( char** argv, int diagnostic, CURL* handle, ClassAd* stats );
int server_supports_resume( CURL* handle, char* url );
void init_stats( ClassAd* stats, char **argv );
static size_t header_callback( char *buffer, size_t size, size_t nitems );
static size_t ftp_write_callback( void* buffer, size_t size, size_t nmemb, void* stream );

static ClassAd* curl_stats;


int main( int argc, char **argv ) {
    CURL *handle = NULL;
    ClassAd stats;
    double start_time, end_time;
    int retry_count = 0;
    int rval = -1;
    int diagnostic = 0;
    MyString stats_string;

    // Point the global curl_stats pointer to our local object
    curl_stats = &stats;

    if(argc == 2 && strcmp(argv[1], "-classad") == 0) {
        printf("%s",
            "PluginVersion = \"0.1\"\n"
            "PluginType = \"FileTransfer\"\n"
            "SupportedMethods = \"http,ftp,file\"\n"
            );

        return 0;
    }

    if ((argc > 3) && ! strcmp(argv[3],"-diagnostic")) {
        diagnostic = 1;
    } else if(argc != 3) {
        return -1;
    }

    // Initialize win32 socket libraries, but not ssl
    curl_global_init( CURL_GLOBAL_WIN32 );

    if ( ( handle = curl_easy_init() ) == NULL ) {
        return -1;
    }

    // Initialize the stats structure
    init_stats( &stats, argv );
    start_time = _condor_debug_get_time_double();

    // Enter the loop that will attempt/retry the curl request
    for(;;) {
    
        // The sleep function is defined differently in Windows and Linux
        #ifdef WIN32
            Sleep( ( retry_count++ ) * 1000 );
        #else
            sleep( retry_count++ );
        #endif
        
        rval = send_curl_request( argv, diagnostic, handle, &stats );

        // If curl request is successful, break out of the loop
        if( rval == CURLE_OK ) {    
            break;
        }
        // If we have not exceeded the maximum number of retries, and we encounter
        // a non-fatal error, stay in the loop and try again
        else if( retry_count <= MAX_RETRY_ATTEMPTS && 
                                  ( rval == CURLE_COULDNT_CONNECT ||
                                    rval == CURLE_PARTIAL_FILE || 
                                    rval == CURLE_WRITE_ERROR || 
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

    // Record some statistics
    end_time = _condor_debug_get_time_double();   
    stats.Assign("TRANSFER_TIME_SECONDS", end_time - start_time);

    // If the transfer was successful, output the statistics to stdout
    if( rval != -1 ) {
        sPrintAd( stats_string, stats );
        fprintf(stdout, "%s", stats_string.c_str() );
    }

    // Cleanup 
    curl_easy_cleanup(handle);
    curl_global_cleanup();

    return rval;    // 0 on success
}

/*
    Perform the curl request, and output the results either to file to stdout.
*/
int send_curl_request( char** argv, int diagnostic, CURL *handle, ClassAd* stats ) {
 
    char error_buffer[CURL_ERROR_SIZE];
    char partial_range[20];
    double bytes_downloaded;    
    double bytes_uploaded; 
    double connected_time;
    double previous_connected_time;
    double transfer_connection_time;
    double transfer_total_time;
    FILE *file = NULL;
    long return_code;
    long previous_total_bytes;
    int previous_tries;
    int rval = -1;
    static int partial_file = 0;
    static long partial_bytes = 0;

    // Input transfer: URL -> file
    if ( !strncasecmp( argv[1], "http://", 7 ) ||
         !strncasecmp( argv[1], "ftp://", 6 ) ||
         !strncasecmp( argv[1], "file://", 7 ) ) {

        int close_output = 1;
        if ( ! strcmp(argv[2],"-")) {
            file = stdout;
            close_output = 0;
            if ( diagnostic ) { 
                fprintf( stderr, "fetching %s to stdout\n", argv[1] ); 
            }
        } 
        else {
            file = partial_file ? fopen( argv[2], "a+" ) : fopen(argv[2], "w" ); 
            close_output = 1;
            if ( diagnostic ) { 
                fprintf( stderr, "fetching %s to %s\n", argv[1], argv[2] ); 
            }
        }

        if( file ) {
            // Libcurl options that apply to all transfer protocols
            curl_easy_setopt( handle, CURLOPT_URL, argv[1] );
            curl_easy_setopt( handle, CURLOPT_CONNECTTIMEOUT, 60 );
            curl_easy_setopt( handle, CURLOPT_WRITEDATA, file );

            // Libcurl options for HTTP and FILE
            if( !strncasecmp( argv[1], "http://", 7 ) || 
                                !strncasecmp( argv[1], "file://", 7 ) ) {
                curl_easy_setopt( handle, CURLOPT_FOLLOWLOCATION, -1 );
                curl_easy_setopt( handle, CURLOPT_HEADERFUNCTION, header_callback );
            }
            // Libcurl options for FTP
            else if( !strncasecmp( argv[1], "ftp://", 6 ) ) {
                curl_easy_setopt( handle, CURLOPT_WRITEFUNCTION, ftp_write_callback );
            }

            // * If the following option is set to 0, then curl_easy_perform()
            // returns 0 even on errors (404, 500, etc.) So we can't identify
            // some failures. I don't think it's supposed to do that?
            // * If the following option is set to 1, then something else bad
            // happens? 500 errors fail before we see HTTP headers but I don't
            // think that's a big deal.
            // * Let's keep it set to 1 for now.
            curl_easy_setopt( handle, CURLOPT_FAILONERROR, 1 );
            
            if( diagnostic ) {
                curl_easy_setopt( handle, CURLOPT_VERBOSE, 1 );
            }

            // If we are attempting to resume a download, set additional flags
            if( partial_file ) {
                sprintf( partial_range, "%lu-", partial_bytes );
                curl_easy_setopt( handle, CURLOPT_RANGE, partial_range );
            }

            // Setup a buffer to store error messages. For debug use.
            error_buffer[0] = 0;
            curl_easy_setopt( handle, CURLOPT_ERRORBUFFER, error_buffer ); 

            // Does curl protect against redirect loops otherwise?  It's
            // unclear how to tune this constant.
            // curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 1000);
            
            // Gather some statistics
            stats->Assign( "TRANSFER_TYPE", "download" );
            stats->LookupInteger( "TRANSFER_TRIES", previous_tries );
            stats->Assign( "TRANSFER_TRIES", previous_tries + 1 );

            // Perform the curl request
            rval = curl_easy_perform( handle );

            // Check if the request completed partially. If so, set some
            // variables so we can attempt a resume on the next try.
            if( rval == CURLE_PARTIAL_FILE ) {
                if( server_supports_resume( handle, argv[1] ) ) {
                    partial_file = 1;
                    partial_bytes = ftell( file );
                }
            }

            // Gather more statistics
            stats->LookupInteger( "TRANSFER_TOTAL_BYTES", previous_total_bytes );
            curl_easy_getinfo( handle, CURLINFO_SIZE_DOWNLOAD, &bytes_downloaded );
            stats->Assign( "TRANSFER_TOTAL_BYTES", 
                        ( long ) ( previous_total_bytes + bytes_downloaded ) );

            stats->LookupFloat( "CONNECTION_TIME_SECONDS", previous_connected_time );
            curl_easy_getinfo( handle, CURLINFO_CONNECT_TIME, 
                            &transfer_connection_time );
            curl_easy_getinfo( handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
            connected_time = previous_connected_time + 
                            ( transfer_total_time - transfer_connection_time );
            stats->Assign( "CONNECTION_TIME_SECONDS", connected_time );
            
            curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &return_code );
            stats->Assign( "TRANSFER_RETURN_CODE", return_code );


            if( rval == CURLE_OK ) {
                stats->Assign( "TRANSFER_SUCCESS", true );
                stats->Delete( "TRANSFER_ERROR" );
                stats->Assign( "TRANSFER_FILE_BYTES", ftell( file ) );
            }
            else {
                stats->Assign( "TRANSFER_SUCCESS", false );
                stats->Assign( "TRANSFER_ERROR", error_buffer );
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
    } 
    
    // Output transfer: file -> URL
    else {
        int close_input = 1;
        int content_length = 0;
        struct stat file_info;

        if ( !strcmp(argv[1], "-") ) {
            fprintf( stderr, "ERROR: must provide a filename for curl_plugin uploads" ); 
            return 1;
        } 
        else {
            file = fopen( argv[1], "r" );
            fstat( fileno( file ), &file_info );
            content_length = file_info.st_size;
            close_input = 1;
            if ( diagnostic ) { 
                fprintf( stderr, "sending %s to %s\n", argv[1], argv[2] ); 
            }
        }
        if(file) {
            curl_easy_setopt( handle, CURLOPT_URL, argv[2] );
            curl_easy_setopt( handle, CURLOPT_UPLOAD, 1 );
            curl_easy_setopt( handle, CURLOPT_READDATA, file );
            curl_easy_setopt( handle, CURLOPT_FOLLOWLOCATION, -1 );
            curl_easy_setopt( handle, CURLOPT_INFILESIZE_LARGE, 
                                            (curl_off_t) content_length );
            curl_easy_setopt( handle, CURLOPT_FAILONERROR, 1 );
            if( diagnostic ) {
                curl_easy_setopt( handle, CURLOPT_VERBOSE, 1 );
            }
        
            // Does curl protect against redirect loops otherwise?  It's
            // unclear how to tune this constant.
            // curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 1000);

            // Gather some statistics
            stats->Assign( "TRANSFER_TYPE", "upload" );
            stats->LookupInteger( "TRANSFER_TRIES", previous_tries );
            stats->Assign( "TRANSFER_TRIES", previous_tries + 1 );

            // Perform the curl request
            rval = curl_easy_perform( handle );

            // Gather more statistics
            stats->LookupInteger( "TRANSFER_TOTAL_BYTES", previous_total_bytes );
            curl_easy_getinfo( handle, CURLINFO_SIZE_UPLOAD, &bytes_uploaded );
            stats->Assign( "TRANSFER_TOTAL_BYTES", 
                        ( long ) ( previous_total_bytes + bytes_uploaded ) );

            stats->LookupFloat( "CONNECTION_TIME_SECONDS", previous_connected_time );
            curl_easy_getinfo( handle, CURLINFO_CONNECT_TIME, 
                            &transfer_connection_time );
            curl_easy_getinfo( handle, CURLINFO_TOTAL_TIME, &transfer_total_time );
            connected_time = previous_connected_time + 
                            ( transfer_total_time - transfer_connection_time );
            stats->Assign( "CONNECTION_TIME_SECONDS", connected_time );
            
            curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, &return_code );
            stats->Assign( "TRANSFER_RETURN_CODE", return_code );

            if( rval == CURLE_OK ) {
                stats->Assign( "TRANSFER_SUCCESS", true );    
                stats->Assign( "TRANSFER_ERROR", NULL );
                stats->Assign( "TRANSFER_FILE_BYTES", ftell( file ) );
            }
            else {
                stats->Assign( "TRANSFER_SUCCESS", false );
                stats->Assign( "TRANSFER_ERROR", error_buffer );
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

    }
    return rval;    // 0 on success
}

/*
    Check if this server supports resume requests using the HTTP "Range" header
    by sending a Range request and checking the return code. Code 206 means
    resume is supported, code 200 means not supported. 
    Return 1 if resume is supported, 0 if not.
*/
int server_supports_resume( CURL* handle, char* url ) {

    int rval = -1;

    // Send a basic request, with Range set to a null range
    curl_easy_setopt( handle, CURLOPT_URL, url );
    curl_easy_setopt( handle, CURLOPT_CONNECTTIMEOUT, 60 );
    curl_easy_setopt( handle, CURLOPT_RANGE, "0-0" );
    
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
    curl_easy_setopt( handle, CURLOPT_RANGE, NULL );
    return 0;    
}

/*
    Initialize the stats ClassAd
*/
void init_stats( ClassAd* stats, char **argv ) {
    
    char* request_url = strdup( argv[1] );
    char* url_token;
    
    // Initial values
    stats->Assign( "TRANSFER_TOTAL_BYTES", 0 );
    stats->Assign( "TRANSFER_TRIES", 0 );
    stats->Assign( "CONNECTION_TIME_SECONDS", 0 );

    // Set the transfer protocol. If it's not http, ftp and file, then just
    // leave it blank because this transfer will fail quickly.
    if ( !strncasecmp( request_url, "http://", 7 ) ) {
        stats->Assign( "TRANSFER_PROTOCOL", "http" );
    }
    else if ( !strncasecmp( request_url, "ftp://", 6 ) ) {
        stats->Assign( "TRANSFER_PROTOCOL", "ftp" );
    }
    else if ( !strncasecmp( request_url, "file://", 7 ) ) {
        stats->Assign( "TRANSFER_PROTOCOL", "file" );
    }
    
    // Set the request host name by parsing it out of the URL
    url_token = strtok( request_url, ":/" );
    url_token = strtok( NULL, "/" );
    stats->Assign( "TRANSFER_HOST_NAME", url_token );

    // Cleanup and exit
    free( request_url );
}

/*
    Callback function provided by libcurl, which is called upon receiving
    HTTP headers. We use this to gather statistics.
*/
static size_t header_callback( char *buffer, size_t size, size_t nitems ) {
    
    size_t numBytes = nitems * size;

    // Parse this HTTP header
    // We should probably add more error checking to this parse method...
    char* token = strtok( buffer, " " );
    while( token ) {
        // X-Cache header provides details about cache hits
        if( strcmp ( token, "X-Cache:" ) == 0 ) {
            token = strtok(NULL, " ");
            curl_stats->Assign( "HTTP_CACHE_HIT_OR_MISS", token );
            curl_stats->Assign( "HTTP_USED_CACHE", true );
        }
        // Via header provides details about cache host
        else if( strcmp ( token, "Via:" ) == 0 ) {
            // The next token is a version number. We can ignore it.
            token = strtok( NULL, " " );
            // Next comes the actual cache host
            token = strtok( NULL, " " );  
            curl_stats->Assign( "HTTP_CACHE_HOST", token );
        }
        token = strtok( NULL, " " );
    }       
    return numBytes;
}

/*
    Write callback function for FTP file transfers.
*/
static size_t ftp_write_callback( void* buffer, size_t size, size_t nmemb, void* stream ) {
    FILE* outfile = ( FILE* ) stream;
    return fwrite( buffer, size, nmemb, outfile); 
 
}
