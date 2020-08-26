/***************************************************************
 *
 * Copyright (C) 2014-2015, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#define CURL_STATICLIB
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <curl/curl.h>
#include <string.h>

#ifdef WIN32
#define strcasecmp _strcmpi
#endif

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
 * 5 - could not fetch url and no cache file was available
 */

using namespace std;

size_t write_data(void *ptr, size_t size, size_t nmemb, void *userp); 
int write_prevfile(const char *prev);
//bool isEmpty(FILE *file);

std::string readBuffer;

/*********************************************************************
    main()
*********************************************************************/

int main(int argc, const char *argv[]) 
{
    std::string readBuffer;

    if((argc < 2) || (argc > 4))
    {
        fprintf(stderr, "usage: condor_urlfetch  [-<subsystem>]  http://my.site/condor_config  last_config.url");
        return 1;
    }
  //Determines whether the -daemon flag is present, and set argv offets accordingly
  //If the -daemon flag is there and is not master, the program will not download
  //a new page. If no flag was specified, it always downloads
    int lastLoc = 2;
    bool forcePull = true;
    bool cacheExists = false;

    if(argv[1][0] == '-')
    {
        lastLoc++;
        forcePull = (0 == strcasecmp(argv[1], "-MASTER"));
    }

  //Check to see if the cache file exists is readable and is non-empty
  // if -MASTER was not passed, we only want to pull
  // if the cached file is missing or cannot be read.
    if ( ! forcePull && argv[lastLoc])
    {
        FILE *fp = fopen(argv[lastLoc], "r");
        if(fp)
        {
            char buf[4];
            size_t cb = fread(buf, 1, sizeof(buf), fp);
            cacheExists = (cb > 0);
            fclose(fp);
        }
    }


    CURL *curl;
    CURLcode res;
    const char *url = argv[lastLoc - 1];

    curl = curl_easy_init();
    if ((curl) && (forcePull || ! cacheExists)) 
    {

      //Setting up curl
		CURLcode r = CURLE_OK;
        r = curl_easy_setopt(curl, CURLOPT_URL, url);
		if (r != CURLE_OK) {
        	fprintf(stderr, "condor_urlfetch: Can't setopt CUROPT_URL\n");
        	return 1;
		}

        r = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		if (r != CURLE_OK) {
        	fprintf(stderr, "condor_urlfetch: Can't setopt CUROPT_WRITEFUNCTION\n");
        	return 1;
		}
        r = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		if (r != CURLE_OK) {
        	fprintf(stderr, "condor_urlfetch: Can't setopt CUROPT_WRITEDATA\n");
        	return 1;
		}
        res = curl_easy_perform(curl);

        if(res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
               curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);


      //If something has been written to the buffer, copy this to our output file.
        if(res == CURLE_OK)
        {
            if ( ! argv[lastLoc])
            {
                fwrite(readBuffer.c_str(), 1, readBuffer.length(), stdout);
                return 0;
            }
            else
            {
                ofstream out(argv[lastLoc]);

                out.write(readBuffer.c_str(), readBuffer.length());
                out.close();
            }
        }
    }

  // open the cache file and write it to stdout.
    if ( ! argv[lastLoc])
    {
        fprintf(stderr, "Error: no cache file specified, and could not fetch URL\n");
        exit(3); 
    }
    else if(write_prevfile(argv[lastLoc]) != 0)
    {
        fprintf(stderr, "Error: could not from cache file %s\n", argv[lastLoc]);
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

/*
// check if our cache file is empty.
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
*/
