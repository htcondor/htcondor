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

size_t write_data(void *ptr, size_t size, size_t nmemb, void *userp); 
int write_prevfile(const char *prev);
bool isEmpty(FILE *file);

std::string readBuffer;

/*********************************************************************
    main()
*********************************************************************/

int main(int argc, const char *argv[]) 
{
    if((argc < 3) || (argc > 4))
    {
        perror("usage: condor_urlfetch  (-daemon)  http://my.site/condor_config  last_config.url");
        return 1;
    }
  //Determines whether the -daemon flag is present, and set argv offets accordingly
  //If the -daemon flag is there and is not master, the program will not download
  //a new page. If no flag was specified, it always downloads
    int lastLoc = 2;
    bool toPull = true;

    if(argv[1][0] == '-')
    {

        lastLoc++;
        if(strncmp(argv[1], "-MASTER", 10) != 0)
        {
            // if -MASTER was not passed, we only want to pull
            // if the cached file is missing or cannot be read.
            FILE *fp = fopen(argv[lastLoc], "r");
            if(fp)
            {
                toPull = false;
                fclose(fp);
            }
        }
    }

    CURL *curl;
    CURLcode res;
    const char *url = argv[lastLoc - 1];

    curl = curl_easy_init();
    if ((curl) && (toPull)) 
    {
        std::string readBuffer;

      //Setting up curl
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
               curl_easy_strerror(res));
            return(4);
        }
        curl_easy_cleanup(curl);


      //If something has been written to the buffer, copy this to our output file.
        if(!readBuffer.empty());
        {
            std:ofstream out(argv[lastLoc]);
            out << readBuffer;
                    
            out.close();
        }
    }

    if(write_prevfile(argv[lastLoc]) != 0)
    {
        perror("Error opening file as read only");
        exit(3); 
    }
    return 0;
}



size_t write_data(void *ptr, size_t size, size_t nmemb, void *userp) 
{

    ((std::string*)userp)->append((char*)ptr, size * nmemb);
    return size * nmemb;
 // Alternate write_data to be used to write to a file rather than buffer
 /*   size_t realsize = size * nmemb;
    readBuffer.append(ptr, realsize);
    return realsize;*/

   /* size_t written = 0;
    if((size != 0) && (nmemb != 0))
    {
        written = fwrite(ptr, size, nmemb, stream);
        if(written!= nmemb)
        {
            perror("Error reading file.");
            exit(2); 
        }
    }

    return written;*/
}

int write_prevfile(const char *prev)
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
