#ifndef __COLLECTOR_H__
#define __COLLECTOR_H__

#include "sched.h"

enum AdTypes
{
    STARTD_AD,
    SCHEDD_AD,
    MASTER_AD,
    GATEWAY_AD,
    CKPT_SRVR_AD,

    NUM_AD_TYPES
};

#include "condor_commands.h"   // collector commands

#endif // __COLLECTOR_H__


