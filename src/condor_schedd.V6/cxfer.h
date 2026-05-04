#ifndef   _CONDOR_CXFER_H
#define   _CONDOR_CXFER_H

//
// #include "scheduler.h"
// #include "catalog_utils.h"
// #include "dc_coroutines.h"
// #include "cxfer.h"
//

enum class CXFER_TYPE {
    FORBIDDEN,
    NONE,
    CANT,
    STAGING,
    MAPPING,
    READY,
    RETIRING,
};

std::tuple<
    CXFER_TYPE,
    std::optional<ListOfCatalogs>
>
determine_cxfer_type(match_rec * mrec, const PROC_ID & jobID );

condor::cr::void_coroutine
start_conversion_to_data_slot( match_rec * mrec, ClassAd requestAd );

#endif /* _CONDOR_CXFER_H */
