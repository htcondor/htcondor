/*
** These are functions for generating internet addresses
** and internet names
**
**             Author : Dhrubajyoti Borthakur
**               28 July, 1994
*/
/* maximum length of a machine name */
#define  MAXHOSTLEN     1024

typedef struct job_id
{
	struct in_addr	inet_addr;
	PROC_ID			id;
} JOB_ID;
