#include <stdio.h>
#include <string.h>


int main(void)
{
  char buf[100];
  int  count, len;

  scanf("%s", buf);
  len = strlen(buf);
  count = 0;
  while ((count < len) && (buf[count] != '='))
    count++;
  if (count != len)
    {
      printf("#ifndef %s\n", buf+count+1);
      printf("#define %s\n", buf+count+1);
      printf("#endif\n");
    }
  return 0;
}
