#include "xferstat2.h"
#include "network2.h"
#include <iostream.h>
#include <stdio.h>


int main(void)
{
  TransferState  xfs;
  int            count;
  struct in_addr addr;
  char           temp[100];

  for (count=100; count<110; count++)
    {
      addr.s_addr = count;
      sprintf(temp, "file%04d", count);
      xfs.Insert(count, count-99, addr, temp, "tsao", count, 4, 0, RECV);
    }
  xfs.Dump();
  cout << "Key for child #104:              " << xfs.GetKey(104) << endl;
  addr.s_addr = 107;
  cout << "Key for 107.0.0.0/tsao/file0107: " << xfs.GetKey(addr, "tsao",
		    "file0107") << endl;
  if (xfs.Delete(100) != OK)
    cout << "Cannot delete 100" << endl;
  if (xfs.Delete(104) != OK)
    cout << "Cannot delete 100" << endl;
  if (xfs.Delete(109) != OK)
    cout << "Cannot delete 100" << endl;
  if (xfs.Delete(110) != OK)
    cout << "Cannot delete 100" << endl;
  xfs.Dump();
  cout << "Key for child #104:              " << xfs.GetKey(104) << endl;
  return 0;
}
