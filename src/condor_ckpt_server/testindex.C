#include "imds2.h"
#include "fileindex2.h"
#include "network2.h"
#include <iostream.h>
#include <stdio.h>


int main(void)
{
  IMDS imds;
  int filenum[14] = { 108, 113, 104, 111, 102, 103, 101, 107, 106, 105, 
		      112, 109, 110, 114 };
  char filename1[100];
  char* path[3] = { "grumpy",
		    "damsel",
		    "sun12" };
  struct hostent* h;
  struct in_addr machine_IP;
  int machine, count, temp;
  
  machine = 0;
  h = gethostbyname(path[machine]);
  memcpy((char*) &machine_IP, (char*) h->h_addr, sizeof(struct in_addr));
  sprintf(filename1, "%d", filenum[0]);
  for (count=0; count<14; count++)
    {
      imds.AddFile(machine_IP, "tsao", filename1, filenum[count]-100);
      imds.Unlock(machine_IP, "tsao", filename1);
    }
  imds.DumpIndex();
  imds.DumpInfo();
  return 0;
}
