#include <iostream.h>
#include "typedefs.h"


int main(void)
{
  u_lint  temp;
  IP_addr IP;

  temp = 10010010;
  cout << temp << endl;
  IP.s_addr = temp;
  cout << IP.s_addr << endl;
}
