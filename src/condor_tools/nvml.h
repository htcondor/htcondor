/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:   
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and 
 * international Copyright laws.  Users and possessors of this source code 
 * are hereby granted a nonexclusive, royalty-free license to use this code 
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE 
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR 
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH 
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF 
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL, 
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS 
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE 
 * OR PERFORMANCE OF THIS SOURCE CODE.  
 *
 * U.S. Government End Users.   This source code is a "commercial item" as 
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of 
 * "commercial computer  software"  and "commercial computer software 
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995) 
 * and is provided to the U.S. Government only as a commercial end item.  
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through 
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the 
 * source code with only those rights set forth herein. 
 *
 * Any use of this source code in individual and commercial software must 
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

/*! 
\mainpage NVML API Reference

The NVIDIA Management Library (NVML) is a C-based programmatic interface for monitoring and 
managing various states within NVIDIA Tesla &tm; GPUs.<br>It is intended to be a platform for building
3rd party applications, and is also the underlying library for the NVIDIA-supported nvidia-smi
tool.<br>NVML is thread-safe so it is safe to make simultaneous NVML calls from multiple threads.
<p>
<hr size="1">
<p>
<h2 align="center">API Documentation</h2>
<p>
Supported OS platforms:
- Windows: Windows Server 2008 R2 64bit, Windows 7 64bit
- Linux: 32-bit and 64-bit

Supported products:
- Full Support
    - NVIDIA Tesla &tm; Line:
            S2050, C2050, C2070, C2075,
            M2050, M2070, M2075, M2090,
            X2070, X2090,
            K10, K20, K20X
    - NVIDIA Quadro &reg; Line: 4000, 5000, 6000, 7000, M2070-Q, 600, 2000, 3000M and 410
    - NVIDIA GeForce &reg; Line: None
- Limited Support
    - NVIDIA Tesla &tm; Line:    S1070, C1060, M1060
    - NVIDIA Quadro &reg; Line:  All other current and previous generation Quadro-branded parts
    - NVIDIA GeForce &reg; Line: All current and previous generation GeForce-branded parts

The NVML library can be found at \%ProgramW6432\%\\"NVIDIA Corporation"\\NVSMI\\ on Windows, but
will not be added to the path. To dynamically link to NVML, add this path to the PATH environmental
variable. To dynamically load NVML, call LoadLibrary with this path.

On Linux the NVML library will be found on the standard library path. For 64 bit Linux, both the 32 bit
and 64 bit NVML libraries will be installed.

The NVML API is divided into five categories:
- Support Methods:
    - \ref nvmlInitializationAndCleanup 
- Query Methods:
    - \ref nvmlSystemQueries 
    - \ref nvmlDeviceQueries 
    - \ref nvmlUnitQueries 
- Control Methods:
    - \ref nvmlUnitCommands 
    - \ref nvmlDeviceCommands 
- Event Handling Methods:
    - \ref nvmlEvents
- Error reporting Methods
    - \ref nvmlErrorReporting

List of changes can be found in the \ref Changelog

\latexonly
\section{Feature Matrix}
\endlatexonly
\image latex FeatureMatrix_TeslaFermiAndKepler-0.png "This chart shows which features are reported for each Fermi and Kepler architecture GPU product." height=21cm
\image latex FeatureMatrix_QuadroAndT10-0.png "This chart shows which features are reported for each Quadro and T10 GPU product." height=21cm
\image latex FeatureMatrix_TeslaFermiAndKepler-1.png "This chart shows which commands are available for each Fermi and Kepler architecture GPU product." width=16cm
\image latex FeatureMatrix_QuadroAndT10-1.png "This chart shows which commands are available for each Quadro and T10 GPU product." width=16cm
\image latex FeatureMatrix_Units.png "This chart shows which unit-level features are available for each S-class product. All GPUs within each S-class product also provide the information listed in the Device chart below." width=10cm
*/

#ifndef __nvml_nvml_h__
#define __nvml_nvml_h__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * On Windows, set up methods for DLL export
 * define NVML_STATIC_IMPORT when using nvml_loader library
 */
#if defined _WINDOWS
    #if !defined NVML_STATIC_IMPORT
        #if defined LIB_EXPORT
            #define DECLDIR __declspec(dllexport)
        #else
            #define DECLDIR __declspec(dllimport)
        #endif
    #else
        #define DECLDIR
    #endif
#else
    #define DECLDIR
#endif

/**
 * NVML API versioning support
 */
#define NVML_API_VERSION            4
#define NVML_API_VERSION_STR        "4"
#define nvmlDeviceGetPciInfo        nvmlDeviceGetPciInfo_v2

/***************************************************************************************************/
/** @defgroup nvmlDeviceStructs Device Structs
 *  @{
 */
/***************************************************************************************************/

/**
 * Special constant that some fields take when they are not available.
 * Used when only part of the struct is not available.
 *
 * Each structure explicitly states when to check for this value.
 */
#define NVML_VALUE_NOT_AVAILABLE (-1)

typedef struct nvmlDevice_st* nvmlDevice_t;

/**
 * PCI information about a GPU device.
 */
typedef struct nvmlPciInfo_st 
{
    char busId[16];                  //!< The tuple domain:bus:device.function PCI identifier (&amp; NULL terminator)
    unsigned int domain;             //!< The PCI domain on which the device's bus resides, 0 to 0xffff
    unsigned int bus;                //!< The bus on which the device resides, 0 to 0xff
    unsigned int device;             //!< The device's id on the bus, 0 to 31
    unsigned int pciDeviceId;        //!< The combined 16-bit device id and 16-bit vendor id
    
    // Added in NVML 2.285 API
    unsigned int pciSubSystemId;     //!< The 32-bit Sub System Device ID
    
    // NVIDIA reserved for internal use only
    unsigned int reserved0;
    unsigned int reserved1;
    unsigned int reserved2;
    unsigned int reserved3;
} nvmlPciInfo_t;

/**
 * Detailed ECC error counts for a device.
 *
 * @deprecated  Different GPU families can have different memory error counters
 *              See \ref nvmlDeviceGetMemoryErrorCounter
 */
typedef struct nvmlEccErrorCounts_st 
{
    unsigned long long l1Cache;      //!< L1 cache errors
    unsigned long long l2Cache;      //!< L2 cache errors
    unsigned long long deviceMemory; //!< Device memory errors
    unsigned long long registerFile; //!< Register file errors
} nvmlEccErrorCounts_t;

/** 
 * Utilization information for a device.
 */
typedef struct nvmlUtilization_st 
{
    unsigned int gpu;                //!< Percent of time over the past second during which one or more kernels was executing on the GPU
    unsigned int memory;             //!< Percent of time over the past second during which global (device) memory was being read or written
} nvmlUtilization_t;

/** 
 * Memory allocation information for a device.
 */
typedef struct nvmlMemory_st 
{
    unsigned long long total;        //!< Total installed FB memory (in bytes)
    unsigned long long free;         //!< Unallocated FB memory (in bytes)
    unsigned long long used;         //!< Allocated FB memory (in bytes). Note that the driver/GPU always sets aside a small amount of memory for bookkeeping
} nvmlMemory_t;

/**
 * Information about running compute processes on the GPU
 */
typedef struct nvmlProcessInfo_st
{
    unsigned int pid;                 //!< Process ID
    unsigned long long usedGpuMemory; //!< Amount of used GPU memory in bytes.
                                      //! Under WDDM, \ref NVML_VALUE_NOT_AVAILABLE is always reported
                                      //! because Windows KMD manages all the memory and not the NVIDIA driver
} nvmlProcessInfo_t;

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlDeviceEnumvs Device Enums
 *  @{
 */
/***************************************************************************************************/

/** 
 * Generic enable/disable enum. 
 */
typedef enum nvmlEnableState_enum 
{
    NVML_FEATURE_DISABLED    = 0,     //!< Feature disabled 
    NVML_FEATURE_ENABLED     = 1      //!< Feature enabled
} nvmlEnableState_t;

//! Generic flag used to specify the default behavior of some functions. See description of particular functions for details.
#define nvmlFlagDefault     0x00      
//! Generic flag used to force some behavior. See description of particular functions for details.
#define nvmlFlagForce       0x01      

/** 
 * Temperature sensors. 
 */
typedef enum nvmlTemperatureSensors_enum 
{
    NVML_TEMPERATURE_GPU      = 0     //!< Temperature sensor for the GPU die
} nvmlTemperatureSensors_t;

/** 
 * Compute mode. 
 *
 * NVML_COMPUTEMODE_EXCLUSIVE_PROCESS was added in CUDA 4.0.
 * Earlier CUDA versions supported a single exclusive mode, 
 * which is equivalent to NVML_COMPUTEMODE_EXCLUSIVE_THREAD in CUDA 4.0 and beyond.
 */
typedef enum nvmlComputeMode_enum 
{
    NVML_COMPUTEMODE_DEFAULT           = 0,  //!< Default compute mode -- multiple contexts per device
    NVML_COMPUTEMODE_EXCLUSIVE_THREAD  = 1,  //!< Compute-exclusive-thread mode -- only one context per device, usable from one thread at a time
    NVML_COMPUTEMODE_PROHIBITED        = 2,  //!< Compute-prohibited mode -- no contexts per device
    NVML_COMPUTEMODE_EXCLUSIVE_PROCESS = 3   //!< Compute-exclusive-process mode -- only one context per device, usable from multiple threads at a time
} nvmlComputeMode_t;

/** 
 * ECC bit types.
 *
 * @deprecated See \ref nvmlMemoryErrorType_t for a more flexible type
 */
#define nvmlEccBitType_t nvmlMemoryErrorType_t

/**
 * Single bit ECC errors
 *
 * @deprecated Mapped to \ref NVML_MEMORY_ERROR_TYPE_CORRECTED
 */
#define NVML_SINGLE_BIT_ECC NVML_MEMORY_ERROR_TYPE_CORRECTED

/**
 * Double bit ECC errors
 *
 * @deprecated Mapped to \ref NVML_MEMORY_ERROR_TYPE_UNCORRECTED
 */
#define NVML_DOUBLE_BIT_ECC NVML_MEMORY_ERROR_TYPE_UNCORRECTED

/**
 * Memory error types
 */
typedef enum nvmlMemoryErrorType_enum
{
    /**
     * A memory error that was corrected
     * 
     * For ECC errors, these are single bit errors
     * For Texture memory, these are errors fixed by resend
     */
    NVML_MEMORY_ERROR_TYPE_CORRECTED = 0,
    /**
     * A memory error that was not corrected
     * 
     * For ECC errors, these are double bit errors
     * For Texture memory, these are errors where the resend fails
     */
    NVML_MEMORY_ERROR_TYPE_UNCORRECTED = 1,
    
    
    // Keep this last
    NVML_MEMORY_ERROR_TYPE_COUNT //!< Count of memory error types

} nvmlMemoryErrorType_t;

/** 
 * ECC counter types. 
 *
 * Note: Volatile counts are reset each time the driver loads. On Windows this is once per boot. On Linux this can be more frequent.
 *       On Linux the driver unloads when no active clients exist. If persistence mode is enabled or there is always a driver 
 *       client active (e.g. X11), then Linux also sees per-boot behavior. If not, volatile counts are reset each time a compute app
 *       is run.
 */
typedef enum nvmlEccCounterType_enum 
{
    NVML_VOLATILE_ECC      = 0,      //!< Volatile counts are reset each time the driver loads.
    NVML_AGGREGATE_ECC     = 1       //!< Aggregate counts persist across reboots (i.e. for the lifetime of the device)
} nvmlEccCounterType_t;

/** 
 * Clock types. 
 * 
 * All speeds are in Mhz.
 */
typedef enum nvmlClockType_enum 
{
    NVML_CLOCK_GRAPHICS  = 0,        //!< Graphics clock domain
    NVML_CLOCK_SM        = 1,        //!< SM clock domain
    NVML_CLOCK_MEM       = 2,        //!< Memory clock domain
    NVML_CLOCK_COUNT
} nvmlClockType_t;

/** 
 * Driver models. 
 *
 * Windows only.
 */
typedef enum nvmlDriverModel_enum 
{
    NVML_DRIVER_WDDM      = 0,       //!< WDDM driver model -- GPU treated as a display device
    NVML_DRIVER_WDM       = 1        //!< WDM (TCC) model (recommended) -- GPU treated as a generic device
} nvmlDriverModel_t;

/**
 * Allowed PStates.
 */
typedef enum nvmlPStates_enum 
{
    NVML_PSTATE_0               = 0,       //!< Performance state 0 -- Maximum Performance
    NVML_PSTATE_1               = 1,       //!< Performance state 1 
    NVML_PSTATE_2               = 2,       //!< Performance state 2
    NVML_PSTATE_3               = 3,       //!< Performance state 3
    NVML_PSTATE_4               = 4,       //!< Performance state 4
    NVML_PSTATE_5               = 5,       //!< Performance state 5
    NVML_PSTATE_6               = 6,       //!< Performance state 6
    NVML_PSTATE_7               = 7,       //!< Performance state 7
    NVML_PSTATE_8               = 8,       //!< Performance state 8
    NVML_PSTATE_9               = 9,       //!< Performance state 9
    NVML_PSTATE_10              = 10,      //!< Performance state 10
    NVML_PSTATE_11              = 11,      //!< Performance state 11
    NVML_PSTATE_12              = 12,      //!< Performance state 12
    NVML_PSTATE_13              = 13,      //!< Performance state 13
    NVML_PSTATE_14              = 14,      //!< Performance state 14
    NVML_PSTATE_15              = 15,      //!< Performance state 15 -- Minimum Performance 
    NVML_PSTATE_UNKNOWN         = 32       //!< Unknown performance state
} nvmlPstates_t;

/**
 * GPU Operation Mode
 *
 * GOM allows to reduce power usage and optimize GPU throughput by disabling GPU features.
 *
 * Each GOM is designed to meet specific user needs.
 */
typedef enum nvmlGom_enum
{
    NVML_GOM_ALL_ON                    = 0, //!< Everything is enabled and running at full speed

    NVML_GOM_COMPUTE                   = 1, //!< Designed for running only compute tasks. Graphics operations
                                            //!< are not allowed

    NVML_GOM_LOW_DP                    = 2  //!< Designed for running graphics applications that don't require
                                            //!< high bandwidth double precision
} nvmlGpuOperationMode_t;

/** 
 * Available infoROM objects.
 */
typedef enum nvmlInforomObject_enum 
{
    NVML_INFOROM_OEM            = 0,       //!< An object defined by OEM
    NVML_INFOROM_ECC            = 1,       //!< The ECC object determining the level of ECC support
    NVML_INFOROM_POWER          = 2,       //!< The power management object

    // Keep this last
    NVML_INFOROM_COUNT                     //!< This counts the number of infoROM objects the driver knows about
} nvmlInforomObject_t;

/** 
 * Return values for NVML API calls. 
 */
typedef enum nvmlReturn_enum 
{
    NVML_SUCCESS = 0,                   //!< The operation was successful
    NVML_ERROR_UNINITIALIZED = 1,       //!< NVML was not first initialized with nvmlInit()
    NVML_ERROR_INVALID_ARGUMENT = 2,    //!< A supplied argument is invalid
    NVML_ERROR_NOT_SUPPORTED = 3,       //!< The requested operation is not available on target device
    NVML_ERROR_NO_PERMISSION = 4,       //!< The current user does not have permission for operation
    NVML_ERROR_ALREADY_INITIALIZED = 5, //!< Deprecated: Multiple initializations are now allowed through ref counting
    NVML_ERROR_NOT_FOUND = 6,           //!< A query to find an object was unsuccessful
    NVML_ERROR_INSUFFICIENT_SIZE = 7,   //!< An input argument is not large enough
    NVML_ERROR_INSUFFICIENT_POWER = 8,  //!< A device's external power cables are not properly attached
    NVML_ERROR_DRIVER_NOT_LOADED = 9,   //!< NVIDIA driver is not loaded
    NVML_ERROR_TIMEOUT = 10,            //!< User provided timeout passed
    NVML_ERROR_IRQ_ISSUE = 11,          //!< NVIDIA Kernel detected an interrupt issue with a GPU
    NVML_ERROR_LIBRARY_NOT_FOUND = 12,  //!< NVML Shared Library couldn't be found or loaded
    NVML_ERROR_FUNCTION_NOT_FOUND = 13, //!< Local version of NVML doesn't implement this function
    NVML_ERROR_CORRUPTED_INFOROM = 14,  //!< infoROM is corrupted
    NVML_ERROR_UNKNOWN = 999            //!< An internal driver error occurred
} nvmlReturn_t;

/**
 * Memory locations
 *
 * See \ref nvmlDeviceGetMemoryErrorCounter
 */
typedef enum nvmlMemoryLocation_enum
{
    NVML_MEMORY_LOCATION_L1_CACHE = 0,       //!< GPU L1 Cache
    NVML_MEMORY_LOCATION_L2_CACHE = 1,       //!< GPU L2 Cache
    NVML_MEMORY_LOCATION_DEVICE_MEMORY = 2,  //!< GPU Device Memory
    NVML_MEMORY_LOCATION_REGISTER_FILE = 3,  //!< GPU Register File
    NVML_MEMORY_LOCATION_TEXTURE_MEMORY = 4, //!< GPU Texture Memory
    
    // Keep this last
    NVML_MEMORY_LOCATION_COUNT              //!< This counts the number of memory locations the driver knows about
} nvmlMemoryLocation_t;

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlUnitStructs Unit Structs
 *  @{
 */
/***************************************************************************************************/

typedef struct nvmlUnit_st* nvmlUnit_t;

/** 
 * Description of HWBC entry 
 */
typedef struct nvmlHwbcEntry_st 
{
    unsigned int hwbcId;
    char firmwareVersion[32];
} nvmlHwbcEntry_t;

/** 
 * Fan state enum. 
 */
typedef enum nvmlFanState_enum 
{
    NVML_FAN_NORMAL       = 0,     //!< Fan is working properly
    NVML_FAN_FAILED       = 1      //!< Fan has failed
} nvmlFanState_t;

/** 
 * Led color enum. 
 */
typedef enum nvmlLedColor_enum 
{
    NVML_LED_COLOR_GREEN       = 0,     //!< GREEN, indicates good health
    NVML_LED_COLOR_AMBER       = 1      //!< AMBER, indicates problem
} nvmlLedColor_t;


/** 
 * LED states for an S-class unit.
 */
typedef struct nvmlLedState_st 
{
    char cause[256];               //!< If amber, a text description of the cause
    nvmlLedColor_t color;          //!< GREEN or AMBER
} nvmlLedState_t;

/** 
 * Static S-class unit info.
 */
typedef struct nvmlUnitInfo_st 
{
    char name[96];                      //!< Product name
    char id[96];                        //!< Product identifier
    char serial[96];                    //!< Product serial number
    char firmwareVersion[96];           //!< Firmware version
} nvmlUnitInfo_t;

/** 
 * Power usage information for an S-class unit.
 * The power supply state is a human readable string that equals "Normal" or contains
 * a combination of "Abnormal" plus one or more of the following:
 *    
 *    - High voltage
 *    - Fan failure
 *    - Heatsink temperature
 *    - Current limit
 *    - Voltage below UV alarm threshold
 *    - Low-voltage
 *    - SI2C remote off command
 *    - MOD_DISABLE input
 *    - Short pin transition 
*/
typedef struct nvmlPSUInfo_st 
{
    char state[256];                 //!< The power supply state
    unsigned int current;            //!< PSU current (A)
    unsigned int voltage;            //!< PSU voltage (V)
    unsigned int power;              //!< PSU power draw (W)
} nvmlPSUInfo_t;

/** 
 * Fan speed reading for a single fan in an S-class unit.
 */
typedef struct nvmlUnitFanInfo_st 
{
    unsigned int speed;              //!< Fan speed (RPM)
    nvmlFanState_t state;            //!< Flag that indicates whether fan is working properly
} nvmlUnitFanInfo_t;

/** 
 * Fan speed readings for an entire S-class unit.
 */
typedef struct nvmlUnitFanSpeeds_st 
{
    nvmlUnitFanInfo_t fans[24];      //!< Fan speed data for each fan
    unsigned int count;              //!< Number of fans in unit
} nvmlUnitFanSpeeds_t;

/** @} */

/***************************************************************************************************/
/** @addtogroup nvmlEvents 
 *  @{
 */
/***************************************************************************************************/

/** 
 * Handle to an event set
 */
typedef struct nvmlEventSet_st* nvmlEventSet_t;

/** @defgroup nvmlEventType Event Types
 * @{
 * Event Types which user can be notified about.
 * See description of particular functions for details.
 *
 * See \ref nvmlDeviceRegisterEvents and \ref nvmlDeviceGetSupportedEventTypes to check which devices 
 * support each event.
 *
 * Types can be combined with bitwise or operator '|' when passed to \ref nvmlDeviceRegisterEvents
 */
//! Event about single bit ECC errors
#define nvmlEventTypeSingleBitEccError     0x0000000000000001LL

//! Event about double bit ECC errors
#define nvmlEventTypeDoubleBitEccError     0x0000000000000002LL

//! Event about PState changes
/**
 *  \note On Fermi architecture PState changes are also an indicator that GPU is throttling down due to
 *  no work being executed on the GPU, power capping or thermal capping. In a typical situation,
 *  Fermi-based GPU should stay in P0 for the duration of the execution of the compute process.
 */
#define nvmlEventTypePState                0x0000000000000004LL

//! Event that Xid critical error occurred
#define nvmlEventTypeXidCriticalError      0x0000000000000008LL

//! Event about clock changes
/**
 * Kepler only
 */
#define nvmlEventTypeClock                 0x0000000000000010LL

//! Mask with no events
#define nvmlEventTypeNone                  0x0000000000000000LL
//! Mask of all events
#define nvmlEventTypeAll (nvmlEventTypeNone    \
        | nvmlEventTypeSingleBitEccError       \
        | nvmlEventTypeDoubleBitEccError       \
        | nvmlEventTypePState                  \
        | nvmlEventTypeClock                   \
        | nvmlEventTypeXidCriticalError        \
        )
/** @} */

/** 
 * Information about occurred event
 */
typedef struct nvmlEventData_st
{
    nvmlDevice_t        device;         //!< Specific device where the event occurred
    unsigned long long  eventType;      //!< Information about what specific event occurred
    unsigned long long  reserved;
} nvmlEventData_t;

/** @} */

/***************************************************************************************************/
/** @addtogroup nvmlClocksThrottleReasons
 *  @{
 */
/***************************************************************************************************/

/** Nothing is running on the GPU and the clocks are dropping to Idle state
 * \note This limiter may be removed in a later release
 */
#define nvmlClocksThrottleReasonGpuIdle                   0x0000000000000001LL

/** GPU clocks are limited by user specified limit
 *
 * @see nvmlDeviceSetApplicationsClocks
 * @see nvmlDeviceGetApplicationsClock
 */
#define nvmlClocksThrottleReasonUserDefinedClocks         0x0000000000000002LL

/** SW Power Scaling algorithm is reducing the clocks below requested clocks 
 *
 * @see nvmlDeviceGetPowerUsage
 * @see nvmlDeviceSetPowerManagementLimit
 * @see nvmlDeviceGetPowerManagementLimit
 */
#define nvmlClocksThrottleReasonSwPowerCap                0x0000000000000004LL

/** HW Slowdown (reducing the core clocks by a factor of 2 or more) is engaged
 * 
 * This is an indicator of:
 *   - temperature being too high
 *   - External Power Brake Assertion is triggered (e.g. by the system power supply)
 *   - Power draw is too high and Fast Trigger protection is reducing the clocks
 *   - May be also reported during PState or clock change
 *      - This behavior may be removed in a later release.
 *
 * @see nvmlDeviceGetTemperature
 * @see nvmlDeviceGetPowerUsage
 */
#define nvmlClocksThrottleReasonHwSlowdown                0x0000000000000008LL

/** Some other unspecified factor is reducing the clocks */
#define nvmlClocksThrottleReasonUnknown                   0x8000000000000000LL

/** Bit mask representing no clocks throttling
 *
 * Clocks are as high as possible.
 * */
#define nvmlClocksThrottleReasonNone                      0x0000000000000000LL

/** Bit mask representing all supported clocks throttling reasons 
 * New reasons might be added to this list in the future
 */
#define nvmlClocksThrottleReasonAll (nvmlClocksThrottleReasonNone \
      | nvmlClocksThrottleReasonGpuIdle                           \
      | nvmlClocksThrottleReasonUserDefinedClocks                 \
      | nvmlClocksThrottleReasonSwPowerCap                        \
      | nvmlClocksThrottleReasonHwSlowdown                        \
      | nvmlClocksThrottleReasonUnknown                           \
        ) 
/** @} */

/***************************************************************************************************/
/** @defgroup nvmlInitializationAndCleanup Initialization and Cleanup
 * This chapter describes the methods that handle NVML initialization and cleanup.
 * It is the user's responsibility to call \ref nvmlInit() before calling any other methods, and 
 * nvmlShutdown() once NVML is no longer being used.
 *  @{
 */
/***************************************************************************************************/

/**
 * Initialize NVML by discovering and attaching to all GPU devices in the system.
 * 
 * For all products.
 *
 * This method should be called once before invoking any other methods in the library.
 * A reference count of the number of initializations is maintained.  Shutdown only occurs
 * when the reference count reaches zero.
 * 
 * @return 
 *         - \ref NVML_SUCCESS                   if NVML has been properly initialized
 *         - \ref NVML_ERROR_NO_PERMISSION       if the user doesn't have permission to talk to any device
 *         - \ref NVML_ERROR_DRIVER_NOT_LOADED   if NVIDIA driver is not running
 *         - \ref NVML_ERROR_INSUFFICIENT_POWER  if any devices have improperly attached external power cables
 *         - \ref NVML_ERROR_UNKNOWN             on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlInit(void);

/**
 * Shut down NVML by releasing all GPU resources previously allocated with \ref nvmlInit().
 * 
 * For all products.
 *
 * This method should be called after NVML work is done, once for each call to \ref nvmlInit()
 * A reference count of the number of initializations is maintained.  Shutdown only occurs
 * when the reference count reaches zero.  For backwards compatibility, no error is reported if
 * nvmlShutdown() is called more times than nvmlInit().
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if NVML has been properly shut down
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlShutdown(void);

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlErrorReporting Error reporting
 * This chapter describes helper functions for error reporting routines.
 *  @{
 */
/***************************************************************************************************/

/**
 * Helper method for converting NVML error codes into readable strings.
 *
 * For all products
 *
 * @param result                               NVML error code to convert
 *
 * @return String representation of the error.
 *
 */
const DECLDIR char* nvmlErrorString(nvmlReturn_t result);
/** @} */


/***************************************************************************************************/
/** @defgroup nvmlConstants Constants
 *  @{
 */
/***************************************************************************************************/

/**
 * Buffer size guaranteed to be large enough for \ref nvmlDeviceGetInforomVersion and \ref nvmlDeviceGetInforomImageVersion
 */
#define NVML_DEVICE_INFOROM_VERSION_BUFFER_SIZE       16

/**
 * Buffer size guaranteed to be large enough for \ref nvmlDeviceGetUUID
 */
#define NVML_DEVICE_UUID_BUFFER_SIZE                  80

/**
 * Buffer size guaranteed to be large enough for \ref nvmlSystemGetDriverVersion
 */
#define NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE        80

/**
 * Buffer size guaranteed to be large enough for \ref nvmlSystemGetNVMLVersion
 */
#define NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE          80

/**
 * Buffer size guaranteed to be large enough for \ref nvmlDeviceGetName
 */
#define NVML_DEVICE_NAME_BUFFER_SIZE                  64

/**
 * Buffer size guaranteed to be large enough for \ref nvmlDeviceGetSerial
 */
#define NVML_DEVICE_SERIAL_BUFFER_SIZE                30

/**
 * Buffer size guaranteed to be large enough for \ref nvmlDeviceGetVbiosVersion
 */
#define NVML_DEVICE_VBIOS_VERSION_BUFFER_SIZE         32

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlSystemQueries System Queries
 * This chapter describes the queries that NVML can perform against the local system. These queries
 * are not device-specific.
 *  @{
 */
/***************************************************************************************************/

/**
 * Retrieves the version of the system's graphics driver.
 * 
 * For all products.
 *
 * The version identifier is an alphanumeric string.  It will not exceed 80 characters in length
 * (including the NULL terminator).  See \ref nvmlConstants::NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE.
 *
 * @param version                              Reference in which to return the version identifier
 * @param length                               The maximum allowed length of the string returned in \a version
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a version is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small 
 */
nvmlReturn_t DECLDIR nvmlSystemGetDriverVersion(char *version, unsigned int length);

/**
 * Retrieves the version of the NVML library.
 * 
 * For all products.
 *
 * The version identifier is an alphanumeric string.  It will not exceed 80 characters in length
 * (including the NULL terminator).  See \ref nvmlConstants::NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE.
 *
 * @param version                              Reference in which to return the version identifier
 * @param length                               The maximum allowed length of the string returned in \a version
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a version is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small 
 */
nvmlReturn_t DECLDIR nvmlSystemGetNVMLVersion(char *version, unsigned int length);

/**
 * Gets name of the process with provided process id
 *
 * For all products.
 *
 * Returned process name is cropped to provided length.
 * name string is encoded in ANSI.
 *
 * @param pid                                  The identifier of the process
 * @param name                                 Reference in which to return the process name
 * @param length                               The maximum allowed length of the string returned in \a name
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a name has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a name is NULL
 *         - \ref NVML_ERROR_NOT_FOUND         if process doesn't exists
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlSystemGetProcessName(unsigned int pid, char *name, unsigned int length);

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlUnitQueries Unit Queries
 * This chapter describes that queries that NVML can perform against each unit. For S-class systems only.
 * In each case the device is identified with an nvmlUnit_t handle. This handle is obtained by 
 * calling \ref nvmlUnitGetHandleByIndex().
 *  @{
 */
/***************************************************************************************************/

 /**
 * Retrieves the number of units in the system.
 *
 * For S-class products.
 *
 * @param unitCount                            Reference in which to return the number of units
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a unitCount has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unitCount is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetCount(unsigned int *unitCount);

/**
 * Acquire the handle for a particular unit, based on its index.
 *
 * For S-class products.
 *
 * Valid indices are derived from the \a unitCount returned by \ref nvmlUnitGetCount(). 
 *   For example, if \a unitCount is 2 the valid indices are 0 and 1, corresponding to UNIT 0 and UNIT 1.
 *
 * The order in which NVML enumerates units has no guarantees of consistency between reboots.
 *
 * @param index                                The index of the target unit, >= 0 and < \a unitCount
 * @param unit                                 Reference in which to return the unit handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a unit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a index is invalid or \a unit is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetHandleByIndex(unsigned int index, nvmlUnit_t *unit);

/**
 * Retrieves the static information associated with a unit.
 *
 * For S-class products.
 *
 * See \ref nvmlUnitInfo_t for details on available unit info.
 *
 * @param unit                                 The identifier of the target unit
 * @param info                                 Reference in which to return the unit information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a info has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a info is NULL
 */
nvmlReturn_t DECLDIR nvmlUnitGetUnitInfo(nvmlUnit_t unit, nvmlUnitInfo_t *info);

/**
 * Retrieves the LED state associated with this unit.
 *
 * For S-class products.
 *
 * See \ref nvmlLedState_t for details on allowed states.
 *
 * @param unit                                 The identifier of the target unit
 * @param state                                Reference in which to return the current LED state
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a state has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a state is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlUnitSetLedState()
 */
nvmlReturn_t DECLDIR nvmlUnitGetLedState(nvmlUnit_t unit, nvmlLedState_t *state);

/**
 * Retrieves the PSU stats for the unit.
 *
 * For S-class products.
 *
 * See \ref nvmlPSUInfo_t for details on available PSU info.
 *
 * @param unit                                 The identifier of the target unit
 * @param psu                                  Reference in which to return the PSU information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a psu has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a psu is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetPsuInfo(nvmlUnit_t unit, nvmlPSUInfo_t *psu);

/**
 * Retrieves the temperature readings for the unit, in degrees C.
 *
 * For S-class products.
 *
 * Depending on the product, readings may be available for intake (type=0), 
 * exhaust (type=1) and board (type=2).
 *
 * @param unit                                 The identifier of the target unit
 * @param type                                 The type of reading to take
 * @param temp                                 Reference in which to return the intake temperature
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a temp has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit or \a type is invalid or \a temp is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetTemperature(nvmlUnit_t unit, unsigned int type, unsigned int *temp);

/**
 * Retrieves the fan speed readings for the unit.
 *
 * For S-class products.
 *
 * See \ref nvmlUnitFanSpeeds_t for details on available fan speed info.
 *
 * @param unit                                 The identifier of the target unit
 * @param fanSpeeds                            Reference in which to return the fan speed information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a fanSpeeds has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid or \a fanSpeeds is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlUnitGetFanSpeedInfo(nvmlUnit_t unit, nvmlUnitFanSpeeds_t *fanSpeeds);

/**
 * Retrieves the set of GPU devices that are attached to the specified unit.
 *
 * For S-class products.
 *
 * The \a deviceCount argument is expected to be set to the size of the input \a devices array.
 *
 * @param unit                                 The identifier of the target unit
 * @param deviceCount                          Reference in which to provide the \a devices array size, and
 *                                             to return the number of attached GPU devices
 * @param devices                              Reference in which to return the references to the attached GPU devices
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a deviceCount and \a devices have been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a deviceCount indicates that the \a devices array is too small
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit is invalid, either of \a deviceCount or \a devices is NULL
 */
nvmlReturn_t DECLDIR nvmlUnitGetDevices(nvmlUnit_t unit, unsigned int *deviceCount, nvmlDevice_t *devices);

/**
 * Retrieves the IDs and firmware versions for any Host Interface Cards (HICs) in the system.
 * 
 * For S-class products.
 *
 * The \a hwbcCount argument is expected to be set to the size of the input \a hwbcEntries array.
 * The HIC must be connected to an S-class system for it to be reported by this function.
 *
 * @param hwbcCount                            Size of hwbcEntries array
 * @param hwbcEntries                          Array holding information about hwbc
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a hwbcCount and \a hwbcEntries have been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if either \a hwbcCount or \a hwbcEntries is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a hwbcCount indicates that the \a hwbcEntries array is too small
 */
nvmlReturn_t DECLDIR nvmlSystemGetHicVersion(unsigned int *hwbcCount, nvmlHwbcEntry_t *hwbcEntries);
/** @} */

/***************************************************************************************************/
/** @defgroup nvmlDeviceQueries Device Queries
 * This chapter describes that queries that NVML can perform against each device.
 * In each case the device is identified with an nvmlDevice_t handle. This handle is obtained by  
 * calling one of \ref nvmlDeviceGetHandleByIndex(), \ref nvmlDeviceGetHandleBySerial() or 
 * \ref nvmlDeviceGetHandleByPciBusId(). 
 *  @{
 */
/***************************************************************************************************/

 /**
 * Retrieves the number of compute devices in the system. A compute device is a single GPU.
 * 
 * For all products.
 *
 * On some platforms not all devices may be accessible due to permission restrictions. In these
 * cases the device count will reflect only the GPUs that NVML can access.
 *
 * @param deviceCount                          Reference in which to return the number of accessible devices
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a deviceCount has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a deviceCount is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetCount(unsigned int *deviceCount);

/**
 * Acquire the handle for a particular device, based on its index.
 * 
 * For all products.
 *
 * Valid indices are derived from the \a accessibleDevices count returned by 
 *   \ref nvmlDeviceGetCount(). For example, if \a accessibleDevices is 2 the valid indices  
 *   are 0 and 1, corresponding to GPU 0 and GPU 1.
 *
 * The order in which NVML enumerates devices has no guarantees of consistency between reboots. For that reason it
 *   is recommended that devices be looked up by their PCI ids or board serial numbers. See 
 *   \ref nvmlDeviceGetHandleBySerial() and \ref nvmlDeviceGetHandleByPciBusId().
 *
 * @param index                                The index of the target GPU, >= 0 and < \a accessibleDevices
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a index is invalid or \a device is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t *device);

/**
 * Acquire the handle for a particular device, based on its board serial number.
 *
 * For all products.
 *
 * This number corresponds to the value printed directly on the board, and to the value returned by
 *   \ref nvmlDeviceGetSerial().
 *
 * @deprecated Since more than one GPU can exist on a single board this function is deprecated in favor 
 *             of \ref nvmlDeviceGetHandleByUUID.
 *             For dual GPU boards this function will return NVML_ERROR_INVALID_ARGUMENT.
 *
 * @param serial                               The board serial number of the target GPU
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a serial is invalid, \a device is NULL or more than one
 *                                             device has the same serial (dual GPU boards)
 *         - \ref NVML_ERROR_NOT_FOUND         if \a serial does not match a valid device on the system
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetSerial
 * @see nvmlDeviceGetHandleByUUID
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleBySerial(const char *serial, nvmlDevice_t *device);

/**
 * Acquire the handle for a particular device, based on its globally unique immutable UUID associated with each device.
 *
 * For all products.
 *
 * @param uuid                                 The UUID of the target GPU
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a uuid is invalid or \a device is null
 *         - \ref NVML_ERROR_NOT_FOUND         if \a uuid does not match a valid device on the system
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetUUID
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleByUUID(const char *uuid, nvmlDevice_t *device);

/**
 * Acquire the handle for a particular device, based on its PCI bus id.
 * 
 * For all products.
 *
 * This value corresponds to the nvmlPciInfo_t::busId returned by \ref nvmlDeviceGetPciInfo().
 *
 * @param pciBusId                             The PCI bus id of the target GPU
 * @param device                               Reference in which to return the device handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a device has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a pciBusId is invalid or \a device is NULL
 *         - \ref NVML_ERROR_NOT_FOUND         if \a pciBusId does not match a valid device on the system
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetHandleByPciBusId(const char *pciBusId, nvmlDevice_t *device);

/**
 * Retrieves the name of this device. 
 * 
 * For all products.
 *
 * The name is an alphanumeric string that denotes a particular product, e.g. Tesla &tm; C2070. It will not
 * exceed 64 characters in length (including the NULL terminator).  See \ref
 * nvmlConstants::NVML_DEVICE_NAME_BUFFER_SIZE.
 *
 * @param device                               The identifier of the target device
 * @param name                                 Reference in which to return the product name
 * @param length                               The maximum allowed length of the string returned in \a name
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a name has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, or \a name is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small
 */
nvmlReturn_t DECLDIR nvmlDeviceGetName(nvmlDevice_t device, char *name, unsigned int length);

/**
 * Retrieves the globally unique board serial number associated with this device's board.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * The serial number is an alphanumeric string that will not exceed 30 characters (including the NULL terminator).
 * This number matches the serial number tag that is physically attached to the board.  See \ref
 * nvmlConstants::NVML_DEVICE_SERIAL_BUFFER_SIZE.
 *
 * @param device                               The identifier of the target device
 * @param serial                               Reference in which to return the board/module serial number
 * @param length                               The maximum allowed length of the string returned in \a serial
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a serial has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, or \a serial is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 */
nvmlReturn_t DECLDIR nvmlDeviceGetSerial(nvmlDevice_t device, char *serial, unsigned int length);

/**
 * Retrieves the globally unique immutable UUID associated with this device, as a 5 part hexadecimal string,
 * that augments the immutable, board serial identifier.
 *
 * For all CUDA capable GPUs.
 *
 * The UUID is a globally unique identifier. It is the only available identifier for pre-Fermi-architecture products.
 * It does NOT correspond to any identifier printed on the board.  It will not exceed 80 characters in length
 * (including the NULL terminator).  See \ref nvmlConstants::NVML_DEVICE_UUID_BUFFER_SIZE.
 *
 * @param device                               The identifier of the target device
 * @param uuid                                 Reference in which to return the GPU UUID
 * @param length                               The maximum allowed length of the string returned in \a uuid
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a uuid has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, or \a uuid is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small 
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetUUID(nvmlDevice_t device, char *uuid, unsigned int length);

/**
 * Retrieves the version information for the device's infoROM object.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * Fermi and higher parts have non-volatile on-board memory for persisting device info, such as aggregate 
 * ECC counts. The version of the data structures in this memory may change from time to time. It will not
 * exceed 16 characters in length (including the NULL terminator).
 * See \ref nvmlConstants::NVML_DEVICE_INFOROM_VERSION_BUFFER_SIZE.
 *
 * See \ref nvmlInforomObject_t for details on the available infoROM objects.
 *
 * @param device                               The identifier of the target device
 * @param object                               The target infoROM object
 * @param version                              Reference in which to return the infoROM version
 * @param length                               The maximum allowed length of the string returned in \a version
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a version is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small 
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have an infoROM
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetInforomImageVersion
 */
nvmlReturn_t DECLDIR nvmlDeviceGetInforomVersion(nvmlDevice_t device, nvmlInforomObject_t object, char *version, unsigned int length);

/**
 * Retrieves the global infoROM image version
 *
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 *
 * Image version just like VBIOS version uniquely describes the exact version of the infoROM flashed on the board 
 * in contrast to infoROM object version which is only an indicator of supported features.
 * Version string will not exceed 16 characters in length (including the NULL terminator).
 * See \ref nvmlConstants::NVML_DEVICE_INFOROM_VERSION_BUFFER_SIZE.
 *
 * @param device                               The identifier of the target device
 * @param version                              Reference in which to return the infoROM image version
 * @param length                               The maximum allowed length of the string returned in \a version
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a version is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small 
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have an infoROM
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetInforomVersion
 */
nvmlReturn_t DECLDIR nvmlDeviceGetInforomImageVersion(nvmlDevice_t device, char *version, unsigned int length);

/**
 * Retrieves the checksum of the configuration stored in the device's infoROM.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * Can be used to make sure that two GPUs have the exact same configuration.
 * Current checksum takes into account configuration stored in PWR and ECC infoROM objects.
 * Checksum can change between driver releases or when user changes configuration (e.g. disable/enable ECC)
 *
 * @param device                               The identifier of the target device
 * @param checksum                             Reference in which to return the infoROM configuration checksum
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a checksum has been set
 *         - \ref NVML_ERROR_CORRUPTED_INFOROM if the device's checksum couldn't be retrieved due to infoROM corruption
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a checksum is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error 
 */
nvmlReturn_t DECLDIR nvmlDeviceGetInforomConfigurationChecksum(nvmlDevice_t device, unsigned int *checksum);

/**
 * Reads the infoROM from the flash and verifies the checksums.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * @param device                               The identifier of the target device
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if infoROM is not corrupted
 *         - \ref NVML_ERROR_CORRUPTED_INFOROM if the device's infoROM is corrupted
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error 
 */
nvmlReturn_t DECLDIR nvmlDeviceValidateInforom(nvmlDevice_t device);

/**
 * Retrieves the display mode for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * This method indicates whether a physical display is currently connected to the device.
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifier of the target device
 * @param display                              Reference in which to return the display mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a display has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a display is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 */
nvmlReturn_t DECLDIR nvmlDeviceGetDisplayMode(nvmlDevice_t device, nvmlEnableState_t *display);

/**
 * Retrieves the persistence mode associated with this device.
 *
 * For all CUDA-capable products.
 * For Linux only.
 *
 * When driver persistence mode is enabled the driver software state is not torn down when the last 
 * client disconnects. By default this feature is disabled. 
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifier of the target device
 * @param mode                                 Reference in which to return the current driver persistence mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetPersistenceMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t *mode);

/**
 * Retrieves the PCI attributes of this device.
 * 
 * For all products.
 *
 * See \ref nvmlPciInfo_t for details on the available PCI info.
 *
 * @param device                               The identifier of the target device
 * @param pci                                  Reference in which to return the PCI info
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a pci has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a pci is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t *pci);

/**
 * Retrieves the maximum PCIe link generation possible with this device and system
 *
 * I.E. for a generation 2 PCIe device attached to a generation 1 PCIe bus the max link generation this function will
 * report is generation 1.
 * 
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * 
 * @param device                               The identifier of the target device
 * @param maxLinkGen                           Reference in which to return the max PCIe link generation
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a maxLinkGen has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a maxLinkGen is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if PCIe link information is not available
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetMaxPcieLinkGeneration(nvmlDevice_t device, unsigned int *maxLinkGen);

/**
 * Retrieves the maximum PCIe link width possible with this device and system
 *
 * I.E. for a device with a 16x PCIe bus width attached to a 8x PCIe system bus this function will report
 * a max link width of 8.
 * 
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * 
 * @param device                               The identifier of the target device
 * @param maxLinkWidth                         Reference in which to return the max PCIe link generation
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a maxLinkWidth has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a maxLinkWidth is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if PCIe link information is not available
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetMaxPcieLinkWidth(nvmlDevice_t device, unsigned int *maxLinkWidth);

/**
 * Retrieves the current PCIe link generation
 * 
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * 
 * @param device                               The identifier of the target device
 * @param currLinkGen                          Reference in which to return the max PCIe link generation
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a currLinkGen has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a currLinkGen is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if PCIe link information is not available
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetCurrPcieLinkGeneration(nvmlDevice_t device, unsigned int *currLinkGen);

/**
 * Retrieves the current PCIe link width
 * 
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * 
 * @param device                               The identifier of the target device
 * @param currLinkWidth                        Reference in which to return the max PCIe link generation
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a currLinkWidth has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a currLinkWidth is null
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if PCIe link information is not available
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetCurrPcieLinkWidth(nvmlDevice_t device, unsigned int *currLinkWidth);

/**
 * Retrieves the current clock speeds for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * See \ref nvmlClockType_t for details on available clock information.
 *
 * @param device                               The identifier of the target device
 * @param type                                 Identify which clock domain to query
 * @param clock                                Reference in which to return the clock speed in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a clock has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clock is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot report the specified clock
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetClockInfo(nvmlDevice_t device, nvmlClockType_t type, unsigned int *clock);

/**
 * Retrieves the maximum clock speeds for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * See \ref nvmlClockType_t for details on available clock information.
 *
 * @param device                               The identifier of the target device
 * @param type                                 Identify which clock domain to query
 * @param clock                                Reference in which to return the clock speed in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a clock has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clock is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot report the specified clock
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetMaxClockInfo(nvmlDevice_t device, nvmlClockType_t type, unsigned int *clock);

/**
 * Retrieves the clock that applications will use unless an overspec situation occurs.
 * Can be changed using \ref nvmlDeviceSetApplicationsClocks.
 *
 * For Tesla &tm; products, and Quadro &reg; products from the Kepler family.
 *
 * @param device                               The identifier of the target device
 * @param clockType                            Identify which clock domain to query
 * @param clockMHz                             Reference in which to return the clock in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if new settings were successfully set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_NOT_FOUND         if the max clock limit is not set
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clockMHz is NULL or \a clockType is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot report the specified clock 
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetApplicationsClock(nvmlDevice_t device, nvmlClockType_t clockType, unsigned int *clockMHz);

/**
 * Resets the application clock to the default value
 *
 * This is the applications clock that will be used after system reboot or driver reload.
 * Default value is constant, but the current value an be changed using \ref nvmlDeviceSetApplicationsClocks.
 *
 * @see nvmlDeviceGetApplicationsClock
 * @see nvmlDeviceSetApplicationsClock
 *
 * For Tesla &tm; products, and Quadro &reg; products from the Kepler family.
 *
 * @param device                               The identifier of the target device
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if new settings were successfully set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_NOT_FOUND         if the max clock limit is not set
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot perform this action
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceResetApplicationsClocks(nvmlDevice_t device);

/**
 * Retrieves the list of possible memory clocks that can be used as an argument for \ref nvmlDeviceSetApplicationsClocks.
 *
 * For Tesla &tm; products, and Quadro &reg; products from the Kepler family.
 *
 * @param device                               The identifier of the target device
 * @param count                                Reference in which to provide the \a clocksMHz array size, and
 *                                             to return the number of elements
 * @param clocksMHz                            Reference in which to return the clock in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if new settings were successfully set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_NOT_FOUND         if the max clock limit is not set
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clock is NULL or \a clockType is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot report the specified clock 
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a count is too small 
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetApplicationsClocks
 * @see nvmlDeviceGetSupportedGraphicsClocks
 */
nvmlReturn_t DECLDIR nvmlDeviceGetSupportedMemoryClocks(nvmlDevice_t device, unsigned int *count, unsigned int *clocksMHz);

/**
 * Retrieves the list of possible graphics clocks that can be used as an argument for \ref nvmlDeviceSetApplicationsClocks.
 *
 * For Tesla &tm; products, and Quadro &reg; products from the Kepler family.
 *
 * @param device                               The identifier of the target device
 * @param memoryClockMHz                       Memory clock for which to return possible graphics clocks
 * @param count                                Reference in which to provide the \a clocksMHz array size, and
 *                                             to return the number of elements
 * @param clocksMHz                            Reference in which to return the clocks in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if new settings were successfully set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_NOT_FOUND         if the max clock limit is not set
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clock is NULL or \a clockType is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device cannot report the specified clock 
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a count is too small 
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetApplicationsClocks
 * @see nvmlDeviceGetSupportedMemoryClocks
 */
nvmlReturn_t DECLDIR nvmlDeviceGetSupportedGraphicsClocks(nvmlDevice_t device, unsigned int memoryClockMHz, unsigned int *count, unsigned int *clocksMHz);


/**
 * Retrieves the intended operating speed of the device's fan.
 *
 * Note: The reported speed is the intended fan speed.  If the fan is physically blocked and unable to spin, the
 * output will not match the actual fan speed.
 * 
 * For all discrete products with dedicated fans.
 *
 * The fan speed is expressed as a percent of the maximum, i.e. full speed is 100%.
 *
 * @param device                               The identifier of the target device
 * @param speed                                Reference in which to return the fan speed percentage
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a speed has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a speed is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have a fan
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int *speed);

/**
 * Retrieves the current temperature readings for the device, in degrees C. 
 * 
 * For all discrete and S-class products.
 *
 * See \ref nvmlTemperatureSensors_t for details on available temperature sensors.
 *
 * @param device                               The identifier of the target device
 * @param sensorType                           Flag that indicates which sensor reading to retrieve
 * @param temp                                 Reference in which to return the temperature reading
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a temp has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, \a sensorType is invalid or \a temp is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not have the specified sensor
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t sensorType, unsigned int *temp);

/**
 * Retrieves the current performance state for the device. 
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * See \ref nvmlPstates_t for details on allowed performance states.
 *
 * @param device                               The identifier of the target device
 * @param pState                               Reference in which to return the performance state reading
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a pState has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a pState is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPerformanceState(nvmlDevice_t device, nvmlPstates_t *pState);

/**
 * Retrieves current clocks throttling reasons.
 *
 * For Tesla &tm; products from Kepler family.
 *
 * \note More than one bit can be enabled at the same time. Multiple reasons can be affecting clocks at once.
 *
 * @param device                                The identifier of the target device
 * @param clocksThrottleReasons                 Reference in which to return bitmask of active clocks throttle
 *                                                  reasons
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a clocksThrottleReasons has been returned successfully
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a clocksThrottleReasons is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlClocksThrottleReasons
 * @see nvmlDeviceGetSupportedClocksThrottleReasons
 */
nvmlReturn_t DECLDIR nvmlDeviceGetCurrentClocksThrottleReasons(nvmlDevice_t device, unsigned long long *clocksThrottleReasons);

/**
 * Retrieves bitmask of supported clocks throttle reasons that can be returned by 
 * \ref nvmlDeviceGetCurrentClocksThrottleReasons
 *
 * For all devices
 *
 * @param device                               The identifier of the target device
 * @param supportedClocksThrottleReasons       Reference in which to return bitmask of supported
 *                                              clocks throttle reasons
 *
 * @return 
 *         - \ref NVML_SUCCESS                 if \a supportedClocksThrottleReasons has been returned successfully
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a supportedClocksThrottleReasons is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlClocksThrottleReasons
 * @see nvmlDeviceGetCurrentClocksThrottleReasons
 */
nvmlReturn_t DECLDIR nvmlDeviceGetSupportedClocksThrottleReasons(nvmlDevice_t device, unsigned long long *supportedClocksThrottleReasons);

/**
 * Deprecated: Use \ref nvmlDeviceGetPerformanceState. This function exposes an incorrect generalization.
 *
 * Retrieve the current performance state for the device. 
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * See \ref nvmlPstates_t for details on allowed performance states.
 *
 * @param device                               The identifier of the target device
 * @param pState                               Reference in which to return the performance state reading
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a pState has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a pState is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerState(nvmlDevice_t device, nvmlPstates_t *pState);

/**
 * Retrieves the power management mode associated with this device.
 *
 * For "GF11x" Tesla &tm; and Quadro &reg; products from the Fermi family.
 *     - Requires \a NVML_INFOROM_POWER version 3.0 or higher.
 *
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 *     - Does not require \a NVML_INFOROM_POWER object.
 *
 * This flag indicates whether any power management algorithm is currently active on the device. An 
 * enabled state does not necessarily mean the device is being actively throttled -- only that 
 * that the driver will do so if the appropriate conditions are met.
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifier of the target device
 * @param mode                                 Reference in which to return the current power management mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerManagementMode(nvmlDevice_t device, nvmlEnableState_t *mode);

/**
 * Retrieves the power management limit associated with this device.
 *
 * For "GF11x" Tesla &tm; and Quadro &reg; products from the Fermi family.
 *     - Requires \a NVML_INFOROM_POWER version 3.0 or higher.
 *
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 *     - Does not require \a NVML_INFOROM_POWER object.
 *
 * The power limit defines the upper boundary for the card's power draw. If
 * the card's total power draw reaches this limit the power management algorithm kicks in.
 *
 * This reading is only available if power management mode is supported. 
 * See \ref nvmlDeviceGetPowerManagementMode.
 *
 * @param device                               The identifier of the target device
 * @param limit                                Reference in which to return the power management limit in milliwatts
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a limit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a limit is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerManagementLimit(nvmlDevice_t device, unsigned int *limit);

/**
 * Retrieves information about possible values of power management limits on this device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 *
 * @param device                               The identifier of the target device
 * @param minLimit                             Reference in which to return the minimum power management limit in milliwatts
 * @param maxLimit                             Reference in which to return the maximum power management limit in milliwatts
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a minLimit and \a maxLimit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a minLimit or \a maxLimit is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetPowerManagementLimit
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerManagementLimitConstraints(nvmlDevice_t device, unsigned int *minLimit, unsigned int *maxLimit);

/**
 * Retrieves default power management limit on this device, in milliwatts.
 * Default power management limit is a power management limit that the device boots with.
 *
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 *
 * @param device                               The identifier of the target device
 * @param defaultLimit                         Reference in which to return the default power management limit in milliwatts
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a defaultLimit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a defaultLimit is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerManagementDefaultLimit(nvmlDevice_t device, unsigned int *defaultLimit);

/**
 * Retrieves power usage for this GPU in milliwatts and its associated circuitry (e.g. memory)
 *
 * For "GF11x" Tesla &tm; and Quadro &reg; products from the Fermi family.
 *     - Requires \a NVML_INFOROM_POWER version 3.0 or higher.
 *
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 *     - Does not require \a NVML_INFOROM_POWER object.
 *
 * On Fermi and Kepler GPUs the reading is accurate to within +/- 5% of current power draw.
 *
 * It is only available if power management mode is supported. See \ref nvmlDeviceGetPowerManagementMode.
 *
 * @param device                               The identifier of the target device
 * @param power                                Reference in which to return the power usage information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a power has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a power is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support power readings
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int *power);

/**
 * Retrieves the current GOM and pending GOM (the one that GPU will switch to after reboot).
 *
 * For GK110 M-class and X-class Tesla &tm; products from the Kepler family.
 * Not supported on Quadro &reg; and Tesla &tm; C-class products.
 *
 * @param device                               The identifier of the target device
 * @param current                              Reference in which to return the current GOM
 * @param pending                              Reference in which to return the pending GOM
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a current or \a pending is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlGpuOperationMode_t
 * @see nvmlDeviceSetGpuOperationMode
 */
nvmlReturn_t DECLDIR nvmlDeviceGetGpuOperationMode(nvmlDevice_t device, nvmlGpuOperationMode_t *current, nvmlGpuOperationMode_t *pending);

/**
 * Retrieves the amount of used, free and total memory available on the device, in bytes.
 * 
 * For all products.
 *
 * Enabling ECC reduces the amount of total available memory, due to the extra required parity bits.
 * Under WDDM most device memory is allocated and managed on startup by Windows.
 *
 * Under Linux and Windows TCC, the reported amount of used memory is equal to the sum of memory allocated 
 * by all active channels on the device.
 *
 * See \ref nvmlMemory_t for details on available memory info.
 *
 * @param device                               The identifier of the target device
 * @param memory                               Reference in which to return the memory information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a memory has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a memory is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t *memory);

/**
 * Retrieves the current compute mode for the device.
 *
 * For all CUDA-capable products.
 *
 * See \ref nvmlComputeMode_t for details on allowed compute modes.
 *
 * @param device                               The identifier of the target device
 * @param mode                                 Reference in which to return the current compute mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetComputeMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetComputeMode(nvmlDevice_t device, nvmlComputeMode_t *mode);

/**
 * Retrieves the current and pending ECC modes for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher.
 *
 * Changing ECC modes requires a reboot. The "pending" ECC mode refers to the target mode following
 * the next reboot.
 *
 * See \ref nvmlEnableState_t for details on allowed modes.
 *
 * @param device                               The identifier of the target device
 * @param current                              Reference in which to return the current ECC mode
 * @param pending                              Reference in which to return the pending ECC mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a current and \a pending have been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or either \a current or \a pending is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceSetEccMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetEccMode(nvmlDevice_t device, nvmlEnableState_t *current, nvmlEnableState_t *pending);

/**
 * Retrieves the total ECC error counts for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher.
 * Requires ECC Mode to be enabled.
 *
 * The total error count is the sum of errors across each of the separate memory systems, i.e. the total set of 
 * errors across the entire device.
 *
 * See \ref nvmlMemoryErrorType_t for a description of available error types.\n
 * See \ref nvmlEccCounterType_t for a description of available counter types.
 *
 * @param device                               The identifier of the target device
 * @param errorType                            Flag that specifies the type of the errors. 
 * @param counterType                          Flag that specifies the counter-type of the errors. 
 * @param eccCounts                            Reference in which to return the specified ECC errors
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a eccCounts has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device, \a errorType or \a counterType is invalid, or \a eccCounts is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceClearEccErrorCounts()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetTotalEccErrors(nvmlDevice_t device, nvmlMemoryErrorType_t errorType, nvmlEccCounterType_t counterType, unsigned long long *eccCounts);

/**
 * Retrieves the detailed ECC error counts for the device.
 *
 * @deprecated   This API supports only a fixed set of ECC error locations
 *               On different GPU architectures different locations are supported
 *               See \ref nvmlDeviceGetMemoryErrorCounter
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * Requires \a NVML_INFOROM_ECC version 2.0 or higher to report aggregate location-based ECC counts.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher to report all other ECC counts.
 * Requires ECC Mode to be enabled.
 *
 * Detailed errors provide separate ECC counts for specific parts of the memory system.
 *
 * Reports zero for unsupported ECC error counters when a subset of ECC error counters are supported.
 *
 * See \ref nvmlMemoryErrorType_t for a description of available bit types.\n
 * See \ref nvmlEccCounterType_t for a description of available counter types.\n
 * See \ref nvmlEccErrorCounts_t for a description of provided detailed ECC counts.
 *
 * @param device                               The identifier of the target device
 * @param errorType                            Flag that specifies the type of the errors. 
 * @param counterType                          Flag that specifies the counter-type of the errors. 
 * @param eccCounts                            Reference in which to return the specified ECC errors
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a eccCounts has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device, \a errorType or \a counterType is invalid, or \a eccCounts is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceClearEccErrorCounts()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetDetailedEccErrors(nvmlDevice_t device, nvmlMemoryErrorType_t errorType, nvmlEccCounterType_t counterType, nvmlEccErrorCounts_t *eccCounts);

/**
 * Retrieves the requested memory error counter for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi family.
 * Requires \a NVML_INFOROM_ECC version 2.0 or higher to report aggregate location-based memory error counts.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher to report all other memory error counts.
 *
 * For all Tesla &tm; and Quadro &reg; products from the Kepler family.
 *
 * Requires ECC Mode to be enabled.
 *
 * See \ref nvmlMemoryErrorType_t for a description of available memory error types.\n
 * See \ref nvmlEccCounterType_t for a description of available counter types.\n
 * See \ref nvmlMemoryLocation_t for a description of available counter locations.\n
 * 
 * @param device                               The identifier of the target device
 * @param errorType                            Flag that specifies the type of error.
 * @param counterType                          Flag that specifies the counter-type of the errors. 
 * @param locationType                         Specifies the location of the counter. 
 * @param count                                Reference in which to return the ECC counter
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a count has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device, \a bitTyp,e \a counterType or \a locationType is
 *                                             invalid, or \a count is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support ECC error reporting in the specified memory
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetMemoryErrorCounter(nvmlDevice_t device, nvmlMemoryErrorType_t errorType,
                                                   nvmlEccCounterType_t counterType,
                                                   nvmlMemoryLocation_t locationType, unsigned long long *count);

/**
 * Retrieves the current utilization rates for the device's major subsystems.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * See \ref nvmlUtilization_t for details on available utilization rates.
 *
 * @param device                               The identifier of the target device
 * @param utilization                          Reference in which to return the utilization information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a utilization has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a utilization is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t *utilization);

/**
 * Retrieves the current and pending driver model for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * For windows only.
 *
 * On Windows platforms the device driver can run in either WDDM or WDM (TCC) mode. If a display is attached
 * to the device it must run in WDDM mode. TCC mode is preferred if a display is not attached.
 *
 * See \ref nvmlDriverModel_t for details on available driver models.
 *
 * @param device                               The identifier of the target device
 * @param current                              Reference in which to return the current driver model
 * @param pending                              Reference in which to return the pending driver model
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a current and \a pending have been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or both \a current and \a pending are NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the platform is not windows
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlDeviceSetDriverModel()
 */
nvmlReturn_t DECLDIR nvmlDeviceGetDriverModel(nvmlDevice_t device, nvmlDriverModel_t *current, nvmlDriverModel_t *pending);

/**
 * Get VBIOS version of the device.
 *
 * For all products.
 *
 * The VBIOS version may change from time to time. It will not exceed 32 characters in length 
 * (including the NULL terminator).  See \ref nvmlConstants::NVML_DEVICE_VBIOS_VERSION_BUFFER_SIZE.
 *
 * @param device                               The identifier of the target device
 * @param version                              Reference to which to return the VBIOS version
 * @param length                               The maximum allowed length of the string returned in \a version
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a version has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, or \a version is NULL
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a length is too small 
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceGetVbiosVersion(nvmlDevice_t device, char *version, unsigned int length);

/**
 * Get information about processes with a compute context on a device
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * This function returns information only about compute running processes (e.g. CUDA application which have
 * active context). Any graphics applications (e.g. using OpenGL, DirectX) won't be listed by this function.
 *
 * To query the current number of running compute processes, call this function with *infoCount = 0. The
 * return code will be NVML_ERROR_INSUFFICIENT_SIZE, or NVML_SUCCESS if none are running. For this call
 * \a infos is allowed to be NULL.
 *
 * Keep in mind that information returned by this call is dynamic and the number of elements might change in
 * time. Allocate more space for \a infos table in case new compute processes are spawned.
 *
 * @param device                               The identifier of the target device
 * @param infoCount                            Reference in which to provide the \a infos array size, and
 *                                             to return the number of returned elements
 * @param infos                                Reference in which to return the process information
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a infoCount and \a infos have been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INSUFFICIENT_SIZE if \a infoCount indicates that the \a infos array is too small
 *                                             \a infoCount will contain minimal amount of space necessary for
 *                                             the call to complete
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid, either of \a infoCount or \a infos is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see \ref nvmlSystemGetProcessName
 */
nvmlReturn_t DECLDIR nvmlDeviceGetComputeRunningProcesses(nvmlDevice_t device, unsigned int *infoCount, nvmlProcessInfo_t *infos);

/**
 * Check if the GPU devices are on the same physical board.
 *
 * @param device1                               The first GPU device
 * @param device2                               The second GPU device
 * @param onSameBoard                           Reference in which to return the status.
 *                                              Non-zero indicates that the GPUs are on the same board.
 *
 * @return
 *         - \ref NVML_SUCCESS                 when onSameBoard has been populated
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a dev1, \a dev2 or \a onSameBoard are invalid
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceOnSameBoard(nvmlDevice_t device1, nvmlDevice_t device2, int *onSameBoard);

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlUnitCommands Unit Commands
 *  This chapter describes NVML operations that change the state of the unit. For S-class products.
 *  Each of these requires root/admin access. Non-admin users will see an NVML_ERROR_NO_PERMISSION
 *  error code when invoking any of these methods.
 *  @{
 */
/***************************************************************************************************/

/**
 * Set the LED state for the unit. The LED can be either green (0) or amber (1).
 *
 * For S-class products.
 * Requires root/admin permissions.
 *
 * This operation takes effect immediately.
 * 
 *
 * <b>Current S-Class products don't provide unique LEDs for each unit. As such, both front 
 * and back LEDs will be toggled in unison regardless of which unit is specified with this command.</b>
 *
 * See \ref nvmlLedColor_t for available colors.
 *
 * @param unit                                 The identifier of the target unit
 * @param color                                The target LED color
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the LED color has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a unit or \a color is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if this is not an S-class product
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlUnitGetLedState()
 */
nvmlReturn_t DECLDIR nvmlUnitSetLedState(nvmlUnit_t unit, nvmlLedColor_t color);

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlDeviceCommands Device Commands
 *  This chapter describes NVML operations that change the state of the device.
 *  Each of these requires root/admin access. Non-admin users will see an NVML_ERROR_NO_PERMISSION
 *  error code when invoking any of these methods.
 *  @{
 */
/***************************************************************************************************/

/**
 * Set the persistence mode for the device.
 *
 * For all CUDA-capable products.
 * For Linux only.
 * Requires root/admin permissions.
 *
 * The persistence mode determines whether the GPU driver software is torn down after the last client
 * exits.
 *
 * This operation takes effect immediately. It is not persistent across reboots. After each reboot the
 * persistence mode is reset to "Disabled".
 *
 * See \ref nvmlEnableState_t for available modes.
 *
 * @param device                               The identifier of the target device
 * @param mode                                 The target persistence mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the persistence mode was set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetPersistenceMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t mode);

/**
 * Set the compute mode for the device.
 *
 * For all CUDA-capable products.
 * Requires root/admin permissions.
 *
 * The compute mode determines whether a GPU can be used for compute operations and whether it can
 * be shared across contexts.
 *
 * This operation takes effect immediately. Under Linux it is not persistent across reboots and
 * always resets to "Default". Under windows it is persistent.
 *
 * Under windows compute mode may only be set to DEFAULT when running in WDDM
 *
 * See \ref nvmlComputeMode_t for details on available compute modes.
 *
 * @param device                               The identifier of the target device
 * @param mode                                 The target compute mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the compute mode was set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetComputeMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetComputeMode(nvmlDevice_t device, nvmlComputeMode_t mode);

/**
 * Set the ECC mode for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher.
 * Requires root/admin permissions.
 *
 * The ECC mode determines whether the GPU enables its ECC support.
 *
 * This operation takes effect after the next reboot.
 *
 * See \ref nvmlEnableState_t for details on available modes.
 *
 * @param device                               The identifier of the target device
 * @param ecc                                  The target ECC mode
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the ECC mode was set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a ecc is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetEccMode()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetEccMode(nvmlDevice_t device, nvmlEnableState_t ecc);  

/**
 * Clear the ECC error counts for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * Requires \a NVML_INFOROM_ECC version 2.0 or higher to clear aggregate location-based ECC counts.
 * Requires \a NVML_INFOROM_ECC version 1.0 or higher to clear all other ECC counts.
 * Requires root/admin permissions.
 * Requires ECC Mode to be enabled.
 *
 * Sets all of the specified ECC counters to 0, including both detailed and total counts.
 *
 * This operation takes effect immediately.
 *
 * See \ref nvmlEccCounterType_t for details on available counter types.
 *
 * @param device                               The identifier of the target device
 * @param counterType                          Flag that indicates which type of errors should be cleared.
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the error counts were cleared
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a counterType is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see 
 *      - nvmlDeviceGetDetailedEccErrors()
 *      - nvmlDeviceGetTotalEccErrors()
 */
nvmlReturn_t DECLDIR nvmlDeviceClearEccErrorCounts(nvmlDevice_t device, nvmlEccCounterType_t counterType);

/**
 * Set the driver model for the device.
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * For windows only.
 * Requires root/admin permissions.
 *
 * On Windows platforms the device driver can run in either WDDM or WDM (TCC) mode. If a display is attached
 * to the device it must run in WDDM mode.  
 *
 * It is possible to force the change to WDM (TCC) while the display is still attached with a force flag (nvmlFlagForce).
 * This should only be done if the host is subsequently powered down and the display is detached from the device
 * before the next reboot. 
 *
 * This operation takes effect after the next reboot.
 * 
 * Windows driver model may only be set to WDDM when running in DEFAULT compute mode.
 *
 * Change driver model to WDDM is not supported when GPU doesn't support graphics acceleration or 
 * will not support it after reboot. See \ref nvmlDeviceSetGpuOperationMode.
 *
 * See \ref nvmlDriverModel_t for details on available driver models.
 * See \ref nvmlFlagDefault and \ref nvmlFlagForce
 *
 * @param device                               The identifier of the target device
 * @param driverModel                          The target driver model
 * @param flags                                Flags that change the default behavior
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the driver model has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a driverModel is invalid
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the platform is not windows or the device does not support this feature
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlDeviceGetDriverModel()
 */
nvmlReturn_t DECLDIR nvmlDeviceSetDriverModel(nvmlDevice_t device, nvmlDriverModel_t driverModel, unsigned int flags);

/**
 * Set clocks that applications will lock to.
 *
 * Sets the clocks that compute and graphics applications will be running at.
 * e.g. CUDA driver requests these clocks during context creation which means this property 
 * defines clocks at which CUDA applications will be running unless some overspec event
 * occurs (e.g. over power, over thermal or external HW brake).
 *
 * Can be used as a setting to request constant performance.
 *
 * For Tesla &tm; products, and Quadro &reg; products from the Kepler family.
 * Requires root/admin permissions. 
 *
 * See \ref nvmlDeviceGetSupportedMemoryClocks and \ref nvmlDeviceGetSupportedGraphicsClocks 
 * for details on how to list available clocks combinations.
 *
 * After system reboot or driver reload applications clocks go back to their default value.
 *
 * @param device                               The identifier of the target device
 * @param memClockMHz                          Requested memory clock in MHz
 * @param graphicsClockMHz                     Requested graphics clock in MHz
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if new settings were successfully set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a memClockMHz and \a graphicsClockMHz 
 *                                                 is not a valid clock combination
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation 
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device doesn't support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 */
nvmlReturn_t DECLDIR nvmlDeviceSetApplicationsClocks(nvmlDevice_t device, unsigned int memClockMHz, unsigned int graphicsClockMHz);

/**
 * Set new power limit of this device.
 * 
 * For Tesla &tm; and Quadro &reg; products from the Kepler family.
 * Requires root/admin permissions.
 *
 * See \ref nvmlDeviceGetPowerManagementLimitConstraints to check the allowed ranges of values.
 *
 * \note Limit is not persistent across reboots or driver unloads.
 * Enable persistent mode to prevent driver from unloading when no application is using the device.
 *
 * @param device                               The identifier of the target device
 * @param limit                                Power management limit in milliwatts to set
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a limit has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a defaultLimit is out of range
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support this feature
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlDeviceGetPowerManagementLimitConstraints
 * @see nvmlDeviceGetPowerManagementDefaultLimit
 */
nvmlReturn_t DECLDIR nvmlDeviceSetPowerManagementLimit(nvmlDevice_t device, unsigned int limit);

/**
 * Sets new GOM. See \a nvmlGpuOperationMode_t for details.
 *
 * For GK110 M-class and X-class Tesla &tm; products from the Kepler family.
 * Not supported on Quadro &reg; and Tesla &tm; C-class products.
 * Requires root/admin permissions.
 * 
 * Changing GOMs requires a reboot. 
 * The reboot requirement might be removed in the future.
 *
 * Compute only GOMs don't support graphics acceleration. Under windows switching to these GOMs when
 * pending driver model is WDDM is not supported. See \ref nvmlDeviceSetDriverModel.
 * 
 * @param device                               The identifier of the target device
 * @param mode                                 Target GOM
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if \a mode has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a device is invalid or \a mode incorrect
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the device does not support GOM or specific mode
 *         - \ref NVML_ERROR_NO_PERMISSION     if the user doesn't have permission to perform this operation
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 *
 * @see nvmlGpuOperationMode_t
 * @see nvmlDeviceGetGpuOperationMode
 */
nvmlReturn_t DECLDIR nvmlDeviceSetGpuOperationMode(nvmlDevice_t device, nvmlGpuOperationMode_t mode);

/** @} */

/***************************************************************************************************/
/** @defgroup nvmlEvents Event Handling Methods
 * This chapter describes methods that NVML can perform against each device to register and wait for 
 * some event to occur.
 *  @{
 */
/***************************************************************************************************/

/**
 * Create an empty set of events.
 * Event set should be freed by \ref nvmlEventSetFree
 *
 * @param set                                  Reference in which to return the event handle
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the event has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a set is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlEventSetFree
 */
nvmlReturn_t DECLDIR nvmlEventSetCreate(nvmlEventSet_t *set);

/**
 * Starts recording of events on a specified devices and add the events to specified \ref nvmlEventSet_t
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 * Ecc events are available only on ECC enabled devices (see \ref nvmlDeviceGetTotalEccErrors)
 * Power capping events are available only on Power Management enabled devices (see \ref nvmlDeviceGetPowerManagementMode)
 *
 * For Linux only.
 *
 * \b IMPORTANT: Operations on \a set are not thread safe
 *
 * This call starts recording of events on specific device.
 * All events that occurred before this call are not recorded.
 * Checking if some event occurred can be done with \ref nvmlEventSetWait
 *
 * @param device                               The identifier of the target device
 * @param eventTypes                           Bitmask of \ref nvmlEventType to record
 * @param set                                  Set to which add new event types
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the event has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a eventTypes is invalid or \a set is NULL
 *         - \ref NVML_ERROR_NOT_SUPPORTED     if the platform does not support this feature or some of requested event types
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlEventType
 * @see nvmlDeviceGetSupportedEventTypes
 * @see nvmlEventSetWait
 * @see nvmlEventSetFree
 */
nvmlReturn_t DECLDIR nvmlDeviceRegisterEvents(nvmlDevice_t device, unsigned long long eventTypes, nvmlEventSet_t set);

/**
 * Returns information about events supported on device
 *
 * For all products.
 *
 * Events are not supported on Windows. So this function returns an empty mask in \a eventTypes on Windows.
 *
 * @param device                               The identifier of the target device
 * @param eventTypes                           Reference in which to return bitmask of supported events
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the eventTypes has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a eventType is NULL
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlEventType
 * @see nvmlDeviceRegisterEvents
 */
nvmlReturn_t DECLDIR nvmlDeviceGetSupportedEventTypes(nvmlDevice_t device, unsigned long long *eventTypes);

/**
 * Waits on events and delivers events
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * If some events are ready to be delivered at the time of the call, function returns immediately.
 * If there are no events ready to be delivered, function sleeps till event arrives 
 * but not longer than specified timeout. This function in certain conditions can return before
 * specified timeout passes (e.g. when interrupt arrives)
 * 
 * @param set                                  Reference to set of events to wait on
 * @param data                                 Reference in which to return event data
 * @param timeoutms                            Maximum amount of wait time in milliseconds for registered event
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the data has been set
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_INVALID_ARGUMENT  if \a data is NULL
 *         - \ref NVML_ERROR_TIMEOUT           if no event arrived in specified timeout or interrupt arrived
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlEventType
 * @see nvmlDeviceRegisterEvents
 */
nvmlReturn_t DECLDIR nvmlEventSetWait(nvmlEventSet_t set, nvmlEventData_t * data, unsigned int timeoutms);

/**
 * Releases events in the set
 *
 * For Tesla &tm; and Quadro &reg; products from the Fermi and Kepler families.
 *
 * @param set                                  Reference to events to be released 
 * 
 * @return 
 *         - \ref NVML_SUCCESS                 if the event has been successfully released
 *         - \ref NVML_ERROR_UNINITIALIZED     if the library has not been successfully initialized
 *         - \ref NVML_ERROR_UNKNOWN           on any unexpected error
 * 
 * @see nvmlDeviceRegisterEvents
 */
nvmlReturn_t DECLDIR nvmlEventSetFree(nvmlEventSet_t set);

/** @} */

/**
 * NVML API versioning support
 */
#if defined(__NVML_API_VERSION_INTERNAL)
#undef nvmlDeviceGetPciInfo
#endif

#ifdef __cplusplus
}
#endif

#endif
