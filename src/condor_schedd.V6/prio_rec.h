////////////////////////////////////////////////////////////////////////////////
//
// prio_rec.h
//
////////////////////////////////////////////////////////////////////////////////

#ifndef _PRIO_REC_H_
#define _PRIO_REC_H_

#define	MAX_PRIO_REC    2048

/* this record contains all the parameters required for
 * assigning priorities to all jobs */
typedef struct rec
{
    PROC_ID     id;
    int         prio;                           /* UPDOWN */
    int         job_prio;
    int         status;
    int         qdate;
    char        *owner;                         /* valid only if D_UPDOWN */
} prio_rec;

#endif
