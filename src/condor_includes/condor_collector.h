#ifndef __COLLECTOR_H__
#define __COLLECTOR_H__

#include "sched.h"

enum AdTypes
{
    STARTD_AD,
    SCHEDD_AD,
    MASTER_AD,
    GATEWAY_AD,
    CONFIG_SRV_AD,

    NUM_AD_TYPES
};

// collector commands
const int UPDATE_STARTD_AD       = 0;
const int UPDATE_SCHEDD_AD       = 1;
const int UPDATE_MASTER_AD       = 2;
const int UPDATE_GATEWAY_AD      = 3;
const int UPDATE_CONFIG_SRVR_AD  = 4;

const int QUERY_STARTD_ADS       = 5;
const int QUERY_SCHEDD_ADS       = 6;
const int QUERY_MASTER_ADS       = 7;
const int QUERY_GATEWAY_ADS      = 8;
const int QUERY_CONFIG_SRVR_ADS  = 9;

#endif // __COLLECTOR_H__


