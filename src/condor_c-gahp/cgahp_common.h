#ifndef __CGAHP_COMMON_H__
#define __CGAHP_COMMON_H__

#define GAHP_REQUEST_PIPE 		100
#define GAHP_RESULT_PIPE		101
#define GAHP_REQUEST_ACK_PIPE	102

int
parse_gahp_command (const char* raw, char *** _argv, int * _argc);


#endif
