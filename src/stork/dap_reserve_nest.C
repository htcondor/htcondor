#include "dap_constants.h"
#include "dap_logger.h"
#include "dap_error.h"
#include "nest_speak.h"
#include <unistd.h>


int reserve_space_on_nest(char *dest_host, char *reserve_size, char *duration, char *lot_id, NestReplyStatus & status)
{
    NestConnection nest_con;
    NestLotHandle *handle;
    //NestReplyStatus status;

    fprintf(stdout, "Reserving %f MB for %f minutes on server %s..\n", atof(reserve_size),atof(duration), dest_host);

    //open connection to nest server
    status = NestOpenConnection(nest_con, dest_host);
    if (status != NEST_SUCCESS) return DAP_ERROR;

    handle = (NestLotHandle *)malloc(sizeof(NestLotHandle));
    status = NestRequestUserLot(handle, nest_con, atoll(reserve_size), atoi(duration));

    if (status != NEST_SUCCESS){
      printf("reserve error: %d\n", status);
      NestCloseConnection(nest_con); 
      return DAP_ERROR;
    }

    sprintf(lot_id, "%lu", (unsigned long)handle->rsv_id);

    NestCloseConnection(nest_con);

    return DAP_SUCCESS;
}

int main(int argc, char *argv[])
{

  char duration[MAXSTR], reserve_size[MAXSTR];
  char dest_host[MAXSTR];
  char lot_id[MAXSTR] = "";
  int status;
  NestReplyStatus nest_status;

  if (argc < 5){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <dest_host> <output_file> <reserve_size> <duration>\n",
	   argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(-1);
  }

  strcpy(dest_host, argv[1]);
  //  strcpy(output_file, argv[2]);
  strcpy(reserve_size, argv[3]);
  strcpy(duration, argv[4]);
  
  status = reserve_space_on_nest(dest_host, reserve_size, duration, lot_id, nest_status);

  //output to a temporary file....
  unsigned int mypid;
  FILE *f;
  char fname[MAXSTR];
  
  mypid = getpid();
  snprintf(fname, MAXSTR, "out.%d", mypid);
  
  f = fopen(fname, "w");
  
  if (status == DAP_SUCCESS){
    fprintf(f, "%s\n", lot_id);
  }
  else{
    fprintf(f, "error in reserve: (NeST error: %d)", nest_status);
  }

  fclose(f);  
  return status;
}
















