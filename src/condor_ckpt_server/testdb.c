#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


extern "C" { struct hostent* gethostbyname(char*); }


int main(void)
{
  struct hostent* h;
  struct in_addr  addr;

  h = gethostbyname("grumpy");
  memcpy((char*) &addr, (char*) h->h_addr, sizeof(struct in_addr));
  printf("%s\n", inet_ntoa(addr));
  return 0;
}
