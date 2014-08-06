#ifdef WIN32
#include <windows.h>
#define CURL_STATICLIB // this has to match the way the curl library was built.
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

#include <curl/curl.h>
#include <string.h>

int main(int argc, char **argv) {
	CURL *handle = NULL;
	int rval = -1;
	FILE *file = NULL;
	int   diagnostic = 0;

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
	curl_global_init(CURL_GLOBAL_WIN32);

	if ( (handle = curl_easy_init()) == NULL ) {
		return -1;
	}

	if ( !strncasecmp( argv[1], "http://", 7 ) ||
		 !strncasecmp( argv[1], "ftp://", 6 ) ||
		 !strncasecmp( argv[1], "file://", 7 ) ) {

		// Input transfer: URL -> file
		int close_output = 1;
		if ( ! strcmp(argv[2],"-")) {
			file = stdout;
			close_output = 0;
			if (diagnostic) { fprintf(stderr, "fetching %s to stdout\n", argv[1]); }
		} else {
			file = fopen(argv[2], "w");
			close_output = 1;
			if (diagnostic) { fprintf(stderr, "fetching %s to %s\n", argv[1], argv[2]); }
		}
		if(file) {
			curl_easy_setopt(handle, CURLOPT_URL, argv[1]);
			curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);
			curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, -1);
			// Does curl protect against redirect loops otherwise?  It's
			// unclear how to tune this constant.
			// curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 1000);
			rval = curl_easy_perform(handle);

			if (diagnostic && rval) {
				fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n", 
						rval, curl_easy_strerror((CURLcode)rval)); 
			}
			if (close_output) {
				fclose(file); file = NULL; 
			}

			if( rval == 0 ) {
				char * finalURL = NULL;
				rval = curl_easy_getinfo( handle, CURLINFO_EFFECTIVE_URL, & finalURL );

				if( rval == 0 ) {
					if( strstr( finalURL, "http" ) == finalURL ) {
						long httpCode = 0;
						rval = curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, & httpCode );

						if( rval == 0 ) {
							if( httpCode != 200 ) { rval = 1; }
						}
					}
				}
			}
		}
	} else {
		// Output transfer: file -> URL
		int close_input = 1;
		if ( ! strcmp(argv[1],"-")) {
			file = stdin;
			close_input = 0;
			if (diagnostic) { fprintf(stderr, "sending stdin to %s\n", argv[2]); }
		} else {
			file = fopen(argv[1], "r");
			close_input = 1;
			if (diagnostic) { fprintf(stderr, "sending %s to %s\n", argv[1], argv[2]); }
		}
		if(file) {
			curl_easy_setopt(handle, CURLOPT_URL, argv[2]);
			curl_easy_setopt(handle, CURLOPT_UPLOAD, 1);
			curl_easy_setopt(handle, CURLOPT_READDATA, file);
			curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, -1);
			// Does curl protect against redirect loops otherwise?  It's
			// unclear how to tune this constant.
			// curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 1000);
			rval = curl_easy_perform(handle);

			if (diagnostic && rval) {
				fprintf(stderr, "curl_easy_perform returned CURLcode %d: %s\n",
						rval, curl_easy_strerror((CURLcode)rval));
			}
			if (close_input) {
				fclose(file); file = NULL;
			}

			if( rval == 0 ) {
				char * finalURL = NULL;
				rval = curl_easy_getinfo( handle, CURLINFO_EFFECTIVE_URL, & finalURL );

				if( rval == 0 ) {
					if( strstr( finalURL, "http" ) == finalURL ) {
						long httpCode = 0;
						rval = curl_easy_getinfo( handle, CURLINFO_RESPONSE_CODE, & httpCode );

						if( rval == 0 ) {
							if( httpCode != 200 ) { rval = 1; }
						}
					}
				}
			}
		}

	}

	curl_easy_cleanup(handle);

	curl_global_cleanup();

	return rval;	// 0 on success
}
