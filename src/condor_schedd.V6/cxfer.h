#ifndef   _CONDOR_CXFER_H
#define   _CONDOR_CXFER_H

//
// #include "scheduler.h"
// #include "cxfer.h"
//

enum class CXFER_TYPE {
    FORBIDDEN,
    NONE,
    CANT,
    STAGING,
    MAPPING,
    READY,
};

CXFER_TYPE
determine_cxfer_type(match_rec * mrec, PROC_ID * jobID );

#endif /* _CONDOR_CXFER_H */
