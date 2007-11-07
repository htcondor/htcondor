/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "dap_constants.h"
#include "dap_server_interface.h"
#include "dap_client_interface.h"
#include "dap_classad_reader.h"
#include "dap_utility.h"

int listenfd_submit;
int listenfd_log;

char logfilename[MAXSTR];


/* =========================================================================== */
void send_requests_to_server(int sockfd)
{

  ssize_t n;
  char line[MAX_TCP_BUF];

  for ( ; ; ){ //for more than one requests

    if ((n = recv(sockfd, line, MAX_TCP_BUF, 0)) == -1){
      printf("Server: recv error!\n");
      exit(1);
    }
    else if (n == 0) break;
    
    //convert request into a classad
    ClassAdParser parser;
    ClassAd *requestAd = NULL;
    PrettyPrint unparser;
    std::string adbuffer = "";
    char last_dapstr[MAXSTR];
    ExprTree *expr = NULL;
    int server_conn;

    std::string linestr = line;
    //check the validity of the request
    requestAd = parser.ParseClassAd(linestr);
    if (requestAd == NULL) {
      printf("Invalid input format! Not a valid classad!\n");
      return;
    }

    unparser.Unparse(adbuffer, requestAd);
    printf("New Request => %s\n",adbuffer.c_str());
    
    char stork_server[MAXSTR];
    getValue(requestAd, "stork_server", stork_server);
    strncpy(stork_server, strip_str(stork_server), MAXSTR);
    
    printf("Will be sent to stork server @%s\n", stork_server);

    //open the connection to the Stork server
    dap_open_connection(server_conn, stork_server);

    fprintf(stdout, "================\n");
    fprintf(stdout, "Sending request:");
    fprintf(stdout, "%s\n", adbuffer.c_str());

    unsigned long dapid;
    dapid = dap_send_request(server_conn, adbuffer.c_str());

    fprintf(stdout, "================\n");

    dap_close_connection(server_conn);

    //send response to the client
    char response[MAXSTR];
    snprintf(response, MAXSTR, "%d", dapid);
    if ( send (sockfd, response, strlen(response),0) == -1){
      dprintf(D_ALWAYS, "Server: send error!\n");
      exit(1);
    }
  }
}

/* =========================================================================== */
void listen_to_submit_requests()
{
  struct sockaddr_in serv_addr;

  if ( (listenfd_submit = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
    perror("Stork client agent: cannot open socket!\n");	
    exit(1);			     		
  }				     		
  
  bzero(&serv_addr, sizeof(serv_addr)); 	
  serv_addr.sin_family      = AF_INET;   	
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port        = htons(CLI_AGENT_SUBMIT_TCP_PORT);

  if(bind(listenfd_submit,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
    perror("Stork client agent: cannot bind to local port..\n");
    exit(1);
  }		

  if (listen(listenfd_submit, LISTENQ) < 0){
    perror("Stork client agent: error in listen!\n");
    exit(1);  
  }	
      
  printf("Listening to stork_submit requests from clients..\n");
}

/* =========================================================================== */
void accept_submit_requests()
{
  int connfd;
  pid_t childpid;
  socklen_t clilen;
  struct sockaddr_in cli_addr;
 
  for ( ; ; ){
    int flags;
    
    flags = fcntl(listenfd_submit, F_GETFL, 0);
    fcntl(listenfd_submit, F_SETFL, flags | O_NONBLOCK); 
    
    clilen = sizeof(cli_addr);
    if ((connfd = accept(listenfd_submit,
			 (struct sockaddr*) &cli_addr, &clilen)) <0){
      return;
    }	

    send_requests_to_server(connfd);
    close(connfd);
  }
}

/* =========================================================================== */
void write_logs_to_file(int sockfd)
{

  ssize_t n;
  char line[MAX_TCP_BUF];
  FILE *logfile;

  for ( ; ; ){ //for more than one requests

    if ((n = recv(sockfd, line, MAX_TCP_BUF, 0)) == -1){
      printf("Server: recv error!\n");
      exit(1);
    }
    else if (n == 0) break;
    
    line[n] = '\0';

    logfile = safe_fopen_wrapper(logfilename, "a");

    if (logfile == NULL) {
      fprintf(stderr, "Cannot open file:%s\n",logfilename);
      exit(1);
    }
    
    fprintf(logfile, "%s", line);
    
    fclose(logfile);

  }
}

/* =========================================================================== */
void listen_to_log_requests()
{
  struct sockaddr_in serv_addr;

  if ( (listenfd_log = socket(AF_INET, SOCK_STREAM, 0)) < 0){ 
    perror("Stork client agent: cannot open socket!\n");	
    exit(1);			     		
  }				     		
  
  bzero(&serv_addr, sizeof(serv_addr)); 	
  serv_addr.sin_family      = AF_INET;   	
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port        = htons(CLI_AGENT_LOG_TCP_PORT);

  if(bind(listenfd_log,(struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0){
    perror("Stork client agent: cannot bind to local port..\n");
    exit(1);
  }		

  if (listen(listenfd_log, LISTENQ) < 0){
    perror("Stork client agent: error in listen!\n");
    exit(1);  
  }	
            
  printf("Listening to stork_log requests from servers..\n");
}



/* =========================================================================== */
void accept_log_requests()
{
  int connfd;
  pid_t childpid;
  socklen_t clilen;
  struct sockaddr_in cli_addr;
  
  for ( ; ; ){
    int flags;
    
    flags = fcntl(listenfd_log, F_GETFL, 0);
    fcntl(listenfd_log, F_SETFL, flags | O_NONBLOCK); 
    
    clilen = sizeof(cli_addr);
    if ((connfd = accept(listenfd_log,
			 (struct sockaddr*) &cli_addr, &clilen)) <0){
      return;
    }	

    write_logs_to_file(connfd);
    close(connfd);
  }
}

/* =========================================================================== */
int main(int argc, char *argv[])
{


  //check number of arguments
  if (argc < 2){
    fprintf(stderr, "==============================================================\n");
    fprintf(stderr, "USAGE: %s <log_file>\n", argv[0]);
    fprintf(stderr, "==============================================================\n");
    exit(1);
  }

  strncpy(logfilename, argv[1], MAXSTR);


  listen_to_submit_requests();
  listen_to_log_requests();

  while(1){
    accept_submit_requests();
    accept_log_requests();
    sleep(1);
  }
}









