#ifndef SERVER_TYPEDEFS2_H
#define SERVER_TYPEDEFS2_H


#include "constants2.h"
#include "network2.h"
#include <time.h>




#ifdef OSF1
typedef unsigned int u_lint;
#else
typedef unsigned long int u_lint;
#endif




#if defined(Solaris)
#if defined(__cplusplus)
typedef void (*SIG_HANDLER)(...);
#else
typedef void (*SIG_HANDLER)();
#endif
#else
typedef void (*SIG_HANDLER)(int);
#endif




typedef enum lock_status
{
  UNLOCKED,
  EXCLUSIVE_LOCK
} lock_status;




typedef enum service_type
{
  CKPT_SERVER_SERVICE_STATUS=190,
  SERVICE_RENAME=191,
  SERVICE_DELETE=192,
  SERVICE_EXIST=193,
  SERVICE_COMMIT_REPLICATION=194,
  SERVICE_ABORT_REPLICATION=195
} service_type;




typedef enum file_state
{
  LOADING=0,
  XMITTING=1,
  ON_SERVER=2,
  NOT_PRESENT=3,
  UNKNOWN=4
} file_state;




typedef enum request_type
{
  SERVICE_REQ,
  STORE_REQ,
  RESTORE_REQ,
  REPLICATE_REQ
} request_type;




typedef struct file_state_info
{
  char       file_name[STATUS_FILENAME_LENGTH];
  char       machine_IP_name[STATUS_MACHINE_NAME_LENGTH];
  char       owner_name[STATUS_OWNER_NAME_LENGTH];
  u_lint     size;
  time_t     last_modified_time;
  u_short    state;
} file_state_info;




typedef struct file_info
{
  struct in_addr machine_IP;
  char           machine_IP_name[MAX_MACHINE_NAME_LENGTH];
  char           owner_name[MAX_NAME_LENGTH];
  char           file_name[MAX_CONDOR_FILENAME_LENGTH];
  u_lint         size;
  time_t         last_modified_time;
  file_state     state;
} file_info;




typedef struct file_info_node
{
  file_info       data;
  lock_status     lock;
  struct file_info_node* prev;
  struct file_info_node* next;
} file_info_node;




typedef struct recv_req_pkt
{
  u_lint file_size;
  u_lint ticket;
  u_lint priority;                                    
  u_lint time_consumed;                             
  u_lint key;
  char   filename[MAX_CONDOR_FILENAME_LENGTH];
  char   owner[MAX_NAME_LENGTH];
} recv_req_pkt;




typedef struct recv_reply_pkt
{
  struct in_addr server_name;
  u_short        port;
  u_short        req_status;
} recv_reply_pkt;




typedef struct xmit_req_pkt
{
  u_lint ticket;
  u_lint priority;
  u_lint key;
  char   filename[MAX_CONDOR_FILENAME_LENGTH];
  char   owner[MAX_NAME_LENGTH];
} xmit_req_pkt;




typedef struct xmit_reply_pkt
{
  struct in_addr server_name;
  u_short        port;
  u_lint         file_size;
  u_short        req_status;
} xmit_reply_pkt;




typedef struct service_req_pkt
{
  u_lint  ticket;
  u_short service;
  u_lint  key;
  char    owner_name[MAX_NAME_LENGTH]; 
  char    file_name[MAX_CONDOR_FILENAME_LENGTH];
  char    new_file_name[MAX_CONDOR_FILENAME_LENGTH-4]; /* -4 to fit shadowIP */
  struct in_addr shadow_IP;
} service_req_pkt;



typedef struct service_reply_pkt
{
  u_short        req_status;
  struct in_addr server_addr;
  u_short        port;
  u_lint         num_files;
  char           capacity_free_ACD[MAX_ASCII_CODED_DECIMAL_LENGTH];
} service_reply_pkt;


typedef struct replicate_req_pkt
{
  u_lint file_size;
  u_lint ticket;
  u_lint priority;                                    
  u_lint time_consumed;                             
  u_lint key;
  char   filename[MAX_CONDOR_FILENAME_LENGTH];
  char   owner[MAX_NAME_LENGTH];
  struct in_addr shadow_IP;
} replicate_req_pkt;
	


typedef recv_req_pkt   store_req_pkt;




typedef recv_reply_pkt store_reply_pkt;




typedef xmit_req_pkt   restore_req_pkt;




typedef xmit_reply_pkt restore_reply_pkt;


typedef recv_reply_pkt replicate_reply_pkt;



#endif


