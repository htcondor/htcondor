#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>


#define PREFIX "/tmp/"


int main(void)
{
  char* machine_name[3] = { "grumpy", "sun12", "damsel" };
  char filename[100];
  char mn[100];
  int count;
  int count2;
  int machine;
  FILE* fp;
  struct hostent* h;
  struct in_addr addr;

  for (machine=0; machine<3; machine++)
    {
printf("Attempting to get IP for %s:\n", machine_name[machine]);
      h = gethostbyname(machine_name[machine]);
      if (h == NULL)
	{
	  fprintf(stderr, "ERROR: name server failure\n");
	  exit(-1);
	}
      memcpy((char*) &addr, h->h_addr, sizeof(struct in_addr));
      strcpy(mn, inet_ntoa(addr));
printf("Machine name: %s\n", mn);
      sprintf(filename, "%s%s", PREFIX, mn);
      mkdir(filename);
      sprintf(filename, "%s%s/%s", PREFIX, mn, "tsao");
      mkdir(filename);
      for (count=1; count<=14; count++)
	{
	  sprintf(filename, "%s%s/%s/%d", PREFIX, mn, "tsao", count+100);
printf("Attempting to create: %s\n");
	  fp = fopen(filename, "w");
	  for (count2=0; count2<count; count2++)
	    fprintf(fp, "X");
	  fclose(fp);
	}
    }
  return 0;
}
