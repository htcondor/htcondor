#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>


int main(void)
{
  char m_name[] = "grumpy";
  int  mode=INT_MAX;

  if (mkdir(m_name, mode) < 0)
    fprintf(stderr, "ERROR: mkdir() failed\n");

}
