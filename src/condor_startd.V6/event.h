#ifndef _CONDOR_STARTD_EVENT_H
#define _CONDOR_STARTD_EVENT_H

typedef int event_t;

#define EV_VACATE	0
#define EV_BLOCK	1
#define EV_UNBLOCK	2
#define EV_START	3
#define EV_SUSPEND	4
#define EV_CONTINUE	5
#define EV_KILL		6
#define EV_EXITED	7

#endif /* _CONDOR_STARTD_EVENT_H */
