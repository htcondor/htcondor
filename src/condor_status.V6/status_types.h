#ifndef __STATUS_TYPES_H__
#define __STATUS_TYPES_H__

// pretty-printing options
enum ppOption {
    PP_NOTSET,
    PP_NORMAL,
    PP_SERVER,
    PP_AVAIL,
    PP_RUN,
    PP_SUBMITTORS,
    PP_VERBOSE,
    PP_CUSTOM
};


// modes for condor_status
enum Mode {
    MODE_NOTSET,
    MODE_NORMAL,
    MODE_AVAIL,
    MODE_SUBMITTORS,
    MODE_RUN,
    MODE_CUSTOM
};
   

#endif //__STATUS_TYPES_H__
