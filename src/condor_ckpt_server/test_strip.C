#include <iostream.h>
#include <string.h>


void StripFilename(char* pathname)
{
  int s_index, len, count;

  len = strlen(pathname);
  s_index = len-1;
  while ((s_index >= 0) && (pathname[s_index] != '/'))
    s_index--;
  if (s_index >= 0)
    {
      s_index++;
      count = 0;
      while (pathname[s_index] != '\0')
	pathname[count++] = pathname[s_index++];
      pathname[count] = '\0';
    }
}


int main(void)
{
  char tfn1[100]="heretic.zip";
  char tfn2[100]="~tsao/private/cs736/chkpt_shadow/heretic.zip";

  StripFilename(tfn1);
  StripFilename(tfn2);
  cout << "Test file name 1: " << tfn1 << endl;
  cout << "Test file name 2: " << tfn2 << endl;
}
