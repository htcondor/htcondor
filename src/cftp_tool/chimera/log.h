/*
** $Id: log.h,v 1.14 2006/06/16 07:55:37 ravenben Exp $
**
** Matthew Allen
** description: 
*/

/* define this to be 0 for no logging, 1 for logging */
#define LOGS 0

#ifndef _CHIMERA_LOG_H_
#define _CHIMERA_LOG_H_

enum
{
    LOG_ERROR,			/* error messages (stderr) */
    LOG_WARN,			/* warning messages (none) */
    LOG_DEBUG,			/* debugging messages (none) */
    LOG_KEYDEBUG,		/* debugging messages for key subsystem (none) */
    LOG_NETWORKDEBUG,		/* debugging messages for network layer (none) */
    LOG_ROUTING,		/* debugging the routing table (none) */
    LOG_SECUREDEBUG,		/* for security module (none) */
    LOG_DATA,			/* for measurement and analysis (none) */
    LOG_COUNT			/* count of log message types */
};


void *log_init ();
void log_message (void *logs, int type, const char *format, ...);
void log_direct (void *logs, int type, FILE * fp);


#endif /* _CHIMERA_LOG_H_ */
