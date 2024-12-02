#ifndef _CONDOR_GUIDANCE_H
#define _CONDOR_GUIDANCE_H

enum class GuidanceResult : int {

    Invalid = -3,
    MalformedRequest = -2,
    UnknownRequest = -1,
    Command = 0,

};

#endif /* defined(_CONDOR_GUIDANCE_H) */
