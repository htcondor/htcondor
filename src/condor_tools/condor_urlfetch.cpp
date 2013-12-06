#define CURL_STATICLIB
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <string.h>
/*condor_urlfetch
 *This program downloads the url specified as the first argument into the 
 *file specified in the second argument, as well as stdout.
 *If the fetch fails, the file will not be overwritten, but it will still
 *be printed to stdout.
 *Errorcode Definitions: (0 successful execution)
 * 1 - usage error
 * 2 - Error writing to file
 * 3 - Error opening the prevfile to read from
 * 4 - curl_easy_perform failed
 * 5 - Error creating a temporary file
 */

using namespace std;


size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);
int write_prevfile(char *prev);
bool isEmpty(FILE *file);


/*********************************************************************
    main()
*********************************************************************/

int main(int argc, char *argv[]) 
{
    if((argc < 3) || (argc > 4))
    {
        perror("usage: condor_urlfetch  (-daemon)  http://my.site/condor_config  last_config.url");
        return 1;
    }
//Determines whether the -daemon flag is present, and set argv offets accordingly
//If the -daemon flag is there and is not master, the program will not download
//a new page.
    int lastLoc = 2;
    bool toPull = true;

    if(argv[1][0] == '-')
    {

        lastLoc++;
        if(strncmp(argv[1], "-MASTER", 10) != 0)
        {
            toPull = false;
        }
    }

    CURL *curl;
    FILE *tempFile;
    CURLcode res;
    char *url = argv[lastLoc - 1];
    char *tempName;
    tempName = tmpnam(NULL);

    tempFile = fopen(tempName, "wb"); 
    if(tempFile == NULL)
    {
        perror("Error creating a temporary file");  
        return(5); 
    }

    curl = curl_easy_init();
    if ((curl) && (toPull)) 
    {

        tempFile = fopen(tempName, "wb"); 
        if(tempFile == NULL)
        {
            perror("Error creating a temporary file");  
            return(5); 
        }
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, tempFile);
        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
               curl_easy_strerror(res));
            return(4);
        }
        curl_easy_cleanup(curl);
        fclose(tempFile);
    }
    tempFile = fopen(tempName, "rb"); 
    if(tempFile == NULL)
    {
        perror("Error opening a temporary file");  
        return(5); 
    }

//If something has been written to tempfile, copy this to our output file.
    if(!isEmpty(tempFile))
    {

        std::ifstream  src(tempName, std::ios::binary);
        std::ofstream  dst(argv[lastLoc], std::ios::binary);

        dst << src.rdbuf();
    }
    if(write_prevfile(argv[lastLoc]) != 0)
    {
        perror("Error opening file as read only");
        exit(3); 
    }
    remove(tempName);
    return 0;
}

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) 
{
    size_t written = 0;
    if((size != 0) && (nmemb != 0))
    {
        written = fwrite(ptr, size, nmemb, stream);
        if(written!= nmemb)
        {
            perror("Error reading file.");
            exit(2); 
        }
    }

    return written;
}

int write_prevfile(char *prev)
{
    FILE *ptrFile;

    ptrFile = fopen(prev, "r");

    if(!ptrFile)
    {
        printf("Error: Failed to open previous file for reading.\n");
        return 1;
    }

    std::ifstream  toPrint(prev, std::ios::binary);

    cout << toPrint.rdbuf();
    fclose(ptrFile);

    return 0;
}

bool isEmpty(FILE *ptrFile)
{
    long offset = ftell(ptrFile);
    fseek(ptrFile, 0, SEEK_END);

    if (ftell(ptrFile) == 0)
    {
        return true;
    }

    fseek(ptrFile, offset, SEEK_SET);
    return false;
}
