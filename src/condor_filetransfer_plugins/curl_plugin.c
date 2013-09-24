#include <curl/curl.h>
#include <string.h>

int main(int argc, char **argv) {
	CURL *handle = NULL;
	int rval = -1;
	FILE *file = NULL;

	if(argc == 2 && strcmp(argv[1], "-classad") == 0) {
		printf("%s",
			"PluginVersion = \"0.1\"\n"
			"PluginType = \"FileTransfer\"\n"
			"SupportedMethods = \"http,ftp,file\"\n"
			);

		return 0;
	}

	if(argc != 3) {
		return -1;
	}

	curl_global_init(CURL_GLOBAL_NOTHING);

	if((handle = curl_easy_init())) {
		if((file = fopen(argv[2], "w"))) {
			curl_easy_setopt(handle, CURLOPT_URL, argv[1]);
			curl_easy_setopt(handle, CURLOPT_WRITEDATA, file);
			rval = curl_easy_perform(handle);
	
			fclose(file);
		}
		curl_easy_cleanup(handle);
	}
	curl_global_cleanup();

	return rval;	// 0 on success
}
