#include "filestat.h"
#include <stdio.h>
#include <iostream.h>


int main(void)
{
  FileStat fs;
  char*    machinename[7]={
                            "grumpy",
			    "damsel",
			    "miss-grabople",
			    "diamond-joe",
			    "jackal",
                            "cheddar",
			    "wiley"
			  };
  char*    owner[7]={
		      "sonali",
		      "tomw",
		      "kirkd",
                      "tsao",
		      "jjason",
		      "tjung",
		      "livny"
		    };
  int      insert_order[16] = {128, 64, 192, 32, 160, 224, 96, 112, 104, 144, 
			       240, 80, 16, 208, 176, 48};
  int      delete_order[16] = {16, 224, 208, 240, 192, 128, 160, 176, 64, 32,
			       48, 112, 96, 80, 104, 144};
  int      del_machine[7]={0, 5, 2, 1, 3, 6, 4};
  int      count;
  char     filename[20];

/*
  for (count=0; count<16; count++)
    {
      sprintf(filename, "%3d", insert_order[count]);
      fs.StoreFile(machinename[0], owner[0], filename);
    }
  fs.Dump();
  for (count=0; count<16; count++)
    {
      fs.DumpOwnerTree(machinename[0], owner[0]);
      cout << "Deleting #" << delete_order[count] << ':' << endl;
      sprintf(filename, "%3d", delete_order[count]);
      fs.DeleteFile(machinename[0], owner[0], filename);
    }
  fs.Dump();

  
  for (count=0; count<7; count++)
    {
      sprintf(filename, "%3d", insert_order[count]);
      fs.StoreFile(machinename, owner[count], filename);
    }
  fs.Dump();
  for (count=0; count<7; count++)
    {
      fs.DumpMachineTree(machinename);
      sprintf(filename, "%3d", insert_order[del_owner[count]]);
      fs.DeleteFile(machinename, owner[del_owner[count]], filename);
    }
  fs.Dump();
*/

// For this test, the hash function has been altered to return 15

  for (count=0; count<7; count++)
    {
      sprintf(filename, "%3d", insert_order[count]);
      fs.StoreFile(machinename[count], owner[0], filename);
    }
  fs.Dump();
  for (count=0; count<7; count++)
    {
      fs.DumpHashTree(15);
      sprintf(filename, "%3d", insert_order[del_machine[count]]);
      fs.DeleteFile(machinename[del_machine[count]], owner[0], filename);
    }
  fs.Dump();

  return(0);
}


