#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dap_constants.h"

#define DAP_CATALOG "DaP_Catalog"


//Stork setup tool.. will be improved..
int main()
{
  char command[MAXSTR];

  sprintf(command, "mkdir %s", DAP_CATALOG);
  system(command);

  chdir(DAP_CATALOG);

  //GsiFTP
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.file-gsiftp");
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.file-ftp");
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.file-http");
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.gsiftp-gsiftp");
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.gsiftp-file");
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.ftp-file");
  system("ln -s ../DaP.transfer.globus-url-copy  DaP.transfer.http-file");

  //DiskRouter
  system("ln -s ../DaP.transfer.diskrouter  DaP.transfer.diskrouter-diskrouter");

  //SRB
  system("ln -s ../DaP.transfer.srb  DaP.transfer.file-srb");
  system("ln -s ../DaP.transfer.srb  DaP.transfer.srb-file");

  //FNALSRM
  system("ln -s ../DaP.transfer.fnalsrm  DaP.transfer.file-fnalsrm");
  system("ln -s ../DaP.transfer.fnalsrm  DaP.transfer.fnalsrm-file");

  //LBNLSRM
  system("ln -s ../DaP.transfer.lbnlsrm  DaP.transfer.file-lbnlsrm");
  system("ln -s ../DaP.transfer.lbnlsrm  DaP.transfer.lbnlsrm-file");

  //NeST
  system("ln -s ../DaP.transfer.nest  DaP.transfer.file-nest");
  system("ln -s ../DaP.transfer.nest  DaP.transfer.nest-nest");
  system("ln -s ../DaP.transfer.nest  DaP.transfer.nest-file");
  system("ln -s ../DaP.reserve.nest  DaP.reserve.nest");
  system("ln -s ../DaP.release.nest  DaP.release.nest");

  //FILE
  system("ln -s ../DaP.transfer.file-file  DaP.transfer.file-file");

  //IBP
  system("ln -s ../DaP.transfer.ibp  DaP.transfer.file-ibp");
  system("ln -s ../DaP.transfer.ibp  DaP.transfer.ibp-ibp");
  system("ln -s ../DaP.transfer.ibp  DaP.transfer.ibp-file");
  system("ln -s ../DaP.reserve.ibp  DaP.reserve.ibp");

  //UniTree
  system("ln -s ../DaP.transfer.unitree  DaP.transfer.file-unitree");
  system("ln -s ../DaP.transfer.unitree  DaP.transfer.unitree-file");

  return 0;
}










