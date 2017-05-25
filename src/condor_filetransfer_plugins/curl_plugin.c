#ifdef WIN32
#include <windows.h>
#define CURL_STATICLIB // this has to match the way the curl library was built.
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#endif

#include <curl/curl.h>
#include <sys/stat.h>
#include <string.h>


#define MAX_RETRY_ATTEMPTS 20

int send_curl_request( char** argv, int diagnostic, CURL* handle );
int server_supports_resume( CURL* handle, char* url );

int main( int argc, char **argv ) {
	CURL *handle = NULL;
	int retry_count = 0;
	int rval = -1;
	int diagnostic = 0;

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
	} else if(argc != 3) {
		return -1;
	}

	// Initialize win32 socket libraries, but not ssl
	curl_global_init( CURL_GLOBAL_WIN32 );

	if ( ( handle = curl_easy_init() ) == NULL ) {
        return -1;
	}

	// Enter the loop that will attempt/retry the curl request
	for(;;) {
    
        // The sleep function is defined differently in Windows and Linux
        #ifdef WIN32
    		Sleep( ( retry_count++ ) * 1000 );
        #else
            sleep( retry_count++ );
        #endif
        
		rval = send_curl_request(argv, diagnostic, handle);

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
    curl_easy_cleanup(handle);
    curl_global_cleanup();
    return rval;	// 0 on success
}

/*
    Perform the curl request, and output the results either to file to stdout.
*/
int send_curl_request( char** argv, int diagnostic, CURL *handle ) {
 
    char error_buffer[CURL_ERROR_SIZE];
    char partial_range[20];
	FILE *file = NULL;
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
 			curl_easy_setopt( handle, CURLOPT_URL, argv[1] );
            curl_easy_setopt( handle, CURLOPT_CONNECTTIMEOUT, 60 );
			curl_easy_setopt( handle, CURLOPT_WRITEDATA, file );
			curl_easy_setopt( handle, CURLOPT_FOLLOWLOCATION, -1 );
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
            fprintf( stderr, "ERROR: stdin not supported for curl_plugin uploads" ); 
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
            curl_easy_setopt( handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t) content_length );
            curl_easy_setopt( handle, CURLOPT_FAILONERROR, 1 );
            if( diagnostic ) {
                curl_easy_setopt( handle, CURLOPT_VERBOSE, 1 );
            }
        
			// Does curl protect against redirect loops otherwise?  It's
			// unclear how to tune this constant.
			// curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 1000);

            // Perform the curl request
			rval = curl_easy_perform( handle );

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
    Return 1 if resume is supported, 0 if not supported.
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
