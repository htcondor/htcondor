/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

#include "condor_common.h"
#include "condor_string.h"
#include "condor_debug.h"
#include "condor_md.h"
#include "dap_constants.h"

#ifdef BUF_SIZE
#undef BUF_SIZE
#endif
#define BUF_SIZE 1024

void reset(char *buffer)
{
  for (int i=0; i< BUF_SIZE;i++){
    buffer[i] = 0;
  } 
}


int main(int argc, char *argv[])
{
  FILE *fin;
  FILE *fout;
  
  
  char inbuffer[BUF_SIZE];
  unsigned char * outbuffer = (unsigned char *) malloc(MAC_SIZE);
  Condor_MD_MAC md5;

  if (argc < 3){
    fprintf(stderr, "=========================================================\n");
    fprintf(stderr, "USAGE: %s <infilename> <md5 filename\n", argv[0]);
    fprintf(stderr, "=========================================================\n");
    exit(-1);
  }

  //reset(inbuffer);
  
  fin = safe_fopen_wrapper(argv[1], "r");
  fout = safe_fopen_wrapper(argv[2], "w");

  int size;

  while ( (size = fread(inbuffer, 1, BUF_SIZE, fin)) > 0){
    if (size < BUF_SIZE) inbuffer[size] = '\0';
    
    memcpy(outbuffer, md5.computeOnce((unsigned char*)inbuffer, strlen(inbuffer)),
	    MAC_SIZE);
    fwrite(outbuffer, 1, MAC_SIZE, fout);
  }

  fclose(fin);
  fclose(fout);
  return 0;  
  
  //printf("ret : %d;in : %s; strlen: %d\n", size, inbuffer, strlen(inbuffer));
  //printf("md5 : %s\n", outbuffer);
  //-------- tests
  //passed = md5.verifyMD(outbuffer1, (unsigned char*)inbuffer1, strlen(inbuffer1));
  //printf("in1 vs out1 passed = %d\n",passed);
}
