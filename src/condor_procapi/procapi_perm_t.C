#include "procapi_t.h"

// this test is not 100% complete it should check for each different os 
// whether seeing the information is an error or not


/////////////////////////////test3/////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int permission_test(bool verbose) {

  piPTR pi = NULL;

  if(verbose){
  printf ( "\n..................................\n" );
  printf ( "This test determines if you can get information on processes\n" );
  printf ( "other than those you own.  If you get a summary of all the\n" );
  printf ( "processes in the system ( parent=1 ) then you can.\n\n" );
  }

  if ( ProcAPI::getFamilyInfo( 1, pi ) < 0 ){
    //if(verbose){
      printf ( "Unable to get info on process 1 (init)\n");
      //}
  }
  else{
   
    printf("user has access to info on all processes:\n init info:\n");
    if(verbose){
      ProcAPI::printProcInfo( pi );
    }
  }
  delete pi;

  return 1;
}
