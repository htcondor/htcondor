#include <stdio.h>


#define PREFIX "/tmp/tsao_server/"


int main(void)
{
  char filename[100];
  int count;
  int count2;
  FILE* fp;

  for (count=1; count<=14; count++)
    {
      sprintf(filename, "%s%d", PREFIX, count+100);
      fp = fopen(filename, "w");
      for (count2=0; count2<count; count2++)
	fprintf(fp, "X");
      fclose(fp);
    }
  return 0;
}
