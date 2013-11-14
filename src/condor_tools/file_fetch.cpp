#ifdef WIN32
#include <windows.h>
#define CURL_STATICLIB // this has to match the way the curl library was built.
#else
#include <stdlib.h>
#endif

#include <curl/curl.h>
#include <string.h>

#include "safe_fopen.h"

struct Downloaded_Data
{
    char* text;
    int size;
};

static size_t WriteCallback(char *contents, size_t size, size_t nmemb, void *userdata)
{
    Downloaded_Data *downloaded_data = (Downloaded_Data*)userdata;
    size_t data_size = size * nmemb;

    if(downloaded_data->text)
    {
        free(downloaded_data->text);
        downloaded_data->text = NULL;
    }

    if(size == 0) return 0;

    downloaded_data->text = (char*)malloc(data_size + 1);
    
    if(!downloaded_data->text) return 0;

    downloaded_data->size = size;
    downloaded_data->text[data_size] = '\0';
    memcpy(downloaded_data->text, contents, data_size);

    return data_size;
}

void print_help()
{
    printf("This utility accepts arguments in the following format:\n");
    printf("config_fetch http://url.to.config.file [path_to_cache_file]\n");
    printf("config_fetch -help");
}

bool print_cached_file(const char *cached_path)
{
    FILE *fp;
    int character;

    fp = safe_fopen_no_create_follow(cached_path, "r");

    if(!fp)
    {
        printf("Error: Failed to open cache file for reading.\n");
        return false;
    }

    while((character = getc(fp)) != EOF)
    {
        putchar(character);
    }

    fclose(fp);

    return true;
}

int main(int argc, char **argv) {
    CURL *handle = NULL;
    int rval = -1;
    const char *cache_path;
    Downloaded_Data downloaded_data;
    FILE *fp = NULL;
    long http_code;

    downloaded_data.text = NULL;
    downloaded_data.size = 0;
	
    if(argc < 2 || argc > 3)
    {
        print_help();
        return -1;
    }

    if(strncmp(argv[1], "-h", 2) == 0)
    {
        print_help();
        return 0;
    }

    if(argc == 3)
        cache_path = argv[2];
    else
        cache_path = "config_cache.local";

#ifndef WIN32
	curl_global_init(CURL_GLOBAL_NOTHING);
#endif
    handle = curl_easy_init();

    if(!handle)
    {
        fprintf(stderr, "Error attempting to initialize CURL library.\n");
        return -1;
    }

    curl_easy_setopt(handle, CURLOPT_URL, argv[1]);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &downloaded_data);
	curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, -1);

    rval = curl_easy_perform(handle);
    if(rval || !downloaded_data.text)
    {
        print_cached_file(cache_path);
        goto Cleanup;
    }

    rval = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);

    if(rval)
    {
        print_cached_file(cache_path);
        goto Cleanup;
    }

    if(http_code != 200)
    {
        print_cached_file(cache_path);
        goto Cleanup;
    }
    
    printf(downloaded_data.text);

    fp = safe_fcreate_replace_if_exists(cache_path, "w");

    if(!fp) goto Cleanup;

    fputs(downloaded_data.text, fp);

    fclose(fp);

Cleanup:
    if(downloaded_data.text) free(downloaded_data.text);
    curl_easy_cleanup(handle);
    curl_global_cleanup();

	return rval;	// 0 on success
}
