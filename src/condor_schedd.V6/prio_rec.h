////////////////////////////////////////////////////////////////////////////////
//
// prio_rec.h
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _PRIO_REC_H_
#define _PRIO_REC_H_

const 	int		INITIAL_MAX_PRIO_REC = 2048;

/* this record contains all the parameters required for
 * assigning priorities to all jobs */

class prio_rec {
public:
    PROC_ID     id;
    int         job_prio;
    int         status;
    int         qdate;
    char        owner[100];                         /* valid only if D_UPDOWN */

	prio_rec() { *owner='\0'; }
};

#endif
