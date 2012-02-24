#pragma once
#include <windows.h>

// This class is exported from the testperflib.dll

#define CONDOR_SRV_REG L"SYSTEM\\CurrentControlSet\\Services\\Condor"
#define PERF_REG_PATH L"SYSTEM\\CurrentControlSet\\Services\\Condor\\Performance"
#define counter_val_size sizeof(int)

#define CONDOR_STATS_OBJECT 0
#define ACTIVE_SLOT_COUNT 2